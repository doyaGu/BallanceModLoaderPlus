#pragma once

#include <BML/BMLAll.h>

MOD_EXPORT IMod *BMLEntry(IBML *bml);
MOD_EXPORT void BMLExit(IMod *mod);

class DeformedWB : public IMod {
public:
    explicit DeformedWB(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "DeformedWB"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "Deformed Wooden Ball"; }
    const char *GetAuthor() override { return "Gamepiaynmo"; }
    const char *GetDescription() override { return "Randomly deform the player wooden ball. This does not affect the physical volume."; }
    DECLARE_BML_VERSION;

    void OnLoad() override;
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                      CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                      XObjectArray *objArray, CKObject *masterObj) override;
    void OnStartLevel() override;

private:
    CKMesh *m_BallMesh = nullptr;
    std::vector<VxVector> m_Vertices;
    std::vector<VxVector> m_Normals;
    IProperty *m_Enabled = nullptr;
    IProperty *m_Extent = nullptr;
};
