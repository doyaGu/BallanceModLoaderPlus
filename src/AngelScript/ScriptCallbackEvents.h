#ifndef BML_SCRIPTCALLBACKEVENTS_H
#define BML_SCRIPTCALLBACKEVENTS_H

#include <string>
#include <vector>

#include "BML/ICommand.h"
#include "BML/IMod.h"

namespace BML {

enum ScriptGameEvent {
    ScriptGameEventPreStartMenu = 0,
    ScriptGameEventPostStartMenu = 1,
    ScriptGameEventExitGame = 2,
    ScriptGameEventPreLoadLevel = 3,
    ScriptGameEventPostLoadLevel = 4,
    ScriptGameEventStartLevel = 5,
    ScriptGameEventPreResetLevel = 6,
    ScriptGameEventPostResetLevel = 7,
    ScriptGameEventPauseLevel = 8,
    ScriptGameEventUnpauseLevel = 9,
    ScriptGameEventPreExitLevel = 10,
    ScriptGameEventPostExitLevel = 11,
    ScriptGameEventPreNextLevel = 12,
    ScriptGameEventPostNextLevel = 13,
    ScriptGameEventDead = 14,
    ScriptGameEventPreEndLevel = 15,
    ScriptGameEventPostEndLevel = 16,
    ScriptGameEventCounterActive = 17,
    ScriptGameEventCounterInactive = 18,
    ScriptGameEventBallNavActive = 19,
    ScriptGameEventBallNavInactive = 20,
    ScriptGameEventCamNavActive = 21,
    ScriptGameEventCamNavInactive = 22,
    ScriptGameEventBallOff = 23,
    ScriptGameEventPreCheckpointReached = 24,
    ScriptGameEventPostCheckpointReached = 25,
    ScriptGameEventLevelFinish = 26,
    ScriptGameEventGameOver = 27,
    ScriptGameEventExtraPoint = 28,
    ScriptGameEventPreSubLife = 29,
    ScriptGameEventPostSubLife = 30,
    ScriptGameEventPreLifeUp = 31,
    ScriptGameEventPostLifeUp = 32,
    ScriptGameEventCount
};

const char *GetScriptGameEventName(int event);

enum ScriptCommandEventPhase {
    ScriptCommandEventPre,
    ScriptCommandEventPost,
    ScriptCommandEventExecute,
    ScriptCommandEventComplete
};

class ScriptRenderEventView {
public:
    explicit ScriptRenderEventView(CK_RENDER_FLAGS flags = static_cast<CK_RENDER_FLAGS>(0));
    int GetFlags() const { return m_Flags; }

private:
    int m_Flags = 0;
};

class ScriptCheatEventView {
public:
    explicit ScriptCheatEventView(bool enabled = false);
    bool IsEnabled() const { return m_Enabled; }
    bool GetEnabled() const { return m_Enabled; }

private:
    bool m_Enabled = false;
};

class ScriptLoadObjectEventView {
public:
    ScriptLoadObjectEventView() = default;
    ScriptLoadObjectEventView(const char *filename,
                              CKBOOL isMap,
                              const char *masterName,
                              CK_CLASSID filterClass,
                              CKBOOL addToScene,
                              CKBOOL reuseMeshes,
                              CKBOOL reuseMaterials,
                              CKBOOL dynamic,
                              CKContext *context,
                              XObjectArray *objectArray,
                              CKObject *masterObject);

    std::string GetFilename() const;
    bool IsMap() const { return m_IsMap; }
    std::string GetMasterName() const;
    int GetFilterClass() const { return m_FilterClass; }
    bool GetAddToScene() const { return m_AddToScene; }
    bool GetReuseMeshes() const { return m_ReuseMeshes; }
    bool GetReuseMaterials() const { return m_ReuseMaterials; }
    bool IsDynamic() const { return m_Dynamic; }
    int GetObjectCount() const;
    int GetObjectId(int index) const;
    CKObject *BorrowObject(int index) const;
    CKObject *BorrowMasterObject() const;

private:
    CKObject *ResolveObject(CK_ID id, CK_CLASSID classId) const;

    std::string m_Filename;
    bool m_IsMap = false;
    std::string m_MasterName;
    int m_FilterClass = 0;
    bool m_AddToScene = false;
    bool m_ReuseMeshes = false;
    bool m_ReuseMaterials = false;
    bool m_Dynamic = false;
    CKContext *m_Context = nullptr;
    std::vector<CK_ID> m_ObjectIds;
    CK_ID m_MasterObjectId = 0;
};

class ScriptLoadScriptEventView {
public:
    ScriptLoadScriptEventView() = default;
    ScriptLoadScriptEventView(CKContext *context, const char *filename, CKBehavior *script);
    std::string GetFilename() const;
    int GetScriptId() const { return m_ScriptId; }
    CKBehavior *BorrowScript() const;

private:
    CKContext *m_Context = nullptr;
    std::string m_Filename;
    CK_ID m_ScriptId = 0;
};

class ScriptCommandEventView {
public:
    ScriptCommandEventView() = default;
    ScriptCommandEventView(ScriptCommandEventPhase phase,
                           ICommand *command,
                           const std::vector<std::string> *args);

