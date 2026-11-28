#include "Gameplay/GameplayTweaks.h"

#include <cstring>
#include <utility>

#include "BML/Guids/TT_ParticleSystems_RT.h"
#include "Loader/ModContext.h"
#include "Config/Config.h"
#include "Logging/Logger.h"

namespace {
namespace Behavior = BML::Behavior;

Behavior::Edit OverclockIngame() {
    Behavior::Edit edit;
    auto manager = edit.Root().Require("BallManager").Graph();

    auto deactivate = manager.Require("Deactivate Ball").Graph();
    const auto pieces = deactivate.Require("reset Ballpieces");
    const auto afterPieces = deactivate.Next(pieces);
    const auto beforeUnphysicalize = deactivate.Next(afterPieces);
    const auto unphysicalize = deactivate.Next(beforeUnphysicalize);
    deactivate.Redirect(deactivate.Leaving(pieces), unphysicalize.In(1));

    auto newBall = manager.Require("New Ball").Graph();
    const auto physicalize = newBall.Require("physicalize new Ball");
    auto beforePhysicalize = newBall.Previous(physicalize);
    beforePhysicalize = newBall.Previous(beforePhysicalize);
    beforePhysicalize = newBall.Previous(beforePhysicalize);
    newBall.Redirect(newBall.Entering(beforePhysicalize), physicalize.In(0));
    return edit;
}

Behavior::Edit OverclockEnergy() {
    Behavior::Edit edit;
    auto root = edit.Root();
    const auto delay = root.Require("Delayer");
    root.Redirect(root.Entering(delay), root.Leaving(delay));
    return edit;
}
}

