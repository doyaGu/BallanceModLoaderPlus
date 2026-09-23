#include "Loader/ModContext.h"

#include <exception>
#include <stdexcept>

#include <intrin.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <oniguruma.h>

#include "BML/BML.h"
#include "BML/Timer.h"
#include "Api/InterfaceRegistry.h"
#include "Api/CommandApi.h"
#include "Console/Shell/ShellExecutor.h"
#include "Console/Shell/ShellIo.h"
#include "Hooks/RenderHook.h"
#include "UI/FontRuntime.h"
#include "UI/Overlay.h"
#include "Logging/Logger.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/AngelScriptBindings.h"
#include "AngelScript/ScriptDevToolsService.h"
#endif
#include "StringUtils.h"
#include "PathUtils.h"

#include "Mods/BMLMod.h"
#include "Mods/NewBallTypeMod.h"

using namespace BML;

namespace {
    constexpr wchar_t kLoaderDirectoryName[] = L"ModLoader";
    constexpr wchar_t kTempDirectoryName[] = L"Temp";
    constexpr wchar_t kInstanceDirectoryName[] = L"Instance";
    constexpr wchar_t kShellStateFileName[] = L"CommandBar.shell.json";

    HMODULE ModuleFromAddress(const void *address) {
        if (!address)
            return nullptr;

        HMODULE module = nullptr;
        if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  reinterpret_cast<LPCSTR>(address),
                                  &module)) {
            return nullptr;
        }
        return module;
    }

    std::wstring ResolveGameDirectoryFromExecutable(const std::wstring &executablePath) {
        if (executablePath.empty())
            return {};

        return utils::GetParentDirectoryW(utils::GetParentDirectoryW(executablePath));
    }

    std::wstring ResolveLoaderDirectory(const std::wstring &gameDirectory) {
        if (gameDirectory.empty())
            return {};

        return utils::CombinePathW(gameDirectory, kLoaderDirectoryName);
    }

    std::wstring BuildFallbackTempDirectory(const std::wstring &baseTempDirectory, unsigned long processId) {
        wchar_t suffix[64] = {};
        _snwprintf(suffix, sizeof(suffix) / sizeof(suffix[0]), L"BML-%lu", processId);
        suffix[(sizeof(suffix) / sizeof(suffix[0])) - 1] = L'\0';
        return utils::CombinePathW(baseTempDirectory, suffix);
    }

    bool PrepareFreshDirectory(const std::wstring &directory) {
        if (directory.empty())
            return false;

        if (utils::FileExistsW(directory) && !utils::DeleteFileW(directory))
            return false;

        if (utils::DirectoryExistsW(directory) && !utils::DeleteDirectoryW(directory))
            return false;

        return utils::CreateFileTreeW(directory);
    }

    std::wstring CreateInstanceTempDirectory(const std::wstring &loaderDirectory, unsigned long processId) {
        std::wstring tempInstanceDirectory = utils::CreateTempFileW(L"BML");
        if (!tempInstanceDirectory.empty()) {
            utils::DeleteFileW(tempInstanceDirectory);
            if (utils::CreateDirectoryW(tempInstanceDirectory))
                return tempInstanceDirectory;

            tempInstanceDirectory.clear();
        }

        const std::wstring fallbackTempDirectory = BuildFallbackTempDirectory(utils::GetTempPathW(), processId);
        if (PrepareFreshDirectory(fallbackTempDirectory))
            return fallbackTempDirectory;

        const std::wstring loaderFallbackDirectory = utils::CombinePathW(
            utils::CombinePathW(loaderDirectory, kTempDirectoryName),
            kInstanceDirectoryName);
        if (utils::CreateFileTreeW(loaderFallbackDirectory))
            return loaderFallbackDirectory;

        return {};
    }

    BML::Behavior::Internal::GraphSource &RequireBehaviorGraph(
        BML::Behavior::Internal::Sessions &sessions) {
        BML::Behavior::Internal::GraphSource *graph = sessions.Graph();
        if (!graph)
            throw std::runtime_error("Behavior graph adapter is unavailable.");
        return *graph;
    }

}

ModContext *g_ModContext = nullptr;

namespace {
    bool ResolveDataShareOwner(const void *callerAddress, const char *ownerId,
                               const void *callbackAddress,
                               const void *cleanupAddress,
                               std::string &owner) {
        ModContext *context = BML_GetModContext();
        if (!context)
            return false;

        owner = context->GetNativeModOwnerId(callerAddress, ownerId);
        if (owner.empty() || (ownerId && owner != ownerId))
            return false;

        if ((!callbackAddress ||
             context->NativeModOwnsAddress(owner, callbackAddress)) &&
            (!cleanupAddress ||
             context->NativeModOwnsAddress(owner, cleanupAddress)))
            return true;

        const HMODULE loader = ModuleFromAddress(
            reinterpret_cast<const void *>(&BML_GetModContext));
        return loader && ModuleFromAddress(callerAddress) == loader &&
               ModuleFromAddress(callbackAddress) == loader &&
               (!cleanupAddress || ModuleFromAddress(cleanupAddress) == loader);
    }
}

ModContext *BML_GetModContext() {
    return g_ModContext;
}

CKContext *BML_GetCKContext() {
    return g_ModContext ? g_ModContext->GetCKContext() : nullptr;
}

CKRenderContext *BML_GetRenderContext() {
    return g_ModContext ? g_ModContext->GetRenderContext() : nullptr;
}

ModContext::ModContext(CKContext *context)
    : m_ObjectRefs(context),
      m_BehaviorPrototypes(BML::Behavior::Internal::MakeCKPrototypeSource(context)),
      m_Behaviors(context, [this](const void *object) {
          if (!object)
              return BML::Behavior::Internal::ObjectRef{};
          const BML_ObjectRef reference = m_ObjectRefs.Issue(
              const_cast<CKObject *>(static_cast<const CKObject *>(object)));
          return BML::Behavior::Internal::ObjectRef{
              reference.Domain, reference.Slot, reference.Generation};
      }, &m_BehaviorPrototypes),
      m_BehaviorSessions(
          m_Behaviors, &m_BehaviorPrototypes,
          BML::Behavior::Internal::MakeCKGraphSource(
              context, m_Behaviors, [this](const void *object) {
                  if (!object)
                      return BML::Behavior::Internal::ObjectRef{};
                  const BML_ObjectRef reference = m_ObjectRefs.Issue(
                      const_cast<CKObject *>(
                          static_cast<const CKObject *>(object)));
                  return BML::Behavior::Internal::ObjectRef{
                      reference.Domain, reference.Slot,
                      reference.Generation};
              })),
      m_BehaviorPatches(context, m_Behaviors, &m_BehaviorPrototypes,
                        RequireBehaviorGraph(m_BehaviorSessions),
                        [this](const BML::Behavior::Internal::ObjectRef &reference) {
                            return m_ObjectRefs.Resolve({
                                reference.Domain, reference.Slot,
                                reference.Generation});
                        },
                        [this](CKObject *object) {
                            const BML_ObjectRef issued =
                                m_ObjectRefs.Issue(object);
                            return BML::Behavior::Internal::ObjectRef{
                                issued.Domain, issued.Slot, issued.Generation};
                        }),
      m_BehaviorScripts(
          BML::Behavior::Internal::MakeCKScriptWorld(
              context, m_BehaviorPatches, [this](const void *object) {
                  if (!object)
                      return BML::Behavior::Internal::ObjectRef{};
                  const BML_ObjectRef reference = m_ObjectRefs.Issue(
                      const_cast<CKObject *>(
                          static_cast<const CKObject *>(object)));
                  return BML::Behavior::Internal::ObjectRef{
                      reference.Domain, reference.Slot,
                      reference.Generation};
              }),
          [this](std::string_view name,
                 const BML::Behavior::Internal::ObjectRef &script) {
              const BML::Behavior::Internal::Status status =
                  m_BehaviorPlans.LoadScript(std::string(name), script);
              if (!status && m_Logger)
                  m_Logger->Error(
                      "Failed to publish an authored Behavior Script: %s",
                      status.Message.c_str());
          }),
      m_PhysicsForce(context, m_Behaviors),
      m_ExecuteBB(m_Behaviors, m_PhysicsForce),
      m_Loader(*this) {
    assert(context != nullptr);
    m_CommandApi = std::make_unique<BML::Api::CommandApi>(*this);
    m_ImcRuntime.SetInvocationGate(&m_Loader.InvocationGate());
    m_CKContext = context;
    m_DataShare = DataShareStore::GetInstance("BML");
    if (m_DataShare) m_DataShare->AddRef();
    m_ShellDispatcher = std::make_unique<ShellDispatcher>(*this);
    m_Shell = std::make_unique<BML::Shell::Executor>(*m_ShellDispatcher, &m_ShellEnvironment, &m_ShellEnvironment);
    g_ModContext = this;
    DataShareStore::SetOwnerResolver(&ResolveDataShareOwner);
    DataShareStore::SetInvocationGate(&m_Loader.InvocationGate());
}

