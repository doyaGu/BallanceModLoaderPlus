#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/IVP.h>

#include "BallScenarioProbe.h"
#include "BallSurfaceRaycastScenario.h"
#include "PlayerProbe.h"

#include <chrono>

namespace {

// Probes the public ray solver against the geometry of the ball the shipped
// composition plays with. PlayerFlowDriver owns the flow that gets the level on
// screen; this Mod only runs the scenario once gameplay input is live.
class IvpBallSurfaceRaycastTest final : public IMod {
public:
    explicit IvpBallSurfaceRaycastTest(IBML *bml) : IMod(bml) {
        AddDependency(IVP_MOD_ID);
    }

    const char *GetID() override { return "IvpBallSurfaceRaycastTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "IVP Ball Surface Raycast Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Probes IVP surface raycasts against Ballance's player ball";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override { BML::PlayerTest::ProbeReport::Reset(); }

    void OnProcess() override {
        using BML::PlayerTest::ProbeReport;
        using Step = BML::PlayerTest::BallScenarioProbe<
            BallSurfaceRaycastScenario>::Step;
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
                Report(false, "ball-surface-raycast-dependencies-unavailable");
            return;
        }
        if (step == Step::Running) {
            if (elapsed > kProbeTimeout)
                Report(false, "ball-surface-raycast-timeout");
            return;
        }

        const BallSurfaceRaycastResult &result = m_Probe.Result();
        GetLogger()->Info(
            "Ball surface raycast: status=%s reason=%s ball=%s mapped=%s "
            "retail_ball=%s first_hit=%s second_hit=%s moving_surface=%s "
            "static_surface=%s direct_ledge=%s "
            "radius=%.6f first_distance=%.6f second_distance=%.6f "
            "center_travel=%.6f static_distance=%.6f direct_distance=%.6f",
            result.Passed ? "pass" : "fail", result.Detail.c_str(),
            result.BallName.c_str(),
            result.MappingPassed ? "true" : "false",
            result.RetailBallObserved ? "true" : "false",
            result.FirstHitPassed ? "true" : "false",
            result.SecondHitPassed ? "true" : "false",
            result.MovingSurfaceTracked ? "true" : "false",
            result.StaticSurfacePassed ? "true" : "false",
            result.DirectLedgePassed ? "true" : "false",
            result.BallRadius, result.FirstHitDistance,
            result.SecondHitDistance, result.CenterTravel,
            result.StaticSurfaceDistance, result.DirectLedgeDistance);
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

    BML::PlayerTest::BallScenarioProbe<BallSurfaceRaycastScenario> m_Probe;
    bool m_Attached = false;
    std::chrono::steady_clock::time_point m_StartedAt{};
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new IvpBallSurfaceRaycastTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
