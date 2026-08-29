#include "Gameplay/GameplayTweaks.h"

#include <cstring>

#include "BML/IBML.h"
#include "BML/IConfig.h"
#include "BML/ILogger.h"
#include "BML/ScriptHelper.h"

using namespace ScriptHelper;

void GameplayTweaks::InitConfig(IConfig &config) {
    m_LanternAlphaTest = config.GetProperty("Tweak", "LanternAlphaTest");
    m_FixLifeBall = config.GetProperty("Tweak", "FixLifeBallFreeze");
    m_Overclock = config.GetProperty("Tweak", "Overclock");

    config.SetCategoryComment("Tweak", "Tweak Settings");

    m_LanternAlphaTest->SetComment("Enable alpha test for lantern material, this option can increase FPS");
    m_LanternAlphaTest->SetDefaultBoolean(true);

    m_FixLifeBall->SetComment("Game won't freeze when picking up life balls");
    m_FixLifeBall->SetDefaultBoolean(true);

    m_Overclock->SetComment("Remove delay of spawn / respawn");
    m_Overclock->SetDefaultBoolean(false);
}

bool GameplayTweaks::OnModifyConfig(const char *category, const char *key, IProperty *property) {
    if (!property || std::strcmp(category ? category : "", "Tweak") != 0)
        return false;

    if (property == m_LanternAlphaTest && std::strcmp(key ? key : "", "LanternAlphaTest") == 0) {
        ApplyLanternAlphaTest(property->GetBoolean());
        return true;
    }
    if (property == m_Overclock && std::strcmp(key ? key : "", "Overclock") == 0) {
        if (m_BML && m_BML->IsIngame())
            ApplyOverclock(property->GetBoolean(), true);
        return true;
    }
    return property == m_FixLifeBall && std::strcmp(key ? key : "", "FixLifeBallFreeze") == 0;
}

void GameplayTweaks::OnLoad(IBML &bml, ILogger &logger) {
    m_BML = &bml;
    m_Logger = &logger;
}

void GameplayTweaks::OnUnload() {
    // During activation rollback the current gameplay graphs can outlive this
    // Module. Restore their original links before dropping the borrowed pointers.
    ApplyOverclock(false, false);
    ClearOverclockPatch();
    m_Logger = nullptr;
    m_BML = nullptr;
}

void GameplayTweaks::OnLoadScript(CKBehavior *script) {
    if (!script || !script->GetName())
        return;

    const char *name = script->GetName();
    if (std::strcmp(name, "Gameplay_Ingame") == 0) {
        DiscoverOverclockPatch(script);
    } else if (std::strcmp(name, "Gameplay_Energy") == 0) {
        CompleteOverclockPatch(script);
    } else if (std::strcmp(name, "Levelinit_build") == 0) {
        PatchLanternAlphaTest(script);
    } else if (m_FixLifeBall && m_FixLifeBall->GetBoolean() &&
               (std::strcmp(name, "P_Extra_Life_Particle_Blob Script") == 0 ||
                std::strcmp(name, "P_Extra_Life_Particle_Fizz Script") == 0)) {
        PatchExtraLife(script);
    }
}

void GameplayTweaks::OnExitGame() {
    ClearOverclockPatch();
}

void GameplayTweaks::ApplyLanternAlphaTest(bool enabled) {
    CKMaterial *material = m_BML ? m_BML->GetMaterialByName("Laterne_Verlauf") : nullptr;
    if (!material)
        return;

    material->EnableAlphaTest(enabled ? TRUE : FALSE);
    material->SetAlphaFunc(VXCMP_GREATEREQUAL);
    material->SetAlphaRef(0);
}

void GameplayTweaks::PatchLanternAlphaTest(CKBehavior *script) {
    CKBehavior *mapping = FindFirstBB(script, "set Mapping and Textures");
    CKBehavior *lantern = mapping ? FindFirstBB(mapping, "Set Mat Laterne") : nullptr;
    CKBehavior *alphaTest = lantern ? FindFirstBB(lantern, "Set Alpha Test") : nullptr;
    CKParameter *enabled = alphaTest && alphaTest->GetInputParameterCount() > 0
                               ? alphaTest->GetInputParameter(0)->GetDirectSource()
                               : nullptr;
    if (!enabled) {
        if (m_Logger)
            m_Logger->Warn("Lantern alpha-test patch is unavailable in the current Levelinit_build graph");
        return;
    }

    const CKBOOL value = m_LanternAlphaTest && m_LanternAlphaTest->GetBoolean() ? TRUE : FALSE;
    enabled->SetValue(&value);
}

void GameplayTweaks::PatchExtraLife(CKBehavior *script) {
    CKBehavior *emitter = FindFirstBB(script, "SphericalParticleSystem");
    if (!emitter) {
        if (m_Logger)
            m_Logger->Warn("Life-ball freeze fix is unavailable in the current particle graph");
        return;
    }

    CKParameterIn *realTimeMode = emitter->CreateInputParameter("Real-Time Mode", CKPGUID_BOOL);
    if (realTimeMode) {
        realTimeMode->SetDirectSource(
            CreateParamValue<CKBOOL>(script, "Real-Time Mode", CKPGUID_BOOL, TRUE));
    }

    CKParameterIn *deltaTime = emitter->CreateInputParameter("DeltaTime", CKPGUID_FLOAT);
    if (deltaTime) {
        deltaTime->SetDirectSource(
            CreateParamValue<float>(script, "DeltaTime", CKPGUID_FLOAT, 20.0f));
    }
}