// The seam between the shell executor and the loader. Every simple command of a
// line comes back through Invoke; captured output is routed by the sink stack.
class ModContext::ShellDispatcher final : public BML::Shell::IDispatcher {
public:
    explicit ShellDispatcher(ModContext &context) : m_Context(context) {}

    int Invoke(const std::vector<std::string> &args, const std::string *input) override {
        return m_Context.InvokeCommandArgs(args, input);
    }

    void WriteError(std::string_view message) override {
        m_Context.WriteShellError(message);
    }

    void PushSink(BML::Shell::OutputSink *sink) override {
        m_Context.m_OutputSinks.push_back(sink);
    }

    void PopSink() override {
        if (!m_Context.m_OutputSinks.empty())
            m_Context.m_OutputSinks.pop_back();
    }

    void LogExecute(std::string_view line) override {
        if (m_Context.m_Logger)
            m_Context.m_Logger->Info("Execute Command: %s", std::string(line).c_str());
    }

private:
    ModContext &m_Context;
};

ModContext::~ModContext() {
    Shutdown();
    m_ImcRuntime.SetInvocationGate(nullptr);
    if (m_DataShare) m_DataShare->Release();
    DataShareStore::SetInvocationGate(nullptr);
    DataShareStore::SetOwnerResolver(nullptr);
    g_ModContext = nullptr;
}

bool ModContext::Init() {
    if (IsInited())
        return true;

    InitDirectories();

    m_UiFonts = std::make_unique<BML::UI::FontRuntime>(
        utils::CombinePathUtf8(m_LoaderDirUtf8, "Fonts"));

    InitLogger();

    m_Logger->Info("Initializing Mod Loader Plus version " BML_VERSION);
    m_Logger->Info("Website: https://github.com/doyaGu/BallanceModLoaderPlus");

    LoadShellEnvironment();

#ifdef _DEBUG
    m_Logger->Info("Player.exe Address: 0x%08x", ::GetModuleHandleA("Player.exe"));
    m_Logger->Info("CK2.dll Address: 0x%08x", ::GetModuleHandleA("CK2.dll"));
    m_Logger->Info("VxMath.dll Address: 0x%08x", ::GetModuleHandleA("VxMath.dll"));
#endif

    OnigEncoding encodings[3] = {ONIG_ENCODING_ASCII, ONIG_ENCODING_UTF8, ONIG_ENCODING_UTF16_LE};
    int err = onig_initialize(encodings, sizeof(encodings) / sizeof(encodings[0]));
    if (err < 0) {
        m_Logger->Error("Failed to initialize regular expression functionality");
        ShutdownLogger();
        return false;
    }

    if (!GetManagers()) {
        m_Logger->Error("Failed to get managers");
        onig_end();
        ShutdownLogger();
        return false;
    }

    if (!InitHooks()) {
        m_Logger->Error("Failed to initialize hooks");
        onig_end();
        ShutdownLogger();
        return false;
    }

    if (!Overlay::ImGuiCreateContext()) {
        m_Logger->Error("Failed to create ImGui context");
        ShutdownHooks();
        onig_end();
        ShutdownLogger();
        return false;
    }

    if (!Overlay::ImGuiInitPlatform(m_CKContext)) {
        m_Logger->Error("Failed to initialize Win32 platform backend for ImGui");
        Overlay::ImGuiDestroyContext();
        ShutdownHooks();
        onig_end();
        ShutdownLogger();
        return false;
    }

    m_Inited = true;
#if BML_ENABLE_ANGELSCRIPT
    BML_TryRegisterAngelScriptBindings(this);
#endif
    return true;
}

void ModContext::Shutdown() {
    if (!IsInited())
        return;

    m_Loader.Stop();
    if (m_Loader.GetModCount() != 0) {
        if (m_Logger)
            m_Logger->Error("Cannot shut down the runtime Context while Mod registrations remain.");
        return;
    }

    m_ImcRuntime.Shutdown();
    ResetVirtoolsWorld();
    m_GameFonts.Reset();

#if BML_ENABLE_ANGELSCRIPT
    if (GetScriptDevTools())
        GetScriptDevTools()->Hide();
    BML_UnregisterAngelScriptBindings(this);
#endif

    m_Logger->Info("Releasing Mod Loader");

    m_UiFonts.reset();
    if (Overlay::GetImGuiContext() != nullptr) {
        {
            Overlay::ImGuiContextScope scope;
            Overlay::ImGuiEndFrame();
        }
        Overlay::ImGuiShutdownRenderer(m_CKContext);
        Overlay::ImGuiShutdownPlatform(m_CKContext);
        Overlay::ImGuiDestroyContext();
    }

    ShutdownHooks();

    m_CKContext = nullptr;

    m_AttributeManager = nullptr;
    m_BehaviorManager = nullptr;
    m_CollisionManager = nullptr;
    m_InputManager = nullptr;
    m_MessageManager = nullptr;
    m_PathManager = nullptr;
    m_ParameterManager = nullptr;
    m_RenderManager = nullptr;
    m_SoundManager = nullptr;
    m_TimeManager = nullptr;

    utils::DeleteDirectoryW(m_TempDir);

    onig_end();

    m_Logger->Info("Goodbye!");

    ShutdownLogger();

    m_Inited = false;
}

void ModContext::ResetVirtoolsWorld() {
    // Runtime owners close native state first. Public object references belong
    // to the API seam and are reset only after internal teardown is complete.
    m_ExecuteBB.Reset();
    m_PhysicsForce.Reset();
    const BML::Behavior::Internal::Status plans = m_BehaviorPlans.ResetWorld();
    if (!plans && m_Logger)
        m_Logger->Error("Failed to leave the current Behavior Plan world: %s",
                        plans.Message.c_str());
    m_BehaviorScripts.ResetWorld();
    m_BehaviorPatches.ResetWorld();
    m_BehaviorSessions.ResetWorld();
    m_ObjectRefs.Reset();
}

