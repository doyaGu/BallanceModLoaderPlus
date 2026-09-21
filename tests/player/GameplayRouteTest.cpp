#include <BML/Behavior.hpp>
#include <BML/IBML.h>
#include <BML/ILogger.h>
#include <BML/IMod.h>
#include <BML/InputHook.h>

#include "GameplayInputDriver.h"
#include "GameplayPilot.h"
#include "PlayerBallLocator.h"
#include "PlayerProbe.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

struct LocatedGraph {
    BML::Behavior::Graph Graph;
    CKBehavior *Native = nullptr;

    [[nodiscard]] explicit operator bool() const noexcept {
        return Native != nullptr;
    }
};

CKBehavior *NativeChild(CKBehavior *parent,
                        const BML::Behavior::Node &node) {
    const int index = node.Index();
    return parent && index >= 0 && index < parent->GetSubBehaviorCount()
        ? parent->GetSubBehavior(index) : nullptr;
}

LocatedGraph FindGraph(BML::Behavior::Session &session, CKBehavior *root,
                       std::string_view name) {
    auto inspected = session.Inspect(root);
    if (!inspected)
        return {};
    BML::Behavior::Graph graph = inspected.Take();
    for (BML::Behavior::Node node : graph.Nodes()) {
        CKBehavior *native = NativeChild(root, node);
        if (!native)
            continue;
        if (node.IsGraph()) {
            LocatedGraph nested = FindGraph(session, native, name);
            if (nested)
                return nested;
        }
        if (node.Name() == name && node.IsGraph()) {
            auto child = graph.Inspect(node);
            if (child)
                return {child.Take(), native};
        }
    }
    return {};
}

int PortCount(const BML::Behavior::Node &node,
              BML::Behavior::SlotKind kind) {
    int count = 0;
    for (BML::Behavior::Port port : node.Ports()) {
        if (port.Kind() == kind)
            ++count;
    }
    return count;
}

std::vector<BML::PlayerTest::GameplayWaypoint> LevelOneSectorOneRoute() {
    using BML::PlayerTest::GameplayWaypoint;
    // Extracted from Level_01.NMO. These are goals along the authored floor and
    // rail route, not positions assigned to the ball by the test.
    return {
        // A01_Floor_01_02 is an S-shaped corridor enclosed by A01_Rail_00.
        // The first goal lands the opening jump; the following goals trace
        // the centre of the actual floor instead of treating the rail as a
        // gate.
        GameplayWaypoint{"opening-landing", VxVector(23.0f, 14.0f, -153.0f), 3.0f},
        GameplayWaypoint{"upper-right-turn", VxVector(22.0f, 14.0f, -138.0f), 2.5f},
        GameplayWaypoint{"upper-left-turn", VxVector(7.0f, 14.0f, -138.0f), 2.5f},
        GameplayWaypoint{"middle-down", VxVector(7.0f, 16.0f, -158.0f), 2.5f},
        GameplayWaypoint{"lower-right-turn", VxVector(17.0f, 18.0f, -168.0f), 2.5f},
        GameplayWaypoint{"bridge-entry", VxVector(17.0f, 18.0f, -183.0f), 2.5f},
        // A01_Modul42 is the authored bridge spanning x=[-49, 7].
        GameplayWaypoint{"bridge-centre", VxVector(-21.0f, 19.0f, -183.0f), 4.0f},
        GameplayWaypoint{"bridge-exit", VxVector(-55.0f, 19.0f, -183.0f), 4.0f},
        GameplayWaypoint{"first-rail", VxVector(-82.0f, 22.0f, -168.0f), 4.0f},
        GameplayWaypoint{"first-rail-end", VxVector(-122.0f, 23.0f, -153.0f), 4.0f},
        GameplayWaypoint{"first-platform", VxVector(-151.0f, 20.0f, -145.0f), 5.0f},
        GameplayWaypoint{"extra-life", VxVector(-171.0f, 20.0f, -138.0f), 4.0f},
        GameplayWaypoint{"south-turn", VxVector(-174.0f, 18.0f, -177.0f), 4.0f},
        GameplayWaypoint{"outer-turn", VxVector(-202.0f, 15.0f, -215.0f), 5.0f},
        GameplayWaypoint{"descending-floor", VxVector(-181.0f, 17.0f, -250.0f), 5.0f},
        GameplayWaypoint{"lower-turn", VxVector(-171.0f, 18.0f, -281.0f), 5.0f},
        GameplayWaypoint{"lower-exit", VxVector(-164.0f, 17.0f, -325.0f), 5.0f},
        GameplayWaypoint{"checkpoint-approach", VxVector(-154.0f, 18.0f, -352.0f), 4.0f},
        GameplayWaypoint{"point-06", VxVector(-85.23f, 22.28f, -363.77f), 3.0f},
        GameplayWaypoint{"point-07", VxVector(-78.04f, 22.28f, -363.77f), 3.0f},
        GameplayWaypoint{"point-08", VxVector(-70.71f, 22.28f, -363.77f), 3.0f},
        GameplayWaypoint{"final-floor", VxVector(-51.0f, 12.0f, -364.0f), 3.5f},
        GameplayWaypoint{"checkpoint", VxVector(-18.65f, 12.0f, -379.10f), 4.0f},
    };
}

