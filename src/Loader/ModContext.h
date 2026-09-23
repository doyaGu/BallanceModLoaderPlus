#ifndef BML_MODCONTEXT_H
#define BML_MODCONTEXT_H

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "BML/BML.h"
#include "BML/IBML.h"
#include "BML/IMod.h"

#include "Config/Config.h"
#include "Config/ConfigStore.h"
#include "Loader/ModLoader.h"
#include "DataShare/DataShare.h"
#include "Console/CommandContext.h"
#include "Console/Shell/ShellEnvironment.h"
#include "Api/ObjectRefs.h"
#include "Imc/ImcRuntime.h"
#include "Gameplay/GameSession.h"
#include "UI/GameFontCatalog.h"
#include "ModMenu/ModMenuPages.h"
#include "Behavior/Runtime.h"
#include "Behavior/Script.h"
#include "Behavior/Sessions.h"
#include "Behavior/Patches.h"
#include "Behavior/Plan.h"
#include "Behavior/PrototypeCatalog.h"
#include "Api/ExecuteBBAdapter.h"
#include "Behavior/Blocks/PhysicsForce.h"

// The ids themselves are public, since BML_GetLoaderPath takes them. This name
// stays for the loader's own call sites and for the script binding.
typedef BML_LoaderDirectory DirectoryType;

class ModContext;

namespace BML::Shell {
    class Executor;
    class OutputSink;
}
namespace BML {
namespace Api {
class CommandApi;
}
namespace UI {
class FontRuntime;
}

#if BML_ENABLE_ANGELSCRIPT
class ScriptMod;
struct ScriptModDefinition;
class ScriptDevToolsService;
struct ScriptModReloadDiagnosticField;
#endif
}

ModContext *BML_GetModContext();
CKContext *BML_GetCKContext();
CKRenderContext *BML_GetRenderContext();

class ModContext final : public IBML {
    friend class BML::Api::CommandApi;
    friend class ModLoader;

public:
    explicit ModContext(CKContext *context);

    ModContext(const ModContext &rhs) = delete;
    ModContext(ModContext &&rhs) noexcept = delete;

    ~ModContext() override;

    ModContext &operator=(const ModContext &rhs) = delete;
    ModContext &operator=(ModContext &&rhs) noexcept = delete;

    bool IsInited() const { return m_Inited; }
    bool Init();
    void Shutdown();

    ModLoader &GetModLoader() { return m_Loader; }
    const ModLoader &GetModLoader() const { return m_Loader; }
    bool AreModsLoaded() const { return m_Loader.AreModsLoaded(); }
    bool AreModsInited() const { return m_Loader.AreModsInited(); }

#if BML_ENABLE_ANGELSCRIPT
    bool IsAngelScriptExtensionRegistered() const { return m_AngelScriptExtensionRegistered; }
    void SetAngelScriptExtensionRegistered(bool registered) { m_AngelScriptExtensionRegistered = registered; }
    bool AreAngelScriptBindingsRegistered() const { return m_AngelScriptBindingsRegistered; }
    void SetAngelScriptBindingsRegistered(bool registered) { m_AngelScriptBindingsRegistered = registered; }
    bool ValidateScriptModReloadDependencies(const BML::ScriptMod *mod,
                                             const BML::ScriptModDefinition &candidate,
                                             std::string &diagnostic,
                                             std::vector<BML::ScriptModReloadDiagnosticField> *fields = nullptr) const;
    bool PromoteFailedScriptModPlaceholder(BML::ScriptMod *mod,
                                           const std::string &oldId,
                                           const BML::ScriptModDefinition &candidate,
                                           std::string &diagnostic);
    void RestoreFailedScriptModPlaceholder(BML::ScriptMod *mod,
                                           const std::string &currentId,
                                           const BML::ScriptModDefinition &oldDefinition);
    BML::ScriptDevToolsService *GetScriptDevTools() const { return m_Loader.GetScriptDevTools(); }
#endif

