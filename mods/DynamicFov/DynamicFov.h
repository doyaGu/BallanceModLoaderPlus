#pragma once

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class DynamicFov : public IMod {
public:
    explicit DynamicFov(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "DynamicFov"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Dynamic Fov"; }
    const char *GetAuthor() override { return "Gamepiaynmo"; }
    const char *GetDescription() override { return "Dynamically adjust camera fov according to ball speed."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;
    void OnProcess() override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;

    void SetInactive() { m_BML->AddTimer(1ul, [this]() { m_IsActive = false; }); }

private:
    CKCamera *m_InGameCam = nullptr;
    CKDataArray *m_CurLevel = nullptr;
    CKBehavior *m_IngameScript = nullptr;
    CKBehavior *m_DynamicPos = nullptr;

    bool m_IsActive = false;
    bool m_WasPaused = false;
    VxVector m_LastPos;

    IProperty *m_Enabled = nullptr;
};