#include <BML/ExecuteBB.h>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>

#include "PlayerProbe.h"

#include <chrono>
#include <cmath>

namespace {

// Probes the ExecuteBB physics operations on a body the test owns. The Player
// flow around it belongs to PlayerFlowDriver, so this Mod only runs once the
// driver reports a settled level and only touches its own object.
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
        return "Probes the ExecuteBB physics operations in Ballance Player";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override { BML::PlayerTest::ProbeReport::Reset(); }

    void OnProcess() override {
        using BML::PlayerTest::ProbeReport;
        if (!ProbeReport::Started() || ProbeReport::Reported())
            return;
        if (m_Phase == Phase::Idle) {
            m_StartedAt = std::chrono::steady_clock::now();
            SetPhase(Phase::Create);
        }
        if (std::chrono::steady_clock::now() - m_StartedAt > kProbeTimeout) {
            Report(false, "probe-timeout");
            return;
        }

        switch (m_Phase) {
        case Phase::Create:
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
        default:
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

    void OnUnload() override { DestroyBody(); }

private:
    enum class Phase {
        Idle,
        Create,
        Body,
        Push,
        Pull,
        Released,
        Done,
    };

    static constexpr float kForceMagnitude = 1000.0f;
    static constexpr float kMinimumTravel = 0.01f;
    static constexpr float kReleasedX = 321.0f;
    static constexpr float kReleaseTolerance = 0.05f;
    static constexpr auto kProbeTimeout = std::chrono::seconds(30);
    static constexpr auto kPhysicalizationSettleTime = std::chrono::milliseconds(500);
    static constexpr auto kForceObservationTimeout = std::chrono::seconds(5);
    static constexpr auto kReleaseObservationTime = std::chrono::milliseconds(250);

    void CreateBody() {
        CKContext *context = m_BML->GetCKContext();
        CKLevel *level = context ? context->GetCurrentLevel() : nullptr;
        CKScene *scene = context ? context->GetCurrentScene() : nullptr;
        if (!context || !level || !scene)
            return;

        auto *body = CK3dObject::Cast(context->CreateObject(
            CKCID_3DOBJECT, const_cast<char *>("__BML_ExecuteBB_Test"),
            static_cast<CK_OBJECTCREATION_OPTIONS>(CK_OBJECTCREATION_DYNAMIC |
                                                    CK_OBJECTCREATION_ACTIVATE)));
        if (!body) {
            Report(false, "body-create-failed");
            return;
        }

        m_Body = body;
        if (level->AddObject(m_Body) != CK_OK) {
            Report(false, "body-add-failed");
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
            Report(false, "invalid-body-position");
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
        if (!BodyPositionIsFinite()) {
            Report(false, "invalid-positive-force-position");
            return;
        }

        m_PushedX = ReadX();
        if (m_PushedX - m_PushStartX <= kMinimumTravel) {
            if (PhaseDone(kForceObservationTimeout))
                Report(false, "positive-force-no-motion");
            return;
        }
        ExecuteBB::SetPhysicsForce(m_Body, VxVector(), nullptr,
                                   VxVector(-1.0f, 0.0f, 0.0f), nullptr,
                                   kForceMagnitude);
        SetPhase(Phase::Pull);
    }

    void Release() {
        if (!BodyPositionIsFinite()) {
            Report(false, "invalid-reverse-force-position");
            return;
        }

        m_PulledX = ReadX();
        if (m_PushedX - m_PulledX <= kMinimumTravel) {
            if (PhaseDone(kForceObservationTimeout))
                Report(false, "reverse-force-no-motion");
            return;
        }
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
        Report(passed, passed ? "completed" : "result-mismatch");
    }

    void Report(bool passed, const char *reason) {
        SetPhase(Phase::Done);
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
        GetLogger()->Info(
            "ExecuteBB probe: status=%s reason=%s x0=%.6f push_start=%.6f "
            "pushed=%.6f pulled=%.6f released=%.6f physicalize_event=%s "
            "unphysicalize_event=%s",
            passed ? "pass" : "fail", reason, m_InitialX, m_PushStartX,
            m_PushedX, m_PulledX, m_ReleasedX,
            m_PhysicalizeSeen ? "true" : "false",
            m_UnphysicalizeSeen ? "true" : "false");
        DestroyBody();
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(reason);
        else
            BML::PlayerTest::ProbeReport::Fail(reason);
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
        m_PhaseStartedAt = std::chrono::steady_clock::now();
    }

    [[nodiscard]] bool PhaseDone(
        std::chrono::steady_clock::duration duration) const {
        return std::chrono::steady_clock::now() - m_PhaseStartedAt >= duration;
    }

    float ReadX() const {
        VxVector position;
        m_Body->GetPosition(&position);
        return position.x;
    }

    [[nodiscard]] bool BodyPositionIsFinite() const {
        if (!m_Body)
            return false;
        VxVector position;
        m_Body->GetPosition(&position);
        return std::isfinite(position.x) && std::isfinite(position.y) &&
               std::isfinite(position.z);
    }

    Phase m_Phase = Phase::Idle;
    CK3dObject *m_Body = nullptr;
    float m_InitialX = 0.0f;
    float m_PushStartX = 0.0f;
    float m_PushedX = 0.0f;
    float m_PulledX = 0.0f;
    float m_ReleasedX = 0.0f;
    bool m_PhysicalizeSeen = false;
    bool m_UnphysicalizeSeen = false;
    bool m_Physicalized = false;
    bool m_ForceSet = false;
    std::chrono::steady_clock::time_point m_StartedAt{};
    std::chrono::steady_clock::time_point m_PhaseStartedAt{};
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new ExecuteBBTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
