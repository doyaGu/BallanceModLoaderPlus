#pragma once

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class TravelMode : public IMod {
public:
    explicit TravelMode(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "TravelMode"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Travel Mode"; }
    const char *GetAuthor() override { return "Gamepiaynmo"; }
    const char *GetDescription() override { return "Travel Mode for Ballance."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnProcess() override;

    void OnPauseLevel() override;
    void OnUnpauseLevel() override;

    void EnterTravelCam();
    void ExitTravelCam();
    bool IsInTravelCam();

private:
    CKRenderContext *m_RenderContext = nullptr;
    InputHook *m_InputHook = nullptr;
    float m_DeltaTime = 0.0f;

    bool m_Paused = false;

    float m_TravelSpeed = 0.2f;
    CKCamera *m_TravelCam = nullptr;

    IProperty *m_CamRot[2] = {};
    IProperty *m_CamY[2] = {};
    IProperty *m_CamZ[2] = {};
    IProperty *m_Cam45 = nullptr;
    IProperty *m_CamReset = nullptr;
    IProperty *m_CamOn = nullptr;

    CK3dEntity *m_CamPos = nullptr;
    CK3dEntity *m_CamOrient = nullptr;
    CK3dEntity *m_CamOrientRef = nullptr;
    CK3dEntity *m_CamTarget = nullptr;
};
