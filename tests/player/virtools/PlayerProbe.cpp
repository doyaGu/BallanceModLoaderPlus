#include <BML/Engine.hpp>
#include <BML/Behavior.hpp>
#include <BML/Gameplay.h>
#include <BML/UI.h>

#include "../GameplayTimerCheck.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

void Require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

class PlayerProbe final : public BML::Mod::Instance {
public:
    void OnLoad(const BML::Mod::Context &ctx) override {
        auto property = ctx.Config().DeclareProperty(
            {"Test", "Chinese", std::string("initial")}).Take();
        if (std::getenv("BML_PORT_CHECK_CONFIG")) {
            Require(property.Get<std::string>() == "配置可读", "configuration did not reload");
            (void)ctx.LogInfo("Win32 config reload PASS");
        } else {
            Require(static_cast<bool>(property.Set(std::string("配置可读"))), "configuration write failed");
            (void)ctx.LogInfo("Win32 config write PASS");
        }
        m_Begin = Clock::now();
    }

    void OnPostStartMenu(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("Win32 menu PASS");
        m_Menu = Clock::now();
    }

    void OnStartLevel(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("Win32 level start PASS");
        m_Level = Clock::now();
    }

    void OnPauseLevel(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("Win32 pause PASS");
    }
    void OnUnpauseLevel(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("Win32 resume PASS");
    }
    void OnPostResetLevel(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("Win32 restart PASS");
    }
    void OnUnload(const BML::Mod::Context &ctx) override {
        (void)ctx.LogInfo("Win32 unload PASS");
    }

    void OnProcess(const BML::Mod::Context &ctx) override {
        if (m_Done)
            return;
        auto *engine = BML::Engine::BorrowContext(ctx).Value();
        const auto now = Clock::now();
        try {
            if (now - m_Begin > std::chrono::seconds(22))
                throw std::runtime_error("gameplay acceptance timed out");
            if (m_Menu == Clock::time_point{})
                return;
            const CKDWORD tick = engine->GetTimeManager()->GetMainTickCount();
            Require(tick != m_Tick, "duplicate Process callback");
            m_Tick = tick;
            if (m_Plan)
                CheckPlanReplacement(ctx);
            if (!m_OpenedLevelMenu && now - m_Menu > std::chrono::milliseconds(300)) {
                Require(MenuButtonVisible(*engine, "M_Main_But_1"), "main menu buttons were never shown");
                ActivateMenuAction(ctx, *engine, "Menu_Main", "Start");
                m_OpenedLevelMenu = true;
            }
            if (m_OpenedLevelMenu && !m_Requested && MenuButtonVisible(*engine, "M_Start_But_01")) {
                Require(!MenuButtonVisible(*engine, "M_Main_But_1"), "main menu buttons remained after Start");
                CheckNativeGameplayTimer(ctx, *engine);
                auto *level = CKDataArray::Cast(engine->GetObjectByNameAndClass("CurrentLevel", CKCID_DATAARRAY));
                auto *handler = CKBehavior::Cast(engine->GetObjectByNameAndClass("Event_handler", CKCID_BEHAVIOR));
                Require(level && handler && handler->GetOwner(), "missing level objects");
                int number = 1;
                Require(level->SetElementValue(0, 0, &number) != FALSE, "cannot choose level 1");
                CheckBehavior(ctx, *handler);
                if (std::getenv("BML_PORT_CHECK_MAP")) {
                    Require(ctx.Commands().ExecuteLine("map load \"中文地图.nmo\"").Take() == 0,
                            "Chinese map command failed");
                } else {
                    // The first Send Message sends Load Level, then the stock graph hides the menu and exits.
                    ActivateMenuAction(ctx, *engine, "Menu_Start", "Send Message");
                }
                m_Requested = true;
            }
            if (m_Level != Clock::time_point{} && !m_CheckedMenuExit &&
                now - m_Level > std::chrono::milliseconds(250)) {
                Require(!MenuButtonVisible(*engine, "M_Main_But_1") &&
                        !MenuButtonVisible(*engine, "M_Start_But_01") &&
                        !MenuButtonVisible(*engine, "M_Start_But_Back"),
                        "menu buttons remained after level start");
                (void)ctx.LogInfo("Win32 menu exit PASS");
                m_CheckedMenuExit = true;
            }
            if (m_Level != Clock::time_point{} && !m_CheckedTweaks && std::getenv("BML_PORT_CHECK_TWEAKS")) {
                CheckGameplayTweaks(ctx, *engine);
                m_CheckedTweaks = true;
            }
            if (m_Level != Clock::time_point{} && std::getenv("BML_PORT_CHECK_RENDER"))
                CheckSkippedFrames(ctx);
            if (m_Level != Clock::time_point{} && std::getenv("BML_PORT_CHECK_EVENTS")) {
                if (m_Transition == 0 && now - m_Level > std::chrono::seconds(1)) {
                    SendLevelEvent(*engine, 5);
                    ++m_Transition;
                } else if (m_Transition == 1 && now - m_Level > std::chrono::seconds(2)) {
                    SendLevelEvent(*engine, 6);
                    ++m_Transition;
                } else if (m_Transition == 2 && now - m_Level > std::chrono::seconds(3)) {
                    SendLevelEvent(*engine, 4);
                    ++m_Transition;
                }
            }
            if (m_Level != Clock::time_point{} && now - m_Level > std::chrono::seconds(7)) {
                if (const char *path = std::getenv("BML_PORT_FRAME_TRACE")) {
                    std::ofstream output(path);
                    for (double frame : m_Intervals)
                        output << frame << '\n';
                }
                (void)ctx.LogInfo("Win32 gameplay probe PASS");
                m_Done = true;
                (void)BML::Gameplay::ExitGame(ctx);
            }
        } catch (const std::exception &error) {
            (void)ctx.LogError(error.what());
            m_Done = true;
            (void)BML::Gameplay::ExitGame(ctx);
        }
    }