    int GetModCount() override;
    IMod *GetMod(int index) override;
    std::uint64_t GetModGeneration(const IMod *mod) const;
    std::uint64_t GetModRegistryRevision() const;
    IMod *FindMod(const char *id) const override;
    std::string GetNativeImcOwnerId(
        const void *callerAddress,
        const char *requestedOwnerId = nullptr) const;
    std::string GetNativeModOwnerId(
        const void *callerAddress,
        const char *requestedOwnerId = nullptr) const;
    bool NativeModOwnsAddress(
        const std::string &ownerId, const void *address) const;
    BML::ModInvocationGate::CallLock LockModInvocation() const { return m_Loader.LockModInvocation(); }
    bool IsModInvocationActiveOnCurrentThread() const {
        return m_Loader.IsModInvocationActiveOnCurrentThread();
    }
    bool IsMainThread() const { return std::this_thread::get_id() == m_MainThreadId; }
    BML::ImcRuntime &GetImcRuntime() { return m_ImcRuntime; }
    const BML::ImcRuntime &GetImcRuntime() const { return m_ImcRuntime; }
    ModMenuPages &GetModMenuPages() { return m_ModMenuPages; }
    const ModMenuPages &GetModMenuPages() const { return m_ModMenuPages; }
    int RegisterDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) override;
    int RegisterOptionalDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) override;
    int CheckDependencies(IMod *mod) const override;
    int GetDependencyCount(IMod *mod) const override;
    int GetDependencyInfo(IMod *mod, int index, char *dependencyId, int idSize,
                          int *major, int *minor, int *patch, int *optional) const override;
    int ClearDependencies(IMod *mod) override;

    void RegisterCommand(ICommand *cmd) override;

    // Removes a command again, but only for the module that registered it.
    // callerAddress belongs to the DLL asking, and has to be the one that called
    // RegisterCommand for this command. Answers BML_OK,
    // BML_ERROR_INVALID_PARAMETER for a null or empty name, BML_ERROR_NOT_FOUND
    // when no command answers to the name, or BML_ERROR_ACCESS_DENIED when one
    // does but another module owns it.
    int UnregisterCommand(const void *callerAddress, const char *name);

    int GetCommandCount() const override;
    ICommand *GetCommand(int index) const override;
    ICommand *FindCommand(const char *name) const override;
    void ExecuteCommand(const char *cmd) override;

    // Runs one line through the console shell (quoting, ; && || and |, $NAME,
    // $(...), aliases) and returns the status of its last pipeline. Diagnostics go
    // to the message board. Game thread only.
    int ExecuteCommandLine(const char *line);

    // Runs one already-split command with no shell parsing: lookup, cheat gate,
    // OnPre/OnPostCommandExecute, Execute. input is the text piped into it or
    // null. Returns a BML::Shell::Status value. Game thread only.
    int InvokeCommandArgs(const std::vector<std::string> &args, const std::string *input = nullptr);

    BML::Shell::Environment &GetShellEnvironment() { return m_ShellEnvironment; }
    // Writes universal variables and aliases to disk when they changed.
    void SaveShellEnvironment();

    std::vector<BML::CommandContext::CommandInfo> GetCommandSnapshot() const;
    bool GetCommandInfo(int index, BML::CommandContext::CommandInfo &info) const;
    bool FindCommandInfo(const char *name, BML::CommandContext::CommandInfo &info) const;
    bool SetCommandEnabled(ICommand *command, bool enabled);
    std::vector<std::string> CompleteCommand(
        const char *name, const std::vector<std::string> &args);

    Config *AddConfig(std::unique_ptr<Config> config);
    bool RemoveConfig(Config *config);
    Config *GetConfig(IMod *mod);
    bool LoadConfig(Config *config);
    bool SaveConfig(Config *config, bool snapshotModMetadata = true);

    ILogger *GetLogger() const {return m_Logger; }
    FILE *GetLogFile() const { return m_Logfile; }

    const wchar_t *GetDirectory(DirectoryType type);
    const char *GetDirectoryUtf8(DirectoryType type);

    // The directory a Mod is installed in. A null or empty modId asks about the DLL
    // that callerAddress belongs to, which needs no Mod registration and so answers
    // during BMLEntry as well. A modId asks about that Mod: a native Mod answers
    // with the directory of its DLL, a script Mod with its script root. Empty when
    // nothing could be resolved.
    std::wstring GetModRootDirectory(const void *callerAddress, const char *modId) const;

    BML::CommandContext &GetCommandContext() { return m_CommandContext; }
    BML::Api::CommandApi &GetCommandApi() { return *m_CommandApi; }
    BML_DataShare *GetDataShare(const char *name = nullptr);

    CKContext *GetCKContext() override { return m_CKContext; }
    CKRenderContext *GetRenderContext() override { return m_CKContext->GetPlayerRenderContext(); }

    CKAttributeManager *GetAttributeManager() override { return m_AttributeManager; }
    CKBehaviorManager *GetBehaviorManager() override { return m_BehaviorManager; }
    CKCollisionManager *GetCollisionManager() override { return m_CollisionManager; }
    InputHook *GetInputManager() override { return m_InputHook; }
    CKMessageManager *GetMessageManager() override { return m_MessageManager; }
    CKPathManager *GetPathManager() override { return m_PathManager; }
    CKParameterManager *GetParameterManager() override { return m_ParameterManager; }
    CKRenderManager *GetRenderManager() override { return m_RenderManager; }
    CKSoundManager *GetSoundManager() override { return m_SoundManager; }
    CKTimeManager *GetTimeManager() override { return m_TimeManager; }

    CKDataArray *GetArrayByName(const char *name) override {
        return (CKDataArray *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_DATAARRAY);
    }
    CKGroup *GetGroupByName(const char *name) override {
        return (CKGroup *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_GROUP);
    }
    CKMaterial *GetMaterialByName(const char *name) override {
        return (CKMaterial *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_MATERIAL);
    }
    CKMesh *GetMeshByName(const char *name) override {
        return (CKMesh *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_MESH);
    }
    CK2dEntity *Get2dEntityByName(const char *name) override {
        return (CK2dEntity *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_2DENTITY);
    }
    CK3dEntity *Get3dEntityByName(const char *name) override {
        return (CK3dEntity *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_3DENTITY);
    }
    CK3dObject *Get3dObjectByName(const char *name) override {
        return (CK3dObject *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_3DOBJECT);
    }
    CKCamera *GetCameraByName(const char *name) override {
        return (CKCamera *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_CAMERA);
    }
    CKTargetCamera *GetTargetCameraByName(const char *name) override {
        return (CKTargetCamera *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_TARGETCAMERA);
    }
    CKLight *GetLightByName(const char *name) override {
        return (CKLight *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_LIGHT);
    }
    CKTargetLight *GetTargetLightByName(const char *name) override {
        return (CKTargetLight *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_TARGETLIGHT);
    }
    CKSound *GetSoundByName(const char *name) override {
        return (CKSound *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_SOUND);
    }
    CKTexture *GetTextureByName(const char *name) override {
        return (CKTexture *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_TEXTURE);
    }
    CKBehavior *GetScriptByName(const char *name) override {
        return (CKBehavior *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_BEHAVIOR);
    }

    void SetIC(CKBeObject *obj, bool hierarchy) override;
    void RestoreIC(CKBeObject *obj, bool hierarchy) override;
    void Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy) override;

    void AddTimer(CKDWORD delay, std::function<void()> callback) override;
    void AddTimerLoop(CKDWORD delay, std::function<bool()> callback) override;
    void AddTimer(float delay, std::function<void()> callback) override;
    void AddTimerLoop(float delay, std::function<bool()> callback) override;

    void ExitGame() override;

    BML::GameSessionSnapshot ReadGameSession() const noexcept { return m_GameSession.Read(); }
    BML::ObjectRefs &ObjectRefs() noexcept { return m_ObjectRefs; }
    BML::Behavior::Internal::Runtime &Behaviors() noexcept { return m_Behaviors; }
    const BML::Behavior::Internal::Runtime &Behaviors() const noexcept { return m_Behaviors; }
    BML::Behavior::Internal::Sessions &BehaviorSessions() noexcept {
        return m_BehaviorSessions;
    }
    BML::Behavior::Internal::Scripts &BehaviorScripts() noexcept {
        return m_BehaviorScripts;
    }
    BML::Behavior::Internal::Patches &BehaviorPatches() noexcept {
        return m_BehaviorPatches;
    }
    // The owner the Loader's built-in modules edit game scripts under: the BML
    // Mod's active generation, or an empty owner before that Mod is registered.
    BML::Behavior::Internal::SessionOwner LoaderBehaviorOwner() const;
    BML::Behavior::Internal::Plans &BehaviorPlans() noexcept {
        return m_BehaviorPlans;
    }
    BML::Behavior::Internal::PrototypeCatalog &BehaviorPrototypes() noexcept {
        return m_BehaviorPrototypes;
    }
    BML::ExecuteBBAdapter &ExecuteBB() noexcept { return m_ExecuteBB; }
    BML::Behavior::Internal::PhysicsForce::Sessions &PhysicsForce() noexcept {
        return m_PhysicsForce;
    }
    void VirtoolsObjectsToBeDeleted(const CK_ID *ids, int count);
    void BehaviorScriptLoaded(CKBehavior *script);
    void ProcessVirtoolsFrame();
    void ResetVirtoolsWorld();
    BML::Behavior::Internal::Status RetireBehaviorOwner(
        const std::string &ownerId);
    BML::GameFontCatalog &GetGameFonts() noexcept { return m_GameFonts; }
    const BML::GameFontCatalog &GetGameFonts() const noexcept { return m_GameFonts; }
    BML::UI::FontRuntime *GetUiFontRuntime() noexcept {
        return m_UiFonts.get();
    }
    const BML::UI::FontRuntime *GetUiFontRuntime() const noexcept {
        return m_UiFonts.get();
    }
    bool IsIngame() override { return ReadGameSession().IsInGame(); }
    bool IsInLevel() const { return ReadGameSession().IsInLevel(); }
    bool IsPaused() override { return ReadGameSession().IsPaused(); }
    bool IsPlaying() override { return ReadGameSession().IsPlaying(); }

    void OpenModsMenu();
    void CloseModsMenu();
    void OpenMapMenu();
    void CloseMapMenu();

    bool IsCheatEnabled() override { return m_CommandContext.IsCheatEnabled(); }
    void EnableCheat(bool enable) override;

    void SendIngameMessage(const char *msg) override;
    void ClearIngameMessages();

    float GetSRScore() override;
    int GetHSScore() override;
    int GetHUD();
    void SetHUD(int mode);
    void ShowTitle(bool show);
    void ShowFPS(bool show);
    void ShowSRTimer(bool show);
    void StartSRTimer();
    void PauseSRTimer();
    void ResetSRTimer();
    float GetSRTime();

    void SkipRenderForNextTick() override;

    void RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                          float friction, float elasticity,
                          float mass, const char *collGroup, float linearDamp, float rotDamp, float force,
                          float radius) override;
    void RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                           const char *collGroup, bool enableColl) override;
    void RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                           const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                           float linearDamp, float rotDamp, float radius) override;
    void RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                             const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                             float linearDamp, float rotDamp) override;
    void RegisterTrafo(const char *modulName) override;
    void RegisterModul(const char *modulName) override;

    template<typename T, typename... Args>
    std::enable_if_t<std::is_member_function_pointer<T>::value, void> BroadcastCallback(T callback, Args&&... args) {
        m_Loader.BroadcastCallback(callback, std::forward<Args>(args)...);
    }

    template<typename T>
    std::enable_if_t<std::is_member_function_pointer<T>::value, void> BroadcastMessage(const char *msg, T func) {
        m_Loader.BroadcastMessage(msg, func);
    }

    void OnProcess();
    void OnRender(CKRenderContext *dev);

    void OnLoadGame();

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

