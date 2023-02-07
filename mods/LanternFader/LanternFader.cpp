#include "LanternFader.h"

IMod *BMLEntry(IBML *bml) {
    return new LanternFader(bml);
}

void BMLExit(IMod *mod) {
    delete mod;
}

void LanternFader::OnLoad() {
    GetConfig()->SetCategoryComment("Alpha Test", "Alpha Test Settings for lantern material");
    m_AlphaTestEnabled = GetConfig()->GetProperty("Alpha Test", "Enable");
    m_AlphaTestEnabled->SetComment("Enable alpha test for lantern material");
    m_AlphaTestEnabled->SetDefaultBoolean(true);
}

void LanternFader::OnLoadScript(const char *filename, CKBehavior *script) {
    if (!strcmp(script->GetName(), "Levelinit_build")) {
        CKBehavior *smat = ScriptHelper::FindFirstBB(script, "set Mapping and Textures");
        if (!smat) return;
        CKBehavior *sml = ScriptHelper::FindFirstBB(smat, "Set Mat Laterne");
        if (!sml) return;
        CKBehavior *sat = ScriptHelper::FindFirstBB(sml, "Set Alpha Test");
        if (!sat) return;

        CKParameter *sate = sat->GetInputParameter(0)->GetDirectSource();
        if (sate) {
            CKBOOL atest = m_AlphaTestEnabled->GetBoolean();
            sate->SetValue(&atest);
        }
    }
}

void LanternFader::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (prop == m_AlphaTestEnabled) {
        CKMaterial *mat = m_BML->GetMaterialByName("Laterne_Verlauf");
        if (mat) {
            CKBOOL atest = m_AlphaTestEnabled->GetBoolean();
            mat->EnableAlphaTest(atest);

            VXCMPFUNC afunc = VXCMP_GREATEREQUAL;
            mat->SetAlphaFunc(afunc);

            int ref = 0;
            mat->SetAlphaRef(ref);
        }
    }
}