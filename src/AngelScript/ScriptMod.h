#ifndef BML_SCRIPTMOD_H
#define BML_SCRIPTMOD_H

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
class asIScriptFunction;
class asIScriptObject;

namespace BML {

class ScriptMod;

class ScriptModReloadPhaseScope;
struct ScriptModReloadOptions {
    bool Automatic = false;
    bool ForceExports = false;
    bool DryRun = false;
};

struct ScriptModReloadDiagnosticField {
    std::string Key;
    std::string Value;
};

struct ScriptModReloadResult {
    bool Success = false;
    bool RetryLater = false;
    unsigned int ReloadAttemptId = 0;
    std::string Diagnostic;
    std::string SourcePath;
    std::vector<ScriptModReloadDiagnosticField> Fields;
};

struct ScriptModHostRegistration {
    std::string Kind;
    std::string Key;
};

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
    bool IsFailedPlaceholder() const;
    bool IsReloading() const { return m_Reloading.load(); }
    bool CanHotReloadNow() const;
    const std::string &GetLastDiagnostic() const { return m_State.GetLastDiagnosticText(); }
    const ScriptDiagnostic &GetLastDiagnosticInfo() const { return m_State.GetLastDiagnostic(); }
    const std::string &GetLastReloadDiagnostic() const { return m_LastReloadDiagnostic; }
    const ScriptDiagnostic &GetLastReloadDiagnosticInfo() const { return m_LastReloadDiagnosticInfo; }
    const ScriptModDefinition &GetDefinition() const { return m_Definition; }
    const ScriptModEntry &GetEntry() const { return m_Entry; }
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
    ScriptTimerRef *AddScriptTimeoutTicks(unsigned int delayTicks, asIScriptFunction *callback, const std::string &name);
    ScriptTimerRef *AddScriptTimeoutMs(float delayMs, asIScriptFunction *callback, const std::string &name);
    ScriptTimerRef *AddScriptIntervalTicks(unsigned int delayTicks, asIScriptFunction *callback, const std::string &name);
    ScriptTimerRef *AddScriptIntervalMs(float delayMs, asIScriptFunction *callback, const std::string &name);
    ScriptCommandRef *RegisterScriptCommand(asIScriptObject *command);
    ScriptCommandRef *RegisterScriptCommand(const ScriptCommandDefinition &definition,
                                            asIScriptFunction *execute,
                                            asIScriptFunction *complete);
    bool UnregisterScriptCommand(const std::string &name);
    ScriptDataShareRequestRef *RequestScriptDataShare(asIScriptObject *request);
    ScriptDataShareRequestRef *RequestScriptDataShare(const std::string &key,
                                                      int type,
                                                      asIScriptFunction *callback,
                                                      const std::string &name);
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
    void GetExportSignatures(const std::string &name, std::vector<std::string> &out) const;
    int GetExportCount() const;
    bool GetExportInfo(int index, std::string &name, std::string &signature) const;
    int CallVoidExport(const std::string &name, const std::string &signature);
    int CallStringExport(const std::string &name,
                         const std::string &signature,
                         const std::string &argument,
                         bool hasArgument,
                         std::string &result);
    int CallExport(const std::string &name, const std::string &signature, BML_CallFrame *frame);
    ScriptModReloadResult TryHotReload(const ScriptModReloadOptions &options);
    ScriptModReloadResult TryHotReloadDryRun(const ScriptModReloadOptions &options);
    void ProcessQueuedScriptServiceCallbacks();
    void ProcessFailureCleanup();
    bool CanDispatchScriptServiceCallback();
    bool EnterScriptCall() const;
    void LeaveScriptCall() const;
    unsigned int GetModGeneration() const { return m_ModGeneration.load(std::memory_order_acquire); }
    unsigned int GetRuntimeGeneration() const { return m_RuntimeGeneration.load(std::memory_order_acquire); }
    unsigned int GetReloadAttemptId() const { return m_ReloadAttemptId.load(std::memory_order_acquire); }
    size_t GetActiveTimerCount() const { return m_Timers.GetActiveCount(); }
    size_t GetActiveCommandCount() const { return m_Commands.GetActiveCount(); }
    size_t GetActiveDataShareRequestCount() const { return m_DataShareRequests.GetActiveCount(); }
    size_t GetQueuedScriptServiceCallbackCount() const;
    size_t GetHostRegistrationCount() const { return m_HostRegistrations.size(); }
    int GetActiveScriptCallCount() const;
    void GetCallbackNames(std::vector<std::string> &out) const { m_EventRouter.GetCallbackNames(out); }

private:
    enum class HostRegistrationMode {
        Capture,
        Validate,
    };

