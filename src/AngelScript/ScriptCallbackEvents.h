#ifndef BML_SCRIPTCALLBACKEVENTS_H
#define BML_SCRIPTCALLBACKEVENTS_H

#include <string>
#include <vector>

#include "BML/ICommand.h"
#include "BML/IMod.h"

namespace BML {

enum ScriptGameEvent {
    ScriptGameEventPreStartMenu,
    ScriptGameEventPostStartMenu,
    ScriptGameEventExitGame,
    ScriptGameEventPreLoadLevel,
    ScriptGameEventPostLoadLevel,
    ScriptGameEventStartLevel,
    ScriptGameEventPreResetLevel,
    ScriptGameEventPostResetLevel,
    ScriptGameEventPauseLevel,
    ScriptGameEventUnpauseLevel,
    ScriptGameEventPreExitLevel,
    ScriptGameEventPostExitLevel,
    ScriptGameEventPreNextLevel,
    ScriptGameEventPostNextLevel,
    ScriptGameEventDead,
    ScriptGameEventPreEndLevel,
    ScriptGameEventPostEndLevel,
    ScriptGameEventCounterActive,
    ScriptGameEventCounterInactive,
    ScriptGameEventBallNavActive,
    ScriptGameEventBallNavInactive,
    ScriptGameEventCamNavActive,
    ScriptGameEventCamNavInactive,
    ScriptGameEventBallOff,
    ScriptGameEventPreCheckpointReached,
    ScriptGameEventPostCheckpointReached,
    ScriptGameEventLevelFinish,
    ScriptGameEventGameOver,
    ScriptGameEventExtraPoint,
    ScriptGameEventPreSubLife,
    ScriptGameEventPostSubLife,
    ScriptGameEventPreLifeUp,
    ScriptGameEventPostLifeUp,
    ScriptGameEventCount
};

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
    ScriptLoadObjectEventView(const char *filename,
                              CKBOOL isMap,
                              const char *masterName,
                              CK_CLASSID filterClass,
                              CKBOOL addToScene,
                              CKBOOL reuseMeshes,
                              CKBOOL reuseMaterials,
                              CKBOOL dynamic);

    std::string GetFilename() const;
    bool IsMap() const { return m_IsMap; }
    std::string GetMasterName() const;
    int GetFilterClass() const { return m_FilterClass; }
    bool GetAddToScene() const { return m_AddToScene; }
    bool GetReuseMeshes() const { return m_ReuseMeshes; }
    bool GetReuseMaterials() const { return m_ReuseMaterials; }
    bool IsDynamic() const { return m_Dynamic; }

private:
    const char *m_Filename = "";
    bool m_IsMap = false;
    const char *m_MasterName = "";
    int m_FilterClass = 0;
    bool m_AddToScene = false;
    bool m_ReuseMeshes = false;
    bool m_ReuseMaterials = false;
    bool m_Dynamic = false;
};

class ScriptLoadScriptEventView {
public:
    ScriptLoadScriptEventView(const char *filename, CK_ID scriptId);
    std::string GetFilename() const;
    int GetScriptId() const { return m_ScriptId; }

private:
    const char *m_Filename = "";
    int m_ScriptId = 0;
};

class ScriptCommandEventView {
public:
    ScriptCommandEventView(ScriptCommandEventPhase phase,
                           ICommand *command,
                           const std::vector<std::string> *args);

    int GetPhase() const { return static_cast<int>(m_Phase); }
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
    size_t GetUserArgStart() const;

    ScriptCommandEventPhase m_Phase = ScriptCommandEventPre;
    ICommand *m_Command = nullptr;
    const std::vector<std::string> *m_Args = nullptr;
};

class ScriptPhysicalizeEventView {
public:
    ScriptPhysicalizeEventView(CK3dEntity *target,
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
                               int ballCnt,
                               int concaveCnt);

    int GetTargetId() const { return m_TargetId; }
    std::string GetTargetName() const;
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
    int GetConvexCount() const { return m_ConvexCount; }
    int GetBallCount() const { return m_BallCount; }
    int GetConcaveCount() const { return m_ConcaveCount; }

private:
    int m_TargetId = 0;
    const char *m_TargetName = "";
    bool m_Fixed = false;
    float m_Friction = 0.0f;
    float m_Elasticity = 0.0f;
    float m_Mass = 0.0f;
    const char *m_CollisionGroup = "";
    bool m_StartFrozen = false;
    bool m_EnableCollision = false;
    bool m_AutoCalcMassCenter = false;
    float m_LinearDamp = 0.0f;
    float m_RotDamp = 0.0f;
    const char *m_CollisionSurface = "";
    VxVector m_MassCenter;
    int m_ConvexCount = 0;
    int m_BallCount = 0;
    int m_ConcaveCount = 0;
};

class ScriptObjectEventView {
public:
    explicit ScriptObjectEventView(CK3dEntity *target = nullptr);
    int GetTargetId() const { return m_TargetId; }
    std::string GetTargetName() const;

private:
    int m_TargetId = 0;
    const char *m_TargetName = "";
};

} // namespace BML

#endif