// Drives Level_01 sector 1 through the shipped Ball Navigation graph and checks
// that commanded input really moves the retail ball past the authored
// objectives. The route itself is still under review, so the probe reports
// SKIPPED until kGameplayPilotEnabled is turned on; everything below it stays
// compiled so the route can be resumed without rebuilding the harness.
class GameplayRouteTest final : public IMod {
public:
    explicit GameplayRouteTest(IBML *bml) : IMod(bml) {
        AddDependency("BML");
    }

    const char *GetID() override { return "GameplayRouteTest"; }
    const char *GetVersion() override { return "1.0.0"; }
    const char *GetName() override { return "Gameplay Route Test"; }
    const char *GetAuthor() override { return "BML"; }
    const char *GetDescription() override {
        return "Drives the authored Level_01 route through gameplay input";
    }
    DECLARE_BML_VERSION;

    void OnLoad() override {
        BML::PlayerTest::ProbeReport::Reset();
        auto opened = BML::Behavior::Session::Open(GetID());
        if (opened) {
            m_Behavior = opened.Take();
        } else {
            GetLogger()->Error(
                "Gameplay graph navigation: session=false code=%d",
                opened.Code());
        }
        CKContext *context = m_BML ? m_BML->GetCKContext() : nullptr;
        auto *input = context ? static_cast<CKInputManager *>(
            context->GetManagerByGuid(INPUT_MANAGER_GUID)) : nullptr;
        m_InputReady = m_GameplayInput.Attach(input);
        GetLogger()->Info("Gameplay input source: attached=%s phase=post-preprocess",
                          m_InputReady ? "true" : "false");
    }

    void OnStartLevel() override {
        if (kGameplayPilotEnabled) {
            m_Pilot = std::make_unique<BML::PlayerTest::GameplayPilot>(
                LevelOneSectorOneRoute());
        }
    }

    void OnPreLifeUp() override {
        if (m_PilotStarted)
            GetLogger()->Info("Gameplay objective: extra_life=pre");
    }

    void OnPostLifeUp() override {
        if (!m_PilotStarted)
            return;
        m_ExtraLifeReached = true;
        GetLogger()->Info("Gameplay objective: extra_life=post");
    }

    void OnExtraPoint() override {
        if (!m_PilotStarted)
            return;
        ++m_ExtraPointsReached;
        GetLogger()->Info("Gameplay objective: extra_point=true count=%d",
                          m_ExtraPointsReached);
    }

    void OnPreCheckpointReached() override {
        if (m_PilotStarted)
            GetLogger()->Info("Gameplay objective: checkpoint=pre");
    }

    void OnPostCheckpointReached() override {
        if (!m_PilotStarted)
            return;
        m_CheckpointReached = true;
        GetLogger()->Info("Gameplay objective: checkpoint=post");
    }

