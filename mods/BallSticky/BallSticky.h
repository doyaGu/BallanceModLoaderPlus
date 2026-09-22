#pragma once

#include <BML\BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class BallSticky : public IMod {
public:
    explicit BallSticky(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "BallSticky"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Ball Sticky"; }
    const char *GetAuthor() override { return "Gamepiaynmo"; }
    const char *GetDescription() override { return "Add a new ball type that can climb walls."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                      CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials,
                      CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;
    void OnProcess() override;

private:
    void OnLoadLevelinit(XObjectArray *objArray);
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);

    CK3dEntity *m_BallRef[2] = {};
    CK3dEntity *m_CamTarget = nullptr;
    CKDataArray *m_CurLevel = nullptr;
    float m_StickyImpulse = 0;
    CKParameter *m_StickyForce[2] = {};
};