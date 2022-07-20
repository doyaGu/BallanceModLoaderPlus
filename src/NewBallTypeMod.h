#ifndef BML_NEWBALLTYPEMOD_H
#define BML_NEWBALLTYPEMOD_H

#include <string>
#include <vector>

#include "IMod.h"
#include "Version.h"

struct BallTypeInfo {
    friend class NewBallTypeMod;

    std::string m_File;
    std::string m_ID;
    std::string m_Name;
    std::string m_ObjName;

    CKGroup *m_AllGroup = nullptr;
    CK3dObject *m_BallObj = nullptr;

    CKGroup *m_PiecesGroup = nullptr;
    CK3dEntity *m_PiecesFrame = nullptr;

    CKBehavior *m_Explosion = nullptr;
    CKBehavior *m_Reset = nullptr;

    std::string m_CollGroup;

    float m_Friction;
    float m_Elasticity;
    float m_Mass;
    float m_LinearDamp;
    float m_RotDamp;
    float m_Force;
    float m_Radius;

private:
    CKParameter *m_BallParam = nullptr;
    CKParameter *m_UsedParam = nullptr;
    CKParameter *m_ResetParam = nullptr;
    CKBehavior *m_Timer = nullptr;
    CKBehavior *m_BinarySwitch[2];
};

struct FloorTypeInfo {
    std::string m_Name;
    std::string m_CollGroup;
    float m_Friction;
    float m_Elasticity;
    float m_Mass;
    CKBOOL m_EnableColl;
};

struct ModulConvexInfo : public FloorTypeInfo {
    CKBOOL m_Fixed;
    CKBOOL m_Frozen;
    CKBOOL m_MassCenter;
    float m_LinearDamp;
    float m_RotDamp;
};

struct ModulBallInfo : public ModulConvexInfo {
    float m_Radius;
};

struct ModulInfo {
    std::string m_Name;
    int m_Type;
};

class NewBallTypeMod : public IMod {
public:
    explicit NewBallTypeMod(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "NewBallType"; }
    const char *GetVersion() override { return BML_VERSION; }
    const char *GetName() override { return "New Ball Type"; }
    const char *GetAuthor() override { return "Gamepiaynmo"; }
    const char *GetDescription() override { return "Implementation of registering new ball types."; }
    DECLARE_BML_VERSION;

    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                              CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials,
                              CKBOOL dynamic, XObjectArray *objArray, CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;

    void RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                          float friction, float elasticity,
                          float mass, const char *collGroup, float linearDamp, float rotDamp, float force,
                          float radius);
    void RegisterFloorType(const char *floorName, float friction, float elasticity, float mass, const char *collGroup,
                           bool enableColl);
    void RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                           const char *collGroup,
                           bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp,
                           float radius);
    void RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                             const char *collGroup,
                             bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp);
    void RegisterTrafo(const char *modulName);
    void RegisterModul(const char *modulName);

private:
    CKDataArray *m_PhysicsBall = nullptr;
    CKGroup *m_AllBalls = nullptr;

    void OnLoadBalls(XObjectArray *objArray);
    void OnLoadLevelinit(XObjectArray *objArray);
    void OnLoadSounds(XObjectArray *objArray);
    void OnEditScript_Gameplay_Ingame(CKBehavior *script);
    void OnEditScript_Base_EventHandler(CKBehavior *script);
    void OnEditScript_PhysicalizeNewBall(CKBehavior *graph);
    void OnEditScript_ResetBallPieces(CKBehavior *graph);

    std::vector<BallTypeInfo> m_BallTypes;
    std::vector<FloorTypeInfo> m_FloorTypes;
    std::vector<ModulInfo> m_Moduls;
    std::vector<ModulConvexInfo> m_ModulConvexes;
    std::vector<ModulBallInfo> m_ModulBalls;
};

#endif // BML_NEWBALLTYPEMOD_H