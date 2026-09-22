#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/IVP.h>

#include "BallScenarioProbe.h"
#include "BallSpeedGovernorScenario.h"
#include "PlayerProbe.h"

#include <chrono>

namespace {

// Probes the IVP speed governor on the ball the shipped composition plays with.
// PlayerFlowDriver owns the flow that gets the level on screen; this Mod only
// runs the scenario once the driver says gameplay input is live.
class IvpBallSpeedGovernorTest final : public IMod {
public:
    explicit IvpBallSpeedGovernorTest(IBML *bml) : IMod(bml) {
        AddDependency(IVP_MOD_ID);
    }

    const char *GetID() override { return "IvpBallSpeedGovernorTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "IVP Ball Speed Governor Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Probes the IVP speed governor on Ballance's player ball";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override { BML::PlayerTest::ProbeReport::Reset(); }

    void OnProcess() override {
        using BML::PlayerTest::ProbeReport;
        using Step = BML::PlayerTest::BallScenarioProbe<
            BallSpeedGovernorScenario>::Step;
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
            // The CurrentLevel array is filled by the shipped graph, so give it
            // a moment before calling the level unusable.
            if (elapsed > kDependencyGrace)
                Report(false, "ball-speed-governor-dependencies-unavailable");
            return;
        }
        if (step == Step::Running) {
            if (elapsed > kProbeTimeout)
                Report(false, "ball-speed-governor-timeout");
            return;
        }

        const BallSpeedGovernorResult &result = m_Probe.Result();
        GetLogger()->Info(
            "Ball speed governor: status=%s reason=%s ball=%s mapped=%s "
            "rolling=%s limit_applied=%s limit_respected=%s "
            "angular_coupled=%s controllable=%s released_acceleration=%s "
            "speed_cap=%.6f entry_speed=%.6f max_limited_speed=%.6f "
            "limited_travel=%.6f released_speed=%.6f "
            "released_travel=%.6f",
            result.Passed ? "pass" : "fail", result.Detail.c_str(),
            result.BallName.c_str(),
            result.MappingPassed ? "true" : "false",
            result.RollingStateObserved ? "true" : "false",
            result.LimitApplied ? "true" : "false",
            result.LimitRespected ? "true" : "false",
            result.AngularVelocityCoupled ? "true" : "false",
            result.ControllableUnderLimit ? "true" : "false",
            result.AcceleratedAfterRelease ? "true" : "false",
            result.SpeedCap, result.EntrySpeed,
            result.MaximumLimitedSpeed, result.LimitedTravel,
            result.ReleasedSpeed, result.ReleasedTravel);
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

    BML::PlayerTest::BallScenarioProbe<BallSpeedGovernorScenario> m_Probe;
    bool m_Attached = false;
    std::chrono::steady_clock::time_point m_StartedAt{};
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new IvpBallSpeedGovernorTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
