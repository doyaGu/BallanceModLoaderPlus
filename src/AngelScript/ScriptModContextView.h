#ifndef BML_SCRIPTMODCONTEXTVIEW_H
#define BML_SCRIPTMODCONTEXTVIEW_H

#include <string>

#include "CKAll.h"
#include "BML/InputHook.h"

class ModContext;
class IMod;
class asIScriptObject;

namespace BML {

class ScriptMod;
class ScriptCommandRef;
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
    bool IsKeyboardAttached() const;
    bool IsMouseAttached() const;
    bool IsKeyDown(CKKEYBOARD key) const;
    bool IsKeyUp(CKKEYBOARD key) const;
    bool IsKeyPressed(CKKEYBOARD key) const;
    bool IsKeyReleased(CKKEYBOARD key) const;
    bool IsKeyToggled(CKKEYBOARD key) const;
    std::string GetKeyName(CKKEYBOARD key) const;
    int GetKeyFromName(const std::string &name) const;
    bool IsMouseButtonDown(CK_MOUSEBUTTON button) const;
    bool IsMouseClicked(CK_MOUSEBUTTON button) const;
    bool IsMouseToggled(CK_MOUSEBUTTON button) const;
    Vx2DVector GetMousePosition(bool absolute) const;
    Vx2DVector GetLastMousePosition() const;
    VxVector GetMouseRelativePosition() const;
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
    ScriptCommandRef *RegisterCommand(asIScriptObject *command) const;
    bool UnregisterCommand(const std::string &name) const;
    ScriptDataShareRequestRef *RequestDataShare(asIScriptObject *request) const;
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
    std::string GetConfigString(const std::string &key, const std::string &defaultValue) const;
    void SetConfigString(const std::string &key, const std::string &value) const;
    bool GetConfigBool(const std::string &key, bool defaultValue) const;
    void SetConfigBool(const std::string &key, bool value) const;
    int GetConfigInt(const std::string &key, int defaultValue) const;
    void SetConfigInt(const std::string &key, int value) const;
    float GetConfigFloat(const std::string &key, float defaultValue) const;
    void SetConfigFloat(const std::string &key, float value) const;
    void LogInfo(const std::string &message) const;
    void LogWarn(const std::string &message) const;
    void LogError(const std::string &message) const;

private:
    ModContext *m_Context = nullptr;
    ScriptMod *m_Owner = nullptr;
};

} // namespace BML

#endif
