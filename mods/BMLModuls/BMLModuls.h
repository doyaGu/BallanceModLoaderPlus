#pragma once

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class BMLModuls : public IMod {
public:
    explicit BMLModuls(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "BMLModuls"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "BML Moduls"; }
    const char *GetAuthor() override { return "Gamepiaynmo"; }
    const char *GetDescription() override { return "Add some new moduls to the game."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
};