void GameplayTweaks::InitConfig(Config &config) {
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

bool GameplayTweaks::OnModifyConfig(const char *category, const char *key, Property *property) {
    if (!property || std::strcmp(category ? category : "", "Tweak") != 0)
        return false;
    if (property == m_LanternAlphaTest &&
        std::strcmp(key ? key : "", "LanternAlphaTest") == 0) {
        ApplyLanternAlphaTest(property->GetBoolean());
        return true;
    }
    if (property == m_Overclock &&
        std::strcmp(key ? key : "", "Overclock") == 0) {
        // The submission outlives each world, so a change made in the menu must
        // reach it too; it applies once the next level's scripts load.
        (void) ApplyOverclock(property->GetBoolean(), true);
        return true;
    }
    if (property == m_FixLifeBall &&
        std::strcmp(key ? key : "", "FixLifeBallFreeze") == 0) {
        const bool enabled = property->GetBoolean();
        if (SetPlanEnabled(m_ExtraLifePlan, enabled,
                           "Life-ball freeze fix", true) && m_Logger) {
            m_Logger->Info(enabled
                ? "Requested the Life-ball freeze fix Plan"
                : "Requested restoration of the Life-ball freeze fix Plan");
        }
        return true;
    }
    return false;
}

void GameplayTweaks::OnLoad(ModContext &bml, Logger &logger) {
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
    m_Behavior = session.Take();

    const bool lanternEnabled = m_LanternAlphaTest &&
        m_LanternAlphaTest->GetBoolean();
    Behavior::Edit lanternEdit;
    auto lanternGraph = lanternEdit.Root()
        .Require("set Mapping and Textures").Graph()
        .Require("Set Mat Laterne").Graph();
    const auto alpha = lanternGraph.Require("Set Alpha Test");
    const auto enabled = lanternGraph.AppendLocal("Lantern Alpha Test", CKPGUID_BOOL);
    lanternGraph.Set(enabled, lanternEnabled);
    lanternGraph.Bind(alpha.Pin(0, CKPGUID_BOOL), enabled);
    auto lantern = m_Behavior.Apply(
        "Lantern alpha test", Behavior::EditOwnership::Submission,
        Behavior::On(Behavior::Scripts::One("Levelinit_build"), lanternEdit));
    if (lantern) {
        m_Lantern = lantern.Take();
        m_LanternInput = enabled;
    } else {
        m_Logger->Warn("Lantern alpha-test edit is unavailable: %s",
            lantern.GetStatus().Message.empty()
                ? "Behavior Apply failed"
                : lantern.GetStatus().Message.c_str());
    }

    Behavior::Edit ingame = OverclockIngame();
    Behavior::Edit energy = OverclockEnergy();
    auto overclock = m_Behavior.Apply(
        "Overclock", Behavior::ApplyOptions{
            .Enabled = m_Overclock && m_Overclock->GetBoolean()},
        Behavior::On(Behavior::Scripts::One("Gameplay_Ingame"),
                     ingame),
        Behavior::On(Behavior::Scripts::One("Gameplay_Energy"),
                     energy));
    if (overclock) {
        m_OverclockSubmission = overclock.Take();
    } else {
        m_Logger->Warn("Overclock submission is unavailable: %s",
            overclock.GetStatus().Message.empty()
                           ? "Behavior Apply failed"
                : overclock.GetStatus().Message.c_str());
    }

    Behavior::Edit lifeBall;
    auto lifeGraph = lifeBall.Root();
    const auto emitter = lifeGraph.Require(
        "SphericalParticleSystem",
        TT_PARTICLESYSTEMS_RT_SPHERICALPARTICLESYSTEM);
    lifeGraph.Bind(
        lifeGraph.AppendPin(emitter, "Real-Time Mode", CKPGUID_BOOL), true);
    lifeGraph.Bind(
        lifeGraph.AppendPin(emitter, "DeltaTime", CKPGUID_FLOAT), 20.0f);
    auto extraLife = m_Behavior.Plan(
        "Life-ball freeze fix",
        Behavior::On(
            Behavior::Scripts::Each("P_Extra_Life_Particle_Blob Script"),
            lifeBall),
        Behavior::On(
            Behavior::Scripts::Each("P_Extra_Life_Particle_Fizz Script"),
            lifeBall));
    if (extraLife) {
        m_ExtraLifePlan = extraLife.Take();
        if (!m_FixLifeBall || !m_FixLifeBall->GetBoolean()) {
            (void) SetPlanEnabled(m_ExtraLifePlan, false,
                                  "Life-ball freeze fix", true);
        }
    } else {
        m_Logger->Warn("Life-ball freeze Plan is unavailable: %s",
            extraLife.GetStatus().Message.empty()
                ? "Behavior Plan creation failed"
                : extraLife.GetStatus().Message.c_str());
    }
}

void GameplayTweaks::OnUnload() {
    ClosePlan(m_ExtraLifePlan, "Life-ball freeze fix");
    auto overclock = m_OverclockSubmission.Close();
    if (!overclock && m_Logger)
        m_Logger->Error("Failed to restore Overclock: %s",
                        overclock.GetStatus().Message.c_str());
    m_OverclockSubmission = {};
    auto lantern = m_Lantern.Close();
    if (!lantern && m_Logger)
        m_Logger->Error("Failed to restore the lantern alpha-test edit: %s",
                        lantern.GetStatus().Message.c_str());
    m_Lantern = {};
    m_LanternInput = {};
    m_Behavior.Reset();
    m_Logger = nullptr;
    m_BML = nullptr;
}

void GameplayTweaks::OnExitGame() {
    // Script rules retain their definitions across the world reset and reconcile
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
    if (m_Lantern) {
        const auto changed = m_Lantern.SetInput(m_LanternInput, enabled);
        if (!changed && m_Logger)
            m_Logger->Warn("Could not update the lantern alpha-test input: %s",
                           changed.GetStatus().Message.c_str());
    }
}

bool GameplayTweaks::ApplyOverclock(bool enabled, bool warnIfUnavailable) {
    if (!m_OverclockSubmission) {
        if (enabled && warnIfUnavailable && m_Logger)
            m_Logger->Warn("Overclock submission is unavailable");
        return !enabled;
    }
    const auto changed = m_OverclockSubmission.SetEnabled(enabled);
    if (!changed) {
        if (warnIfUnavailable && m_Logger)
            m_Logger->Warn("Overclock could not change state: %s",
                           changed.GetStatus().Message.c_str());
        return false;
    }
    if (m_Logger)
        m_Logger->Info(enabled
            ? "Requested the Overclock Behavior edit"
            : "Requested restoration of the Overclock Behavior edit");
    return true;
}

bool GameplayTweaks::SetPlanEnabled(Behavior::Plan &plan, bool enabled,
                                    const char *name,
                                    bool warnIfUnavailable) {
    if (!plan) {
        if (enabled && warnIfUnavailable && m_Logger) {
            m_Logger->Warn("%s Plan is unavailable", name);
        }
        return false;
    }

    auto changed = enabled ? plan.Enable() : plan.Disable();
    if (changed)
        return true;

    if (warnIfUnavailable && m_Logger) {
        m_Logger->Warn("%s Plan could not change state: %s", name,
                       changed.GetStatus().Message.empty()
                           ? "Behavior Plan state change failed"
                           : changed.GetStatus().Message.c_str());
    }
    return false;
}

void GameplayTweaks::ClosePlan(Behavior::Plan &plan, const char *name) {
    if (!plan)
        return;

    auto closed = plan.Close();
    if (!closed && m_Logger) {
        m_Logger->Error("Failed to restore the %s Plan: %s", name,
                        closed.GetStatus().Message.empty()
                            ? "Behavior Plan close failed"
                            : closed.GetStatus().Message.c_str());
    }
}
