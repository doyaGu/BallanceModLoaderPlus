#include "Gameplay/GameplayTweaks.h"

#include <cstring>
#include <utility>

#include "BML/Guids/TT_ParticleSystems_RT.h"
#include "BML/IBML.h"
#include "BML/IConfig.h"
#include "BML/ILogger.h"

namespace {
namespace Behavior = BML::Behavior;

Behavior::Edit LanternAlphaTest(bool enabled) {
    Behavior::Edit edit;
    auto mapping = edit.Root()
        .Require("set Mapping and Textures")
        .Graph();
    auto lantern = mapping.Require("Set Mat Laterne").Graph();
    const auto alpha = lantern.Require("Set Alpha Test");
    lantern.Bind(alpha.Pin(0, CKPGUID_BOOL), enabled);
    return edit;
}

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
    m_Behavior = session.Take();

    const bool lanternEnabled = m_LanternAlphaTest &&
        m_LanternAlphaTest->GetBoolean();
    Behavior::Edit lanternEdit = LanternAlphaTest(lanternEnabled);
    auto lantern = m_Behavior.Plan(
        "Lantern alpha test",
        Behavior::On(Behavior::Scripts::One("Levelinit_build"),
                     lanternEdit));
    if (lantern)
        m_LanternPlan = lantern.Take();
    else
        m_Logger->Warn("Lantern alpha-test Plan is unavailable: %s",
            lantern.GetStatus().Message.empty()
                ? "Behavior Plan creation failed"
                : lantern.GetStatus().Message.c_str());

    Behavior::Edit ingame = OverclockIngame();
    Behavior::Edit energy = OverclockEnergy();
    auto overclock = m_Behavior.Plan(
        "Overclock",
        Behavior::On(Behavior::Scripts::One("Gameplay_Ingame"),
                     ingame),
        Behavior::On(Behavior::Scripts::One("Gameplay_Energy"),
                     energy));
    if (overclock) {
        m_OverclockPlan = overclock.Take();
        if (!m_Overclock || !m_Overclock->GetBoolean())
            (void) m_OverclockPlan.Disable();
    } else {
        m_Logger->Warn("Overclock Plan is unavailable: %s",
            overclock.GetStatus().Message.empty()
                ? "Behavior Plan creation failed"
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
    m_Behavior.Reset();
    m_Logger = nullptr;
    m_BML = nullptr;
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
    if (m_LanternPlan)
        (void) ApplyLanternScript(enabled, true);
}

bool GameplayTweaks::ApplyLanternScript(bool enabled,
                                        bool warnIfUnavailable) {
    if (!m_LanternPlan)
        return false;
    Behavior::Edit edit = LanternAlphaTest(enabled);
    auto replaced = m_LanternPlan.Replace(Behavior::On(
        Behavior::Scripts::One("Levelinit_build"),
        edit));
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

bool GameplayTweaks::ApplyOverclock(bool enabled, bool warnIfUnavailable) {
    if (!m_OverclockPlan) {
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
