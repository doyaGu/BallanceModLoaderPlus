#include "Gameplay/GameplayTweaks.h"

#include <cstring>
#include <utility>

#include "BML/Guids/TT_ParticleSystems_RT.h"
#include "BML/IBML.h"
#include "BML/IConfig.h"
#include "BML/ILogger.h"
#include "Gameplay/BehaviorGraph.h"

namespace {
namespace Behavior = BML::Behavior;
using namespace Gameplay::Graph;
}

void GameplayTweaks::InitConfig(IConfig &config) {
    m_LanternAlphaTest = config.GetProperty("Tweak", "LanternAlphaTest");
    m_FixLifeBall = config.GetProperty("Tweak", "FixLifeBallFreeze");
    m_Overclock = config.GetProperty("Tweak", "Overclock");

    config.SetCategoryComment("Tweak", "Tweak Settings");
    m_LanternAlphaTest->SetComment(
        "Enable alpha test for lantern material, this option can increase FPS");
    m_LanternAlphaTest->SetDefaultBoolean(true);
    m_FixLifeBall->SetComment(
        "Game won't freeze when picking up life balls");
    m_FixLifeBall->SetDefaultBoolean(true);
    m_Overclock->SetComment("Remove delay of spawn / respawn");
    m_Overclock->SetDefaultBoolean(false);
}

bool GameplayTweaks::OnModifyConfig(const char *category, const char *key,
                                    IProperty *property) {
    if (!property || std::strcmp(category ? category : "", "Tweak") != 0)
        return false;
    if (property == m_LanternAlphaTest &&
        std::strcmp(key ? key : "", "LanternAlphaTest") == 0) {
        ApplyLanternAlphaTest(property->GetBoolean());
        return true;
    }
    if (property == m_Overclock &&
        std::strcmp(key ? key : "", "Overclock") == 0) {
        if (m_BML && m_BML->IsIngame())
            (void) ApplyOverclock(property->GetBoolean(), true);
        return true;
    }
    if (property == m_FixLifeBall &&
        std::strcmp(key ? key : "", "FixLifeBallFreeze") == 0) {
        if (m_ExtraLifePlan) {
            if (property->GetBoolean())
                (void) m_ExtraLifePlan.Enable();
            else
                (void) m_ExtraLifePlan.Disable();
        }
        return true;
    }
    return false;
}

void GameplayTweaks::OnLoad(IBML &bml, ILogger &logger) {
    m_BML = &bml;
    m_Logger = &logger;

    auto session = Behavior::Session::Open("BML");
    if (!session) {
        m_Logger->Warn("Behavior authoring is unavailable: %s",
                       session.GetStatus().Message.empty()
                           ? "could not open the BML session"
                           : session.GetStatus().Message.c_str());
        return;
    }
    m_Behavior = std::move(session).Value();

    auto lantern = m_Behavior.Plan(
        "Lantern alpha test",
        Behavior::On(Behavior::Scripts::One("Levelinit_build"),
                     m_LanternEdits[0]));
    if (lantern)
        m_LanternPlan = std::move(lantern).Value();
    else
        m_Logger->Warn("Lantern alpha-test Plan is unavailable: %s",
            lantern.GetStatus().Message.empty()
                ? "Behavior Plan creation failed"
                : lantern.GetStatus().Message.c_str());

    auto overclock = m_Behavior.Plan(
        "Overclock",
        Behavior::On(Behavior::Scripts::One("Gameplay_Ingame"),
                     m_OverclockEdits[0]),
        Behavior::On(Behavior::Scripts::One("Gameplay_Energy"),
                     m_OverclockEdits[1]));
    if (overclock) {
        m_OverclockPlan = std::move(overclock).Value();
        if (!m_Overclock || !m_Overclock->GetBoolean())
            (void) m_OverclockPlan.Disable();
    } else {
        m_Logger->Warn("Overclock Plan is unavailable: %s",
            overclock.GetStatus().Message.empty()
                ? "Behavior Plan creation failed"
                : overclock.GetStatus().Message.c_str());
    }

    Behavior::Edit lifeBall;
    const auto emitter = lifeBall.Root().Require(
        "SphericalParticleSystem",
        TT_PARTICLESYSTEMS_RT_SPHERICALPARTICLESYSTEM);
    lifeBall.Bind(
        lifeBall.AppendPin(emitter, "Real-Time Mode", CKPGUID_BOOL), true);
    lifeBall.Bind(
        lifeBall.AppendPin(emitter, "DeltaTime", CKPGUID_FLOAT), 20.0f);
    auto extraLife = m_Behavior.Plan(
        "Life-ball freeze fix",
        Behavior::On(
            Behavior::Scripts::Each("P_Extra_Life_Particle_Blob Script"),
            lifeBall),
        Behavior::On(
            Behavior::Scripts::Each("P_Extra_Life_Particle_Fizz Script"),
            lifeBall));
    if (extraLife) {
        m_ExtraLifePlan = std::move(extraLife).Value();
        if (!m_FixLifeBall || !m_FixLifeBall->GetBoolean())
            (void) m_ExtraLifePlan.Disable();
    } else {
        m_Logger->Warn("Life-ball freeze Plan is unavailable: %s",
            extraLife.GetStatus().Message.empty()
                ? "Behavior Plan creation failed"
                : extraLife.GetStatus().Message.c_str());
    }
}

