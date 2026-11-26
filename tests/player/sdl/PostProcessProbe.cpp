#include <BML/BMLAll.h>
#include <BML/Behavior.hpp>
#include <BML/Engine.hpp>
#include <BML/ImGui.hpp>
#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <string>
#ifdef BML_TEST_SCRIPT_REFLECTION
#include "ScriptApiDump.h"
#endif

namespace {
class PostProcessProbe final : public BML::Mod::Instance {
public:
    static BML::Mod::Definition Define(){return {"SdlPostProcessProbe","SDL PostProcess regression","1.0.0","BML tests","Checks the shipped loader frame ordering and input edges",BML_VERSION,{}};}
    void OnLoad(const BML::Mod::Context &context) override {
        m_Context=context;
#ifdef BML_TEST_SCRIPT_REFLECTION
        if (const char *dump = std::getenv("BML_SCRIPT_API_DUMP"))
            Write(dump, DumpBmlScriptApi(context));
#endif

        if (const char *path = std::getenv("BML_POSTPROCESS_RESULT"))
            m_ResultPath = path;
    }
    void OnPostStartMenu(const BML::Mod::Context &) override { m_MenuReady = true; }
    void OnRender(const BML::Mod::Context &,const BML::Mod::RenderEvent &) override { ++m_Renders; }
    void OnUnload(const BML::Mod::Context &) override {
        (void)m_Block.Release().Code();
        Write(m_ResultPath + ".unloaded", "unloaded\n");
    }

    void OnPreCommandExecute(const BML::Mod::Context &,
                             const BML::Mod::CommandEvent &event) override {
        if (event.Name != "bml_probe_command")
            return;
        CheckCommandEvent(event, BML_COMMAND_EVENT_PRE);
        m_CommandEvents.push_back(1);
        if (m_RemoveBeforeExecute)
            m_ProbeCommand.Unregister().Take();
        Require(!m_CommandLease.expired(), "command state retained through pre callback");
    }

    void OnPostCommandExecute(const BML::Mod::Context &,
                              const BML::Mod::CommandEvent &event) override {
        if (event.Name != "bml_probe_command")
            return;
        CheckCommandEvent(event, BML_COMMAND_EVENT_POST);
        Require(!m_CommandLease.expired(), "command state retained through post callback");
        m_CommandEvents.push_back(3);
    }

    void OnModifyConfig(const BML::Mod::Context &, const BML::Mod::ConfigEvent &event) override {
        if (event.Category != "Contract")
            return;
        Require(event.ModId == "SdlPostProcessProbe" && event.Property != 0,
                "configuration notification owner and property");
        if (event.Key == "Count") {
            Require(event.Type == BML_CONFIG_TYPE_INTEGER, "configuration notification type");
            ++m_CountNotifications;
        } else if (event.Key == "Text") {
            Require(event.Type == BML_CONFIG_TYPE_STRING, "text notification type");
            ++m_TextNotifications;
        }
    }

