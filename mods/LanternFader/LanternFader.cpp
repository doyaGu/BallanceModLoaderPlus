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

    m_AlphaTestRef = GetConfig()->GetProperty("Alpha Test", "Referential Value");
    m_AlphaTestRef->SetComment("Set alpha test referential value for lantern material (0-255)");
    m_AlphaTestRef->SetInteger(0);
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

        CKParameter *arv = sat->GetInputParameter(2)->GetDirectSource();
        if (arv) {
            int ref = m_AlphaTestRef->GetInteger();
            if (ref < 0) ref = 0;
            if (ref > 255) ref = 255;
            arv->SetValue(&ref);
        }
    }
}

void LanternFader::OnModifyConfig(const char *category, const char *key, IProperty *prop) {
    if (prop == m_AlphaTestEnabled || prop == m_AlphaTestRef) {
        CKMaterial *mat = m_BML->GetMaterialByName("Laterne_Verlauf");
        if (mat) {
            CKBOOL atest = m_AlphaTestEnabled->GetBoolean();
            mat->EnableAlphaTest(atest);

            VXCMPFUNC afunc = VXCMP_GREATEREQUAL;
            mat->SetAlphaFunc(afunc);

            int ref = m_AlphaTestRef->GetInteger();
            if (ref < 0) ref = 0;
            if (ref > 255) ref = 255;
            mat->SetAlphaRef(ref);
        }
    }
}