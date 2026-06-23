#ifndef BML_SCRIPTMODCONTEXTVIEW_H
#define BML_SCRIPTMODCONTEXTVIEW_H

#include <string>

#include "CKAll.h"
#include "BML/InputHook.h"

class ModContext;
class IMod;
class asIScriptFunction;
class asIScriptObject;

namespace BML {

enum class ScriptModReloadPhase : int {
    None = 0,
    Unload = 1,
    Load = 2,
    Rollback = 3,
    Recovery = 4,
    Cleanup = 5,
};

class ScriptMod;
class ScriptCommandRef;
struct ScriptCommandDefinition;
class ScriptDataShareRequestRef;
class ScriptTimerRef;

class ScriptModContextView {
public:
    ScriptModContextView() = default;
    ScriptModContextView(ModContext *context, ScriptMod *owner);

    bool HasContext() const { return m_Context != nullptr; }
    std::string GetModId() const;
    std::string GetModName() const;
    CKContext *GetCKContext() const;
    bool IsReloading() const;
    ScriptModReloadPhase GetReloadPhase() const;
    unsigned int GetReloadAttemptId() const;
    unsigned int GetModGeneration() const;
    unsigned int GetRuntimeGeneration() const;
    CKRenderContext *GetRenderContext() const;
    CKAttributeManager *GetAttributeManager() const;
    CKBehaviorManager *GetBehaviorManager() const;
    CKCollisionManager *GetCollisionManager() const;
    InputHook *GetInputManager() const;
    CKMessageManager *GetMessageManager() const;
    CKPathManager *GetPathManager() const;
    CKParameterManager *GetParameterManager() const;
    CKRenderManager *GetRenderManager() const;
    CKSoundManager *GetSoundManager() const;
    CKTimeManager *GetTimeManager() const;
    bool IsInGame() const;
    bool IsInLevel() const;
    bool IsPaused() const;
    bool IsPlaying() const;
    bool IsCheatEnabled() const;
    void EnableCheat(bool enable) const;
    void ExitGame() const;
    float GetSRScore() const;
    int GetHSScore() const;
    std::string GetDirectoryUtf8(int type) const;
    float GetTimeMs() const;
    float GetAbsoluteTimeMs() const;
    float GetDeltaTimeMs() const;
    unsigned int GetFrameCount() const;
    bool IsObjectValid(CKObject *object) const;
    int GetObjectId(CKObject *object) const;
    std::string GetObjectName(CKObject *object) const;
    int GetObjectClassId(CKObject *object) const;
    bool IsObjectVisible(CKObject *object) const;
    bool IsObjectDynamic(CKObject *object) const;
    int GetBeObjectPriority(CKBeObject *object) const;
    int GetBeObjectScriptCount(CKBeObject *object) const;
    int GetBeObjectAttributeCount(CKBeObject *object) const;
    VxVector Get3dEntityPosition(CK3dEntity *entity) const;
    VxVector Get3dEntityScale(CK3dEntity *entity, bool local) const;
    int Get3dEntityChildCount(CK3dEntity *entity) const;
    CK3dEntity *Get3dEntityParent(CK3dEntity *entity) const;
    CKDataArray *GetArrayByName(const std::string &name) const;
    CKGroup *GetGroupByName(const std::string &name) const;
    CKMaterial *GetMaterialByName(const std::string &name) const;
    CKMesh *GetMeshByName(const std::string &name) const;
    CK2dEntity *Get2dEntityByName(const std::string &name) const;
    CK3dEntity *Get3dEntityByName(const std::string &name) const;
    CK3dObject *Get3dObjectByName(const std::string &name) const;
    CKCamera *GetCameraByName(const std::string &name) const;
    CKTargetCamera *GetTargetCameraByName(const std::string &name) const;
    CKLight *GetLightByName(const std::string &name) const;
    CKTargetLight *GetTargetLightByName(const std::string &name) const;
    CKSound *GetSoundByName(const std::string &name) const;
    CKTexture *GetTextureByName(const std::string &name) const;
    CKBehavior *GetScriptByName(const std::string &name) const;
    void SetIC(CKBeObject *object, bool hierarchy) const;
    void RestoreIC(CKBeObject *object, bool hierarchy) const;
    void Show(CKBeObject *object, CK_OBJECT_SHOWOPTION show, bool hierarchy) const;
    int GetHUD() const;
    void SetHUD(int mode) const;
    void ShowTitle(bool show) const;
    void ShowFPS(bool show) const;
    void ShowSRTimer(bool show) const;
    void StartSRTimer() const;
    void PauseSRTimer() const;
    void ResetSRTimer() const;
    float GetSRTime() const;
    ScriptTimerRef *AddTimer(asIScriptObject *timer) const;
    ScriptTimerRef *SetTimeoutTicks(unsigned int delayTicks, asIScriptFunction *callback, const std::string &name) const;
    ScriptTimerRef *SetTimeout(float delayMs, asIScriptFunction *callback, const std::string &name) const;
    ScriptTimerRef *SetIntervalTicks(unsigned int delayTicks, asIScriptFunction *callback, const std::string &name) const;
    ScriptTimerRef *SetInterval(float delayMs, asIScriptFunction *callback, const std::string &name) const;
    ScriptCommandRef *RegisterCommand(asIScriptObject *command) const;
    ScriptCommandRef *RegisterCommand(const ScriptCommandDefinition &definition,
                                      asIScriptFunction *execute,
                                      asIScriptFunction *complete) const;
    bool UnregisterCommand(const std::string &name) const;
    ScriptDataShareRequestRef *RequestDataShare(asIScriptObject *request) const;
    ScriptDataShareRequestRef *RequestDataShare(const std::string &key,
                                                int type,
                                                asIScriptFunction *callback,
                                                const std::string &name) const;
    bool RegisterBallType(const std::string &ballFile,
                          const std::string &ballId,
                          const std::string &ballName,
                          const std::string &objName,
                          float friction,
                          float elasticity,
                          float mass,
                          const std::string &collGroup,
                          float linearDamp,
                          float rotDamp,
                          float force,
                          float radius) const;
    bool RegisterFloorType(const std::string &floorName,
                           float friction,
                           float elasticity,
                           float mass,
                           const std::string &collGroup,
                           bool enableColl) const;
    bool RegisterModulBall(const std::string &modulName,
                           bool fixed,
                           float friction,
                           float elasticity,
                           float mass,
                           const std::string &collGroup,
                           bool frozen,
                           bool enableColl,
                           bool calcMassCenter,
                           float linearDamp,
                           float rotDamp,
                           float radius) const;
    bool RegisterModulConvex(const std::string &modulName,
                             bool fixed,
                             float friction,
                             float elasticity,
                             float mass,
                             const std::string &collGroup,
                             bool frozen,
                             bool enableColl,
                             bool calcMassCenter,
                             float linearDamp,
                             float rotDamp) const;
    bool RegisterTrafo(const std::string &modulName) const;
    bool RegisterModul(const std::string &modulName) const;
    std::string GetModRootUtf8() const;
    std::string ResolveModPathUtf8(const std::string &relativePath) const;
    bool ModFileExistsUtf8(const std::string &relativePath) const;
    bool ModDirectoryExistsUtf8(const std::string &relativePath) const;
    std::string ReadModTextFileUtf8(const std::string &relativePath, const std::string &defaultValue) const;
    int GetCommandCount() const;
    std::string GetCommandName(int index) const;
    std::string GetCommandAlias(int index) const;
    std::string GetCommandDescription(int index) const;
    bool HasCommand(const std::string &name) const;
    bool IsCommandCheat(const std::string &name) const;
    int GetGlobalModCount() const;
    std::string GetGlobalModId(int index) const;
    void SendIngameMessage(const std::string &message) const;
    void ClearIngameMessages() const;
    void ExecuteCommand(const std::string &command) const;
    void SkipRenderForNextTick() const;
    void OpenModsMenu() const;
    void CloseModsMenu() const;
    void OpenMapMenu() const;
    void CloseMapMenu() const;

private:
    ModContext *m_Context = nullptr;
    ScriptMod *m_Owner = nullptr;
};

} // namespace BML

#endif