    void OnProcess(const BML::Mod::Context &) override {
        if (m_Finished)
            return;
        ++m_Frames;
        CKContext *context = BML::Engine::BorrowContext(m_Context).Value();
        auto *window = static_cast<SDL_Window *>(context->GetMainWindow());
        const SDL_WindowID windowId = SDL_GetWindowID(window);
        if (!windowId || SDL_GetWindowFromID(windowId) != window) {
            Finish(false, "sdl-window-not-owned-by-module-runtime");
            return;
        }
        auto *physics = context->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
        auto *loader = context->GetManagerByGuid(CKGUID(0x32a40332, 0x3bf12a51));
        auto *native = static_cast<CKInputManager *>(context->GetManagerByGuid(INPUT_MANAGER_GUID));
        auto input = m_Context.Input();
        const auto firstFiltered = input.ReadKeyboard().Value();
        const auto raw = input.ReadKeyboard(BML_INPUT_RAW).Value();
        const auto filtered = input.ReadKeyboard().Value();
        if (firstFiltered.Keys.data() != filtered.Keys.data() ||
            firstFiltered.Events.data() != filtered.Events.data()) {
            Finish(false, "filtered-snapshot-invalidated-by-raw-read");
            return;
        }
        const auto mouse = input.ReadMouse(BML_INPUT_RAW).Value();
        if (!physics || !loader || !native) {
            Finish(false, "missing-manager");
            return;
        }
        m_Priority = loader->GetFunctionPriority(CKMANAGER_FUNC_PostProcess);
        for (int i = 0; i < context->GetManagerCount(); ++i) {
            auto *manager = context->GetManager(i);
            if (manager != loader && (manager->GetValidFunctionsMask() & CKMANAGER_FUNC_PostProcess) &&
                manager->GetFunctionPriority(CKMANAGER_FUNC_PostProcess) <= m_Priority) {
                Finish(false, "loader-precedes-engine-manager");
                return;
            }
        }
        // CK2 resets this public profiling field before every Process. Physics
        // has no PreProcess, so a positive value proves this tick's PostProcess
        // has returned before the real BML OnProcess dispatch reaches us.
        if (physics->m_ProcessingTime > 0)
            ++m_PhysicsFrames;
        const CKDWORD tick = context->GetTimeManager()->GetMainTickCount();
        if (m_Frames > 1 && tick == m_LastTick) {
            Finish(false, "duplicate-mod-callback-in-tick");
            return;
        }
        m_LastTick = tick;
        if (!m_MenuReady) {
            if (m_Frames > 600)
                Finish(false, "menu-timeout");
            return;
        }

        const auto repeated=input.ReadKeyboard(BML_INPUT_RAW).Value();
        if(raw.Keys.data()!=repeated.Keys.data() || raw.Events.data()!=repeated.Events.data()) {Finish(false,"unstable-input-snapshot");return;}
        bool passed = true;
        switch (m_Phase) {
        case 0:
            try { CheckNativeServices(context); }
            catch (const std::exception &error) { Finish(false, std::string("native-services: ") + error.what()); return; }
            passed = Inject(context, true);
            break;
        case 1:
            passed = raw.IsKeyPressed(CKKEY_A) && native->IsKeyDown(CKKEY_A) &&
                     (mouse.ClickedMask & 1u) && Inject(context, false);
            break;
        case 2:
            passed = raw.IsKeyReleased(CKKEY_A) && filtered.IsKeyReleased(CKKEY_A) &&
                     native->IsKeyToggled(CKKEY_A) && native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            break;
        case 3:
            passed = !raw.IsKeyReleased(CKKEY_A) && !native->IsKeyDown(CKKEY_A) &&
                     !native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            m_Block = input.AcquireBlock(BML_INPUT_BLOCK_KEYBOARD | BML_INPUT_BLOCK_MOUSE).Take();
            passed = passed && m_Block.IsValid() && Inject(context, true);
            break;
        case 4:
            passed = raw.IsKeyPressed(CKKEY_A) && !native->IsKeyDown(CKKEY_A) &&
                     (mouse.ClickedMask & 1u) &&
                     !native->IsMouseButtonDown(CK_MOUSEBUTTON_LEFT) && Inject(context, false);
            break;
        case 5:
            passed = raw.IsKeyReleased(CKKEY_A) && !filtered.IsKeyReleased(CKKEY_A) &&
                     !native->IsKeyToggled(CKKEY_A) && (mouse.ToggledMask & 1u) &&
                     !native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            passed = passed && m_Block.Release().Code()==BML_OK;
            break;
        case 6:
            passed = !raw.IsKeyReleased(CKKEY_A) && !native->IsKeyDown(CKKEY_A) &&
                     !native->IsMouseToggled(CK_MOUSEBUTTON_LEFT);
            break;
        default:
            if (std::getenv("BML_EXPECT_SCRIPTS") && std::string_view(std::getenv("BML_EXPECT_SCRIPTS")) == "1") {
                try { if (!CheckScriptRuntime()) return; }
                catch (const std::exception &error) { Finish(false, std::string("script-runtime: ") + error.what()); return; }
            }
            if (m_Frames >= 120) {
                const bool timers = m_NoArgCalls == 1 && m_VoidCalls == 2 && !m_NoArgs.IsValid() &&
                                    !m_VoidLoop.IsValid() && m_SelfCalls == 1 &&
                                    m_RepeatCalls == 3 && !m_Self.IsValid() &&
                                    !m_Repeat.IsValid() && m_SelfLease.expired() &&
                                    m_CountNotifications == 1 && m_TextNotifications == 1;
                Finish(m_PhysicsFrames > 0 && timers, !timers ? "timer-lifecycle" : m_PhysicsFrames ? "passed" : "physics-not-observed-before-mod");
            }
            return;
        }
        if (!passed) {
            Finish(false, "input-phase-" + std::to_string(m_Phase));
            return;
        }
        ++m_Phase;
    }

private:
    static void Require(bool value, const char *message) {
        if (!value)
            throw std::runtime_error(message);
    }

    void CheckCommandEvent(const BML::Mod::CommandEvent &event, uint32_t phase) {
        Require(event.Phase == phase && event.InvokedAs == "bpcheck" && !event.Cheat,
                "command event identity");
        Require(event.Arguments.size() == 2 && std::string_view(event.Arguments[0]) == "one" &&
                    std::string_view(event.Arguments[1]) == "two words",
                "command event arguments after shell expansion");
    }

    int ExecuteCommand(std::string_view line, int &out) {
        auto result = m_Context.Commands().ExecuteLine(line);
        if (result) out = result.Value();
        return result.Code();
    }