    void OnRender(const BML::Mod::Context &, const BML::Mod::RenderEvent &) override {
        const auto now = Clock::now();
        ++m_Renders;
        if (m_Level != Clock::time_point{} && now - m_Level >= std::chrono::seconds(2))
            m_Intervals.push_back(std::chrono::duration<double, std::milli>(now - m_LastRender).count());
        m_LastRender = now;
    }

private:
    static bool MenuButtonVisible(CKContext &engine, const char *name) {
        auto *button = engine.GetObjectByNameAndClass(const_cast<CKSTRING>(name), CKCID_2DENTITY);
        Require(button, "missing retail menu button");
        return button->IsVisible() == CKSHOW;
    }

    static void ActivateMenuAction(const BML::Mod::Context &ctx, CKContext &engine,
                                   const char *scriptName, const char *actionName) {
        using namespace BML::Behavior;
        auto *script = CKBehavior::Cast(engine.GetObjectByNameAndClass(const_cast<CKSTRING>(scriptName), CKCID_BEHAVIOR));
        Require(script, "missing retail menu script");
        auto session = Session::Open().Take();
        auto graph = session.Inspect(script, View::Logical).Take();
        const auto action = graph.Find(Named(actionName, 0)).Take().Object();
        auto *node = BML::Engine::BorrowObject<CKBehavior>(ctx, action).Take();
        Require(node, "missing retail menu action");
        node->ActivateInput(0);
        node->Activate();
    }

    void CheckSkippedFrames(const BML::Mod::Context &ctx) {
        constexpr unsigned frames = 60;
        if (m_RenderFrame == 0) {
            Require(BML::UI::ShowFPS(true) == BML_OK, "cannot show HUD FPS");
            m_LastRenderCount = m_Renders;
        }
        if (m_RenderFrame < frames && m_RenderFrame % 2 == 0)
            Require(BML::UI::SkipNextRender(ctx) == BML_OK, "cannot request Win32 render skip");
        if (m_RenderFrame == frames) {
            Require(m_Renders - m_LastRenderCount == frames / 2,
                    "render callbacks included skipped frames or missed rendered frames");
            (void)ctx.LogInfo("Win32 render skipping PASS, 30 rendered frames in 60 updates");
        }
        ++m_RenderFrame;
    }