private:
    void InitDirectories();
    void InitLogger();
    void ShutdownLogger();
    bool InitHooks();
    bool ShutdownHooks();
    bool GetManagers();

    bool RegisterOwnedCommand(const void *registrar, ICommand *command);
    BML::CommandContext::UnregisterResult UnregisterOwnedCommand(
        const void *registrar, const char *name);

    class ShellDispatcher;
    void WriteShellError(std::string_view message);
    std::wstring GetShellEnvironmentPath() const;
    void LoadShellEnvironment();

    void SnapshotConfigMetadata();
    void FlushConfigChanges(bool saveAll = false, bool dispatchNotifications = true);
    bool RegisterModOwner(const std::string &ownerId) noexcept;
    void RetireFailedModOwner(const std::string &ownerId) noexcept;
    BML::Behavior::Internal::Status RetireBehaviorEdits(const std::string &ownerId);
    void RetireModBehaviorState(const std::string &ownerId) noexcept;
    bool CleanupModRegistrations(const std::string &ownerId) noexcept;
    void CleanupModState(const std::string &ownerId) noexcept;
    void ClearLegacyCommands();
    void AddDataPath(const char *path);
    bool CanScheduleTimer() const;
    bool m_Inited = false;
    BML::GameSession m_GameSession;
    BML::ObjectRefs m_ObjectRefs;
    BML::Behavior::Internal::PrototypeCatalog m_BehaviorPrototypes;
    BML::Behavior::Internal::Runtime m_Behaviors;
    BML::Behavior::Internal::Sessions m_BehaviorSessions;
    BML::Behavior::Internal::Patches m_BehaviorPatches;
    BML::Behavior::Internal::Plans m_BehaviorPlans;
    BML::Behavior::Internal::Scripts m_BehaviorScripts;
    BML::Behavior::Internal::PhysicsForce::Sessions m_PhysicsForce;
    BML::ExecuteBBAdapter m_ExecuteBB;
    BML::GameFontCatalog m_GameFonts;
    ModMenuPages m_ModMenuPages;
    std::unique_ptr<BML::UI::FontRuntime> m_UiFonts;
