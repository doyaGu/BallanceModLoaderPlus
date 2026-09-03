#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/InputHook.h>

#include "BehaviorRuntimeSemanticsApi.h"
#include "PlayerFrameCapture.h"
#include "PlayerNavigator.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

int RunScriptHookSource(const CKBehaviorContext &context) {
    CKBehavior *behavior = context.Behavior;
    if (!behavior)
        return CKBR_BEHAVIORERROR;
    for (int index = 0; index < behavior->GetInputCount(); ++index)
        behavior->ActivateInput(index, FALSE);
    for (int index = 0; index < behavior->GetOutputCount(); ++index)
        behavior->ActivateOutput(index);
    return CKBR_OK;
}

// Drives the shipped menu graph into Level 01 so the Behavior probe Mods run
// against a real Virtools composition, then reports the acceptance verdict.
// The IVP gameplay scenarios live in ExecuteBBTest and are deliberately not
// interleaved with this flow.
class BehaviorAcceptanceTest final : public IMod {
public:
    explicit BehaviorAcceptanceTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "BehaviorAcceptanceTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Behavior Acceptance Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Drives the Behavior Runtime acceptance flow in Ballance Player";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        m_Navigator.Attach(m_BML, GetLogger());
        const char *disableAngelScript =
            std::getenv("BML_PLAYER_DISABLE_ANGELSCRIPT");
        m_AngelScriptDisabled = disableAngelScript &&
            std::strcmp(disableAngelScript, "1") == 0;
        if (m_AngelScriptDisabled) {
            m_ScriptHookRetirementPassed = true;
            GetLogger()->Info(
                "ScriptHook retirement: skipped=true reason=angelscript-disabled");
        }
        m_ScriptHookSetupFailed =
            !m_AngelScriptDisabled && !CreateScriptHookGraph();
    }

    void OnPostStartMenu() override { m_Navigator.OnPostStartMenu(); }

    void OnStartLevel() override { m_Navigator.OnStartLevel(); }

    void OnBallNavActive() override {
        if (m_Navigator.ControlReady())
            return;
        m_Navigator.OnControlReady();
        GetLogger()->Info("Gameplay input: ready=true");
    }

    void OnProcess() override {
        if (m_Done)
            return;

        if (m_ScriptHookSetupFailed) {
            Finish(false, "script-hook-graph-create-failed");
            return;
        }
        if (!m_ScriptHookRetirementPassed)
            AdvanceScriptHook();
        if (m_Done)
            return;

        ++m_TotalFrames;
        m_Navigator.Observe(m_BML ? m_BML->GetInputManager() : nullptr);
        if (m_Navigator.TutorialFrameRequested())
            m_TutorialFrameCaptureRequested = true;

        const auto now = std::chrono::steady_clock::now();
        if (m_Navigator.LevelStarted() &&
            now - m_Navigator.LevelStartedAt() >
                std::chrono::seconds(kTestTimeoutSeconds)) {
            Finish(false, "test-timeout");
        }
        if (m_Navigator.MenuReady() && !m_Navigator.LevelStarted() &&
            now - m_Navigator.MenuStartedAt() >
                std::chrono::seconds(kMenuTimeoutSeconds)) {
            Finish(false, m_Navigator.Error());
        }

        switch (m_Phase) {
        case Phase::Menu:
            Navigate(m_Navigator.OpenLevelMenu(), Phase::LevelMenu);
            break;
        case Phase::LevelMenu:
            Navigate(m_Navigator.ChooseLevel(), Phase::Loading);
            break;
        case Phase::Loading:
            WaitForControl();
            break;
        case Phase::RuntimeProbe:
            AdvanceRuntimeProbe();
            break;
        case Phase::Stopping:
            Stop();
            break;
        }
    }

    void OnRender(CK_RENDER_FLAGS) override {
        if (m_TutorialFrameCaptureRequested &&
            !m_TutorialFrameCaptureAttempted) {
            m_TutorialFrameCaptureAttempted = true;
            m_TutorialFrameCaptured = CaptureFrame(
                std::getenv("BML_PLAYER_TUTORIAL_FRAME_PATH"),
                "Tutorial frame");
        }
        if (!m_FrameCaptureRequested || m_FrameCaptureAttempted)
            return;
        m_FrameCaptureAttempted = true;
        m_FrameCaptured = CaptureFrame(
            std::getenv("BML_PLAYER_FRAME_PATH"), "Player frame");
    }

    void OnExitGame() override {
        GetLogger()->Info("Behavior acceptance exit: status=%s",
                          m_Passed ? "pass" : "fail");
    }

    void OnUnload() override { DestroyScriptHookGraph(); }

