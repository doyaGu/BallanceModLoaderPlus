#define BML_LOADER_IMPLEMENTATION
#include <bml_module.hpp>
#include "bml_topics.h"
#include "bml_scripting.h"

#include "ScriptEngine.h"
#include "ScriptInstanceManager.h"
#include "RuntimeProvider.h"
#include "ModuleScope.h"
#include "CoroutineManager.h"
#include "bindings/BindCore.h"
#include "bindings/BindImc.h"
#include "bindings/BindVirtools.h"
#include "bindings/BindConfig.h"
#include "bindings/BindInput.h"
#include "bindings/BindTimer.h"
#include "bindings/BindCoroutine.h"
#include "bindings/ScriptVxMath.h"
#include "bindings/ScriptCK2.h"
#include "bindings/ScriptNativePointer.h"
#include "bindings/ScriptNativeBuffer.h"

// Global pointers for the published interface callbacks.
static asIScriptEngine *g_ScriptEnginePtr = nullptr;
static BML::Scripting::ScriptInstanceManager *g_InstanceMgrPtr = nullptr;
static BML_ScriptEngineInterface g_EngineInterface = {};

class ScriptingMod : public bml::Module {
    BML::Scripting::ScriptEngine m_Engine;
    std::unique_ptr<BML::Scripting::ScriptInstanceManager> m_Manager;
    std::unique_ptr<BML::Scripting::CoroutineManager> m_Coroutines;
    bml::imc::SubscriptionManager m_Subs;
    bml::PublishedInterface m_EngineService;

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        services.Log().Info("script", "Initializing AngelScript runtime");

        BML::Scripting::g_Builtins = &Services().Builtins();

        if (!m_Engine.Initialize()) {
            services.Log().Error("script", "Failed to create AngelScript engine");
            return BML_RESULT_FAIL;
        }
        m_Engine.SetGetProc(m_GetProc);
        g_ScriptEnginePtr = m_Engine.Get();

        m_Manager = std::make_unique<BML::Scripting::ScriptInstanceManager>(m_Engine.Get());
        BML::Scripting::SetInstanceManager(m_Manager.get());
        g_InstanceMgrPtr = m_Manager.get();

        // Register BML's own script-facing bindings
        BML::Scripting::RegisterCoreBindings(m_Engine.Get());
        BML::Scripting::RegisterImcBindings(m_Engine.Get(), m_Manager.get());
        RegisterNativePointer(m_Engine.Get());
        RegisterNativeBuffer(m_Engine.Get());
        RegisterVxMath(m_Engine.Get());
        RegisterCK2(m_Engine.Get());
        BML::Scripting::RegisterVirtoolsBindings(m_Engine.Get());
        BML::Scripting::RegisterConfigBindings(m_Engine.Get());
        BML::Scripting::RegisterInputBindings(m_Engine.Get());
        BML::Scripting::RegisterTimerBindings(m_Engine.Get(), m_Manager.get());

        m_Coroutines = std::make_unique<BML::Scripting::CoroutineManager>(
            m_Engine.Get(), m_Manager.get());
        m_Manager->SetCoroutineManager(m_Coroutines.get());
        BML::Scripting::RegisterCoroutineBindings(m_Engine.Get(), m_Coroutines.get());

        // Publish the engine interface so other modules can register
        // their own AS bindings before scripts are compiled.
        g_EngineInterface.header = BML_IFACE_HEADER(
            BML_ScriptEngineInterface,
            BML_SCRIPT_ENGINE_INTERFACE_ID, 1, 0);
        g_EngineInterface.GetEngine = +[]() -> asIScriptEngine * {
            return g_ScriptEnginePtr;
        };
        g_EngineInterface.GetModule = +[](BML_Mod mod) -> asIScriptModule * {
            return g_InstanceMgrPtr
                ? static_cast<asIScriptModule *>(g_InstanceMgrPtr->GetModulePtr(mod))
                : nullptr;
        };
        g_EngineInterface.IsScriptMod = +[](BML_Mod mod) -> BML_Bool {
            return (g_InstanceMgrPtr && g_InstanceMgrPtr->IsScript(mod))
                ? BML_TRUE : BML_FALSE;
        };
        g_EngineInterface.InvokeFunction = +[](BML_Mod mod, const char *fn) -> BML_Result {
            return g_InstanceMgrPtr ? g_InstanceMgrPtr->InvokeByName(mod, fn)
                                    : BML_RESULT_NOT_INITIALIZED;
        };
        g_EngineInterface.InvokeFunctionInt = +[](BML_Mod mod, const char *fn, int arg) -> BML_Result {
            return g_InstanceMgrPtr ? g_InstanceMgrPtr->InvokeByNameInt(mod, fn, arg)
                                    : BML_RESULT_NOT_INITIALIZED;
        };
        g_EngineInterface.InvokeFunctionString = +[](BML_Mod mod, const char *fn, const char *arg) -> BML_Result {
            return g_InstanceMgrPtr ? g_InstanceMgrPtr->InvokeByNameString(mod, fn, arg)
                                    : BML_RESULT_NOT_INITIALIZED;
        };
        m_EngineService = Publish(
            BML_SCRIPT_ENGINE_INTERFACE_ID,
            &g_EngineInterface, 1, 0, 0,
            BML_INTERFACE_FLAG_HOST_OWNED | BML_INTERFACE_FLAG_IMMUTABLE);
        if (!m_EngineService) {
            services.Log().Warn("script",
                "Failed to publish script engine interface (non-fatal)");
        }

