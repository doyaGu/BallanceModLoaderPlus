#include <BML/ExecuteBB.h>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/ScriptHelper.h>

#include <chrono>
#include <cmath>
#include <cstring>

namespace {

class ExecuteBBTest final : public IMod {
public:
    explicit ExecuteBBTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "ExecuteBBTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "ExecuteBB Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Tests the public ExecuteBB interface in Ballance Player";
    }
    DECLARE_BML_VERSION;

    void OnPostStartMenu() override {
        m_MenuReady = true;
        m_MenuStartedAt = std::chrono::steady_clock::now();
    }

    void OnStartLevel() override {
        m_LevelStarted = true;
        m_LevelStartedAt = std::chrono::steady_clock::now();
    }

    void OnBallNavActive() override {
        m_ControlReady = true;
        m_ControlReadyAt = std::chrono::steady_clock::now();
    }

    void OnProcess() override {
        if (m_Done)
            return;

        ++m_TotalFrames;
        if (m_LevelStarted && std::chrono::steady_clock::now() - m_LevelStartedAt >
            std::chrono::seconds(kTestTimeoutSeconds)) {
            Finish(false, "test-timeout");
        }
        if (m_MenuReady && !m_LevelStarted &&
            std::chrono::steady_clock::now() - m_MenuStartedAt >
                std::chrono::seconds(kMenuTimeoutSeconds)) {
            Finish(false, m_MenuError);
        }

        switch (m_Phase) {
        case Phase::Menu:
            OpenLevelMenu();
            break;
        case Phase::LevelMenu:
            ChooseLevel();
            break;
        case Phase::Loading:
            WaitForControl();
            break;
        case Phase::World:
            CreateBody();
            break;
        case Phase::Body:
            Push();
            break;
        case Phase::Push:
            Pull();
            break;
        case Phase::Pull:
            Release();
            break;
        case Phase::Released:
            Check();
            break;
        case Phase::Stopping:
            Stop();
            break;
        }
    }

    void OnPhysicalize(CK3dEntity *target, CKBOOL, float, float, float, const char *,
                       CKBOOL, CKBOOL, CKBOOL, float, float, const char *, VxVector,
                       int, CKMesh **, int, VxVector *, float *, int, CKMesh **) override {
        if (target == m_Body)
            m_PhysicalizeSeen = true;
    }

    void OnUnphysicalize(CK3dEntity *target) override {
        if (target == m_Body)
            m_UnphysicalizeSeen = true;
    }

    void OnExitGame() override {
        GetLogger()->Info("ExecuteBB test exit: status=%s",
                          m_Passed ? "pass" : "fail");
    }

    void OnUnload() override {
        DestroyBody();
    }

private:
    enum class Phase {
        Menu,
        LevelMenu,
        Loading,
        World,
        Body,
        Push,
        Pull,
        Released,
        Stopping,
    };

    static constexpr int kMenuDelayFrames = 5;
    static constexpr int kMenuTimeoutSeconds = 15;
    static constexpr int kTestTimeoutSeconds = 60;
    static constexpr int kMinimumVisibleLevelSeconds = 5;
    static constexpr float kForceMagnitude = 1000.0f;
    static constexpr float kMinimumTravel = 0.01f;
    static constexpr float kReleasedX = 321.0f;
    static constexpr float kReleaseTolerance = 0.05f;
    static constexpr auto kGameplaySettleTime = std::chrono::milliseconds(500);
    static constexpr auto kWorldSettleTime = std::chrono::milliseconds(250);
    static constexpr auto kPhysicalizationSettleTime = std::chrono::milliseconds(500);
    static constexpr auto kPositiveForceTime = std::chrono::milliseconds(1000);
    static constexpr auto kReverseForceTime = std::chrono::milliseconds(1500);
    static constexpr auto kReleaseObservationTime = std::chrono::milliseconds(250);
    static constexpr auto kStopDelay = std::chrono::milliseconds(100);

    void OpenLevelMenu() {
        if (!m_MenuReady)
            return;
        if (++m_PhaseFrames < kMenuDelayFrames)
            return;

        CKBehavior *menuMain = m_BML->GetScriptByName("Menu_Main");
        if (!menuMain) {
            m_MenuError = "menu-main-not-ready";
            return;
        }

        CKBehavior *start = ScriptHelper::FindFirstBB(
            menuMain, "Start", false, 1, 1);
        if (!start) {
            Finish(false, "menu-start-path-not-found");
            return;
        }

        // Menu.nmo: Main Menu.Button 1 pressed -> Menu_Main/Start.In 0.
        // Activate the click's proven downstream seam and leave the original
        // Activate Script graph to open Menu_Start.
        start->ActivateInput(0);
        start->Activate();
        m_LevelMenuOpened = true;
        GetLogger()->Info(
            "ExecuteBB test menu: opened=true path=Menu_Main/Start.In0");
        SetPhase(Phase::LevelMenu);
    }