    bool CheckPlanSymbolBinding() {
        using namespace BML::Behavior;
        if (m_CheckedPlanSymbols && !m_SurvivingGraph.Domain)
            return true;
        if (m_CheckedPlanSymbols) {
            auto session = Session::Open().Take();
            Require(session.Inspect(m_SurvivingGraph, View::Live).Take()
                        .FindAll("__BML_Batch_Child").size() == 1,
                    "deleting a graph retired its graph-owned sibling edit");
            (void)m_Context.LogInfo("Behavior installations PASS graph-owned target lifetime");
            m_SurvivingGraph = {};
            return true;
        }
        if (!m_SymbolPlan) {
            auto session = Session::Open().Take();
            auto graph = m_SymbolEdit.Root();
            m_SymbolNode = graph.AddGraph("__BML_Installations_Native");
            m_SymbolPort = graph.AppendLocal("__BML_Installations_Value", CKPGUID_INT);
            graph.Bind(m_SymbolPort, 41);
            m_SymbolPlan = session.Plan(
                "native symbol binding", Scripts::One("Event_handler"), m_SymbolEdit).Take();
            return false;
        }

        const auto info = m_SymbolPlan.Info().Take();
        if (info.State != PlanState::Active) {
            Require(info.LastStatus.Error == Error::None, "native Behavior Plan installation");
            return false;
        }
        auto instances = m_SymbolPlan.Instances().Take();
        Require(instances.size() == 1, "native Behavior Plan instance");
        const auto &instance = instances.front();
        const auto node = instance.Resolve(m_SymbolNode).Take();
        auto value = instance.Read(m_SymbolPort).Take();
        Require(std::get<int32_t>(value.Data) == 41, "native Behavior Plan value");
        if (!m_PreviousSymbolInstance) {
            m_PreviousSymbolInstance = instance;
            m_PreviousSymbolNode = node;
            (void)m_SymbolPlan.Replace(On(Scripts::One("Event_handler"), m_SymbolEdit)).Take();
            return false;
        }

        Require(node.Slot == m_PreviousSymbolNode.Slot &&
                    node.Generation == m_PreviousSymbolNode.Generation,
                "unchanged Plan reused its native symbol");
        Require(!m_PreviousSymbolInstance.Resolve(m_SymbolNode),
                "previous Plan instance is stale after rebinding");
        instance.Set(m_SymbolPort, Value(42)).Take();
        value = instance.Read(m_SymbolPort).Take();
        Require(std::get<int32_t>(value.Data) == 42, "rebound Plan symbol can be written");
        instance.Set(m_SymbolPort, Value(41)).Take();
        CheckUnifiedGraphEdit(instance.Script());
        m_CheckedPlanSymbols = true;
        return false;
    }