private:
    enum class Phase {
        Menu,
        LevelMenu,
        Loading,
        RuntimeProbe,
        Stopping,
    };

    static constexpr int kMenuTimeoutSeconds = 15;
    static constexpr int kTestTimeoutSeconds = 180;
    static constexpr int kMinimumVisibleLevelSeconds = 5;
    static constexpr int kScriptHookTimeoutFrames = 1800;
    static constexpr auto kStopDelay = std::chrono::milliseconds(100);

    bool CaptureFrame(const char *path, const char *label) {
        CKRenderContext *render = m_BML ? m_BML->GetRenderContext() : nullptr;
        std::uint32_t directXVersion = 0;
        long nativeError = E_INVALIDARG;
        const bool captured = path && *path && render &&
            BML::PlayerTest::SaveRenderFrame(render, path, directXVersion,
                                             nativeError);
        GetLogger()->Info("%s: captured=%s directx=0x%04x native_error=%ld",
                          label, captured ? "true" : "false", directXVersion,
                          nativeError);
        return captured;
    }

    void Navigate(BML::PlayerTest::PlayerNavigator::Step step, Phase next) {
        using Step = BML::PlayerTest::PlayerNavigator::Step;
        if (step == Step::Failed) {
            Finish(false, m_Navigator.Error());
            return;
        }
        if (step == Step::Done)
            SetPhase(next);
    }

    void WaitForControl() {
        if (!m_Navigator.TutorialExited() || !m_Navigator.ControlReady() ||
            !m_BML->IsPlaying())
            return;
        SetPhase(Phase::RuntimeProbe);
    }

    bool CreateScriptHookGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (!context)
            return false;

        m_ScriptHookGraph = static_cast<CKBehavior *>(context->CreateObject(
            CKCID_BEHAVIOR,
            const_cast<char *>("__BML_ScriptHook_Fixture"),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_ScriptHookGraph)
            return false;
        m_ScriptHookGraph->UseGraph();
        m_ScriptHookGraph->SetType(CKBEHAVIORTYPE_SCRIPT);
        CKBehaviorIO *input = m_ScriptHookGraph->CreateInput("In");
        CKBehaviorIO *output = m_ScriptHookGraph->CreateOutput("Out");
        if (!input || !output)
            return false;

        m_ScriptHookSourceBlock = CKBehavior::Cast(context->CreateObject(
            CKCID_BEHAVIOR,
            const_cast<char *>("__BML_ScriptHook_Source"),
            CK_OBJECTCREATION_DYNAMIC));
        if (!m_ScriptHookSourceBlock)
            return false;
        m_ScriptHookSourceBlock->UseFunction();
        m_ScriptHookSourceBlock->SetType(CKBEHAVIORTYPE_BASE);
        m_ScriptHookSourceBlock->SetFunction(RunScriptHookSource);
        m_ScriptHookSourceBlock->CreateInput("In");
        m_ScriptHookSourceBlock->CreateOutput("Out");
        if (m_ScriptHookGraph->AddSubBehavior(m_ScriptHookSourceBlock) != CK_OK)
            return false;

        auto createLink = [&](CKBehaviorIO *from, CKBehaviorIO *to) {
            auto *link = static_cast<CKBehaviorLink *>(context->CreateObject(
                CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
            if (!link || link->SetInBehaviorIO(from) != CK_OK ||
                link->SetOutBehaviorIO(to) != CK_OK ||
                m_ScriptHookGraph->AddSubBehaviorLink(link) != CK_OK) {
                if (link)
                    context->DestroyObject(link);
                return false;
            }
            return true;
        };
        if (!createLink(input, m_ScriptHookSourceBlock->GetInput(0)) ||
            !createLink(m_ScriptHookSourceBlock->GetOutput(0), output)) {
            return false;
        }
        bool outgoing = false;
        for (int index = 0;
             index < m_ScriptHookGraph->GetSubBehaviorLinkCount(); ++index) {
            CKBehaviorLink *link = m_ScriptHookGraph->GetSubBehaviorLink(index);
            if (link && link->GetInBehaviorIO() ==
                            m_ScriptHookSourceBlock->GetOutput(0)) {
                outgoing = true;
            }
        }
        GetLogger()->Info(
            "ScriptHook fixture: lookup=%s name=%s children=%d links=%d "
            "source_outputs=%d outgoing=%s",
            m_BML->GetScriptByName("__BML_ScriptHook_Fixture") ==
                    m_ScriptHookGraph
                ? "true"
                : "false",
            m_ScriptHookGraph->GetName(),
            m_ScriptHookGraph->GetSubBehaviorCount(),
            m_ScriptHookGraph->GetSubBehaviorLinkCount(),
            m_ScriptHookSourceBlock->GetOutputCount(),
            outgoing ? "true" : "false");
        return true;
    }

    void AdvanceScriptHook() {
        if (!m_ScriptHookGraph) {
            Finish(false, "script-hook-graph-missing");
            return;
        }
        if (++m_ScriptHookFrames > kScriptHookTimeoutFrames) {
            Finish(false, "script-hook-timeout");
            return;
        }
        const int children = m_ScriptHookGraph->GetSubBehaviorCount();
        if (!m_ScriptHookInstalled) {
            if (children < 2)
                return;
            m_ScriptHookInstalled = true;
            CKBehavior *inserted = nullptr;
            for (int index = 0; index < children; ++index) {
                CKBehavior *candidate = m_ScriptHookGraph->GetSubBehavior(index);
                if (candidate && candidate->GetName() &&
                    std::strcmp(candidate->GetName(),
                                "__BML_ScriptHook_Inserted") == 0) {
                    inserted = candidate;
                    break;
                }
            }
            if (!inserted || !inserted->GetInput(0)) {
                Finish(false, "script-hook-inserted-block-missing");
                return;
            }
            const float delta =
                m_BML->GetCKContext()->m_BehaviorContext.DeltaTime;
            inserted->ActivateInput(0, TRUE);
            inserted->Activate(TRUE, FALSE);
            (void) inserted->Execute(delta);
            inserted->ActivateInput(0, TRUE);
            inserted->Activate(TRUE, FALSE);
            (void) inserted->Execute(delta);
            return;
        }
        if (children != 1)
            return;

        m_ScriptHookRetirementPassed = true;
        DestroyScriptHookGraph();
    }

    void AdvanceRuntimeProbe() {
        HMODULE module = ::GetModuleHandleA(
            "BehaviorRuntimeSemanticsTest.bmodp");
        const auto read = module
            ? reinterpret_cast<BMLBehaviorRuntimeSemanticsReadFn>(
                  ::GetProcAddress(module,
                                   "BMLBehaviorRuntimeSemanticsRead"))
            : nullptr;
        if (!read) {
            if (PhaseDone(std::chrono::seconds(2)))
                Finish(false, "runtime-semantics-missing");
            return;
        }
        BMLBehaviorRuntimeSemanticsResult result;
        if (!read(&result)) {
            Finish(false, "runtime-semantics-read");
            return;
        }
        if (result.State == BML_BEHAVIOR_RUNTIME_SEMANTICS_PENDING)
            return;
        m_RuntimeProbePassed =
            result.State == BML_BEHAVIOR_RUNTIME_SEMANTICS_PASSED;
        m_LifecycleProbePassed = result.LifecyclePassed != 0;
        m_AdditiveEditProbePassed = result.AdditiveEditPassed != 0;
        m_RuntimeProbeDetail = result.Detail;
        m_FrameCaptureRequested = true;
        if (!m_RuntimeProbePassed) {
            Finish(false, "runtime-semantics-failed");
            return;
        }
        Check();
    }

    void Check() {
        const bool passed = m_RuntimeProbePassed && m_LifecycleProbePassed &&
                            m_AdditiveEditProbePassed &&
                            m_ScriptHookRetirementPassed &&
                            m_Navigator.MenuOpened() &&
                            m_Navigator.LevelChosen() &&
                            m_Navigator.ControlReady() &&
                            m_Navigator.TutorialExitDeclared() &&
                            m_Navigator.TutorialExitReadyObserved() &&
                            m_Navigator.TutorialExitInputObserved() &&
                            m_Navigator.TutorialExited() &&
                            m_Navigator.TutorialExitedByInput();
        Finish(passed, passed ? "completed" : "result-mismatch");
    }

    void Finish(bool passed, const char *reason) {
        if (m_Phase == Phase::Stopping)
            return;
        m_Passed = passed;
        m_Reason = reason;
        SetPhase(Phase::Stopping);
    }

    void Stop() {
        if (!PhaseDone(kStopDelay))
            return;
        if (m_Navigator.LevelStarted() &&
            std::chrono::steady_clock::now() - m_Navigator.LevelStartedAt() <
                std::chrono::seconds(kMinimumVisibleLevelSeconds))
            return;

        DestroyScriptHookGraph();
        GetLogger()->Info(
            "Behavior acceptance: status=%s reason=%s menu_opened=%s "
            "level_chosen=%s control_ready=%s runtime_probe=%s "
            "lifecycle_probe=%s additive_edit=%s script_hook_retirement=%s "
            "tutorial_declared=%s tutorial_listener=%s tutorial_exited=%s "
            "tutorial_by_input=%s runtime_detail=%s frames=%d",
            m_Passed ? "pass" : "fail", m_Reason,
            m_Navigator.MenuOpened() ? "true" : "false",
            m_Navigator.LevelChosen() ? "true" : "false",
            m_Navigator.ControlReady() ? "true" : "false",
            m_RuntimeProbePassed ? "true" : "false",
            m_LifecycleProbePassed ? "true" : "false",
            m_AdditiveEditProbePassed ? "true" : "false",
            m_ScriptHookRetirementPassed ? "true" : "false",
            m_Navigator.TutorialExitDeclared() ? "true" : "false",
            m_Navigator.TutorialExitReadyObserved() ? "true" : "false",
            m_Navigator.TutorialExited() ? "true" : "false",
            m_Navigator.TutorialExitedByInput() ? "true" : "false",
            m_RuntimeProbeDetail.c_str(), m_TotalFrames);
        m_Done = true;
        m_BML->ExitGame();
    }

    void DestroyScriptHookGraph() {
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (context && m_ScriptHookGraph) {
            if (CKScene *scene = context->GetCurrentScene())
                scene->DeActivate(m_ScriptHookGraph);
            while (m_ScriptHookGraph->GetSubBehaviorLinkCount() > 0) {
                CKBehaviorLink *link = m_ScriptHookGraph->RemoveSubBehaviorLink(
                    m_ScriptHookGraph->GetSubBehaviorLinkCount() - 1);
                if (link)
                    context->DestroyObject(link);
            }
            if (m_ScriptHookSourceBlock) {
                m_ScriptHookGraph->RemoveSubBehavior(m_ScriptHookSourceBlock);
                context->DestroyObject(m_ScriptHookSourceBlock);
            }
            context->DestroyObject(m_ScriptHookGraph);
        }
        m_ScriptHookSourceBlock = nullptr;
        m_ScriptHookGraph = nullptr;
    }

    void SetPhase(Phase phase) {
        m_Phase = phase;
        m_PhaseStartedAt = std::chrono::steady_clock::now();
    }

    [[nodiscard]] bool PhaseDone(
        std::chrono::steady_clock::duration duration) const {
        return std::chrono::steady_clock::now() - m_PhaseStartedAt >= duration;
    }

    BML::PlayerTest::PlayerNavigator m_Navigator;
    Phase m_Phase = Phase::Menu;
    CKBehavior *m_ScriptHookGraph = nullptr;
    CKBehavior *m_ScriptHookSourceBlock = nullptr;
    const char *m_Reason = "not-completed";
    int m_TotalFrames = 0;
    int m_ScriptHookFrames = 0;
    bool m_AngelScriptDisabled = false;
    bool m_ScriptHookSetupFailed = false;
    bool m_ScriptHookInstalled = false;
    bool m_ScriptHookRetirementPassed = false;
    bool m_RuntimeProbePassed = false;
    bool m_LifecycleProbePassed = false;
    bool m_AdditiveEditProbePassed = false;
    bool m_FrameCaptureRequested = false;
    bool m_FrameCaptureAttempted = false;
    bool m_FrameCaptured = false;
    bool m_TutorialFrameCaptureRequested = false;
    bool m_TutorialFrameCaptureAttempted = false;
    bool m_TutorialFrameCaptured = false;
    bool m_Passed = false;
    bool m_Done = false;
    std::string m_RuntimeProbeDetail = "not-run";
    std::chrono::steady_clock::time_point m_PhaseStartedAt{};
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new BehaviorAcceptanceTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