void ModContext::VirtoolsObjectsToBeDeleted(const CK_ID *ids, int count) {
    // Runtime owners observe the deletion first; public references are the
    // final observer because they do not participate in native teardown.
    m_PhysicsForce.ObjectsToBeDeleted(ids, count);
    if (ids && count > 0) {
        for (int index = 0; index < count; ++index)
            m_BehaviorScripts.ObjectToBeDeleted(
                static_cast<std::uint32_t>(ids[index]));
    }
    m_BehaviorPatches.ObjectsToBeDeleted(ids, count);
    // Dropping the target only marks the world dirty. Reconciliation waits for
    // the next Loader frame: Virtools is still inside DeleteObjects here, so a
    // Plan that installed onto the doomed script must not try to reach it, and
    // Patches::Close already succeeds for an installation this pass erased.
    if (ids && count > 0) {
        for (int index = 0; index < count; ++index) {
            m_BehaviorPlans.RemoveObject(
                BML_OBJECT_DOMAIN_VIRTOOLS,
                static_cast<std::uint32_t>(ids[index]));
        }
    }
    m_BehaviorSessions.ObjectsToBeDeleted(ids, count);
    m_Behaviors.ObjectsToBeDeleted(ids, count);
    m_ObjectRefs.Invalidate(ids, count);
}

void ModContext::BehaviorScriptLoaded(CKBehavior *script) {
    if (!script)
        return;
    const BML_ObjectRef reference = m_ObjectRefs.Issue(script);
    const char *name = script->GetName();
    const BML::Behavior::Internal::Status status = m_BehaviorPlans.LoadScript(
        name ? name : "",
        {reference.Domain, reference.Slot, reference.Generation});
    if (!status && m_Logger)
        m_Logger->Error("Failed to retain a live Behavior script: %s",
                        status.Message.c_str());
}

void ModContext::ProcessVirtoolsFrame() {
    m_BehaviorPrototypes.ProcessFrame();
    m_PhysicsForce.ProcessFrame();
    m_Behaviors.ProcessFrame();
    m_BehaviorSessions.ProcessFrame();
    const BML::Behavior::Internal::Status plans = m_BehaviorPlans.ProcessFrame();
    if (!plans && m_Logger)
        m_Logger->Error("Failed to reconcile Behavior Plans: %s",
                        plans.Message.c_str());
    m_BehaviorPatches.ProcessFrame(m_BehaviorPlans);
    m_BehaviorScripts.ProcessFrame();
    m_ExecuteBB.ProcessFrame();
}

BML::Behavior::Internal::SessionOwner ModContext::LoaderBehaviorOwner() const {
    BML::Behavior::Internal::SessionOwner owner;
    if (!m_Loader.GetBuiltinMod())
        return owner;
    (void) m_BehaviorSessions.ReadOwner(m_Loader.GetBuiltinMod()->GetID(), owner);
    return owner;
}

BML::Behavior::Internal::Status ModContext::RetireBehaviorEdits(
    const std::string &ownerId) {
    // A Plan can be waiting on a Patch request that was already queued by an
    // off-thread Close. Request Plan retirement, complete every owned Patch at
    // the CK edit safe point, then collect Plans whose installations closed.
    (void) m_BehaviorPlans.RetireOwner(ownerId);
    const BML::Behavior::Internal::Status patches =
        m_BehaviorPatches.RetireOwner(ownerId);
    const BML::Behavior::Internal::Status plans =
        m_BehaviorPlans.RetireOwner(ownerId);
    if (!plans)
        return plans;
    return patches;
}

BML::Behavior::Internal::Status ModContext::RetireBehaviorOwner(
    const std::string &ownerId) {
    using BML::Behavior::Internal::Error;
    using BML::Behavior::Internal::Status;
    if (!IsMainThread()) {
        return Status(Error::WrongThread, CKERR_INVALIDPARAMETER, CKBR_OK,
                      "Behavior owners can only retire on the game thread.");
    }

    Status result = RetireBehaviorEdits(ownerId);
    const Status scripts = m_BehaviorScripts.RetireOwner(ownerId);
    if (result && !scripts)
        result = scripts;
    m_BehaviorSessions.RetireOwner(ownerId);
    return result;
}

void ModContext::RegisterCommand(ICommand *cmd) {
    // Taken before the lock: this is a virtual the Mod calls across the DLL
    // boundary, so the return address is in whichever module is registering.
    void *const registrar = ModuleFromAddress(_ReturnAddress());

    if (!IsMainThread()) {
        if (m_Logger)
            m_Logger->Error("RegisterCommand must run on the game thread.");
        return;
    }

    if (RegisterOwnedCommand(registrar, cmd)) {
        return;
    }

    if (!cmd) {
        m_Logger->Error("Failed to register a null command.");
        return;
    }

    m_Logger->Error(
        "Failed to register command '%s'. Command names are case-insensitive and must be valid UTF-8 tokens without spaces.",
        cmd->GetName().c_str());
}

bool ModContext::RegisterOwnedCommand(const void *registrar, ICommand *command) {
    if (!IsMainThread() || !registrar || !command)
        return false;

    BML::CommandContext::CommandInfo info;
    info.Name = command->GetName();
    info.Alias = command->GetAlias();
    info.Description = command->GetDescription();
    info.Cheat = command->IsCheat();

    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CommandContext.RegisterCommand(registrar, command, std::move(info));
}

BML::CommandContext::UnregisterResult ModContext::UnregisterOwnedCommand(
    const void *registrar, const char *name) {
    if (!IsMainThread() || m_CommandInvocationGate.IsCallActiveOnCurrentThread())
        return BML::CommandContext::UnregisterResult::Busy;

    auto invocationLock = m_CommandInvocationGate.LockMutation();
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CommandContext.UnregisterCommand(registrar, name);
}

int ModContext::UnregisterCommand(const void *callerAddress, const char *name) {
    if (!name || name[0] == '\0')
        return BML_ERROR_INVALID_PARAMETER;
    if (!IsMainThread())
        return BML_ERROR_WRONG_THREAD;

    void *const caller = ModuleFromAddress(callerAddress);

    switch (UnregisterOwnedCommand(caller, name)) {
        case BML::CommandContext::UnregisterResult::Success:
            return BML_OK;
        case BML::CommandContext::UnregisterResult::InvalidName:
            return BML_ERROR_INVALID_PARAMETER;
        case BML::CommandContext::UnregisterResult::NotFound:
            return BML_ERROR_NOT_FOUND;
        case BML::CommandContext::UnregisterResult::AccessDenied:
            m_Logger->Error("Refused to unregister command '%s': it belongs to another module.", name);
            return BML_ERROR_ACCESS_DENIED;
        case BML::CommandContext::UnregisterResult::Busy:
            return BML_ERROR_BUSY;
        case BML::CommandContext::UnregisterResult::InternalError:
            return BML_ERROR_FAIL;
    }

    return BML_ERROR_FAIL;
}

int ModContext::GetCommandCount() const {
    if (!IsMainThread())
        return 0;
    std::lock_guard<std::mutex> lock(m_Mutex);
    return static_cast<int>(m_CommandContext.GetCommandCount());
}

ICommand *ModContext::GetCommand(int index) const {
    if (!IsMainThread())
        return nullptr;
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CommandContext.GetCommandByIndex(index);
}

ICommand *ModContext::FindCommand(const char *name) const {
    if (!IsMainThread())
        return nullptr;
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CommandContext.GetCommandByName(name);
}

std::vector<BML::CommandContext::CommandInfo> ModContext::GetCommandSnapshot() const {
    if (!IsMainThread())
        return {};
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CommandContext.GetCommandSnapshot();
}

bool ModContext::GetCommandInfo(int index, BML::CommandContext::CommandInfo &info) const {
    if (!IsMainThread() || index < 0)
        return false;
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CommandContext.GetCommandInfoByIndex(
        static_cast<std::size_t>(index), info);
}

bool ModContext::FindCommandInfo(
    const char *name, BML::CommandContext::CommandInfo &info) const {
    if (!IsMainThread())
        return false;
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CommandContext.GetCommandInfoByName(name, info);
}

bool ModContext::SetCommandEnabled(ICommand *command, bool enabled) {
    if (!IsMainThread())
        return false;
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_CommandContext.SetCommandEnabled(command, enabled);
}