    static void CheckGameplayTweaks(const BML::Mod::Context &ctx, CKContext &engine) {
        using namespace BML::Behavior;
        auto *level = CKBehavior::Cast(engine.GetObjectByNameAndClass("Levelinit_build", CKCID_BEHAVIOR));
        auto *energy = CKBehavior::Cast(engine.GetObjectByNameAndClass("Gameplay_Energy", CKCID_BEHAVIOR));
        auto *ingame = CKBehavior::Cast(engine.GetObjectByNameAndClass("Gameplay_Ingame", CKCID_BEHAVIOR));
        Require(level && energy && ingame, "missing retail gameplay scripts");
        auto session = Session::Open().Take();
        auto levelGraph = session.Inspect(level, View::Live).Take();
        auto mapping = levelGraph.Inspect(levelGraph.Find(Unique("set Mapping and Textures")).Take()).Take();
        auto lantern = mapping.Inspect(mapping.Find(Unique("Set Mat Laterne")).Take()).Take();
        auto alpha = lantern.Find(Unique("Set Alpha Test")).Take();
        const auto alphaValue = lantern.Read(alpha.Pin(At(0))).Take();
        Require(std::get<bool>(alphaValue.Data), "lantern alpha test was not applied");
        (void)ctx.LogInfo("Win32 lantern alpha test PASS");

        auto energyGraph = session.Inspect(energy, View::Live).Take();
        auto delay = energyGraph.Find(Unique("Delayer")).Take();
        const bool enabled = std::string_view(std::getenv("BML_PORT_CHECK_TWEAKS")) == "1";
        Require(static_cast<bool>(energyGraph.Entering(delay)) != enabled,
                "Overclock did not configure the retail energy delay");
        auto gameplay = session.Inspect(ingame, View::Live).Take();
        auto manager = gameplay.Inspect(gameplay.Find(Unique("BallManager")).Take()).Take();
        auto deactivate = manager.Inspect(manager.Find(Unique("Deactivate Ball")).Take()).Take();
        auto reset = deactivate.Find(Unique("reset Ballpieces")).Take();
        auto next = deactivate.Next(reset).Take();
        Require((next.Name() == "Physicalize") == enabled,
                "Overclock did not configure the retail deactivate path");
        auto newBall = manager.Inspect(manager.Find(Unique("New Ball")).Take()).Take();
        auto physicalize = newBall.Find(Unique("physicalize new Ball")).Take();
        Require(newBall.Incoming(physicalize).size() == (enabled ? 2u : 1u),
                "Overclock did not configure the retail spawn path");
        (void)ctx.LogInfo(enabled ? "Win32 Overclock enabled PASS" : "Win32 Overclock disabled PASS");
    }

    static void SendLevelEvent(CKContext &engine, int slot) {
        auto *handler = CKBehavior::Cast(engine.GetObjectByNameAndClass("Event_handler", CKCID_BEHAVIOR));
        Require(handler && handler->GetOwner(), "event handler missing");
        for (int i = 0; i < handler->GetSubBehaviorCount(); ++i) {
            auto *block = handler->GetSubBehavior(i);
            if (block && block->GetInputParameterCount() == 11 &&
                block->GetOutputCount() == 11 && std::string_view(block->GetName()) == "Switch On Message") {
                CKMessageType message{};
                Require(block->GetInputParameterValue(slot, &message) == CK_OK,
                        "cannot read level dispatch message");
                engine.GetMessageManager()->SendMessageSingle(message, handler->GetOwner());
                return;
            }
        }
        throw std::runtime_error("level dispatch not found");
    }

    void CheckBehavior(const BML::Mod::Context &ctx, CKBehavior &target) {
        using namespace BML::Behavior;
        auto session = Session::Open().Take();
        const auto object = BML::Engine::ReferenceObject(ctx, &target).Take();
        auto graph = session.Inspect(object).Take();
        Edit bad;
        (void)bad.Root().AddGraph("__Win32_Test_Failed").Graph().Require("__Win32_Test_Missing");
        auto failed = session.Apply("failed graph edit", EditOwnership::Submission, On(graph, bad));
        Require(!failed && failed.GetStatus().Error == Error::QueryNotFound, "failed Apply status");
        graph = session.Inspect(object).Take();
        Require(graph.FindAll("__Win32_Test_Failed").empty(), "failed Apply left a graph mutation");
        Edit good;
        auto root = good.Root();
        auto local = root.AppendLocal("__Win32_Test_Local", CKPGUID_INT);
        root.Bind(local, 41);
        auto applied = session.Apply("temporary graph edit", EditOwnership::Submission, On(graph, good)).Take();
        auto receipt = applied.Find(object).Take();
        Require(std::get<int32_t>(receipt.Read(local).Take().Data) == 41, "incorrect Behavior value");
        applied.SetInput(local, 42).Take();
        Require(std::get<int32_t>(receipt.Read(local).Take().Data) == 42, "Behavior write failed");
        Require(applied.Close().Take() == CloseState::Closed, "cannot close graph edit");
        Require(!receipt.Read(local), "retired receipt remained valid");
        (void)ctx.LogInfo("Win32 Behavior Inspect, Apply, parameter and rollback PASS");
        CheckNativeRuns(ctx, session, target);
        CheckHook(ctx, session, target);
        StartPlanReplacement(session, target, object);
        CheckObjectRetirement(ctx, *target.GetCKContext());
        CheckPrototypeRetirement(ctx, session);
    }