    ScriptCommandEventPhase GetPhase() const { return m_Phase; }
    bool IsPre() const { return m_Phase == ScriptCommandEventPre; }
    bool IsPost() const { return m_Phase == ScriptCommandEventPost; }
    bool IsExecute() const { return m_Phase == ScriptCommandEventExecute; }
    bool IsComplete() const { return m_Phase == ScriptCommandEventComplete; }
    std::string GetCommandName() const;
    int GetArgCount() const;
    std::string GetArg(int index) const;
    std::string GetArgsText() const;
    bool IsCheat() const;

private:
    ScriptCommandEventPhase m_Phase = ScriptCommandEventPre;
    std::string m_CommandName;
    std::vector<std::string> m_Args;
    bool m_IsCheat = false;
};

class ScriptConfigEventView {
public:
    ScriptConfigEventView() = default;
    ScriptConfigEventView(const char *modId, const char *category, const char *key, IProperty *property);
    std::string GetModId() const;
    std::string GetCategory() const;
    std::string GetKey() const;
    int GetType() const { return m_Type; }
    bool HasProperty() const { return m_HasProperty; }

private:
    std::string m_ModId;
    std::string m_Category;
    std::string m_Key;
    int m_Type = 0;
    bool m_HasProperty = false;
};

class ScriptPhysicalizeEventView {
public:
    ScriptPhysicalizeEventView() = default;
    ScriptPhysicalizeEventView(CKContext *context,
                               CK3dEntity *target,
                               CKBOOL fixed,
                               float friction,
                               float elasticity,
                               float mass,
                               const char *collGroup,
                               CKBOOL startFrozen,
                               CKBOOL enableColl,
                               CKBOOL calcMassCenter,
                               float linearDamp,
                               float rotDamp,
                               const char *collSurface,
                               VxVector massCenter,
                               int convexCnt,
                               CKMesh **convexMesh,
                               int ballCnt,
                               VxVector *ballCenter,
                               float *ballRadius,
                               int concaveCnt,
                               CKMesh **concaveMesh);

    int GetTargetId() const { return m_TargetId; }
    std::string GetTargetName() const;
    CK3dEntity *BorrowTarget() const;
    bool GetFixed() const { return m_Fixed; }
    float GetFriction() const { return m_Friction; }
    float GetElasticity() const { return m_Elasticity; }
    float GetMass() const { return m_Mass; }
    std::string GetCollisionGroup() const;
    bool GetStartFrozen() const { return m_StartFrozen; }
    bool GetEnableCollision() const { return m_EnableCollision; }
    bool GetAutoCalcMassCenter() const { return m_AutoCalcMassCenter; }
    float GetLinearDamp() const { return m_LinearDamp; }
    float GetRotDamp() const { return m_RotDamp; }
    std::string GetCollisionSurface() const;
    float GetMassCenterX() const { return m_MassCenter.x; }
    float GetMassCenterY() const { return m_MassCenter.y; }
    float GetMassCenterZ() const { return m_MassCenter.z; }
    VxVector GetMassCenter() const { return m_MassCenter; }
    int GetConvexCount() const { return m_ConvexCount; }
    CKMesh *BorrowConvexMesh(int index) const;
    int GetBallCount() const { return m_BallCount; }
    VxVector GetBallCenter(int index) const;
    float GetBallRadius(int index) const;
    int GetConcaveCount() const { return m_ConcaveCount; }
    CKMesh *BorrowConcaveMesh(int index) const;

private:
    CKObject *ResolveObject(CK_ID id, CK_CLASSID classId) const;

    CKContext *m_Context = nullptr;
    CK_ID m_TargetId = 0;
    std::string m_TargetName;
    bool m_Fixed = false;
    float m_Friction = 0.0f;
    float m_Elasticity = 0.0f;
    float m_Mass = 0.0f;
    std::string m_CollisionGroup;
    bool m_StartFrozen = false;
    bool m_EnableCollision = false;
    bool m_AutoCalcMassCenter = false;
    float m_LinearDamp = 0.0f;
    float m_RotDamp = 0.0f;
    std::string m_CollisionSurface;
    VxVector m_MassCenter;
    int m_ConvexCount = 0;
    std::vector<CK_ID> m_ConvexMeshIds;
    int m_BallCount = 0;
    std::vector<VxVector> m_BallCenters;
    std::vector<float> m_BallRadii;
    int m_ConcaveCount = 0;
    std::vector<CK_ID> m_ConcaveMeshIds;
};

class ScriptObjectEventView {
public:
    explicit ScriptObjectEventView(CKContext *context = nullptr, CK3dEntity *target = nullptr);
    int GetTargetId() const { return m_TargetId; }
    std::string GetTargetName() const;
    CK3dEntity *BorrowTarget() const;

private:
    CKContext *m_Context = nullptr;
    CK_ID m_TargetId = 0;
    std::string m_TargetName;
};

} // namespace BML

#endif