std::vector<std::string> ModContext::CompleteCommand(
    const char *name, const std::vector<std::string> &args) {
    if (!IsMainThread() || !name || name[0] == '\0')
        return {};

    auto invocationLock = m_CommandInvocationGate.LockCall();
    BML::CommandContext::CommandCall call;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_CommandContext.AcquireCommand(name, call);
    }
    std::vector<std::string> completions;
    if (call.Command && call.Info.Enabled)
        completions = call.Command->GetTabCompletion(this, args);
    m_CommandApi->FlushPending();
    return completions;
}

void ModContext::ExecuteCommand(const char *cmd) {
    if (!cmd || cmd[0] == '\0')
        return;
    ExecuteCommandLine(cmd);
}

int ModContext::ExecuteCommandLine(const char *line) {
    if (!IsMainThread() || !line || !m_Shell)
        return BML::Shell::Status::Failure;

    auto invocationLock = m_CommandInvocationGate.LockCall();
    const int status = m_Shell->Execute(line);
    if (m_ShellEnvironment.IsDirty())
        SaveShellEnvironment();
    return status;
}

int ModContext::InvokeCommandArgs(const std::vector<std::string> &args, const std::string *input) {
    if (!IsMainThread())
        return BML::Shell::Status::Failure;
    if (args.empty() || args[0].empty()) {
        WriteShellError(BML::Shell::FormatError("Error: Empty command"));
        return BML::Shell::Status::Unknown;
    }

    BML::Shell::CommandDispatchScope dispatchScope;
    if (!dispatchScope) {
        WriteShellError(BML::Shell::FormatError("Error: Command dispatch nested too deeply"));
        return BML::Shell::Status::Failure;
    }

    auto invocationLock = m_CommandInvocationGate.LockCall();
    BML::CommandContext::CommandCall call;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_CommandContext.AcquireCommand(args[0].c_str(), call);
    }
    if (!call.Command) {
        WriteShellError(BML::Shell::FormatError("Error: Unknown Command " + args[0]));
        return BML::Shell::Status::Unknown;
    }

    if (!call.Info.Enabled) {
        WriteShellError(BML::Shell::FormatError(
            "Error: Command is disabled " + call.Info.Name));
        return BML::Shell::Status::Disabled;
    }

    if (call.Info.Cheat && !IsCheatEnabled()) {
        WriteShellError(BML::Shell::FormatError("Error: Can not execute cheat command " + args[0]));
        return BML::Shell::Status::CheatRefused;
    }

    int status = BML::Shell::Status::Failure;
    try {
        BroadcastCallback(&IMod::OnPreCommandExecute, call.Command, args);
        {
            BML::Shell::InvocationScope scope(input);
            call.Command->Execute(this, args);
            status = scope.Status();
        }
        BroadcastCallback(&IMod::OnPostCommandExecute, call.Command, args);
    } catch (const std::exception &e) {
        m_Logger->Error("Exception executing command '%s': %s", args[0].c_str(), e.what());
        WriteShellError(BML::Shell::FormatError("Error: Command failed - " + std::string(e.what())));
    } catch (...) {
        m_Logger->Error("Unknown exception executing command '%s'", args[0].c_str());
        WriteShellError(BML::Shell::FormatError("Error: Command failed with unknown exception"));
    }
    m_CommandApi->FlushPending();
    return status;
}

void ModContext::WriteShellError(std::string_view message) {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->AddIngameMessage(std::string(message).c_str());
}

std::wstring ModContext::GetShellEnvironmentPath() const {
    if (m_LoaderDir.empty())
        return {};
    return utils::CombinePathW(m_LoaderDir, kShellStateFileName);
}

void ModContext::LoadShellEnvironment() {
    std::string error;
    if (!m_ShellEnvironment.Load(GetShellEnvironmentPath(), error) && m_Logger)
        m_Logger->Warn("Failed to load the shell state file: %s", error.c_str());
}

void ModContext::SaveShellEnvironment() {
    std::string error;
    if (m_ShellEnvironment.Save(GetShellEnvironmentPath(), error)) {
        m_ShellEnvironment.ClearDirty();
    } else if (m_Logger) {
        m_Logger->Warn("Failed to save the shell state file: %s", error.c_str());
    }
}

Config *ModContext::AddConfig(std::unique_ptr<Config> config) {
    if (!config)
        return nullptr;

    IMod *mod = config->GetMod();
    if (!mod)
        return nullptr;

    const std::string modId = mod->GetID();
    Config *rawConfig = config.get();
    LoadConfig(rawConfig);

    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_ConfigStore.Contains(modId)) {
        if (m_Logger)
            m_Logger->Error("Can not add duplicate config for %s.", modId.c_str());
        return nullptr;
    }
    return m_ConfigStore.Add(modId, mod, std::move(config));
}

bool ModContext::RemoveConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    const std::string modId = mod->GetID();
    std::unique_ptr<Config> removed;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        removed = m_ConfigStore.Remove(modId, mod, config);
        if (!removed)
            return false;
    }

    if (mod->m_Config == config)
        mod->m_Config = nullptr;

    try {
        if (!SaveConfig(removed.get(), !m_Loader.IsShuttingDown()) && m_Logger)
            m_Logger->Error("Failed to save config for mod %s during removal", modId.c_str());
    } catch (const std::exception &e) {
        if (m_Logger)
            m_Logger->Error("Exception while saving config for mod %s during removal: %s",
                            modId.c_str(), e.what());
    } catch (...) {
        if (m_Logger)
            m_Logger->Error("Unknown exception while saving config for mod %s during removal",
                            modId.c_str());
    }

    return true;
}

Config *ModContext::GetConfig(IMod *mod) {
    if (!mod)
        return nullptr;

    // Resolve the key before taking the registry mutex: GetID is a virtual call
    // into the Mod and may legally call back into the loader.
    const std::string modId = mod->GetID();

    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_ConfigStore.Find(modId, mod);
}

bool ModContext::LoadConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    std::wstring configPath = m_LoaderDir;
    configPath.append(L"\\Configs\\").append(utils::ToWString(mod->GetID())).append(L".cfg");
    return config->Load(configPath.c_str());
}

bool ModContext::SaveConfig(Config *config, bool snapshotModMetadata) {
    if (!config)
        return false;

    if (snapshotModMetadata)
        config->SnapshotModMetadata();

    const std::string &modId = config->GetModID();
    if (modId.empty())
        return false;

    std::wstring configPath = m_LoaderDir;
    configPath.append(L"\\Configs\\").append(utils::ToWString(modId)).append(L".cfg");
    return config->Save(configPath.c_str());
}

void ModContext::SnapshotConfigMetadata() {
    std::vector<Config *> configs;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        configs = m_ConfigStore.Snapshot();
    }

    for (Config *config : configs) {
        if (!config)
            continue;
        try {
            config->SnapshotModMetadata();
        } catch (const std::exception &e) {
            if (m_Logger) {
                m_Logger->Error("Exception while snapshotting config metadata for mod %s: %s",
                                config->GetModID().empty() ? "<unknown>" : config->GetModID().c_str(),
                                e.what());
            }
        } catch (...) {
            if (m_Logger) {
                m_Logger->Error("Unknown exception while snapshotting config metadata for mod %s",
                                config->GetModID().empty() ? "<unknown>" : config->GetModID().c_str());
            }
        }
    }
}

