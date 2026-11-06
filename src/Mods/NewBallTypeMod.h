#ifndef BML_NEWBALLTYPEMOD_H
#define BML_NEWBALLTYPEMOD_H

#include <string>
#include <vector>

#include "BML/Behavior.hpp"
#include "Loader/ModRecord.h"
#include "BML/Version.h"

struct TypeRegistration { std::string m_Owner; bool m_Applied = false; };

struct BallTypeInfo : TypeRegistration {
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

};

struct FloorTypeInfo : TypeRegistration {
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

struct ModulInfo : TypeRegistration {
    std::string m_Name;
    int m_Type;
};

class NewBallTypeMod {
public:
    ModRecord &Registration() noexcept { return m_Record; }

    const ModRecord &Registration() const noexcept { return m_Record; }

    explicit NewBallTypeMod(ModContext *bml) : m_BML(bml), m_Record(bml) { m_Record.Bind(*this); }

    const char *GetID() { return "NewBallType"; }

    const char *GetVersion() { return BML_VERSION; }

    const char *GetName() { return "New Ball Type"; }

    const char *GetAuthor() { return "doyaGu & Gamepiaynmo"; }

    const char *GetDescription() { return "Implementation of registering new ball types."; }

    BMLVersion GetBMLVersion() { return {}; }

    void OnLoad();
    void OnUnload();
    void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName,
                      CK_CLASSID filterClass, CKBOOL addToScene, CKBOOL reuseMeshes,
                      CKBOOL reuseMaterials, CKBOOL dynamic, XObjectArray *objArray,
                      CKObject *masterObj);
    void OnLoadScript(const char *filename, CKBehavior *script);
    void OnProcess();
    void OnExitGame();

    void CleanupOwner(const std::string &owner);
    void RegisterBallType(const std::string &owner, const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                          float friction, float elasticity,
                          float mass, const char *collGroup, float linearDamp, float rotDamp, float force,
                          float radius);
    void RegisterFloorType(const std::string &owner, const char *floorName, float friction, float elasticity, float mass, const char *collGroup,
                           bool enableColl);
    void RegisterModulBall(const std::string &owner, const char *modulName, bool fixed, float friction, float elasticity, float mass,
                           const char *collGroup,
                           bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp,
                           float radius);
    void RegisterModulConvex(const std::string &owner, const char *modulName, bool fixed, float friction, float elasticity, float mass,
                             const char *collGroup,
                             bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp);
    void RegisterTrafo(const std::string &owner, const char *modulName);
    void RegisterModul(const std::string &owner, const char *modulName);

private:
    ModContext *m_BML;
    ModRecord m_Record;

    CKDataArray *m_PhysicsBall = nullptr;
    CKGroup *m_AllBalls = nullptr;

    void OnLoadBalls(XObjectArray *objArray);
    void OnLoadLevelinit(XObjectArray *objArray);
    void OnLoadSounds(XObjectArray *objArray);
    void InstallBallBehaviorPatch();
    void CloseBallBehaviorPatch(const char *operation);

    BML::Behavior::Session m_Behavior;
    BML::Behavior::Patch m_BallPatch;
    BML::Behavior::ObjectRef m_GameplayScript{};
    BML::Behavior::ObjectRef m_EventHandler{};
    bool m_BallPatchPending = false;

    std::vector<BallTypeInfo> m_BallTypes;
    std::vector<FloorTypeInfo> m_FloorTypes;
    std::vector<ModulInfo> m_Moduls;
    std::vector<ModulConvexInfo> m_ModulConvexes;
    std::vector<ModulBallInfo> m_ModulBalls;
};

#endif // BML_NEWBALLTYPEMOD_H