    bool CompileAndCreate();
    bool LoadCurrentRuntime(bool validateHostRegistrations, bool failedLoadRecovery = false);
    void CallGameEvent(size_t eventIndex);
    void CleanupFailedLoad();
    void FailIfEventCallFailed(bool ok, const ScriptDiagnostic &diagnostic);
    void Record(const ScriptDiagnostic &diagnostic);
    void Fail(const std::string &diagnostic);
    void Fail(const ScriptDiagnostic &diagnostic);
    void ScheduleFailureCleanup();
    bool ReleaseScriptServices();
    bool ReleaseScriptMethodHandles();
    ScriptModReloadPhase GetReloadPhase() const {
        return static_cast<ScriptModReloadPhase>(m_ReloadPhase.load(std::memory_order_acquire));
    }
    bool ReleaseRuntime();
    bool ReleaseRuntimeOnly(ScriptModRuntime &runtime);
    void RebindServices();
    bool CanDispatchScriptCallback();
    void FenceCallbacksForCurrentFrame();
    void SetReloadDiagnostic(const std::string &diagnostic);
    void SetReloadDiagnostic(const ScriptDiagnostic &diagnostic);
    ScriptDiagnostic RewriteRuntimeDiagnosticPaths(ScriptDiagnostic diagnostic) const;
    void SetRuntimeDiagnosticPathRewrite(std::string from, std::string to);
    void TouchModGeneration();
    void TouchRuntimeGeneration();
    void TouchReloadAttempt();
    bool ValidateReloadDefinition(const ScriptModDefinition &candidate,
                                  const ScriptExportTable &candidateExports,
                                  const ScriptModReloadOptions &options,
    friend class ScriptModReloadPhaseScope;

                                  std::string &diagnostic,
                                  std::vector<ScriptModReloadDiagnosticField> *fields) const;
    bool ValidateHostRegistrationSet(std::string &diagnostic, bool failedLoadRecovery);
    bool NoteHostRegistration(const char *kind, const std::string &key);
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
    mutable std::mutex m_ReloadMutex;
    mutable int m_ActiveScriptCalls = 0;
    mutable std::thread::id m_ReloadThreadId;
    std::atomic<bool> m_Reloading{false};
    std::atomic<bool> m_PendingFailureCleanup{false};
    void SetReloadPhase(ScriptModReloadPhase phase);
    std::atomic<bool> m_CallbackFenceActive{false};
    std::atomic<unsigned int> m_CallbackFenceFrame{0};
    std::string m_LastReloadDiagnostic;
    ScriptDiagnostic m_LastReloadDiagnosticInfo;
    std::string m_RuntimeDiagnosticPathFrom;
    std::string m_RuntimeDiagnosticPathTo;
    std::atomic<unsigned int> m_ModGeneration{1};
    std::atomic<unsigned int> m_RuntimeGeneration{1};
    std::atomic<unsigned int> m_ReloadAttemptId{0};
    HostRegistrationMode m_HostRegistrationMode = HostRegistrationMode::Capture;
    std::vector<ScriptModHostRegistration> m_HostRegistrations;
    std::vector<ScriptModHostRegistration> m_PendingHostRegistrations;
    std::vector<std::pair<std::string, std::string>> m_ActiveConfigEvents;
};

bool IsFailedScriptMod(const IMod *mod);

} // namespace BML

#endif
    std::atomic<int> m_ReloadPhase{static_cast<int>(ScriptModReloadPhase::None)};
