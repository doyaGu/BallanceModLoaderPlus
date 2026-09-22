#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/IVP.h>

#include "BallScenarioProbe.h"
#include "BallStateRoundTripScenario.h"
#include "PlayerProbe.h"

#include <chrono>

namespace {

// Probes capturing and restoring the IVP state of the ball the shipped
// composition plays with. PlayerFlowDriver owns the flow that gets the level on
// screen; this Mod only runs the scenario once gameplay input is live.
class IvpBallStateRoundTripTest final : public IMod {
public:
    explicit IvpBallStateRoundTripTest(IBML *bml) : IMod(bml) {
        AddDependency(IVP_MOD_ID);
    }

    const char *GetID() override { return "IvpBallStateRoundTripTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "IVP Ball State Round Trip Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Probes IVP state capture and restore on Ballance's player ball";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override { BML::PlayerTest::ProbeReport::Reset(); }

    void OnProcess() override {
        using BML::PlayerTest::ProbeReport;
        using Step = BML::PlayerTest::BallScenarioProbe<
            BallStateRoundTripScenario>::Step;
        if (!ProbeReport::Started() || ProbeReport::Reported())
            return;
        if (!m_Attached) {
            m_Attached = true;
            m_StartedAt = std::chrono::steady_clock::now();
            if (!m_Probe.Attach(m_BML)) {
                Report(false, "input-source-unavailable");
                return;
            }
        }

        const auto elapsed = std::chrono::steady_clock::now() - m_StartedAt;
        const Step step = m_Probe.Advance(m_BML);
        if (step == Step::Failed) {
            if (elapsed > kDependencyGrace)
                Report(false, "ball-state-round-trip-dependencies-unavailable");
            return;
        }
        if (step == Step::Running) {
            if (elapsed > kProbeTimeout)
                Report(false, "ball-state-round-trip-timeout");
            return;
        }

        const BallStateRoundTripResult &result = m_Probe.Result();
        GetLogger()->Info(
            "Ball state round-trip: status=%s reason=%s ball=%s mapped=%s "
            "moving_capture=%s divergence=%s position=%s rotation=%s "
            "linear_velocity=%s angular_velocity=%s "
            "simulation_continued=%s capture_speed=%.6f "
            "diverged_distance=%.6f position_error=%.6f "
            "rotation_error=%.6f linear_velocity_error=%.6f "
            "angular_velocity_error=%.6f continued_distance=%.6f",
            result.Passed ? "pass" : "fail", result.Detail.c_str(),
            result.BallName.c_str(),
            result.MappingPassed ? "true" : "false",
            result.MovingStateCaptured ? "true" : "false",
            result.DivergenceObserved ? "true" : "false",
            result.PositionRestored ? "true" : "false",
            result.RotationRestored ? "true" : "false",
            result.LinearVelocityRestored ? "true" : "false",
            result.AngularVelocityRestored ? "true" : "false",
            result.SimulationContinued ? "true" : "false",
            result.CapturedSpeed, result.DivergedDistance,
            result.PositionError, result.RotationError,
            result.LinearVelocityError, result.AngularVelocityError,
            result.ContinuedDistance);
        Report(result.Passed, result.Detail.c_str());
    }

    void OnUnload() override { m_Probe.Detach(); }

private:
    static constexpr auto kDependencyGrace = std::chrono::seconds(2);
    static constexpr auto kProbeTimeout = std::chrono::seconds(60);

    void Report(bool passed, const char *detail) {
        m_Probe.Detach();
        if (passed)
            BML::PlayerTest::ProbeReport::Pass(detail);
        else
            BML::PlayerTest::ProbeReport::Fail(detail);
    }

    BML::PlayerTest::BallScenarioProbe<BallStateRoundTripScenario> m_Probe;
    bool m_Attached = false;
    std::chrono::steady_clock::time_point m_StartedAt{};
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new IvpBallStateRoundTripTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