void ModContext::FlushConfigChanges(bool saveAll, bool dispatchNotifications) {
    auto invocationLock = LockModInvocation();
    std::vector<Config *> configs;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        configs = m_ConfigStore.Snapshot();
    }

    for (Config *config : configs) {
        if (!config)
            continue;

        IMod *mod = config->GetMod();
        std::vector<Config::PendingNotification> notifications = config->TakePendingNotifications();
        if (dispatchNotifications && mod) {
            for (const auto &notification : notifications) {
                m_Loader.DispatchConfigChange(mod, notification.Category.c_str(),
                                              notification.Key.c_str(), notification.ChangedProperty);
            }
        }

        if (saveAll || config->IsDirty()) {
            const char *modId = config->GetModID().empty() ? "<unknown>" : config->GetModID().c_str();
            try {
                if (!SaveConfig(config, dispatchNotifications) && m_Logger) {
                    m_Logger->Error("Failed to save config for mod %s",
                                    modId);
                }
            } catch (const std::exception &e) {
                if (m_Logger) {
                    m_Logger->Error("Exception while saving config for mod %s: %s",
                                    modId, e.what());
                }
            } catch (...) {
                if (m_Logger) {
                    m_Logger->Error("Unknown exception while saving config for mod %s",
                                    modId);
                }
            }
        }
    }
}

const wchar_t *ModContext::GetDirectory(DirectoryType type) {
    switch (type) {
    case BML_DIR_WORKING:
        return m_WorkingDir.c_str();
    case BML_DIR_TEMP:
        return m_TempDir.c_str();
    case BML_DIR_GAME:
        return m_GameDir.c_str();
    case BML_DIR_LOADER:
        return m_LoaderDir.c_str();
    case BML_DIR_CONFIG:
        return m_ConfigDir.c_str();
    default:
        break;
    }

    return nullptr;
}

const char *ModContext::GetDirectoryUtf8(DirectoryType type) {
    switch (type) {
    case BML_DIR_WORKING:
        return m_WorkingDirUtf8.c_str();
    case BML_DIR_TEMP:
        return m_TempDirUtf8.c_str();
    case BML_DIR_GAME:
        return m_GameDirUtf8.c_str();
    case BML_DIR_LOADER:
        return m_LoaderDirUtf8.c_str();
    case BML_DIR_CONFIG:
        return m_ConfigDirUtf8.c_str();
    default:
        break;
    }

    return nullptr;
}

std::wstring ModContext::GetModRootDirectory(const void *callerAddress, const char *modId) const {
    return m_Loader.GetModRootDirectory(callerAddress, modId);
}

#if BML_ENABLE_ANGELSCRIPT
bool ModContext::ValidateScriptModReloadDependencies(const BML::ScriptMod *mod,
                                                       const BML::ScriptModDefinition &candidate,
                                                       std::string &diagnostic,
                                                       std::vector<BML::ScriptModReloadDiagnosticField> *fields) const {
    return m_Loader.ValidateScriptModReloadDependencies(mod, candidate, diagnostic, fields);
}

bool ModContext::PromoteFailedScriptModPlaceholder(BML::ScriptMod *mod, const std::string &oldId,
                                                    const BML::ScriptModDefinition &candidate,
                                                    std::string &diagnostic) {
    return m_Loader.PromoteFailedScriptModPlaceholder(mod, oldId, candidate, diagnostic);
}

void ModContext::RestoreFailedScriptModPlaceholder(BML::ScriptMod *mod, const std::string &currentId,
                                                    const BML::ScriptModDefinition &oldDefinition) {
    m_Loader.RestoreFailedScriptModPlaceholder(mod, currentId, oldDefinition);
}
#endif

int ModContext::GetModCount() { return m_Loader.GetModCount(); }
IMod *ModContext::GetMod(int index) { return m_Loader.GetMod(index); }
std::uint64_t ModContext::GetModGeneration(const IMod *mod) const { return m_Loader.GetModGeneration(mod); }
std::uint64_t ModContext::GetModRegistryRevision() const { return m_Loader.GetModRegistryRevision(); }
IMod *ModContext::FindMod(const char *id) const { return m_Loader.FindMod(id); }
std::string ModContext::GetNativeImcOwnerId(const void *callerAddress, const char *ownerId) const {
    return m_Loader.GetNativeImcOwnerId(callerAddress, ownerId);
}
std::string ModContext::GetNativeModOwnerId(const void *callerAddress, const char *ownerId) const {
    return m_Loader.GetNativeModOwnerId(callerAddress, ownerId);
}
bool ModContext::NativeModOwnsAddress(const std::string &ownerId, const void *address) const {
    return m_Loader.NativeModOwnsAddress(ownerId, address);
}
int ModContext::RegisterDependency(IMod *mod, const char *id, int major, int minor, int patch) {
    return m_Loader.RegisterDependency(mod, id, major, minor, patch);
}
int ModContext::RegisterOptionalDependency(IMod *mod, const char *id, int major, int minor, int patch) {
    return m_Loader.RegisterOptionalDependency(mod, id, major, minor, patch);
}
int ModContext::CheckDependencies(IMod *mod) const { return m_Loader.CheckDependencies(mod); }
int ModContext::GetDependencyCount(IMod *mod) const { return m_Loader.GetDependencyCount(mod); }
int ModContext::GetDependencyInfo(IMod *mod, int index, char *id, int idSize,
                                  int *major, int *minor, int *patch, int *optional) const {
    return m_Loader.GetDependencyInfo(mod, index, id, idSize, major, minor, patch, optional);
}
int ModContext::ClearDependencies(IMod *mod) { return m_Loader.ClearDependencies(mod); }

BML_DataShare *ModContext::GetDataShare(const char *name) {
    if (!name || !*name)
        return reinterpret_cast<BML_DataShare *>(m_DataShare);
    return reinterpret_cast<BML_DataShare *>(BML::DataShareStore::GetInstance(name));
}