    void CheckUnifiedGraphEdit(BML_ObjectRef graphRef) {
        using namespace BML::Behavior;
        auto session = Session::Open().Take();

        Edit invalid;
        auto nested = invalid.Root().AddGraph("__BML_Apply_Rollback").Graph();
        nested.Require("__BML_Apply_Missing");
        auto graph = session.Inspect(graphRef).Take();
        auto failed = session.Apply("failed exact edit", EditOwnership::Submission,
                                    On(graph, invalid));
        Require(!failed && failed.GetStatus().Error == Error::QueryNotFound,
                "exact Apply reported its primary failure");
        auto live = session.Inspect(graphRef, View::Live).Take();
        Require(live.FindAll("__BML_Apply_Rollback").empty(),
                "failed exact Apply restored the graph");

        Edit edit;
        auto body = edit.Root();
        body.AddGraph("__BML_Apply_GraphOwned");
        auto input = body.AppendLocal("__BML_Apply_Native_Input", CKPGUID_INT);
        body.Bind(input, 12);
        graph = session.Inspect(graphRef).Take();
        auto applied = session.Apply("durable exact edit", EditOwnership::Graph,
                                     On(graph, edit)).Take();
        Require(applied.Info().Take().State == SubmissionState::Applied,
                "exact Apply published its edit");
        auto installed = applied.Find(graphRef).Take();
        Require(installed.Graph().Generation == graphRef.Generation &&
                    std::get<int32_t>(installed.Read(input).Take().Data) == 12,
                "exact Apply returned a typed installation receipt");
        Require(applied.Close().Take() == CloseState::Closed && !installed.Read(input),
                "closing invalidates native receipts");
        live = session.Inspect(graphRef, View::Live).Take();
        Require(live.FindAll("__BML_Apply_GraphOwned").size() == 1,
                "graph-owned edits survive closing their submission");

        const auto makeGraph = [&](const char *name) {
            Edit root;
            root.Root().AddGraph(name);
            auto created = session.Apply(name, EditOwnership::Graph,
                On(session.Inspect(graphRef).Take(), root)).Take();
            Require(created.Close().Take() == CloseState::Closed, "graph ownership transfer");
            return session.Inspect(graphRef).Take().FindAll(name).at(0).Object();
        };
        const auto first = makeGraph("__BML_Batch_First");
        m_SurvivingGraph = makeGraph("__BML_Batch_Second");
        Edit shared;
        shared.Root().AddGraph("__BML_Batch_Child");
        auto local = shared.Root().AppendLocal("shared", CKPGUID_INT);
        auto batch = session.Apply("graph batch", EditOwnership::Graph,
            On(session.Inspect(first).Take(), shared),
            On(session.Inspect(m_SurvivingGraph).Take(), shared)).Take();
        batch.SetInput(local, 31).Take();
        for (const auto target : {first, m_SurvivingGraph}) {
            auto receipt = batch.Find(target, local).Take();
            Require(std::get<int32_t>(receipt.Read(local).Take().Data) == 31,
                    "SetInput missed an installation of the same Edit");
        }
        (void)m_Context.LogInfo("Behavior installations PASS repeated Edit SetInput");
        Require(batch.Close().Take() == CloseState::Closed, "batch ownership transfer");
        auto target = BML::Engine::BorrowObject<CKBehavior>(m_Context, first).Value();
        Require(target->GetCKContext()->DestroyObject(target) == CK_OK, "delete first target");

        Edit parent;
        parent.Root().AddGraph("__BML_Dependency");
        auto owner = session.Apply("dependency owner", EditOwnership::Submission,
            On(session.Inspect(m_SurvivingGraph).Take(), parent)).Take();
        Edit child;
        child.Root().Require("__BML_Dependency").Graph().AddGraph("__BML_Dependent");
        auto dependent = session.Apply("dependent", EditOwnership::Submission,
            On(session.Inspect(m_SurvivingGraph).Take(), child)).Take();
        Require(!owner.Close(), "dependent edit did not block close");
        const auto conflict = owner.Info().Take();
        Require(conflict.RestoreFailure.Error == Error::RevertConflict &&
                conflict.ApplyFailure.Error == Error::None, "close conflict diagnostic lost");
        Require(dependent.Close().Take() == CloseState::Closed, "release dependent edit");
        Require(owner.Close().Take() == CloseState::Closed, "retry close after releasing dependency");
        (void)m_Context.LogInfo("Behavior installations PASS close conflict query and retry");
    }

    bool CheckScriptRuntime() {
        if (m_Frames < 90) return false;
        const auto marker = [&](const char *name) { return bool(m_Context.Commands().Find(name)); };
        if (!marker("bml_probe_behavior_installations") || !CheckPlanSymbolBinding()) {
            if (m_Frames < 360) return false;
            Require(false, "Behavior installation regression");
        }
        if (m_ReloadStep == 0) {
            Require(marker("bml_probe_command_contracts"), "script command validation and event fields");
            Require(marker("bml_probe_combined_event") && marker("bml_probe_individual_event") &&
                    marker("bml_probe_combined_command") && marker("bml_probe_individual_command"), "event dispatch once");
            m_ScriptPath = m_Context.GetDirectory(BML_DIR_GAME).Take() + "/ModLoader/Mods/ScriptRuntimeProbe.mod.as";
            size_t length = 0;
            auto *data = SDL_LoadFile(m_ScriptPath.c_str(), &length);
            Require(data != nullptr, "read staged script");
            m_ScriptSource.assign(static_cast<const char *>(data), length); SDL_free(data);
            auto candidate = m_ScriptSource;
            auto at = candidate.find("bool FailCandidate = false;");
            Require(at != std::string::npos, "candidate source marker");
            candidate.replace(at, std::string("bool FailCandidate = false;").size(), "bool FailCandidate = true;");
            Require(Write(m_ScriptPath, candidate), "write failing candidate");
            Require(m_Context.Commands().ExecuteLine("script reload SdlRuntimeProbe").Take() == 0,
                    "queue failing candidate");
            ++m_ReloadStep;
        } else if (m_ReloadStep == 1 && marker("bml_probe_rollback_scope")) {
            auto candidate = m_ScriptSource;
            auto at = candidate.find("bool NewRuntime = false;");
            candidate.replace(at, std::string("bool NewRuntime = false;").size(), "bool NewRuntime = true;");
            at = candidate.find("definition.DefaultValue = BML::ConfigValue(false);");
            candidate.replace(at, std::string("definition.DefaultValue = BML::ConfigValue(false);").size(), "definition.DefaultValue = BML::ConfigValue(2);");
            Require(Write(m_ScriptPath, candidate), "write valid candidate");
            Require(m_Context.Commands().ExecuteLine("script reload SdlRuntimeProbe").Take() == 0,
                    "queue valid candidate");
            ++m_ReloadStep;
        } else if (m_ReloadStep == 2 && marker("bml_probe_new_scope")) {
            Require(m_Context.Commands().ExecuteLine("bml_probe_fail_provider").Take() != 0,
                    "provider callback reports command failure");
            ++m_ReloadStep;
        } else if (m_ReloadStep == 3 && marker("bml_probe_dependency_failed")) {
            return true;
        }
        Require(m_Frames < 600, "reload, rollback or dependency query timed out");
        return false;
    }