    void ChooseLevel() {
        CKBehavior *menuScript = m_BML->GetScriptByName("Menu_Start");
        CK2dEntity *levelButton = m_BML->Get2dEntityByName("M_Start_But_01");
        if (!menuScript || !levelButton || !levelButton->IsVisible()) {
            m_MenuError = "level-menu-not-ready";
            return;
        }

        CKBehavior *levelMenu = ScriptHelper::FindFirstBB(
            menuScript, "Start Menu", false, 1, 2);
        if (!levelMenu) {
            Finish(false, "level-menu-path-not-found");
            return;
        }

        CKBehavior *buttonBehavior = nullptr;
        for (int i = 0; i < levelMenu->GetSubBehaviorCount(); ++i) {
            CKBehavior *candidate = levelMenu->GetSubBehavior(i);
            if (!candidate || !candidate->GetName() ||
                std::strcmp(candidate->GetName(), "TT PushButton2") != 0 ||
                candidate->GetOutputCount() <= 2 || !candidate->GetTargetParameter()) {
                continue;
            }
            CKParameter *targetSource = candidate->GetTargetParameter()->GetRealSource();
            if (targetSource && targetSource->GetValueObject() == levelButton) {
                buttonBehavior = candidate;
                break;
            }
        }
        if (!buttonBehavior) {
            Finish(false, "level-button-path-not-found");
            return;
        }

        CKBehaviorLink *mouseDown = ScriptHelper::FindNextLink(
            levelMenu, buttonBehavior, "Parameter Selector", 2);
        CKBehaviorIO *selectorInput = mouseDown ? mouseDown->GetOutBehaviorIO() : nullptr;
        CKBehavior *selector = selectorInput ? selectorInput->GetOwner() : nullptr;
        int selectorInputIndex = -1;
        if (selector) {
            for (int i = 0; i < selector->GetInputCount(); ++i) {
                if (selector->GetInput(i) == selectorInput) {
                    selectorInputIndex = i;
                    break;
                }
            }
        }
        if (!selector || selectorInputIndex != 0) {
            Finish(false, "level-button-link-mismatch");
            return;
        }

        // Menu.nmo: M_Start_But_01.Mouse Down -> Parameter Selector.In 0.
        // Deliver exactly that link's effect; the selector and every following
        // message/test/load block remain the shipped graph's responsibility.
        selector->ActivateInput(selectorInputIndex);
        selector->Activate();
        m_LevelChosen = true;
        GetLogger()->Info(
            "ExecuteBB test menu: level=1 path=Start_Menu/Parameter_Selector.In0");
        SetPhase(Phase::Loading);
    }

    void WaitForControl() {
        if (!m_LevelStarted || !m_ControlReady || !m_BML->IsPlaying())
            return;
        if (std::chrono::steady_clock::now() - m_ControlReadyAt <
            kGameplaySettleTime)
            return;
        SetPhase(Phase::World);
    }

    void CreateBody() {
        CKContext *context = m_BML->GetCKContext();
        CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
        CKScene *scene = context ? context->GetCurrentScene() : nullptr;
        if (!context || !level || !scene)
            return;

        if (!PhaseDone(kWorldSettleTime))
            return;

        auto *body = CK3dObject::Cast(context->CreateObject(
            CKCID_3DOBJECT, const_cast<char *>("__BML_ExecuteBB_Test"),
            static_cast<CK_OBJECTCREATION_OPTIONS>(CK_OBJECTCREATION_DYNAMIC |
                                                    CK_OBJECTCREATION_ACTIVATE)));
        if (!body) {
            Finish(false, "body-create-failed");
            return;
        }

        m_Body = body;
        if (level->AddObject(m_Body) != CK_OK) {
            Finish(false, "body-add-failed");
            return;
        }
        if (scene != level->GetLevelScene())
            scene->AddObject(m_Body);
        scene->Activate(m_Body, TRUE);

        const VxVector origin(0.0f, 0.0f, 5000.0f);
        m_Body->SetPosition(&origin);
        m_InitialX = ReadX();

        ExecuteBB::PhysicalizeBall(
            m_Body, FALSE, 0.0f, 0.0f, 1.0f, "", FALSE, FALSE, FALSE,
            0.0f, 0.0f, "", VxVector(), VxVector(), 2.0f);
        m_Physicalized = true;
        SetPhase(Phase::Body);
    }

    void Push() {
        if (!PhaseDone(kPhysicalizationSettleTime))
            return;

        if (!BodyPositionIsFinite()) {
            Finish(false, "invalid-body-position");
            return;
        }

        m_PushStartX = ReadX();
        ExecuteBB::SetPhysicsForce(m_Body, VxVector(), nullptr,
                                   VxVector(1.0f, 0.0f, 0.0f), nullptr,
                                   kForceMagnitude);
        m_ForceSet = true;
        SetPhase(Phase::Push);
    }

    void Pull() {
        if (!PhaseDone(kPositiveForceTime))
            return;

        m_PushedX = ReadX();
        ExecuteBB::SetPhysicsForce(m_Body, VxVector(), nullptr,
                                   VxVector(-1.0f, 0.0f, 0.0f), nullptr,
                                   kForceMagnitude);
        SetPhase(Phase::Pull);
    }