void GameplayTweaks::OnUnload() {
    (void) m_ExtraLifePlan.Close();
    const bool overclockOwned = static_cast<bool>(m_OverclockPlan);
    (void) m_OverclockPlan.Close();
    if (overclockOwned && m_Logger)
        m_Logger->Info("Restore the Overclock Behavior Plan");
    (void) m_LanternPlan.Close();
    m_Behavior.Close();
    m_Logger = nullptr;
    m_BML = nullptr;
}

void GameplayTweaks::OnLoadScript(CKBehavior *script) {
    if (!script || !script->GetName() || !m_Behavior)
        return;
    const char *name = script->GetName();
    if (std::strcmp(name, "Gameplay_Ingame") != 0 &&
        std::strcmp(name, "Gameplay_Energy") != 0 &&
        std::strcmp(name, "Levelinit_build") != 0)
        return;

    auto inspected = m_Behavior.Inspect(script);
    if (!inspected) {
        if (m_Logger)
            m_Logger->Warn("Gameplay graph %s could not be inspected: %s",
                name, inspected.GetStatus().Message.empty()
                    ? "Behavior inspection failed"
                    : inspected.GetStatus().Message.c_str());
        return;
    }
    if (std::strcmp(name, "Gameplay_Ingame") == 0)
        DiscoverOverclockPatch(*inspected);
    else if (std::strcmp(name, "Gameplay_Energy") == 0)
        CompleteOverclockPatch(*inspected);
    else
        PatchLanternAlphaTest(*inspected);
}

void GameplayTweaks::OnExitGame() {
    // Plans retain their definitions across the world reset and reconcile
    // against the next matching scripts. No per-world Patch collection exists.
}

void GameplayTweaks::ApplyLanternAlphaTest(bool enabled) {
    CKMaterial *material = m_BML
        ? m_BML->GetMaterialByName("Laterne_Verlauf") : nullptr;
    if (material) {
        material->EnableAlphaTest(enabled ? TRUE : FALSE);
        material->SetAlphaFunc(VXCMP_GREATEREQUAL);
        material->SetAlphaRef(0);
    }
    if (m_LanternReady)
        (void) ApplyLanternScript(enabled, true);
}

