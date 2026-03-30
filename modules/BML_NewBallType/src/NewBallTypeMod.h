#ifndef BML_NEWBALLTYPEMOD_H
#define BML_NEWBALLTYPEMOD_H

#include "bml_services.hpp"
#include "bml_ck_handle.hpp"

#include "CKAll.h"
#include "BML/ObjectLoadBehaviors.h"

#include <string>
#include <vector>

struct BallTypeInfo {
    friend class NewBallTypeMod;

    std::string m_File;
    std::string m_ID;
    std::string m_Name;
    std::string m_ObjName;

    bml::CKHandle<CKGroup> m_AllGroup;
    bml::CKHandle<CK3dObject> m_BallObj;

    bml::CKHandle<CKGroup> m_PiecesGroup;
    bml::CKHandle<CK3dEntity> m_PiecesFrame;

    bml::CKHandle<CKBehavior> m_Explosion;
    bml::CKHandle<CKBehavior> m_Reset;

    std::string m_CollGroup;

    float m_Friction;
    float m_Elasticity;
    float m_Mass;
    float m_LinearDamp;
    float m_RotDamp;
    float m_Force;
    float m_Radius;

private:
    bml::CKHandle<CKParameter> m_BallParam;
    bml::CKHandle<CKParameter> m_UsedParam;
    bml::CKHandle<CKParameter> m_ResetParam;
    bml::CKHandle<CKBehavior> m_Timer;
    bml::CKHandle<CKBehavior> m_BinarySwitch[2];
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

class NewBallTypeMod {
public:
    void SetServices(const bml::ModuleServices *services);
    void SetContext(CKContext *context);
    bool InitObjectLoader(CKContext *ctx);
    void HandleLoadObject(const char *filename,
                          CKBOOL isMap,
                          CK_CLASSID filterClass,
                          const CK_ID *objectIds,
                          uint32_t objectCount);
    void HandleLoadScript(CKBehavior *script);

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
    const bml::ModuleServices *m_Services = nullptr;
    CKContext *m_Context = nullptr;
    bml::ObjectLoadCache m_ObjectLoader{nullptr};
    bml::CKHandle<CKDataArray> m_PhysicsBall;
    bml::CKHandle<CKGroup> m_AllBalls;

    void SetInitialConditions(CKBeObject *object, bool hierarchy = false) const;

    void OnLoadBalls(const CK_ID *objectIds, uint32_t objectCount);
    void OnLoadLevelinit();
    void OnLoadSounds();
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