    void Release() {
        if (!PhaseDone(kReverseForceTime))
            return;

        m_PulledX = ReadX();
        ExecuteBB::UnsetPhysicsForce(m_Body);
        m_ForceSet = false;
        ExecuteBB::Unphysicalize(m_Body);
        m_Physicalized = false;

        const VxVector released(kReleasedX, 0.0f, 5000.0f);
        m_Body->SetPosition(&released);
        SetPhase(Phase::Released);
    }

    void Check() {
        if (!PhaseDone(kReleaseObservationTime))
            return;

        m_ReleasedX = ReadX();
        const float pushTravel = m_PushedX - m_PushStartX;
        const float pullTravel = m_PushedX - m_PulledX;
        const bool passed = BodyPositionIsFinite() &&
                            pushTravel > kMinimumTravel &&
                            pullTravel > kMinimumTravel &&
                            std::fabs(m_ReleasedX - kReleasedX) <= kReleaseTolerance &&
                            m_PhysicalizeSeen && m_UnphysicalizeSeen;
        Finish(passed, passed ? "completed" : "result-mismatch");
    }

    void Finish(bool passed, const char *reason) {
        if (m_Phase == Phase::Stopping)
            return;

        m_Passed = passed;
        m_Reason = reason;
        if (m_Body) {
            if (m_ForceSet) {
                ExecuteBB::UnsetPhysicsForce(m_Body);
                m_ForceSet = false;
            }
            if (m_Physicalized) {
                ExecuteBB::Unphysicalize(m_Body);
                m_Physicalized = false;
            }
        }
        SetPhase(Phase::Stopping);
    }

    void Stop() {
        if (!PhaseDone(kStopDelay))
            return;
        if (m_LevelStarted && std::chrono::steady_clock::now() - m_LevelStartedAt <
                                  std::chrono::seconds(kMinimumVisibleLevelSeconds))
            return;

        DestroyBody();
        GetLogger()->Info(
            "ExecuteBB test: status=%s reason=%s x0=%.6f push_start=%.6f "
            "pushed=%.6f pulled=%.6f released=%.6f "
            "physicalize_event=%s unphysicalize_event=%s menu_opened=%s "
            "level_chosen=%s control_ready=%s frames=%d",
            m_Passed ? "pass" : "fail", m_Reason, m_InitialX, m_PushStartX,
            m_PushedX, m_PulledX, m_ReleasedX,
            m_PhysicalizeSeen ? "true" : "false",
            m_UnphysicalizeSeen ? "true" : "false",
            m_LevelMenuOpened ? "true" : "false",
            m_LevelChosen ? "true" : "false",
            m_ControlReady ? "true" : "false", m_TotalFrames);
        m_Done = true;
        m_BML->ExitGame();
    }

    void DestroyBody() {
        if (!m_Body)
            return;
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        if (context)
            context->DestroyObject(m_Body);
        m_Body = nullptr;
    }

    void SetPhase(Phase phase) {
        m_Phase = phase;
        m_PhaseFrames = 0;
        m_PhaseStartedAt = std::chrono::steady_clock::now();
    }

    bool PhaseDone(std::chrono::steady_clock::duration duration) const {
        return std::chrono::steady_clock::now() - m_PhaseStartedAt >= duration;
    }

    float ReadX() const {
        VxVector position;
        m_Body->GetPosition(&position);
        return position.x;
    }

    bool BodyPositionIsFinite() const {
        if (!m_Body)
            return false;
        VxVector position;
        m_Body->GetPosition(&position);
        return std::isfinite(position.x) && std::isfinite(position.y) &&
               std::isfinite(position.z);
    }

    Phase m_Phase = Phase::Menu;
    CK3dObject *m_Body = nullptr;
    const char *m_Reason = "not-completed";
    const char *m_MenuError = "menu-timeout";
    int m_TotalFrames = 0;
    int m_PhaseFrames = 0;
    float m_InitialX = 0.0f;
    float m_PushStartX = 0.0f;
    float m_PushedX = 0.0f;
    float m_PulledX = 0.0f;
    float m_ReleasedX = 0.0f;
    bool m_PhysicalizeSeen = false;
    bool m_UnphysicalizeSeen = false;
    bool m_Physicalized = false;
    bool m_ForceSet = false;
    bool m_MenuReady = false;
    bool m_LevelMenuOpened = false;
    bool m_LevelChosen = false;
    bool m_LevelStarted = false;
    bool m_ControlReady = false;
    bool m_Passed = false;
    bool m_Done = false;
    std::chrono::steady_clock::time_point m_MenuStartedAt{};
    std::chrono::steady_clock::time_point m_LevelStartedAt{};
    std::chrono::steady_clock::time_point m_ControlReadyAt{};
    std::chrono::steady_clock::time_point m_PhaseStartedAt{};
};

} // namespace

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new ExecuteBBTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