void GameplayTweaks::PatchLanternAlphaTest(
    const Behavior::Graph &script) {
    const Behavior::Node mapping = Find(script, "set Mapping and Textures");
    auto mappingGraph = mapping ? script.Inspect(mapping)
                                : Behavior::Result<Behavior::Graph>::Failure(BML_ERROR_NOT_FOUND);
    const Behavior::Node lantern = mappingGraph
        ? Find(*mappingGraph, "Set Mat Laterne") : Behavior::Node{};
    auto lanternGraph = lantern ? mappingGraph->Inspect(lantern)
                                : Behavior::Result<Behavior::Graph>::Failure(BML_ERROR_NOT_FOUND);
    const Behavior::Node alpha = lanternGraph
        ? Find(*lanternGraph, "Set Alpha Test") : Behavior::Node{};
    if (!m_LanternPlan || !mappingGraph || !lanternGraph || !alpha ||
        !alpha.Pin(0)) {
        if (m_Logger)
            m_Logger->Warn(
                "Lantern alpha-test is unavailable in Levelinit_build");
        return;
    }

    const auto makeEdit = [&](bool enabled) {
        Behavior::Edit edit;
        auto mappingBody = edit.Root().Require(mapping).Graph();
        auto lanternBody = mappingBody.Require(lantern).Graph();
        lanternBody.Bind(lanternBody.Require(alpha).Pin(0, CKPGUID_BOOL),
                         enabled);
        return edit;
    };
    m_LanternEdits[0] = makeEdit(false);
    m_LanternEdits[1] = makeEdit(true);
    m_LanternReady = true;
    (void) ApplyLanternScript(
        m_LanternAlphaTest && m_LanternAlphaTest->GetBoolean(), true);
}

bool GameplayTweaks::ApplyLanternScript(bool enabled,
                                        bool warnIfUnavailable) {
    if (!m_LanternPlan || !m_LanternReady)
        return false;
    auto replaced = m_LanternPlan.Replace(Behavior::On(
        Behavior::Scripts::One("Levelinit_build"),
        m_LanternEdits[enabled ? 1 : 0]));
    if (!replaced) {
        if (warnIfUnavailable && m_Logger)
            m_Logger->Warn("Lantern alpha-test graph could not be edited: %s",
                replaced.GetStatus().Message.empty()
                    ? "Behavior Plan replacement failed"
                    : replaced.GetStatus().Message.c_str());
        return false;
    }
    if (m_Logger)
        m_Logger->Info(
            "Configure the lantern alpha-test through a Behavior Plan");
    return true;
}

void GameplayTweaks::DiscoverOverclockPatch(
    const Behavior::Graph &script) {
    const Behavior::Node ballManager = Find(script, "BallManager");
    auto managerGraph = ballManager ? script.Inspect(ballManager)
                                    : Behavior::Result<Behavior::Graph>::Failure(BML_ERROR_NOT_FOUND);
    const Behavior::Node deactivate = managerGraph
        ? Find(*managerGraph, "Deactivate Ball") : Behavior::Node{};
    const Behavior::Node newBall = managerGraph
        ? Find(*managerGraph, "New Ball") : Behavior::Node{};
    auto deactivateGraph = deactivate ? managerGraph->Inspect(deactivate)
                                      : Behavior::Result<Behavior::Graph>::Failure(BML_ERROR_NOT_FOUND);
    auto newBallGraph = newBall ? managerGraph->Inspect(newBall)
                                : Behavior::Result<Behavior::Graph>::Failure(BML_ERROR_NOT_FOUND);

    const Behavior::Node pieces = deactivateGraph
        ? Find(*deactivateGraph, "reset Ballpieces") : Behavior::Node{};
    const Behavior::Link deactivateLink = deactivateGraph
        ? Leaving(*deactivateGraph, pieces) : Behavior::Link{};
    const Behavior::Node afterPieces = deactivateGraph
        ? Sink(*deactivateGraph, deactivateLink) : Behavior::Node{};
    const Behavior::Node beforeUnphysicalize = deactivateGraph
        ? Next(*deactivateGraph, afterPieces) : Behavior::Node{};
    const Behavior::Node unphysicalize = deactivateGraph
        ? Next(*deactivateGraph, beforeUnphysicalize) : Behavior::Node{};

    const Behavior::Node physicalize = newBallGraph
        ? Find(*newBallGraph, "physicalize new Ball") : Behavior::Node{};
    Behavior::Node previous = physicalize;
    if (newBallGraph) {
        previous = Previous(*newBallGraph, previous);
        previous = Previous(*newBallGraph, previous);
        previous = Previous(*newBallGraph, previous);
    }
    const Behavior::Link newBallLink = newBallGraph
        ? Entering(*newBallGraph, previous) : Behavior::Link{};

    if (!managerGraph || !deactivateGraph || !newBallGraph ||
        !deactivateLink || !unphysicalize.In(1) || !newBallLink ||
        !physicalize.In(0)) {
        RejectOverclockPatch(
            "Gameplay_Ingame does not match the expected vanilla script graph");
        return;
    }

    Behavior::Edit edit;
    auto manager = edit.Root().Require(ballManager).Graph();
    auto deactivateBody = manager.Require(deactivate).Graph();
    deactivateBody.Redirect(
        deactivateBody.Require(deactivateLink),
        deactivateBody.Require(unphysicalize).In(1));
    auto newBallBody = manager.Require(newBall).Graph();
    newBallBody.Redirect(
        newBallBody.Require(newBallLink),
        newBallBody.Require(physicalize).In(0));
    m_OverclockEdits[0] = std::move(edit);
    m_OverclockIngameReady = true;
    (void) ReplaceOverclockPlan(m_Overclock && m_Overclock->GetBoolean());
}