void ModContext::SetIC(CKBeObject *obj, bool hierarchy) {
    if (!obj)
        return;

    m_CKContext->GetCurrentScene()->SetObjectInitialValue(obj, CKSaveObjectState(obj));

    if (hierarchy) {
        if (CKIsChildClassOf(obj, CKCID_2DENTITY)) {
            auto *entity = (CK2dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                SetIC(entity->GetChild(i), true);
        }
        if (CKIsChildClassOf(obj, CKCID_3DENTITY)) {
            auto *entity = (CK3dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                SetIC(entity->GetChild(i), true);
        }
    }
}

void ModContext::RestoreIC(CKBeObject *obj, bool hierarchy) {
    if (!obj)
        return;

    CKStateChunk *chunk = m_CKContext->GetCurrentScene()->GetObjectInitialValue(obj);
    if (chunk)
        CKReadObjectState(obj, chunk);

    if (hierarchy) {
        if (CKIsChildClassOf(obj, CKCID_2DENTITY)) {
            auto *entity = (CK2dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                RestoreIC(entity->GetChild(i), true);
        }
        if (CKIsChildClassOf(obj, CKCID_3DENTITY)) {
            auto *entity = (CK3dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                RestoreIC(entity->GetChild(i), true);
        }
    }
}

void ModContext::Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy) {
    if (!obj)
        return;

    obj->Show(show);

    if (hierarchy) {
        if (CKIsChildClassOf(obj, CKCID_2DENTITY)) {
            auto *entity = (CK2dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                Show(entity->GetChild(i), show, true);
        }
        if (CKIsChildClassOf(obj, CKCID_3DENTITY)) {
            auto *entity = (CK3dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                Show(entity->GetChild(i), show, true);
        }
    }
}

void ModContext::AddTimer(CKDWORD delay, std::function<void()> callback) {
    if (!CanScheduleTimer())
        return;

    Delay(static_cast<size_t>(delay), callback, m_TimeManager->GetMainTickCount());
}

void ModContext::AddTimerLoop(CKDWORD delay, std::function<bool()> callback) {
    if (!CanScheduleTimer())
        return;

    Interval(static_cast<size_t>(delay), callback, m_TimeManager->GetMainTickCount());
}

void ModContext::AddTimer(float delay, std::function<void()> callback) {
    if (!CanScheduleTimer())
        return;

    Delay(delay / 1000.0f, callback, m_TimeManager->GetAbsoluteTime() / 1000.0f);
}

void ModContext::AddTimerLoop(float delay, std::function<bool()> callback) {
    if (!CanScheduleTimer())
        return;

    Interval(delay / 1000.0f, callback, m_TimeManager->GetAbsoluteTime() / 1000.0f);
}

void ModContext::ExitGame() {
    OnExitGame();
    AddTimer(1ul, [this]() {
        ::PostMessage((HWND) m_CKContext->GetMainWindow(), 0x5FA, 0, 0);
    });
}

void ModContext::OpenModsMenu() {
    m_Logger->Info("Open Mods Menu");
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->OpenModsMenu();
}

void ModContext::CloseModsMenu() {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->CloseModsMenu();
}

void ModContext::OpenMapMenu() {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->OpenMapMenu();
}

void ModContext::CloseMapMenu() {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->CloseMapMenu();
}

void ModContext::EnableCheat(bool enable) {
    if (m_CommandContext.SetCheatEnabled(enable)) {
        BroadcastCallback(&IMod::OnCheatEnabled, enable);
    }
}

void ModContext::SendIngameMessage(const char *msg) {
    // While a pipeline stage or a $(...) runs, its output belongs to the shell.
    if (IsMainThread() && !m_OutputSinks.empty()) {
        m_OutputSinks.back()->Write(msg ? msg : "");
        return;
    }
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->AddIngameMessage(msg ? msg : "");
}

void ModContext::ClearIngameMessages() {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->ClearIngameMessages();
}

float ModContext::GetSRScore() {
    return m_Loader.GetBuiltinMod() ? m_Loader.GetBuiltinMod()->GetSRTime() : 0.0f;
}

int ModContext::GetHSScore() {
    return m_Loader.GetBuiltinMod() ? m_Loader.GetBuiltinMod()->GetHSScore() : 0;
}

int ModContext::GetHUD() {
    return m_Loader.GetBuiltinMod() ? m_Loader.GetBuiltinMod()->GetHUD() : 0;
}

void ModContext::SetHUD(int mode) {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->SetHUD(mode);
}

void ModContext::ShowTitle(bool show) {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->ShowTitle(show);
}

void ModContext::ShowFPS(bool show) {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->ShowFPS(show);
}

void ModContext::ShowSRTimer(bool show) {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->ShowSRTimer(show);
}

void ModContext::StartSRTimer() {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->StartSRTimer();
}

void ModContext::PauseSRTimer() {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->PauseSRTimer();
}

void ModContext::ResetSRTimer() {
    if (m_Loader.GetBuiltinMod())
        m_Loader.GetBuiltinMod()->ResetSRTimer();
}

float ModContext::GetSRTime() {
    return m_Loader.GetBuiltinMod() ? m_Loader.GetBuiltinMod()->GetSRTime() : 0.0f;
}

void ModContext::SkipRenderForNextTick() {
    RenderHook::DisableRender(true);
    AddTimer(1ul, []() { RenderHook::DisableRender(false); });
}

void ModContext::RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                                  float friction, float elasticity, float mass, const char *collGroup,
                                  float linearDamp, float rotDamp, float force, float radius) {
    m_Loader.GetBallTypeMod()->RegisterBallType(ballFile, ballId, ballName, objName, friction, elasticity,
                                    mass, collGroup, linearDamp, rotDamp, force, radius);
}

void ModContext::RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                                   const char *collGroup, bool enableColl) {
    m_Loader.GetBallTypeMod()->RegisterFloorType(floorName, friction, elasticity, mass, collGroup, enableColl);
}

void ModContext::RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                   const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                   float linearDamp, float rotDamp, float radius) {
    m_Loader.GetBallTypeMod()->RegisterModulBall(modulName, fixed, friction, elasticity, mass, collGroup,
                                     frozen, enableColl, calcMassCenter, linearDamp, rotDamp, radius);
}

void ModContext::RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                     const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                     float linearDamp, float rotDamp) {
    m_Loader.GetBallTypeMod()->RegisterModulConvex(modulName, fixed, friction, elasticity, mass, collGroup,
                                       frozen, enableColl, calcMassCenter, linearDamp, rotDamp);
}

void ModContext::RegisterTrafo(const char *modulName) {
    m_Loader.GetBallTypeMod()->RegisterTrafo(modulName);
}

void ModContext::RegisterModul(const char *modulName) {
    m_Loader.GetBallTypeMod()->RegisterModul(modulName);
}

void ModContext::OnProcess() {
    if (!IsInited() || !m_TimeManager)
        return;

#if BML_ENABLE_ANGELSCRIPT
    BML_TryRegisterAngelScriptBindings(this);
    m_Loader.ProcessScriptState();
#endif
    m_ImcRuntime.Pump();
    Timer::ProcessAll(m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime() / 1000.0f);
    BroadcastCallback(&IMod::OnProcess);
    FlushConfigChanges();
}

void ModContext::OnRender(CKRenderContext *dev) {
    if (!IsInited() || !dev)
        return;

    BroadcastCallback(&IMod::OnRender, static_cast<CK_RENDER_FLAGS>(dev->GetCurrentRenderOptions()));
}

void ModContext::OnLoadGame() {
    BroadcastCallback(&IMod::OnLoadObject, "base.cmo", false, "", CKCID_3DOBJECT,
                      true, true, true, false, nullptr, nullptr);

    int scriptCnt = m_CKContext->GetObjectsCountByClassID(CKCID_BEHAVIOR);
    CK_ID *scripts = m_CKContext->GetObjectsListByClassID(CKCID_BEHAVIOR);
    for (int i = 0; i < scriptCnt; i++) {
        auto *behavior = (CKBehavior *) m_CKContext->GetObject(scripts[i]);
        if (behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
            BehaviorScriptLoaded(behavior);
            BroadcastCallback(&IMod::OnLoadScript, "base.cmo", behavior);
        }
    }
}

void ModContext::OnPreStartMenu() {
    BroadcastMessage("PreStartMenu", &IMod::OnPreStartMenu);
}

void ModContext::OnPostStartMenu() {
    BroadcastMessage("PostStartMenu", &IMod::OnPostStartMenu);
}

void ModContext::OnExitGame() {
    BroadcastMessage("ExitGame", &IMod::OnExitGame);
}

void ModContext::OnPreLoadLevel() {
    BroadcastMessage("PreLoadLevel", &IMod::OnPreLoadLevel);
}

void ModContext::OnPostLoadLevel() {
    BroadcastMessage("PostLoadLevel", &IMod::OnPostLoadLevel);
}

void ModContext::OnStartLevel() {
    BroadcastMessage("StartLevel", &IMod::OnStartLevel);
    m_GameSession.ActivateLevel();
}

void ModContext::OnPreResetLevel() {
    BroadcastMessage("PreResetLevel", &IMod::OnPreResetLevel);
    m_GameSession.BeginTransition();
}

void ModContext::OnPostResetLevel() {
    BroadcastMessage("PostResetLevel", &IMod::OnPostResetLevel);
}

void ModContext::OnPauseLevel() {
    BroadcastMessage("PauseLevel", &IMod::OnPauseLevel);
    m_GameSession.PauseLevel();
}

void ModContext::OnUnpauseLevel() {
    BroadcastMessage("UnpauseLevel", &IMod::OnUnpauseLevel);
    m_GameSession.ResumeLevel();
}

void ModContext::OnPreExitLevel() {
    BroadcastMessage("PreExitLevel", &IMod::OnPreExitLevel);
}

void ModContext::OnPostExitLevel() {
    BroadcastMessage("PostExitLevel", &IMod::OnPostExitLevel);
    m_GameSession.ReturnToFrontEnd();
}

void ModContext::OnPreNextLevel() {
    BroadcastMessage("PreNextLevel", &IMod::OnPreNextLevel);
}

void ModContext::OnPostNextLevel() {
    BroadcastMessage("PostNextLevel", &IMod::OnPostNextLevel);
    m_GameSession.BeginTransition();
}

void ModContext::OnDead() {
    BroadcastMessage("Dead", &IMod::OnDead);
    m_GameSession.ReturnToFrontEnd();
}

void ModContext::OnPreEndLevel() {
    BroadcastMessage("PreEndLevel", &IMod::OnPreEndLevel);
}

void ModContext::OnPostEndLevel() {
    BroadcastMessage("PostEndLevel", &IMod::OnPostEndLevel);
    m_GameSession.ReturnToFrontEnd();
}

void ModContext::OnCounterActive() {
    BroadcastMessage("CounterActive", &IMod::OnCounterActive);
}

void ModContext::OnCounterInactive() {
    BroadcastMessage("CounterInactive", &IMod::OnCounterInactive);
}

void ModContext::OnBallNavActive() {
    BroadcastMessage("BallNavActive", &IMod::OnBallNavActive);
}

void ModContext::OnBallNavInactive() {
    BroadcastMessage("BallNavInactive", &IMod::OnBallNavInactive);
}

void ModContext::OnCamNavActive() {
    BroadcastMessage("CamNavActive", &IMod::OnCamNavActive);
}

void ModContext::OnCamNavInactive() {
    BroadcastMessage("CamNavInactive", &IMod::OnCamNavInactive);
}

void ModContext::OnBallOff() {
    BroadcastMessage("BallOff", &IMod::OnBallOff);
}

void ModContext::OnPreCheckpointReached() {
    BroadcastMessage("PreCheckpoint", &IMod::OnPreCheckpointReached);
}

void ModContext::OnPostCheckpointReached() {
    BroadcastMessage("PostCheckpoint", &IMod::OnPostCheckpointReached);
}

void ModContext::OnLevelFinish() {
    BroadcastMessage("LevelFinish", &IMod::OnLevelFinish);
    m_GameSession.BeginTransition();
}

void ModContext::OnGameOver() {
    BroadcastMessage("GameOver", &IMod::OnGameOver);
}

void ModContext::OnExtraPoint() {
    BroadcastMessage("ExtraPoint", &IMod::OnExtraPoint);
}

void ModContext::OnPreSubLife() {
    BroadcastMessage("PreSubLife", &IMod::OnPreSubLife);
}

void ModContext::OnPostSubLife() {
    BroadcastMessage("PostSubLife", &IMod::OnPostSubLife);
}

void ModContext::OnPreLifeUp() {
    BroadcastMessage("PreLifeUp", &IMod::OnPreLifeUp);
}

void ModContext::OnPostLifeUp() {
    BroadcastMessage("PostLifeUp", &IMod::OnPostLifeUp);
}

void ModContext::InitDirectories() {
    wchar_t path[MAX_PATH];

    // Set up working directory
    _wgetcwd(path, MAX_PATH);
    path[MAX_PATH - 1] = '\0';
    m_WorkingDir = path;
    m_WorkingDirUtf8 = utils::ToString(m_WorkingDir);

    // Set up game directory
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    path[MAX_PATH - 1] = '\0';
    m_GameDir = ResolveGameDirectoryFromExecutable(path);
    m_GameDirUtf8 = utils::ToString(m_GameDir);

    // Set up loader directory
    m_LoaderDir = ResolveLoaderDirectory(m_GameDir);
    utils::CreateFileTreeW(m_LoaderDir);
    m_LoaderDirUtf8 = utils::ToString(m_LoaderDir);

    // Set up temp directory
    m_TempDir = CreateInstanceTempDirectory(
        m_LoaderDir,
        static_cast<unsigned long>(::GetCurrentProcessId()));
    m_TempDirUtf8 = utils::ToString(m_TempDir);

    // Set up config directory
    m_ConfigDir = m_LoaderDir + L"\\Configs";
    utils::CreateFileTreeW(m_ConfigDir);
    m_ConfigDirUtf8 = utils::ToString(m_ConfigDir);
}

void ModContext::InitLogger() {
    std::wstring logfilePath = m_LoaderDir + L"\\ModLoader.log";
    m_Logfile = _wfopen(logfilePath.c_str(), L"w");
    auto *logger = new Logger("ModLoader");
    Logger::SetDefault(logger);
    m_Logger = logger;

#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
#endif
}

void ModContext::ShutdownLogger() {
#ifdef _DEBUG
    FreeConsole();
#endif

    Logger::SetDefault(nullptr);
    delete m_Logger;
    m_Logger = nullptr;
    if (m_Logfile) {
        fclose(m_Logfile);
        m_Logfile = nullptr;
    }
}

extern bool HookObjectLoad();
extern bool HookPhysicalize();

extern bool UnhookObjectLoad();
extern bool UnhookPhysicalize();

bool ModContext::InitHooks() {
    bool result = true;

    m_InputHook = new InputHook(m_InputManager);
    if (!m_InputHook) {
        m_Logger->Error("Failed to create InputHook");
        return false;
    }

    bool objectLoadHookSuccess = HookObjectLoad();
    if (objectLoadHookSuccess) {
        m_Logger->Info("Hook ObjectLoad Success");
    } else {
        m_Logger->Error("Hook ObjectLoad Failed");
        result = false;
    }

    bool physicalizeHookSuccess = HookPhysicalize();
    if (physicalizeHookSuccess) {
        m_Logger->Info("Hook Physicalize Success");
    } else {
        m_Logger->Error("Hook Physicalize Failed");
        result = false;
    }

    if (!result) {
        if (objectLoadHookSuccess) UnhookObjectLoad();
        if (physicalizeHookSuccess) UnhookPhysicalize();
        delete m_InputHook;
        m_InputHook = nullptr;
    }

    return result;
}

bool ModContext::ShutdownHooks() {
    bool result = true;

    delete m_InputHook;
    m_InputHook = nullptr;

    if (UnhookObjectLoad()) {
        m_Logger->Info("Unhook ObjectLoad Success");
    } else {
        m_Logger->Info("Unhook ObjectLoad Failed");
        result = false;
    }

    if (UnhookPhysicalize()) {
        m_Logger->Info("Unhook Physicalize Success");
    } else {
        m_Logger->Info("Unhook Physicalize Failed");
        result = false;
    }

    return result;
}

bool ModContext::GetManagers() {
    m_AttributeManager = m_CKContext->GetAttributeManager();
    if (m_AttributeManager) {
        m_Logger->Info("Get Attribute Manager pointer 0x%08x", m_AttributeManager);
    } else {
        m_Logger->Info("Failed to get Attribute Manager");
        return false;
    }

    m_BehaviorManager = m_CKContext->GetBehaviorManager();
    if (m_BehaviorManager) {
        m_Logger->Info("Get Behavior Manager pointer 0x%08x", m_BehaviorManager);
    } else {
        m_Logger->Info("Failed to get Behavior Manager");
        return false;
    }

    m_CollisionManager = (CKCollisionManager *) m_CKContext->GetManagerByGuid(COLLISION_MANAGER_GUID);
    if (m_CollisionManager) {
        m_Logger->Info("Get Collision Manager pointer 0x%08x", m_CollisionManager);
    } else {
        m_Logger->Info("Failed to get Collision Manager");
        return false;
    }

    m_InputManager = (CKInputManager *) m_CKContext->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (m_InputManager) {
        m_Logger->Info("Get Input Manager pointer 0x%08x", m_InputManager);
    } else {
        m_Logger->Info("Failed to get Input Manager");
        return false;
    }

    m_MessageManager = m_CKContext->GetMessageManager();
    if (m_MessageManager) {
        m_Logger->Info("Get Message Manager pointer 0x%08x", m_MessageManager);
    } else {
        m_Logger->Info("Failed to get Message Manager");
        return false;
    }

    m_PathManager = m_CKContext->GetPathManager();
    if (m_PathManager) {
        m_Logger->Info("Get Path Manager pointer 0x%08x", m_PathManager);
    } else {
        m_Logger->Info("Failed to get Path Manager");
        return false;
    }

    m_ParameterManager = m_CKContext->GetParameterManager();
    if (m_ParameterManager) {
        m_Logger->Info("Get Parameter Manager pointer 0x%08x", m_ParameterManager);
    } else {
        m_Logger->Info("Failed to get Parameter Manager");
        return false;
    }

    m_RenderManager = m_CKContext->GetRenderManager();
    if (m_RenderManager) {
        m_Logger->Info("Get Render Manager pointer 0x%08x", m_RenderManager);
    } else {
        m_Logger->Info("Failed to get Render Manager");
        return false;
    }

    m_SoundManager = (CKSoundManager *) m_CKContext->GetManagerByGuid(SOUND_MANAGER_GUID);
    if (m_SoundManager) {
        m_Logger->Info("Get Sound Manager pointer 0x%08x", m_SoundManager);
    } else {
        m_Logger->Info("Failed to get Sound Manager");
        return false;
    }

    m_TimeManager = m_CKContext->GetTimeManager();
    if (m_TimeManager) {
        m_Logger->Info("Get Time Manager pointer 0x%08x", m_TimeManager);
    } else {
        m_Logger->Info("Failed to get Time Manager");
        return false;
    }

    return true;
}

bool ModContext::RegisterModOwner(const std::string &ownerId) noexcept {
    try {
        if (m_BehaviorSessions.RegisterOwner(ownerId) == 0) {
            if (m_Logger)
                m_Logger->Error("Mod registration failed: cannot register Behavior owner %s.", ownerId.c_str());
            return false;
        }
    } catch (...) {
        if (m_Logger)
            m_Logger->Error("Mod registration failed: cannot register Behavior owner %s.", ownerId.c_str());
        return false;
    }

    if (BML::DataShareStore::ActivateCallbacksFromOwner(ownerId))
        return true;

    try {
        (void) m_BehaviorSessions.RetireOwner(ownerId);
    } catch (...) {
    }
    if (m_Logger)
        m_Logger->Error("Mod registration failed: cannot register DataShare owner %s.", ownerId.c_str());
    return false;
}

void ModContext::RetireFailedModOwner(const std::string &ownerId) noexcept {
    (void) BML::DataShareStore::RetireCallbacksFromOwner(ownerId);
    try {
        (void) m_BehaviorSessions.RetireOwner(ownerId);
    } catch (...) {
    }
}

bool ModContext::CleanupModRegistrations(const std::string &ownerId) noexcept {
    bool cleaned = true;
    if (m_CommandApi && !m_CommandApi->CleanupOwner(ownerId))
        cleaned = false;

    if (!BML::DataShareStore::RetireCallbacksFromOwner(ownerId)) {
        if (m_Logger)
            m_Logger->Error("Failed to clean DataShare requests for Mod %s.",
                            ownerId.c_str());
        cleaned = false;
    }

    try {
        m_ImcRuntime.CleanupOwner(ownerId);
    } catch (...) {
        if (m_Logger)
            m_Logger->Error("Failed to clean IMC state for Mod %s.", ownerId.c_str());
        cleaned = false;
    }

    try {
        m_ModMenuPages.RemoveOwner(ownerId);
    } catch (...) {
        if (m_Logger) {
            m_Logger->Error("Failed to clean Mod Menu state for Mod %s.",
                            ownerId.c_str());
        }
        cleaned = false;
    }

    BML::Api::UnregisterInterfacesForOwner(ownerId);
    return cleaned;
}

void ModContext::RetireModBehaviorState(const std::string &ownerId) noexcept {
    try {
        const BML::Behavior::Internal::Status edits = RetireBehaviorEdits(ownerId);
        if (!edits && m_Logger) {
            m_Logger->Error("Failed to retire Behavior edits for Mod %s: %s",
                            ownerId.c_str(), edits.Message.c_str());
        }
    } catch (...) {
        if (m_Logger)
            m_Logger->Error("Failed to retire Behavior edits for Mod %s.", ownerId.c_str());
    }

    try {
        const BML::Behavior::Internal::Status scripts = m_BehaviorScripts.RetireOwner(ownerId);
        if (!scripts && m_Logger) {
            m_Logger->Error("Failed to retire Behavior Scripts for Mod %s: %s",
                            ownerId.c_str(), scripts.Message.c_str());
        }
    } catch (...) {
        if (m_Logger)
            m_Logger->Error("Failed to retire Behavior Scripts for Mod %s.", ownerId.c_str());
    }

    try {
        m_BehaviorSessions.RetireOwner(ownerId);
    } catch (...) {
        if (m_Logger)
            m_Logger->Error("Failed to retire Behavior sessions for Mod %s.", ownerId.c_str());
    }
}

void ModContext::CleanupModState(const std::string &ownerId) noexcept {
    RetireModBehaviorState(ownerId);
    (void) CleanupModRegistrations(ownerId);
}

void ModContext::ClearLegacyCommands() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_CommandContext.ClearCommands();
}