    void CheckCommandLifetime(bool removeBeforeExecute) {
        m_RemoveBeforeExecute = removeBeforeExecute;
        m_CommandEvents.clear();
        auto retained = std::make_shared<int>(7);
        m_CommandLease = retained;
        m_ProbeCommand =
            m_Context.Commands()
                .Register({"bml_probe_command", "bpcheck", "Retained command invocation"},
                          [this, retained](const BML::Command::Invocation &invocation) {
                              Require(*retained == 7 && invocation.InvokedAs() == "bpcheck" &&
                                          invocation.Argument(0) == "one" &&
                                          invocation.Argument(1) == "two words",
                                      "command execute after registration removal");
                              m_CommandEvents.push_back(2);
                              if (!m_RemoveBeforeExecute)
                                  m_ProbeCommand.Unregister().Take();
                              return 7;
                          })
                .Take();
        retained.reset();
        int status = 0;
        Require(ExecuteCommand("bpcheck one 'two words'", status) == BML_OK &&
                    status == 7,
                "command invocation status");
        Require(m_CommandEvents == std::vector<int>({1, 2, 3}), "pre execute post ordering");
        Require(m_CommandLease.expired(), "command state released after post callback");
        BML::Command::Info info;
        Require(m_Context.Commands().Find("bpcheck").Code() == BML_ERROR_NOT_FOUND,
                "removed command and alias no longer resolve");
    }

    template <class Operation> double MeasureApi(Operation &&operation) {
        constexpr unsigned iterations = 10000;
        std::array<double, 7> samples{};
        uint64_t checksum = 0;
        for (unsigned i = 0; i < 100; ++i)
            checksum += operation();
        for (auto &sample : samples) {
            const auto start = std::chrono::steady_clock::now();
            for (unsigned i = 0; i < iterations; ++i)
                checksum += operation();
            const auto elapsed = std::chrono::steady_clock::now() - start;
            sample = std::chrono::duration<double, std::nano>(elapsed).count() / iterations;
        }
        m_ApiBenchmarkChecksum += checksum;
        std::sort(samples.begin(), samples.end());
        return samples[samples.size() / 2];
    }

    void BenchmarkCommonApi() {
        const char *enabled = std::getenv("BML_BENCHMARK_API");
        if (!enabled || *enabled != '1')
            return;
        auto input = m_Context.Input();
        const auto directory = MeasureApi(
            [&] { return m_Context.GetDirectory(BML_MOD_DIRECTORY_ROOT).Value().size(); });
        const auto keyName = MeasureApi([&] { return input.GetKeyName(CKKEY_A).Value().size(); });
        auto block = input.AcquireBlock(BML_INPUT_BLOCK_KEYBOARD).Take();
        const auto filtered = MeasureApi([&] {
            const auto keyboard = input.ReadKeyboard().Value();
            Require(!keyboard.IsKeyDown(CKKEY_A) && keyboard.Events.empty(),
                    "blocked benchmark keyboard");
            return keyboard.Keys.size();
        });
        const auto raw =
            MeasureApi([&] { return input.ReadKeyboard(BML_INPUT_RAW).Value().Keys.size(); });
        block.Release().Take();
        std::ostringstream values;
        values << ",\"api_benchmark_ns\":{\"directory\":" << directory
               << ",\"key_name\":" << keyName << ",\"keyboard_blocked\":" << filtered
               << ",\"keyboard_raw\":" << raw
               << "},\"api_benchmark_checksum\":" << m_ApiBenchmarkChecksum;
        m_ApiBenchmark = values.str();
    }