void GameplayTweaks::CompleteOverclockPatch(
    const Behavior::Graph &script) {
    const Behavior::Node delay = Find(script, "Delayer");
    const Behavior::Link afterDelay = Leaving(script, delay);
    const Behavior::Node sink = Sink(script, afterDelay);
    const Behavior::Link beforeDelay = Entering(script, delay);
    const int input = afterDelay ? afterDelay.Target().Index() : -1;
    if (!delay || !afterDelay || !sink || !beforeDelay || input < 0 ||
        !sink.In(input)) {
        RejectOverclockPatch(
            "Gameplay_Energy does not match the expected vanilla script graph");
        return;
    }

    Behavior::Edit edit;
    auto root = edit.Root();
    root.Redirect(root.Require(beforeDelay), root.Require(sink).In(input));
    m_OverclockEdits[1] = std::move(edit);
    m_OverclockEnergyReady = true;
    (void) ReplaceOverclockPlan(m_Overclock && m_Overclock->GetBoolean());
}

bool GameplayTweaks::ReplaceOverclockPlan(bool warnIfUnavailable) {
    if (!m_OverclockPlan || !m_OverclockIngameReady ||
        !m_OverclockEnergyReady)
        return false;
    auto replaced = m_OverclockPlan.Replace(
        Behavior::On(Behavior::Scripts::One("Gameplay_Ingame"),
                     m_OverclockEdits[0]),
        Behavior::On(Behavior::Scripts::One("Gameplay_Energy"),
                     m_OverclockEdits[1]));
    if (!replaced) {
        if (warnIfUnavailable && m_Logger)
            m_Logger->Warn("Overclock could not edit the gameplay graphs: %s",
                replaced.GetStatus().Message.empty()
                    ? "Behavior Plan replacement failed"
                    : replaced.GetStatus().Message.c_str());
        return false;
    }
    return ApplyOverclock(m_Overclock && m_Overclock->GetBoolean(),
                          warnIfUnavailable);
}

bool GameplayTweaks::ApplyOverclock(bool enabled, bool warnIfUnavailable) {
    if (!m_OverclockPlan ||
        (enabled && (!m_OverclockIngameReady || !m_OverclockEnergyReady))) {
        if (enabled && warnIfUnavailable && m_Logger)
            m_Logger->Warn(
                "Overclock is unavailable for the current gameplay scripts");
        return !enabled;
    }
    auto changed = enabled ? m_OverclockPlan.Enable()
                           : m_OverclockPlan.Disable();
    if (!changed) {
        if (warnIfUnavailable && m_Logger)
            m_Logger->Warn("Overclock Plan could not change state: %s",
                changed.GetStatus().Message.empty()
                    ? "Behavior Plan state change failed"
                    : changed.GetStatus().Message.c_str());
        return false;
    }
    if (m_Logger)
        m_Logger->Info(enabled
            ? "Enable Overclock through one Behavior Plan"
            : "Restore the Overclock Behavior Plan");
    return true;
}

void GameplayTweaks::RejectOverclockPatch(const char *reason) {
    m_OverclockIngameReady = false;
    m_OverclockEnergyReady = false;
    if (m_OverclockPlan)
        (void) m_OverclockPlan.Disable();
    if (m_Logger) {
        m_Logger->Error("Overclock script Plan is unavailable: %s",
                        reason ? reason : "unknown reason");
    }
}