    void OnBallOff() override {
        if (!m_PilotStarted || m_RoutePassed)
            return;
        m_BallFell = true;
        GetLogger()->Error(
            "Gameplay pilot: ball_off=true waypoint=%d",
            static_cast<int>(m_LastPilotWaypoint));
    }

    void OnProcess() override {
        using BML::PlayerTest::ProbeReport;
        if (!ProbeReport::Started() || ProbeReport::Reported())
            return;
        ++m_TotalFrames;

        if (!kGameplayPilotEnabled) {
            GetLogger()->Info(
                "Gameplay pilot: skipped=true reason=route-under-review");
            ReportRoute(BML_PLAYER_PROBE_SKIPPED, "route-under-review");
            return;
        }

        if (m_StartedAt == std::chrono::steady_clock::time_point{})
            m_StartedAt = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::steady_clock::now() - m_StartedAt;
        if (elapsed > kProbeTimeout) {
            ReportRoute(BML_PLAYER_PROBE_FAILED, "gameplay-route-timeout");
            return;
        }
        if (m_BallFell) {
            CommandGameplayKeys(BML::PlayerTest::GameplayKeyNone, nullptr, 0,
                                0.0f, 0.0f);
            ReportRoute(BML_PLAYER_PROBE_FAILED, "gameplay-ball-off");
            return;
        }
        if (!m_Pilot) {
            if (elapsed > kDependencyGrace)
                ReportRoute(BML_PLAYER_PROBE_FAILED, "gameplay-route-missing");
            return;
        }

        CK3dEntity *ball = BML::PlayerTest::ResolveRetailBall(m_BML);
        CK3dEntity *camera = ResolveBallDirectionReference();
        if (!ball || !camera || !m_InputReady) {
            if (!m_DependenciesLogged) {
                m_DependenciesLogged = true;
                CKDataArray *currentLevel = m_BML->GetArrayByName("CurrentLevel");
                CKObject *cell = currentLevel && currentLevel->GetRowCount() > 0 &&
                    currentLevel->GetColumnCount() > 1
                    ? currentLevel->GetElementObject(0, 1) : nullptr;
                GetLogger()->Error(
                    "Gameplay pilot dependencies: current_level=%s rows=%d "
                    "columns=%d cell=%s parameter=%s ball=%s direction_ref=%s "
                    "input_source=%s",
                    currentLevel ? "true" : "false",
                    currentLevel ? currentLevel->GetRowCount() : -1,
                    currentLevel ? currentLevel->GetColumnCount() : -1,
                    cell ? "true" : "false",
                    cell && CKIsChildClassOf(cell, CKCID_PARAMETER)
                        ? "true" : "false",
                    ball ? "true" : "false",
                    camera ? "true" : "false",
                    m_InputReady ? "true" : "false");
            }
            if (elapsed > kDependencyGrace)
                ReportRoute(BML_PLAYER_PROBE_FAILED,
                            "gameplay-dependencies-unavailable");
            return;
        }

        VxVector position;
        ball->GetPosition(&position);
        VxVector cameraRight;
        VxVector cameraForward;
        const VxVector localRight(1.0f, 0.0f, 0.0f);
        const VxVector localForward(0.0f, 0.0f, 1.0f);
        camera->TransformVector(&cameraRight, &localRight, nullptr);
        camera->TransformVector(&cameraForward, &localForward, nullptr);
        ObserveGameplayControl(position, cameraRight, cameraForward);
        if (ProbeReport::Reported())
            return;

        const BML::PlayerTest::GameplayPilotSample sample{
            position, VxVector(0.0f, 0.0f, 0.0f), cameraRight, cameraForward,
            std::chrono::steady_clock::now(), false};
        if (!m_PilotStarted) {
            m_Pilot->Reset(sample);
            m_PilotStarted = true;
            m_GameplayInput.SetEnabled(true);
            m_GameplayStart = position;
            GetLogger()->Info(
                "Gameplay pilot: started=true ball=%s start=(%.3f,%.3f,%.3f)",
                ball->GetName() ? ball->GetName() : "<unnamed>",
                position.x, position.y, position.z);
        }

        const BML::PlayerTest::GameplayPilotDecision decision =
            m_Pilot->Step(sample);
        m_LastPilotWaypoint = decision.Waypoint;
        CommandGameplayKeys(decision.Keys, decision.WaypointName,
                            decision.Waypoint, decision.Distance,
                            decision.Speed, decision.Advanced);
        if (decision.Stalled) {
            ReportRoute(BML_PLAYER_PROBE_FAILED, "gameplay-stalled");
            return;
        }
        if ((m_TotalFrames % 30) == 0 || decision.Advanced) {
            GetLogger()->Info(
                "Gameplay pilot sample: waypoint=%d position=(%.3f,%.3f,%.3f) "
                "distance=%.3f speed=%.3f acceleration=%.3f keys=%u",
                static_cast<int>(decision.Waypoint), position.x, position.y,
                position.z, decision.Distance, decision.Speed,
                decision.EffectiveAcceleration, decision.Keys);
        }
        if (!decision.Arrived || !m_CheckpointReached ||
            m_ExtraPointsReached == 0)
            return;

        CommandGameplayKeys(BML::PlayerTest::GameplayKeyNone, nullptr,
                            decision.Waypoint, 0.0f, decision.Speed);
        m_RoutePassed = true;
        GetLogger()->Info(
            "Gameplay pilot: completed=true checkpoint=true "
            "extra_life=%s extra_points=%d",
            m_ExtraLifeReached ? "true" : "false",
            m_ExtraPointsReached);
        const bool passed = m_InputApplied && m_MotionObserved;
        ReportRoute(passed ? BML_PLAYER_PROBE_PASSED : BML_PLAYER_PROBE_FAILED,
                    passed ? "completed" : "gameplay-input-unproven");
    }