        // Register runtime provider (triggers script loading later)
        BML_Result r = BML::Scripting::RegisterProvider(m_GetProc, "com.bml.scripting");
        if (r != BML_RESULT_OK) {
            services.Log().Error("script", "Failed to register runtime provider: %d",
                                 static_cast<int>(r));
            m_EngineService.Reset();
            m_Coroutines.reset();
            m_Manager.reset();
            BML::Scripting::SetInstanceManager(nullptr);
            g_ScriptEnginePtr = nullptr;
            m_Engine.Shutdown();
            return r;
        }

        // Subscribe to engine events
        m_Subs = services.CreateSubscriptions();
        m_Subs.Add(BML_TOPIC_CONSOLE_COMMAND, [this](const bml::imc::Message &msg) {
            OnConsoleCommand(msg);
        });
        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &) {
            m_Manager->SetVirtoolsReady(true);
            BML::Scripting::SetVirtoolsBindingsReady(true);
            BML::Scripting::SetInputBindingsReady(true);
            m_Manager->InitAll();
        });
        m_Subs.Add(BML_TOPIC_ENGINE_END, [this](const bml::imc::Message &) {
            BML::Scripting::SetInputBindingsReady(false);
            BML::Scripting::SetVirtoolsBindingsReady(false);
            m_Manager->SetVirtoolsReady(false);
        });
        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &) {
            m_Coroutines->Tick();
        });

        services.Log().Info("script", "AngelScript %s runtime ready", asGetLibraryVersion());
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        m_Subs = bml::imc::SubscriptionManager();
        BML::Scripting::UnregisterProvider(m_GetProc);
        m_EngineService.Reset();
        m_Manager->SetCoroutineManager(nullptr);
        m_Coroutines.reset();
        g_InstanceMgrPtr = nullptr;
        BML::Scripting::SetInstanceManager(nullptr);
        m_Manager.reset();
        g_ScriptEnginePtr = nullptr;
        m_Engine.Shutdown();
        BML::Scripting::g_Builtins = nullptr;
    }

private:
    void OnConsoleCommand(const bml::imc::Message &msg) {
        if (msg.Size() < sizeof(size_t) + sizeof(const char *)) return;
        auto *cmd_ptr = *reinterpret_cast<const char *const *>(
            static_cast<const char *>(msg.Data()) + sizeof(size_t));
        if (!cmd_ptr) return;

        std::string_view command(cmd_ptr);
        if (!command.starts_with("script")) return;

        auto args = command.substr(6);
        while (!args.empty() && args.front() == ' ') args.remove_prefix(1);

        if (args == "list") CmdList();
        else if (args.starts_with("reload")) {
            auto ra = args.substr(6);
            while (!ra.empty() && ra.front() == ' ') ra.remove_prefix(1);
            CmdReload(ra);
        }
    }

    void CmdList() {
        PrintToConsole("Script mods: " + std::to_string(m_Manager->GetInstanceCount()));
        m_Manager->ForEachInstance([this](const BML::Scripting::ScriptInstance &inst) {
            const char *s = "unknown";
            switch (inst.state) {
                case BML::Scripting::ScriptInstance::State::Unloaded: s = "unloaded"; break;
                case BML::Scripting::ScriptInstance::State::Compiled: s = "compiled"; break;
                case BML::Scripting::ScriptInstance::State::Attached: s = "attached"; break;
                case BML::Scripting::ScriptInstance::State::InitDone: s = "running"; break;
                case BML::Scripting::ScriptInstance::State::Error:    s = "ERROR"; break;
            }
            PrintToConsole("  " + inst.mod_id + " [" + s + "]");
        });
    }

    void CmdReload(std::string_view args) {
        if (args.empty() || args == "all") {
            m_Manager->ReloadAll();
            PrintToConsole("All scripts reloaded");
        } else {
            std::string id(args);
            BML_Result r = m_Manager->ReloadById(id);
            if (r == BML_RESULT_OK) PrintToConsole("Reloaded: " + id);
            else if (r == BML_RESULT_NOT_FOUND) PrintToConsole("Script not found: " + id);
            else PrintToConsole("Reload failed: " + id);
        }
    }

    void PrintToConsole(const std::string &message) {
        if (!BML::Scripting::g_Builtins || !BML::Scripting::g_Builtins->ImcBus) return;
        BML_TopicId id = BML_TOPIC_ID_INVALID;
        if (BML::Scripting::g_Builtins->ImcBus->GetTopicId(
                BML_TOPIC_CONSOLE_OUTPUT, &id) != BML_RESULT_OK) return;
        struct { size_t struct_size; const char *msg; uint32_t flags; } event{sizeof(event), message.c_str(), 0};
        BML::Scripting::g_Builtins->ImcBus->Publish(id, &event, sizeof(event));
    }
};

BML_DEFINE_MODULE(ScriptingMod)
