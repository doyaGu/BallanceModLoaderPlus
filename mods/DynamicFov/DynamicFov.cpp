#include "DynamicFov.h"

DynamicFov *g_Mod = nullptr;

IMod *BMLEntry(IBML *bml) {
    g_Mod = new DynamicFov(bml);
    return g_Mod;
}

void BMLExit(IMod *mod) {
    delete mod;
}

void DynamicFov::OnLoad() {
    GetConfig()->SetCategoryComment("Misc", "Miscellaneous");
    m_Enabled = GetConfig()->GetProperty("Misc", "Enable");
    m_Enabled->SetComment("Enable Dynamic Fov");
    m_Enabled->SetDefaultBoolean(false);
}

void DynamicFov::OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                              CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                              XObjectArray *objArray, CKObject *masterObj) {
    if (!strcmp(filename, "3D Entities\\Camera.nmo")) {
        m_InGameCam = m_BML->GetTargetCameraByName("InGameCam");
    }

    if (!strcmp(filename, "3D Entities\\Gameplay.nmo")) {
        m_CurLevel = m_BML->GetArrayByName("CurrentLevel");
    }
}

void DynamicFov::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!strcmp(script->GetName(), "Gameplay_Ingame")) {
        m_IngameScript = script;
        CKBehavior *ballMgr = ScriptHelper::FindFirstBB(script, "BallManager");
        ScriptHelper::CreateLink(script, ballMgr, ExecuteBB::CreateHookBlock(script, [](const CKBehaviorContext*, void*) -> int {
            g_Mod->SetInactive();
            return CKBR_OK;
            }));
        CKBehavior *init = ScriptHelper::FindFirstBB(script, "Init Ingame");
        m_DynamicPos = ScriptHelper::FindNextBB(script, init, "TT Set Dynamic Position");
    }
}

void DynamicFov::OnProcess() {
    if (m_IngameScript && m_InGameCam && m_Enabled->GetBoolean()
        && m_BML->Get3dObjectByName("PS_FourFlames_01_Dual") == nullptr) {
        if (m_IngameScript->IsActive() && m_DynamicPos->IsActive() && !m_DynamicPos->IsOutputActive(1)) {
            auto *ball = (CK3dObject *) m_CurLevel->GetElementObject(0, 1);
            if (ball != nullptr) {
                VxVector position;
                ball->GetPosition(&position);
                CKRenderContext *rc = m_BML->GetRenderContext();

                if (!m_IsActive) {
                    m_LastPos = position;
                    m_InGameCam->SetFov(0.75f * rc->GetWidth() / rc->GetHeight());
                } else {
                    float delta = m_BML->GetTimeManager()->GetLastDeltaTime();
                    float speed = (position - m_LastPos).Magnitude() / delta * 6;
                    float newfov = (0.75f + speed) * rc->GetWidth() / rc->GetHeight();
                    newfov = (std::min)(newfov, PI / 2);
                    float curfov = m_InGameCam->GetFov();
                    m_InGameCam->SetFov((newfov - curfov) * (std::min)(delta, 20.0f) / 1000 + curfov);
                }

                m_IsActive = true;
                m_LastPos = position;
            }
        } else if (!m_WasPaused) {
            m_IsActive = false;
        }

        m_WasPaused = m_BML->IsPaused();
    }
}

void DynamicFov::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (prop == m_Enabled && !m_Enabled->GetBoolean()) {
        CKRenderContext *rc = m_BML->GetRenderContext();
        m_InGameCam->SetFov(0.75f * rc->GetWidth() / rc->GetHeight());
        m_IsActive = false;
    }
}