void GameplayTweaks::DiscoverOverclockPatch(CKBehavior *script) {
    ClearOverclockPatch();

    CKBehavior *ballManager = FindFirstBB(script, "BallManager");
    CKBehavior *deactivateBall = ballManager ? FindFirstBB(ballManager, "Deactivate Ball") : nullptr;
    CKBehavior *pieces = deactivateBall ? FindFirstBB(deactivateBall, "reset Ballpieces") : nullptr;
    CKBehaviorLink *deactivateLink = pieces ? FindNextLink(deactivateBall, pieces) : nullptr;
    CKBehaviorIO *deactivateTarget = deactivateLink ? deactivateLink->GetOutBehaviorIO() : nullptr;
    CKBehavior *afterPieces = deactivateTarget ? deactivateTarget->GetOwner() : nullptr;
    CKBehavior *beforeUnphysicalize = afterPieces ? FindNextBB(deactivateBall, afterPieces) : nullptr;
    CKBehavior *unphysicalize = beforeUnphysicalize
                                    ? FindNextBB(deactivateBall, beforeUnphysicalize)
                                    : nullptr;

    CKBehavior *newBall = ballManager ? FindFirstBB(ballManager, "New Ball") : nullptr;
    CKBehavior *physicalizeNewBall = newBall ? FindFirstBB(newBall, "physicalize new Ball") : nullptr;
    CKBehavior *previous = physicalizeNewBall ? FindPreviousBB(newBall, physicalizeNewBall) : nullptr;
    previous = previous ? FindPreviousBB(newBall, previous) : nullptr;
    previous = previous ? FindPreviousBB(newBall, previous) : nullptr;
    CKBehaviorLink *newBallLink = previous ? FindPreviousLink(newBall, previous) : nullptr;

    if (!deactivateLink || !unphysicalize || unphysicalize->GetInputCount() <= 1 ||
        !newBallLink || !physicalizeNewBall || physicalizeNewBall->GetInputCount() == 0) {
        RejectOverclockPatch("Gameplay_Ingame does not match the expected vanilla script graph");
        return;
    }

    m_OverclockLinks[0] = deactivateLink;
    m_OverclockLinkIO[0][1] = unphysicalize->GetInput(1);
    m_OverclockLinks[1] = newBallLink;
    m_OverclockLinkIO[1][1] = physicalizeNewBall->GetInput(0);
}

void GameplayTweaks::CompleteOverclockPatch(CKBehavior *script) {
    CKBehavior *delay = FindFirstBB(script, "Delayer");
    CKBehaviorLink *energyLink = delay ? FindPreviousLink(script, delay) : nullptr;
    CKBehaviorLink *afterDelay = delay ? FindNextLink(script, delay) : nullptr;
    CKBehaviorIO *energyTarget = afterDelay ? afterDelay->GetOutBehaviorIO() : nullptr;

    if (!m_OverclockLinks[0] || !m_OverclockLinks[1] ||
        !m_OverclockLinkIO[0][1] || !m_OverclockLinkIO[1][1] ||
        !energyLink || !energyTarget) {
        RejectOverclockPatch("the gameplay scripts do not match the expected vanilla graph");
        return;
    }

    CKBehaviorIO *originalTargets[3] = {
        m_OverclockLinks[0]->GetOutBehaviorIO(),
        m_OverclockLinks[1]->GetOutBehaviorIO(),
        energyLink->GetOutBehaviorIO(),
    };
    if (!originalTargets[0] || !originalTargets[1] || !originalTargets[2]) {
        RejectOverclockPatch("an expected gameplay link has no target");
        return;
    }

    m_OverclockLinks[2] = energyLink;
    m_OverclockLinkIO[2][1] = energyTarget;
    for (int i = 0; i < 3; ++i)
        m_OverclockLinkIO[i][0] = originalTargets[i];

    ApplyOverclock(m_Overclock && m_Overclock->GetBoolean(), false);
}

bool GameplayTweaks::ApplyOverclock(bool enabled, bool warnIfUnavailable) {
    const int target = enabled ? 1 : 0;
    for (int i = 0; i < 3; ++i) {
        if (!m_OverclockLinks[i] || !m_OverclockLinkIO[i][target]) {
            if (warnIfUnavailable && m_Logger)
                m_Logger->Warn("Overclock is unavailable for the current gameplay scripts");
            return false;
        }
    }

    for (int i = 0; i < 3; ++i)
        m_OverclockLinks[i]->SetOutBehaviorIO(m_OverclockLinkIO[i][target]);
    return true;
}

void GameplayTweaks::ClearOverclockPatch() {
    for (int i = 0; i < 3; ++i) {
        m_OverclockLinks[i] = nullptr;
        m_OverclockLinkIO[i][0] = nullptr;
        m_OverclockLinkIO[i][1] = nullptr;
    }
}

void GameplayTweaks::RejectOverclockPatch(const char *reason) {
    ClearOverclockPatch();
    if (m_Logger)
        m_Logger->Error("Overclock script patch is unavailable: %s", reason ? reason : "unknown reason");
}
