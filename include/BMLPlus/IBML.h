#ifndef BML_IBML_H
#define BML_IBML_H

#include <functional>

#include "CKAll.h"

#include "Defines.h"
#include "ILogger.h"
#include "ICommand.h"
#include "IMessageReceiver.h"

class IMod;
class InputHook;

class BML_EXPORT IBML : public IMessageReceiver {
public:
    virtual ~IBML() = default;

    virtual CKContext *GetCKContext() = 0;
    virtual CKRenderContext *GetRenderContext() = 0;

    virtual void ExitGame() = 0;

    virtual CKAttributeManager *GetAttributeManager() = 0;
    virtual CKBehaviorManager *GetBehaviorManager() = 0;
    virtual CKCollisionManager *GetCollisionManager() = 0;
    virtual InputHook *GetInputManager() = 0;
    virtual CKMessageManager *GetMessageManager() = 0;
    virtual CKPathManager *GetPathManager() = 0;
    virtual CKParameterManager *GetParameterManager() = 0;
    virtual CKRenderManager *GetRenderManager() = 0;
    virtual CKSoundManager *GetSoundManager() = 0;
    virtual CKTimeManager *GetTimeManager() = 0;

    virtual void AddTimer(CKDWORD delay, std::function<void()> callback) = 0;
    virtual void AddTimerLoop(CKDWORD delay, std::function<bool()> callback) = 0;
    virtual void AddTimer(float delay, std::function<void()> callback) = 0;
    virtual void AddTimerLoop(float delay, std::function<bool()> callback) = 0;

    virtual bool IsCheatEnabled() = 0;
    virtual void EnableCheat(bool enable) = 0;

    virtual void SendIngameMessage(const char *msg) = 0;

    virtual void RegisterCommand(ICommand *cmd) = 0;

    virtual void SetIC(CKBeObject *obj, bool hierarchy = false) = 0;
    virtual void RestoreIC(CKBeObject *obj, bool hierarchy = false) = 0;
    virtual void Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy = false) = 0;

    virtual bool IsIngame() = 0;
    virtual bool IsPaused() = 0;
    virtual bool IsPlaying() = 0;

    virtual CKDataArray *GetArrayByName(const char *name) = 0;
    virtual CKGroup *GetGroupByName(const char *name) = 0;
    virtual CKMaterial *GetMaterialByName(const char *name) = 0;
    virtual CKMesh *GetMeshByName(const char *name) = 0;
    virtual CK2dEntity *Get2dEntityByName(const char *name) = 0;
    virtual CK3dEntity *Get3dEntityByName(const char *name) = 0;
    virtual CK3dObject *Get3dObjectByName(const char *name) = 0;
    virtual CKCamera *GetCameraByName(const char *name) = 0;
    virtual CKTargetCamera *GetTargetCameraByName(const char *name) = 0;
    virtual CKLight *GetLightByName(const char *name) = 0;
    virtual CKTargetLight *GetTargetLightByName(const char *name) = 0;
    virtual CKSound *GetSoundByName(const char *name) = 0;
    virtual CKTexture *GetTextureByName(const char *name) = 0;
    virtual CKBehavior *GetScriptByName(const char *name) = 0;

    virtual void RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                                  float friction, float elasticity, float mass, const char *collGroup, float linearDamp,
                                  float rotDamp, float force, float radius) = 0;
    virtual void RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                                   const char *collGroup, bool enableColl) = 0;
    virtual void RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                   const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                   float linearDamp, float rotDamp, float radius) = 0;
    virtual void RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                     const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                     float linearDamp, float rotDamp) = 0;
    virtual void RegisterTrafo(const char *modulName) = 0;
    virtual void RegisterModul(const char *modulName) = 0;

    virtual int GetModCount() = 0;
    virtual IMod *GetMod(int index) = 0;

    virtual float GetSRScore() = 0;
    virtual int GetHSScore() = 0;

    virtual void SkipRenderForNextTick() = 0;
};


#endif // BML_IBML_H