void ModContext::AddDataPath(const char *path) {
    if (!path || path[0] == '\0')
        return;

    XString dataPath = path;
    if (!m_PathManager->PathIsAbsolute(dataPath)) {
        char buf[MAX_PATH];
        VxGetCurrentDirectory(buf);
        dataPath.Format("%s\\%s", buf, dataPath.CStr());
    }
    if (dataPath[dataPath.Length() - 1] != '\\')
        dataPath << '\\';

    if (utils::DirectoryExistsA(dataPath.CStr()) &&
        m_PathManager->GetPathIndex(DATA_PATH_IDX, dataPath) == -1) {
        m_PathManager->AddPath(DATA_PATH_IDX, dataPath);

        XString subDataPath1 = dataPath + "3D Entities\\";
        if (utils::DirectoryExistsA(subDataPath1.CStr()) &&
            m_PathManager->GetPathIndex(DATA_PATH_IDX, subDataPath1) == -1) {
            m_PathManager->AddPath(DATA_PATH_IDX, subDataPath1);
        }

        XString subDataPath2 = dataPath + "3D Entities\\PH\\";
        if (utils::DirectoryExistsA(subDataPath2.CStr()) &&
            m_PathManager->GetPathIndex(DATA_PATH_IDX, subDataPath2) == -1) {
            m_PathManager->AddPath(DATA_PATH_IDX, subDataPath2);
        }
    }

    XString texturePath = dataPath + "Textures\\";
    if (utils::DirectoryExistsA(texturePath.CStr()) &&
        m_PathManager->GetPathIndex(BITMAP_PATH_IDX, texturePath) == -1) {
        m_PathManager->AddPath(BITMAP_PATH_IDX, texturePath);
    }

    XString soundPath = dataPath + "Sounds\\";
    if (utils::DirectoryExistsA(soundPath.CStr()) &&
        m_PathManager->GetPathIndex(SOUND_PATH_IDX, soundPath) == -1) {
        m_PathManager->AddPath(SOUND_PATH_IDX, soundPath);
    }
}

bool ModContext::CanScheduleTimer() const {
    return IsInited() && m_TimeManager != nullptr && !m_Loader.IsShuttingDown();
}