    static void CheckNativeRuns(const BML::Mod::Context &ctx, BML::Behavior::Session &session,
                                CKBehavior &target) {
        using namespace BML::Behavior;
        auto block = session.Use(TT_TOOLBOX_RT_TT_GETTIME);
        auto call = block.Call(At(0), Latest()).Take();
        Require(!call.TakeFrames().Take().Empty(), "native Call produced no frame");
        (void)call.Close().Take();
        (void)ctx.LogInfo("Win32 Behavior Call PASS");

        auto task = block.Start(At(0), Latest()).Take();
        Require(!task.TakeFrames().Take().Empty(), "native Start produced no initial frame");
        (void)task.Close().Take();
        (void)ctx.LogInfo("Win32 Behavior Start PASS");

        auto instance = block.Spawn(Latest()).Take();
        (void)instance.Pulse(At(0)).Take();
        Require(!instance.TakeFrames().Take().Empty(), "native Spawn pulse produced no frame");
        (void)instance.Close().Take();
        (void)ctx.LogInfo("Win32 Behavior Spawn PASS");

        Edit body;
        auto root = body.Root();
        auto local = root.AppendLocal("__Win32_Script_Local", CKPGUID_INT);
        root.Bind(local, 37);
        auto script = session.CreateScript(target.GetOwner(), "__Win32_Acceptance_Script", body).Take();
        Require(!script.Info().Take().Active, "new graph script should start inactive");
        Require(static_cast<bool>(session.Inspect(script.Object())), "cannot inspect newly created script");
        (void)script.Close().Take();
        (void)ctx.LogInfo("Win32 Behavior CreateScript PASS");
    }

    static void CheckHook(const BML::Mod::Context &ctx, BML::Behavior::Session &session,
                          CKBehavior &target) {
        using namespace BML::Behavior;
        int calls = 0;
        auto hooked = session.SpawnIn(&target, Hook([&](const HookEvent &event) {
            Require(event.Block.Domain && event.Owner.Domain, "missing Hook object references");
            ++calls;
            return HookResult::Ok;
        })).Take();
        (void)hooked.Pulse(At(0)).Take();
        Require(calls == 1, "native Hook did not dispatch once");
        (void)hooked.Close().Take();
        (void)ctx.LogInfo("Win32 Behavior Hook PASS");
    }

    void StartPlanReplacement(BML::Behavior::Session &session, CKBehavior &target,
                              BML_ObjectRef object) {
        using namespace BML::Behavior;
        m_PlanObject = object;
        m_PlanScriptName = target.GetName();
        Edit original;
        auto root = original.Root();
        m_PlanLocal = root.AppendLocal("__Win32_Plan_Baseline", CKPGUID_INT);
        root.Bind(*m_PlanLocal, 91);
        m_Plan.emplace(session.Apply("Win32 replacement acceptance", EditOwnership::Submission,
                                    On(Scripts::One(m_PlanScriptName), original)).Take());
    }

    void CheckPlanReplacement(const BML::Mod::Context &ctx) {
        using namespace BML::Behavior;
        auto &plan = *m_Plan;
        auto read = [&] {
            return std::get<int32_t>(plan.Find(m_PlanObject).Take().Read(*m_PlanLocal).Take().Data);
        };
        switch (m_PlanStep++) {
        case 0:
            Require(read() == 91, "initial Plan did not apply");
            plan.SetEnabled(false).Take();
            break;
        case 1:
            Require(!plan.Find(m_PlanObject), "disabled Plan retained its installation");
            plan.SetEnabled(true).Take();
            break;
        case 2: {
            Require(read() == 91, "Plan cannot be reenabled");
            Edit bad;
            (void)bad.Root().Require("__Win32_Plan_Not_Found");
            (void)plan.Replace(On(Scripts::One(m_PlanScriptName), bad));
            break;
        }
        case 3:
            Require(plan.Info().Take().ApplyFailure.Error == Error::QueryNotFound,
                    "failed Plan replacement lost its diagnostic");
            Require(read() == 91, "failed Plan replacement did not restore the prior definition");
            plan.SetEnabled(false).Take();
            plan.SetEnabled(true).Take();
            break;
        default:
            Require(read() == 91, "failed replacement lost the definition on the next enable");
            (void)plan.Close().Take();
            m_Plan.reset();
            (void)ctx.LogInfo("Win32 Behavior Plan enable, disable and failed replacement PASS");
            break;
        }
    }