    void OnUnload() override { m_GameplayInput.Detach(); }

private:
    // The authored route is still under review, so the pilot does not drive the
    // shipped composition yet. Flipping this on is the only change needed to
    // put the route back in the acceptance run.
    static constexpr bool kGameplayPilotEnabled = false;

    static constexpr auto kDependencyGrace = std::chrono::seconds(2);
    static constexpr auto kProbeTimeout = std::chrono::seconds(240);

    void ReportRoute(std::uint32_t state, const char *reason) {
        GetLogger()->Info(
            "Gameplay route: status=%s reason=%s input_applied=%s motion=%s "
            "route=%s checkpoint=%s extra_life=%s extra_points=%d "
            "waypoint=%d commanded_travel=%.3f",
            state == BML_PLAYER_PROBE_PASSED ? "pass"
                : state == BML_PLAYER_PROBE_SKIPPED ? "skip" : "fail",
            reason,
            m_InputApplied ? "true" : "false",
            m_MotionObserved ? "true" : "false",
            m_RoutePassed ? "true" : "false",
            m_CheckpointReached ? "true" : "false",
            m_ExtraLifeReached ? "true" : "false",
            m_ExtraPointsReached, static_cast<int>(m_LastPilotWaypoint),
            m_CommandedTravel);
        m_GameplayInput.SetMask(BML::PlayerTest::GameplayKeyNone);
        m_GameplayInput.SetEnabled(false);
        BML::PlayerTest::ProbeReport::Report(state, reason);
    }

    void CommandGameplayKeys(std::uint32_t keys, const char *waypointName,
                             std::size_t waypoint, float distance, float speed,
                             bool advanced = false) {
        if (keys == m_GameplayKeys && waypoint == m_CommandWaypoint && !advanced)
            return;
        m_GameplayKeys = keys;
        m_GameplayInput.SetMask(keys);
        m_CommandWaypoint = waypoint;
        ++m_GameplayCommandSequence;
        GetLogger()->Info(
            "Gameplay pilot: command=%u sequence=%u waypoint=%d name=%s "
            "distance=%.3f speed=%.3f",
            keys, m_GameplayCommandSequence, static_cast<int>(waypoint),
            waypointName ? waypointName : "complete", distance, speed);
    }