#if BML_ENABLE_ANGELSCRIPT
    bool m_AngelScriptExtensionRegistered = false;
    bool m_AngelScriptBindingsRegistered = false;
#endif

    std::wstring m_WorkingDir;
    std::wstring m_TempDir;
    std::wstring m_GameDir;
    std::wstring m_LoaderDir;
    std::wstring m_ConfigDir;

    std::string m_WorkingDirUtf8;
    std::string m_TempDirUtf8;
    std::string m_GameDirUtf8;
    std::string m_LoaderDirUtf8;
    std::string m_ConfigDirUtf8;

    BML::CommandContext m_CommandContext;
    std::unique_ptr<BML::Api::CommandApi> m_CommandApi;
    std::unique_ptr<ShellDispatcher> m_ShellDispatcher;
    std::unique_ptr<BML::Shell::Executor> m_Shell;
    BML::Shell::Environment m_ShellEnvironment;
    std::vector<BML::Shell::OutputSink *> m_OutputSinks;
    BML::DataShareStore *m_DataShare = nullptr;

    FILE *m_Logfile = nullptr;
    ILogger *m_Logger = nullptr;

    CKContext *m_CKContext = nullptr;

    CKAttributeManager *m_AttributeManager = nullptr;
    CKBehaviorManager *m_BehaviorManager = nullptr;
    CKCollisionManager *m_CollisionManager = nullptr;
    CKInputManager *m_InputManager = nullptr;
    CKMessageManager *m_MessageManager = nullptr;
    CKPathManager *m_PathManager = nullptr;
    CKParameterManager *m_ParameterManager = nullptr;
    CKRenderManager *m_RenderManager = nullptr;
    CKSoundManager *m_SoundManager = nullptr;
    CKTimeManager *m_TimeManager = nullptr;

    InputHook *m_InputHook = nullptr;

    BML::ImcRuntime m_ImcRuntime;
    ConfigStore m_ConfigStore;

    const std::thread::id m_MainThreadId = std::this_thread::get_id();

    mutable BML::ModInvocationGate m_CommandInvocationGate;
    mutable std::mutex m_Mutex;
    ModLoader m_Loader;
};

#endif // BML_MODCONTEXT_H
