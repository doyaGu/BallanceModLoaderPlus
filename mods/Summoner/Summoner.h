#pragma once

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class Summoner : public IMod {
public:
    explicit Summoner(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "Summoner"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Summoner"; }
    const char *GetAuthor() override { return "Gamepiaynmo & Kakuty"; }
    const char *GetDescription() override { return "Ballance Entity Summoner."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;
    void OnProcess() override;

    void OnPostResetLevel() override;

private:
    void OnEditScript_Gameplay_Events(CKBehavior *script);

    InputHook *m_InputHook = nullptr;
    float m_DeltaTime = 0.0f;

    CK3dEntity *m_CamOrientRef = nullptr;
    CK3dEntity *m_CamTarget = nullptr;
    CKParameter *m_CurSector = nullptr;

    IProperty *m_AddBall[4] = {};
    int m_CurSel = -1;
    CK3dEntity *m_CurObj = nullptr;
    CK3dEntity *m_Balls[4] = {};
    std::vector<std::pair<int, CK3dEntity *>> m_TempBalls;
    IProperty *m_MoveKeys[6] = {};
};