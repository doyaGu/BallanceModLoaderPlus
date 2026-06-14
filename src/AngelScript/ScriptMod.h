#ifndef BML_SCRIPTMOD_H
#define BML_SCRIPTMOD_H

#include <string>

#include "BML/Interop.h"
#include "BML/ICommand.h"
#include "BML/IMod.h"
#include "ScriptCallbackEvents.h"
#include "ScriptDiagnostic.h"
#include "ScriptCommandService.h"
#include "ScriptDataShareService.h"
#include "ScriptExportDispatcher.h"
#include "ScriptModEntryScanner.h"
#include "ScriptModContextView.h"
#include "ScriptModDefinition.h"
#include "ScriptModEventRouter.h"
#include "ScriptModRuntime.h"
#include "ScriptModState.h"
#include "ScriptTimerService.h"

class ModContext;
class asIScriptObject;

namespace BML {

class ScriptMod;

class ScriptMod final : public IMod {
public:
    ScriptMod(ModContext *context,
              ScriptModDefinition definition,
              ScriptModEntry entry,
              ScriptModRuntime runtime);
    ~ScriptMod() override;

    const char *GetID() override;
    const char *GetVersion() override;
    const char *GetName() override;
    const char *GetAuthor() override;
    const char *GetDescription() override;
    BMLVersion GetBMLVersion() override;

    void OnLoad() override;
    void OnUnload() override;
    void OnLoadObject(const char *filename,
                      CKBOOL isMap,
                      const char *masterName,
                      CK_CLASSID filterClass,
                      CKBOOL addToScene,
                      CKBOOL reuseMeshes,
                      CKBOOL reuseMaterials,
                      CKBOOL dynamic,
                      XObjectArray *objArray,
                      CKObject *masterObj) override;
    void OnLoadScript(const char *filename, CKBehavior *script) override;
    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override;
    void OnPreStartMenu() override;
    void OnPostStartMenu() override;
    void OnExitGame() override;
    void OnPreLoadLevel() override;
    void OnPostLoadLevel() override;
    void OnStartLevel() override;
    void OnPreResetLevel() override;
    void OnPostResetLevel() override;
    void OnPauseLevel() override;
    void OnUnpauseLevel() override;
    void OnPreExitLevel() override;
    void OnPostExitLevel() override;
    void OnPreNextLevel() override;
    void OnPostNextLevel() override;
    void OnDead() override;
    void OnPreEndLevel() override;
    void OnPostEndLevel() override;
    void OnCounterActive() override;
    void OnCounterInactive() override;
    void OnBallNavActive() override;
    void OnBallNavInactive() override;
    void OnCamNavActive() override;
    void OnCamNavInactive() override;
    void OnBallOff() override;
    void OnPreCheckpointReached() override;
    void OnPostCheckpointReached() override;
    void OnLevelFinish() override;
    void OnGameOver() override;
    void OnExtraPoint() override;
    void OnPreSubLife() override;
    void OnPostSubLife() override;
    void OnPreLifeUp() override;
    void OnPostLifeUp() override;
    void OnProcess() override;
    void OnRender(CK_RENDER_FLAGS flags) override;
    void OnCheatEnabled(bool enable) override;
    void OnPhysicalize(CK3dEntity *target,
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
                       CKMesh **concaveMesh) override;
    void OnUnphysicalize(CK3dEntity *target) override;
    void OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) override;
    void OnPostCommandExecute(ICommand *command, const std::vector<std::string> &args) override;