    // The four SetPhysicsForce blocks inside Ball Navigation all read the same
    // direction reference. Resolving it through them keeps the probe aligned
    // with whatever the shipped graph actually steers by.
    CK3dEntity *ResolveBallDirectionReference() {
        CKBehavior *script = m_BML ?
            m_BML->GetScriptByName("Gameplay_Ingame") : nullptr;
        LocatedGraph navigation = script ? FindGraph(
            m_Behavior, script, "Ball Navigation") : LocatedGraph{};
        if (!navigation)
            return nullptr;

        CKParameter *commonSource = nullptr;
        CK3dEntity *reference = nullptr;
        int forceCount = 0;
        bool mismatch = false;
        for (BML::Behavior::Node node : navigation.Graph.Nodes()) {
            if (node.Name() != "SetPhysicsForce" ||
                PortCount(node, BML::Behavior::SlotKind::In) != 2 ||
                PortCount(node, BML::Behavior::SlotKind::Out) != 2 ||
                PortCount(node, BML::Behavior::SlotKind::Pin) != 5 ||
                PortCount(node, BML::Behavior::SlotKind::Pout) != 0) {
                continue;
            }
            CKBehavior *force = NativeChild(navigation.Native, node);
            if (!force) {
                mismatch = true;
                continue;
            }
            ++forceCount;
            CKParameterIn *input = force->GetInputParameter(3);
            CKParameter *source = input ? input->GetRealSource() : nullptr;
            CK3dEntity *candidate = source ?
                CK3dEntity::Cast(source->GetValueObject()) : nullptr;
            if (!source || !candidate) {
                mismatch = true;
                continue;
            }
            if (!commonSource) {
                commonSource = source;
                reference = candidate;
            } else if (source != commonSource || candidate != reference) {
                mismatch = true;
            }
        }

        if (!m_DirectionReferenceLogged) {
            m_DirectionReferenceLogged = true;
            GetLogger()->Info(
                "Gameplay direction reference: script=%s navigation=%s "
                "forces=%d common_source=%s object=%s",
                script ? "true" : "false", navigation ? "true" : "false",
                forceCount, commonSource && !mismatch ? "true" : "false",
                reference && !mismatch ? "true" : "false");
        }
        return forceCount >= 4 && !mismatch ? reference : nullptr;
    }

    static std::uint32_t ReadGameplayDirectionMask(InputHook *input) {
        if (!input)
            return BML::PlayerTest::GameplayKeyNone;
        std::uint32_t mask = BML::PlayerTest::GameplayKeyNone;
        if (input->oIsKeyDown(CKKEY_LEFT))
            mask |= BML::PlayerTest::GameplayKeyLeft;
        if (input->oIsKeyDown(CKKEY_RIGHT))
            mask |= BML::PlayerTest::GameplayKeyRight;
        if (input->oIsKeyDown(CKKEY_UP))
            mask |= BML::PlayerTest::GameplayKeyUp;
        if (input->oIsKeyDown(CKKEY_DOWN))
            mask |= BML::PlayerTest::GameplayKeyDown;
        return mask;
    }