    static void CheckPrototypeRetirement(const BML::Mod::Context &ctx,
                                         BML::Behavior::Session &session) {
        using namespace BML::Behavior;
        const CKGUID type(0x63e4f18a, 0x16bfc0e4);
        auto *plugins = CKGetPluginManager();
        auto *entry = plugins->FindComponent(type, plugins->GetCategoryIndex(CKPLUGIN_BEHAVIOR));
        Require(entry != nullptr, "prototype fixture was not loaded");
        const int index = entry->m_PluginDllIndex;
        const auto before = session.Layout(type).Take().PrototypeRef;
        Require(before.Generation != 0, "initial prototype lacks a generation");
        Require(plugins->UnLoadPluginDll(index) == CK_OK, "cannot unload the prototype fixture");
        Require(plugins->ReLoadPluginDll(index) == CK_OK, "cannot reload the prototype fixture");
        const auto after = session.Layout(type).Take().PrototypeRef;
        Require(after.Generation != before.Generation && !session.Layout(before),
                "same-version prototype reload reused the retired identity");
        (void)ctx.LogInfo("Win32 same-version prototype reload PASS");
    }

    static void CheckObjectRetirement(const BML::Mod::Context &ctx, CKContext &engine) {
        auto *first = engine.CreateObject(CKCID_3DENTITY, const_cast<char *>("Win32 reference test"),
                                          CK_OBJECTCREATION_DYNAMIC);
        Require(first != nullptr, "cannot create object reference fixture");
        const auto id = first->GetID();
        const auto reference = BML::Engine::ReferenceObject(ctx, first).Take();
        Require(BML::Engine::BorrowObject<CK3dEntity>(ctx, reference).Take() == first,
                "cannot borrow live object reference");
        Require(engine.DestroyObject(first) == CK_OK, "cannot delete object reference fixture");
        Require(!BML::Engine::BorrowObject<CK3dEntity>(ctx, reference),
                "object reference remained valid after deletion");
        bool reused = false;
        for (int i = 0; i < 128 && !reused; ++i) {
            auto *next = engine.CreateObject(CKCID_3DENTITY, const_cast<char *>("Win32 replacement object"),
                                             CK_OBJECTCREATION_DYNAMIC);
            Require(next != nullptr, "cannot create replacement object");
            reused = next->GetID() == id;
            if (reused) {
                const auto replacement = BML::Engine::ReferenceObject(ctx, next).Take();
                Require(!BML::Engine::BorrowObject<CK3dEntity>(ctx, reference) &&
                        BML::Engine::BorrowObject<CK3dEntity>(ctx, replacement).Take() == next,
                        "stale reference resolved to a recycled CK object");
            }
            Require(engine.DestroyObject(next) == CK_OK, "cannot delete replacement fixture");
        }
        (void)ctx.LogInfo(reused ? "Win32 object deletion and ID reuse PASS"
                                : "Win32 object deletion PASS, CK ID was not reused");
    }

    Clock::time_point m_Begin{}, m_Menu{}, m_Level{}, m_LastRender{};
    std::vector<double> m_Intervals;
    CKDWORD m_Tick{};
    unsigned m_Transition{}, m_PlanStep{};
    BML_ObjectRef m_PlanObject{};
    std::string m_PlanScriptName;
    std::optional<BML::Behavior::Submission> m_Plan;
    std::optional<BML::Behavior::Edit::Port> m_PlanLocal;
    unsigned m_RenderFrame = 0, m_Renders = 0, m_LastRenderCount = 0;
    bool m_OpenedLevelMenu = false, m_Requested = false, m_CheckedMenuExit = false;
    bool m_Done = false, m_CheckedTweaks = false;
};
}

BML_DEFINE_MOD(PlayerProbe, {
    .Id = "acceptance.virtools",
    .Name = "Virtools Player acceptance",
    .Version = "1.0.0",
    .Author = "BML",
    .MinimumLoaderVersion = BML_VERSION,
})