    bool IsFailed() const { return m_State.IsFailed(); }
    bool IsLoaded() const { return m_State.IsLoaded(); }
    const std::string &GetLastDiagnostic() const { return m_State.GetLastDiagnosticText(); }
    const ScriptModDefinition &GetDefinition() const { return m_Definition; }
    std::string GetRootDirectoryUtf8() const;
    std::string ResolveResourcePathUtf8(const std::string &relativePath) const;
    bool ModFileExistsUtf8(const std::string &relativePath) const;
    bool ModDirectoryExistsUtf8(const std::string &relativePath) const;
    std::string ReadModTextFileUtf8(const std::string &relativePath, const std::string &defaultValue) const;
    void LogInfo(const std::string &message);
    void LogWarn(const std::string &message);
    void LogError(const std::string &message);
    bool HasConfigCategory(const std::string &category);
    bool HasConfigKey(const std::string &category, const std::string &key);
    IProperty *GetConfigProperty(const std::string &category, const std::string &key);
    void SetConfigCategoryComment(const std::string &category, const std::string &comment);
    void SetLoadFailure(const std::string &diagnostic);
    void SetLoadFailure(const ScriptDiagnostic &diagnostic);
    void RecordScriptDiagnostic(const ScriptDiagnostic &diagnostic);
    bool IsInLoadCallback() const { return m_InLoadCallback; }
    ScriptTimerRef *AddScriptTimer(asIScriptObject *timer);
    ScriptCommandRef *RegisterScriptCommand(asIScriptObject *command);
    bool UnregisterScriptCommand(const std::string &name);
    ScriptDataShareRequestRef *RequestScriptDataShare(asIScriptObject *request);
    ScriptModContextView *BorrowContextView() { return &m_ContextView; }
    bool RegisterScriptBallType(const std::string &ballFile,
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
                                float radius);
    bool RegisterScriptFloorType(const std::string &floorName,
                                 float friction,
                                 float elasticity,
                                 float mass,
                                 const std::string &collGroup,
                                 bool enableColl);
    bool RegisterScriptModulBall(const std::string &modulName,
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
                                 float radius);
    bool RegisterScriptModulConvex(const std::string &modulName,
                                   bool fixed,
                                   float friction,
                                   float elasticity,
                                   float mass,
                                   const std::string &collGroup,
                                   bool frozen,
                                   bool enableColl,
                                   bool calcMassCenter,
                                   float linearDamp,
                                   float rotDamp);
    bool RegisterScriptTrafo(const std::string &modulName);
    bool RegisterScriptModul(const std::string &modulName);
    bool HasExport(const std::string &name, const std::string &signature = std::string()) const;
    const ScriptExportBinding *ResolveExport(const std::string &name, const std::string &signature = std::string()) const;
    std::string GetExportSignature(const std::string &name) const;
    int GetExportCount() const;
    bool GetExportInfo(int index, std::string &name, std::string &signature) const;
    int CallVoidExport(const std::string &name, const std::string &signature);
    int CallStringExport(const std::string &name,
                         const std::string &signature,
                         const std::string &argument,
                         bool hasArgument,
                         std::string &result);
    int CallExport(const std::string &name, const std::string &signature, BML_CallFrame *frame);
    int CallResolvedExport(const ScriptExportBinding *binding, BML_CallFrame *frame);

private:
    bool CompileAndCreate();
    void CallGameEvent(size_t eventIndex);
    void CleanupFailedLoad();
    void FailIfEventCallFailed(bool ok, const ScriptDiagnostic &diagnostic);
    void Record(const ScriptDiagnostic &diagnostic);
    void Fail(const std::string &diagnostic);
    void Fail(const ScriptDiagnostic &diagnostic);
    bool ReleaseRuntime();
    std::wstring GetEntryPath() const;

    ModContext *m_Context = nullptr;
    ScriptModDefinition m_Definition;
    ScriptModEntry m_Entry;
    ScriptModContextView m_ContextView;
    ScriptModRuntime m_Runtime;
    ScriptModEventRouter m_EventRouter;
    ScriptTimerService m_Timers;
    ScriptCommandService m_Commands;
    ScriptDataShareService m_DataShareRequests;
    ScriptExportTable m_Exports;
    ScriptModState m_State;
    bool m_InLoadCallback = false;
};

bool IsFailedScriptMod(const IMod *mod);

} // namespace BML

#endif
