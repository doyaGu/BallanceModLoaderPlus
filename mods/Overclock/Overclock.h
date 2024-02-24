#pragma once

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class Overclock : public IMod {
public:
    explicit Overclock(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "Overclock"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Overclock"; }
    const char *GetAuthor() override { return "Gamepiaynmo & Kakuty"; }
    const char *GetDescription() override { return "Remove delay of spawn / respawn."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;

private:
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);
    void OnEditScript_Gameplay_Energy(CKBehavior *script);

    IProperty *m_Overclock = nullptr;
    CKBehaviorLink *m_OverclockLinks[3] = {};
    CKBehaviorIO *m_OverclockLinkIO[3][2] = {};
};
