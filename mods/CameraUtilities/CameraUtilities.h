#pragma once

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class CameraUtilities : public IMod {
public:
    explicit CameraUtilities(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "CameraUtilities"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Camera Utilities"; }
    const char *GetAuthor() override { return "Gamepiaynmo & Kakuty"; }
    const char *GetDescription() override { return "Camera utilities for Ballance."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnProcess() override;

private:
    InputHook *m_InputHook = nullptr;
    float m_DeltaTime = 0.0f;

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