    void CheckScriptAccess(CKContext *engine) {
        auto helper = m_Context.FindMod("SdlScriptAccessProbe");
        BML::Command::Info info;
        Require(helper && helper.Value().Read().Value().State == BML_MOD_STATE_LOADED &&
                    m_Context.Commands().Find("bml_probe_script_access").Code() == BML_OK,
                "script access probe registration");
        auto *entity = static_cast<CK3dEntity *>(
            engine->CreateObject(CKCID_3DENTITY, const_cast<char *>("BML_ScriptAccess_借用"),
                                 CK_OBJECTCREATION_DYNAMIC));
        Require(entity != nullptr, "script access object creation");
        const CK_ID id = entity->GetID();
        entity->SetPriority(17);
        entity->SetPosition3f(1.0f, 2.0f, 3.0f);
        entity->SetScale3f(2.0f, 3.0f, 4.0f);
        entity->Show(CKSHOW);
        int status = 0;
        const int executed = ExecuteCommand("bml_probe_script_access live", status);
        engine->DestroyObject(id);
        Require(executed == BML_OK && status == 0 &&
                    m_Context.Commands().Find("bml_probe_script_access_live").Code() == BML_OK,
                "script CK values, time, input conversions and nested blocks");
        Require(ExecuteCommand("bml_probe_script_access deleted", status) == BML_OK &&
                    status == 0 &&
                    m_Context.Commands().Find("bml_probe_script_access_deleted").Code() == BML_OK,
                "script borrowed object validity after engine deletion");
    }

    void CheckCommandValidation() {
        const std::array<std::string, 6> invalid{
            std::string("\xff"), std::string("\xc3"), std::string("\xc0\xaf"),
            std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"),
            std::string(1, '\0')};
        auto commands = m_Context.Commands();
        const auto execute = [](const BML::Command::Invocation &) { return 0; };
        for (const auto &text : invalid) {
            for (unsigned field = 0; field < 3; ++field) {
                BML::Command::Definition definition;
                definition.Name = "bml_probe_native_metadata";
                if (field == 0) definition.Description = text;
                else if (field == 1) definition.Usage = text;
                else definition.Category = text;
                Require(commands.Register(definition, execute).Code() == BML_ERROR_INVALID_PARAMETER,
                        "native invalid command metadata rejection");
                definition.Description = definition.Usage = definition.Category = "中文";
                auto registration = commands.Register(definition, execute).Take();
                registration.Unregister().Take();
            }
            Require(commands.Find(text).Code() == BML_ERROR_INVALID_PARAMETER,
                    "native invalid command lookup rejection");
        }
        for (const auto &name : {std::string(), std::string("two words"),
                                 std::string("tab\tname"),
                                 std::string(BML_COMMAND_MAX_NAME_BYTES + 1, 'x')}) {
            Require(commands.Find(name).Code() == BML_ERROR_INVALID_PARAMETER,
                    "native invalid command name rejection");
        }
        Require(commands.Find("bml_probe_missing_valid_name").Code() == BML_ERROR_NOT_FOUND,
                "native valid command lookup preserves not found");
    }

