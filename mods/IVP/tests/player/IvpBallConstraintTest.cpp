#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/IVP.h>

#include "BallConstraintScenario.h"
#include "BallScenarioProbe.h"
#include "PlayerProbe.h"

#include <chrono>

namespace {

// Probes a retail ballsocket between the world and the ball the shipped
// composition plays with. PlayerFlowDriver owns the flow that gets the level on
// screen; this Mod only runs the scenario once gameplay input is live.
class IvpBallConstraintTest final : public IMod {
public:
    explicit IvpBallConstraintTest(IBML *bml) : IMod(bml) {
        AddDependency(IVP_MOD_ID);
    }

    const char *GetID() override { return "IvpBallConstraintTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "IVP Ball Constraint Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Probes IVP constraints on Ballance's player ball";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override { BML::PlayerTest::ProbeReport::Reset(); }

    void OnProcess() override {
        using BML::PlayerTest::ProbeReport;
        using Step = BML::PlayerTest::BallScenarioProbe<
            BallConstraintScenario>::Step;
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
                Report(false, "ball-constraint-dependencies-unavailable");
            return;
        }
        if (step == Step::Running) {
            if (elapsed > kProbeTimeout)
                Report(false, "ball-constraint-timeout");
            return;
        }

        const BallConstraintResult &result = m_Probe.Result();
        GetLogger()->Info(
            "Ball constraint: status=%s reason=%s ball=%s mapped=%s "
            "created=%s endpoints=%s cores=%s anchored=%s "
            "axes_freed=%s resumed=%s anchored_travel=%.6f "
            "released_travel=%.6f",
            result.Passed ? "pass" : "fail", result.Detail.c_str(),
            result.BallName.c_str(),
            result.MappingPassed ? "true" : "false",
            result.ConstraintCreated ? "true" : "false",
            result.EndpointMappingPassed ? "true" : "false",
            result.CoreMembershipPassed ? "true" : "false",
            result.AnchoredUnderInput ? "true" : "false",
            result.TranslationAxesFreed ? "true" : "false",
            result.MovementResumed ? "true" : "false",
            result.AnchoredTravel, result.ReleasedTravel);
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

    BML::PlayerTest::BallScenarioProbe<BallConstraintScenario> m_Probe;
    bool m_Attached = false;
    std::chrono::steady_clock::time_point m_StartedAt{};
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

MOD_EXPORT IMod *BMLEntry(IBML *bml) {
    return new IvpBallConstraintTest(bml);
}

MOD_EXPORT void BMLExit(IMod *mod) {
    delete mod;
}