    // Proves that the keys the pilot commands are the keys the shipped graph
    // reads, and that they move the ball the way the camera basis says they
    // should. A mismatch means the run was not a real gameplay run.
    void ObserveGameplayControl(const VxVector &position,
                                const VxVector &cameraRight,
                                const VxVector &cameraForward) {
        if (!m_GameplayInput.IsEnabled() || m_GameplayInput.Frame() == 0 ||
            m_GameplayInput.Frame() == m_LastObservedInputFrame)
            return;
        m_LastObservedInputFrame = m_GameplayInput.Frame();

        InputHook *input = m_BML ? m_BML->GetInputManager() : nullptr;
        const std::uint32_t applied = m_GameplayInput.AppliedMask();
        const std::uint32_t observed = ReadGameplayDirectionMask(input);
        const std::uint32_t physical = m_GameplayInput.PhysicalMask();
        if (physical != 0) {
            GetLogger()->Error(
                "Gameplay input: external_interference=true frame=%llu mask=%u",
                static_cast<unsigned long long>(m_LastObservedInputFrame),
                physical);
            ReportRoute(BML_PLAYER_PROBE_FAILED, "gameplay-external-input");
            return;
        }
        if (observed != applied) {
            GetLogger()->Error(
                "Gameplay input: mismatch=true frame=%llu applied=%u observed=%u",
                static_cast<unsigned long long>(m_LastObservedInputFrame),
                applied, observed);
            ReportRoute(BML_PLAYER_PROBE_FAILED, "gameplay-input-mismatch");
            return;
        }
        if (applied != 0 && !m_InputApplied) {
            m_InputApplied = true;
            GetLogger()->Info(
                "Gameplay input: applied=true frame=%llu mask=%u observed=%u",
                static_cast<unsigned long long>(m_LastObservedInputFrame),
                applied, observed);
        }

        if (m_HaveControlPosition) {
            VxVector movement = position - m_LastControlPosition;
            movement.y = 0.0f;
            VxVector intended(0.0f, 0.0f, 0.0f);
            if (applied & BML::PlayerTest::GameplayKeyRight)
                intended += cameraRight;
            if (applied & BML::PlayerTest::GameplayKeyLeft)
                intended -= cameraRight;
            if (applied & BML::PlayerTest::GameplayKeyUp)
                intended += cameraForward;
            if (applied & BML::PlayerTest::GameplayKeyDown)
                intended -= cameraForward;
            intended.y = 0.0f;
            const float intendedLength = std::sqrt(
                intended.x * intended.x + intended.z * intended.z);
            if (applied != 0 && intendedLength > 0.0001f) {
                const float progress =
                    (movement.x * intended.x + movement.z * intended.z) /
                    intendedLength;
                if (progress > 0.0f)
                    m_CommandedTravel += progress;
                if (!m_MotionObserved && m_CommandedTravel >= 0.25f) {
                    m_MotionObserved = true;
                    GetLogger()->Info(
                        "Gameplay motion: observed=true frame=%llu "
                        "commanded_travel=%.3f position=(%.3f,%.3f,%.3f)",
                        static_cast<unsigned long long>(m_LastObservedInputFrame),
                        m_CommandedTravel, position.x, position.y,
                        position.z);
                }
            }
        }
        m_LastControlPosition = position;
        m_HaveControlPosition = true;
    }

    std::unique_ptr<BML::PlayerTest::GameplayPilot> m_Pilot;
    BML::Behavior::Session m_Behavior;
    BML::PlayerTest::GameplayInputDriver m_GameplayInput;
    int m_TotalFrames = 0;
    int m_ExtraPointsReached = 0;
    bool m_InputReady = false;
    bool m_PilotStarted = false;
    bool m_RoutePassed = false;
    bool m_ExtraLifeReached = false;
    bool m_CheckpointReached = false;
    bool m_BallFell = false;
    bool m_DependenciesLogged = false;
    bool m_DirectionReferenceLogged = false;
    bool m_InputApplied = false;
    bool m_MotionObserved = false;
    bool m_HaveControlPosition = false;
    std::uint32_t m_GameplayKeys = BML::PlayerTest::GameplayKeyNone;
    std::uint32_t m_GameplayCommandSequence = 0;
    std::uint64_t m_LastObservedInputFrame = 0;
    std::size_t m_CommandWaypoint = static_cast<std::size_t>(-1);
    std::size_t m_LastPilotWaypoint = 0;
    float m_CommandedTravel = 0.0f;
    VxVector m_GameplayStart;
    VxVector m_LastControlPosition;
    std::chrono::steady_clock::time_point m_StartedAt{};
};

} // namespace

BML_PLAYER_PROBE_EXPORTS()

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) {
    return new GameplayRouteTest(bml);
}

BML_MOD_ENTRY(void) BMLExit(IMod *mod) {
    delete mod;
}