    void CheckNativeServices(CKContext *engine) {
        if (const char *expected = std::getenv("BML_EXPECT_MODS")) {
            std::istringstream ids(expected);
            for (std::string id; std::getline(ids, id, ',');) {
                if (id.empty()) continue;
                auto found = m_Context.FindMod(id);
                Require(found && found.Value().Read().Value().State == BML_MOD_STATE_LOADED,
                        ("Mod did not load: " + id).c_str());
            }
        }
        if (const char *scripts = std::getenv("BML_EXPECT_SCRIPTS"); scripts && *scripts == '1') {
            auto helper = m_Context.FindMod("SdlDependencyProbe");
            BML::Command::Info info;
            Require(helper && helper.Value().Read().Value().State == BML_MOD_STATE_LOADED &&
                        m_Context.Commands().Find("bml_probe_dependencies").Code() == BML_OK,
                    "script dependency values and invalid indices");
            CheckScriptAccess(engine);
        }

        BenchmarkCommonApi();

        CheckCommandValidation();
        CheckCommandLifetime(true);
        CheckCommandLifetime(false);

        auto config = m_Context.Config();
        auto value = config.DeclareProperty({"Contract", "Count", int32_t(42)}).Take();
        Require(value.Get<int32_t>() == 42, "numeric configuration read");
        Require(value.Set(int32_t(43)).Code() == BML_OK, "configuration write");
        auto same = config.DeclareProperty({"Contract", "Count", int32_t(7)}).Take();
        Require(same.Read().Value().Handle == value.Read().Value().Handle && same.Get<int32_t>() == 43, "idempotent declaration");
        Require(config.DeclareProperty({"Contract", "Count", std::string("wrong")}).Code() == BML_ERROR_TYPE_MISMATCH, "configuration type conflict");
        auto text = config.DeclareProperty({"Contract", "Text", std::string(128, 'x')}).Take();
        const void *raw = nullptr;
        Require(BML_GetInterface(BML_CONFIG_INTERFACE_ID, 1, &raw) == BML_OK, "config C binding");
        const auto *api = static_cast<const BML_ConfigInterface *>(raw);
        struct Visitor { BML::Config::Property Property; bool Preserved = false; } visitor{text};
        const int read = api->ReadProperty(m_Context.Handle(), text.Read().Value().Handle,
            BML::Detail::Cdecl<[](void *data, const BML_ConfigPropertyInfo *info) {
                auto &state = *static_cast<Visitor *>(data);
                const char *borrowed = info->Value->Data.String;
                if (state.Property.Set(std::string(128, 'y')).Code() != BML_OK) return BML_ERROR_FAIL;
                state.Preserved = std::string_view(borrowed) == std::string(128, 'x');
                return BML_OK;
            }>::Call, &visitor);
        Require(read == BML_OK && visitor.Preserved && text.Get<std::string>() == std::string(128, 'y'), "reentrant configuration borrow");

        auto retained = std::make_shared<int>(7); m_SelfLease = retained;
        m_Self = m_Context.Timers().SetIntervalTicks(1,
            [this, retained](const auto &, const auto &) {
                ++m_SelfCalls;
                Require(m_Self.Cancel().Code() == BML_OK, "self cancellation");
                return true;
            }).Take();
        m_NoArgs = m_Context.Timers().SetTimeoutTicks(1, [this] { ++m_NoArgCalls; }).Take();
        m_VoidLoop = m_Context.Timers().SetIntervalTicks(1, [this] {
            if (++m_VoidCalls == 2) Require(m_VoidLoop.Cancel().Code() == BML_OK, "void interval cancellation");
        }).Take();
        std::function<void()> empty;
        Require(m_Context.Timers().SetTimeoutTicks(1, empty).Code() == BML_ERROR_INVALID_PARAMETER, "empty timer callback");
        BML::Timer::Definition repeat;
        repeat.Type = BML_TIMER_REPEAT; repeat.TimeBase = BML_TIMER_TIMEBASE_TICK;
        repeat.DelayTicks = 1; repeat.RepeatCount = 3;
        m_Repeat = m_Context.Timers().Register(repeat, [this](const auto &, const auto &event) {
            Require(event.CompletedIterations == uint64_t(m_RepeatCalls), "timer event count");
            ++m_RepeatCalls; return true;
        }).Take();

        Require(BML_GetInterface(BML_INPUT_INTERFACE_ID, 1, &raw) == BML_OK, "input C binding");
        const auto *input = static_cast<const BML_InputInterface *>(raw);
        struct Mouse { BML_InputMouseState State{}; uint64_t Tail = 0xdeadbeef; } mouse;
        mouse.State.StructSize = sizeof(mouse);
        Require(input->ReadMouse(m_Context.Handle(), BML_INPUT_RAW, &mouse.State) == BML_OK && mouse.Tail == 0xdeadbeef && mouse.State.StructSize == BML_INPUT_MOUSE_STATE_1_0_SIZE, "output prefix and unknown tail");
        mouse.State.StructSize = BML_INPUT_MOUSE_STATE_1_0_SIZE - 1;
        Require(input->ReadMouse(m_Context.Handle(), 0, &mouse.State) == BML_ERROR_INVALID_PARAMETER && mouse.Tail == 0xdeadbeef, "short output rejection");

        Require(BML_GetInterface(BML_GUI_INTERFACE_ID, 1, &raw) == BML_OK, "GUI C binding");
        const auto *gui = static_cast<const BML_GuiInterface *>(raw);
        for (uint32_t kind = BML_GUI_PANEL; kind <= BML_GUI_ELEMENT; ++kind) {
            BML_ObjectRef object{};
            Require(gui->CreateElement(m_Context.Handle(), kind, "NativeContractElement", &object) == BML_OK, "GUI construction");
            BML_GuiElementInfo info = BML_GUI_ELEMENT_INFO_INIT;
            Require(gui->ReadElement(m_Context.Handle(), object, &info) == BML_OK && info.Kind == kind, "GUI read");
            if (kind == BML_GUI_TEXT) {
                Require(gui->SetSize(m_Context.Handle(), object, {.15f, .03f}) == BML_OK, "GUI text resize");
                Require(gui->SetSize(m_Context.Handle(), object, {1e30f, .1f}) == BML_ERROR_INVALID_PARAMETER, "GUI overflow rejection");
            }
            if (kind == BML_GUI_INPUT) {
                Require(gui->SetText(m_Context.Handle(), object, "ABC") == BML_OK, "GUI input text");
                Require(gui->SetFocused(m_Context.Handle(), object, 1) == BML_OK, "GUI input focus");
                BML_GuiEvent event = BML_GUI_EVENT_INIT;
                Require(gui->OnCharTyped(m_Context.Handle(), object, CKKEY_BACK, &event) == BML_OK, "GUI backspace");
                char text[16]; size_t required = 0;
                Require(gui->GetText(m_Context.Handle(), object, text, sizeof text, &required) == BML_OK && std::string_view(text) == "AB", "GUI text editing");
            }
            if (kind == BML_GUI_PANEL) {
                auto entity = BML::Engine::BorrowObject<CK2dEntity>(m_Context, object).Value();
                engine->DestroyObject(entity->GetID());
                Require(gui->ReadElement(m_Context.Handle(), object, &info) == BML_ERROR_OBJECT_INVALID, "GUI engine deletion");
            }
            const int destroyed = gui->DestroyElement(m_Context.Handle(), object);
            Require(destroyed == BML_OK || (kind == BML_GUI_PANEL && destroyed == BML_ERROR_OBJECT_INVALID), "GUI destruction");
        }
        struct Window : Bui::Window { using Bui::Window::Window; void OnDraw() override {} };
        Window window(m_Context, "Native API contract");
        window.Show();
        if (window.Begin()) {
            Require(Bui::At(m_Context, {.1f, .1f}, [] { return 73; }) == 73, "Bui value callback");
            bool recovered = false;
            try { Bui::At(m_Context, {.1f, .1f}, [] { throw std::runtime_error("local"); }); }
            catch (const std::runtime_error &) { recovered = true; }
            Require(recovered, "Bui local exception propagation");
        }
        window.End();
    }
    static bool Write(const std::string &path, const std::string &text) {
        SDL_IOStream *file = SDL_IOFromFile(path.c_str(), "w");
        if (!file)
            return false;
        const bool written = SDL_WriteIO(file, text.data(), text.size()) == text.size();
        return SDL_CloseIO(file) && written;
    }
    static bool Inject(CKContext *context, bool down) {
        auto *window = static_cast<SDL_Window *>(context->GetMainWindow());
        SDL_Event key{};
        key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        key.key.windowID = SDL_GetWindowID(window);
        key.key.scancode = SDL_SCANCODE_A;
        key.key.key = SDLK_A;
        key.key.down = down;
        key.key.timestamp = SDL_GetTicksNS();
        SDL_Event mouse{};
        mouse.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        mouse.button.windowID = key.key.windowID;
        mouse.button.button = SDL_BUTTON_LEFT;
        mouse.button.down = down;
        mouse.button.x = 1;
        mouse.button.y = 1;
        return SDL_PushEvent(&key) && SDL_PushEvent(&mouse);
    }
    void Finish(bool passed, const std::string &detail) {
        m_Finished = true;
        std::ostringstream result;
        result << "{\"passed\":" << (passed ? "true" : "false") << ",\"detail\":\"" << detail
               << "\",\"frames\":" << m_Frames << ",\"physics_frames\":" << m_PhysicsFrames
               << ",\"renders\":" << m_Renders << ",\"input_phases\":" << m_Phase
               << ",\"bml_priority\":" << m_Priority << m_ApiBenchmark << "}\n";
        if (!Write(m_ResultPath, result.str()))
            (void)m_Context.LogError(std::string("Cannot write PostProcess test result: ")+SDL_GetError());
        (void)m_Context.LogInfo(std::string("PostProcess test: ")+result.str());
        (void)BML::Gameplay::ExitGame(m_Context);
    }

