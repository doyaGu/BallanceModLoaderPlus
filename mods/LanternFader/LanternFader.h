#pragma once

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class LanternFader : public IMod {
public:
    explicit LanternFader(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "LanternFader"; }
    const char *GetVersion() override { return "0.1.0"; }
    const char *GetName() override { return "Lantern Fader"; }
    const char *GetAuthor() override { return "Kakuty"; }
    const char *GetDescription() override { return "Alleviate FPS drops by turning on alpha test for lantern material."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;

private:
    IProperty *m_AlphaTestEnabled = nullptr;
};