    std::string m_ApiBenchmark;
    uint64_t m_ApiBenchmarkChecksum = 0;
    std::string m_ResultPath;
    BML::Mod::Context m_Context;
    BML::Behavior::Edit m_SymbolEdit;
    BML::Behavior::Edit::Node m_SymbolNode;
    BML::Behavior::Edit::Port m_SymbolPort;
    BML::Behavior::Plan m_SymbolPlan;
    BML::Behavior::Plan::Instance m_PreviousSymbolInstance;
    BML_ObjectRef m_PreviousSymbolNode{};
    BML_ObjectRef m_SurvivingGraph{};
    bool m_CheckedPlanSymbols = false;
    BML::Input::Block m_Block;
    BML::Timer::Registration m_Self, m_Repeat, m_NoArgs, m_VoidLoop;
    std::weak_ptr<int> m_SelfLease;
    BML::Command::Registration m_ProbeCommand;
    std::weak_ptr<int> m_CommandLease;
    std::vector<int> m_CommandEvents;
    bool m_RemoveBeforeExecute = false;
    int m_CountNotifications = 0;
    int m_TextNotifications = 0;
    int m_SelfCalls = 0, m_RepeatCalls = 0, m_NoArgCalls = 0, m_VoidCalls = 0;
    CKDWORD m_LastTick = 0;
    int m_ReloadStep = 0;
    std::string m_ScriptPath, m_ScriptSource;
    int m_Frames = 0, m_PhysicsFrames = 0, m_Renders = 0, m_Phase = 0, m_Priority = 0;
    bool m_MenuReady = false, m_Finished = false;
};
} // namespace

BML_DEFINE_MOD(PostProcessProbe, PostProcessProbe::Define)
