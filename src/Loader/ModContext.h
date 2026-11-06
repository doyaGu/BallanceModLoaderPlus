#ifndef BML_MODCONTEXT_H
#define BML_MODCONTEXT_H

struct SDL_IOStream;

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BML/BML.h"
#include "BML/Mod.h"
#include "Hooks/InputHook.h"
#include "Loader/IMod.h"

#include "Config/Config.h"
#include "Config/ConfigStore.h"
#include "Api/ConfigApi.h"
#include "Api/TimerApi.h"
#include "Api/InputApi.h"
#include "Api/BuiApi.h"
#include "Api/GuiApi.h"
#include "Loader/ModLoader.h"
#include "DataShare/DataShare.h"
#include "Console/CommandContext.h"
#include "Console/Shell/ShellEnvironment.h"
#include "Api/ObjectRefs.h"
#include "Imc/ImcRuntime.h"
#include "UI/GameFontCatalog.h"
#include "ModMenu/ModMenuPages.h"
#include "Behavior/Runtime.h"
#include "Behavior/Script.h"
#include "Behavior/Sessions.h"
#include "Behavior/Install/Installations.h"
#include "Behavior/PrototypeCatalog.h"
#include "Api/ExecuteBBAdapter.h"
#include "Behavior/Blocks/PhysicsForce.h"
#include "Hooks/HookLifecycle.h"

// Use the same directory ids as the public Mod Context table.
using DirectoryType = BML_LoaderDirectory;

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

// Keeps the private runtime alive for one public API call, including nested calls.
class ModContextLease final {
public:
    ModContextLease();
    ~ModContextLease();

    ModContextLease(const ModContextLease &) = delete;
    ModContextLease &operator=(const ModContextLease &) = delete;

    explicit operator bool() const { return m_Context != nullptr; }
    ModContext *get() const { return m_Context; }
    ModContext *operator->() const { return m_Context; }
    ModContext &operator*() const { return *m_Context; }

private:
    std::shared_lock<std::shared_mutex> m_Lock;
    ModContext *m_Context = nullptr;
};

class ModContext final {
public:
    explicit ModContext(CKContext *context);

    ModContext(const ModContext &rhs) = delete;
    ModContext(ModContext &&rhs) noexcept = delete;

    ~ModContext();

    // A failed shutdown must leave the Context and its Mod DLLs alive.
    static void Destroy(ModContext *context) noexcept;

    ModContext &operator=(const ModContext &rhs) = delete;
    ModContext &operator=(ModContext &&rhs) noexcept = delete;

    bool IsInited() const { return m_Inited; }
    HookSnapshot InspectHooks() const;
    bool AttachRenderHook(CKRenderContext *renderContext);
    bool DetachRenderHook();
    bool Init();
    bool Shutdown();

    // Loader lifecycle operations. These keep service cleanup inside Context;
    // the Loader controls when each stage runs.
    bool RegisterModOwner(const std::string &ownerId) noexcept;
    void StopModWork(IMod *owner) noexcept;
    void RetireFailedModOwner(const std::string &ownerId) noexcept;
    bool PrepareModUnload(const std::string &ownerId);
    void RetireModBehaviorState(const std::string &ownerId) noexcept;
    void CleanupModState(const std::string &ownerId) noexcept;
    void SnapshotConfigMetadata();
    void FlushConfigChanges(bool saveAll = false, bool dispatchNotifications = true);
    bool UnregisterNativeCommands(const void *module) noexcept;
    void ClearCommands();
    void SetModDataPath(const IMod *mod, const std::wstring &root);
    void RemoveModDataPath(const IMod *mod);

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

    int GetModCount();
    IMod *GetMod(int index);
    std::uint64_t GetModGeneration(const IMod *mod) const;
    std::uint64_t GetModRegistryRevision() const;
    IMod *FindMod(const char *id) const;
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
    int RegisterDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch);
    int RegisterOptionalDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch);
    int CheckDependencies(IMod *mod) const;
    int GetDependencyCount(IMod *mod) const;
    int GetDependencyInfo(IMod *mod, int index, char *dependencyId, int idSize,
                          int *major, int *minor, int *patch, int *optional) const;
    int ClearDependencies(IMod *mod);

    template <class T> void RegisterCommand(std::unique_ptr<T> command) {
        std::shared_ptr<T> owner(std::move(command));
        auto record = BML::MakeCommandRecord(owner.get(), owner);
        if (RegisterCallbackCommand(this, std::move(record)) != BML_OK && m_Logger)
            m_Logger->Error("Failed to register built-in command.");
    }

    // Retained callback commands may remove lookup during their own invocation;
    // the registry's lifetime token keeps the active callback alive.
    int RegisterCallbackCommand(const void *registrar, std::shared_ptr<BML::CommandRecord> command);
    BML::CommandContext::UnregisterResult UnregisterCallbackCommand(
        const void *registrar, const char *name);
    bool RetireCallbackCommands(const void *registrar) noexcept;
    bool IsCommandInvocationActiveOnCurrentThread() const {
        return m_CommandInvocationGate.IsCallActiveOnCurrentThread();
    }

    int GetCommandCount() const;
    BML::CommandRecord *GetCommand(int index) const;
    BML::CommandRecord *FindCommand(const char *name) const;
    void ExecuteCommand(const char *cmd);

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
    bool SetCommandEnabled(const void *implementation, bool enabled);
    std::vector<std::string> CompleteCommand(
        const char *name, const std::vector<std::string> &args);

    Config *AddConfig(std::unique_ptr<Config> config);
    bool RemoveConfig(Config *config);
    Config *GetConfig(IMod *mod);
    BML::Api::GuiApi &GetGuiApi() noexcept { return m_GuiApi; }
    BML::Api::BuiApi &GetBuiApi() noexcept { return m_BuiApi; }
    BML::Api::InputApi &GetInputApi() noexcept { return m_InputApi; }
    BML::Api::TimerApi &GetTimerApi() noexcept { return m_TimerApi; }
    BML::Api::ConfigApi &GetConfigApi() noexcept { return m_ConfigApi; }
    bool LoadConfig(Config *config);
    bool SaveConfig(Config *config, bool snapshotModMetadata = true);

    Logger *GetLogger() const {return m_Logger; }
    void WriteLogLine(const std::string &line);

    const wchar_t *GetDirectory(DirectoryType type);
    const char *GetDirectoryUtf8(DirectoryType type);

    // Resolve the directory of a registered Mod by id.
    std::wstring GetModRootDirectory(const char *modId) const;

    BML::CommandContext &GetCommandContext() { return m_CommandContext; }
    BML::Api::CommandApi &GetCommandApi() { return *m_CommandApi; }
    BML_DataShare *GetDataShare(const char *name = nullptr);

    CKContext *GetCKContext() { return m_CKContext; }
    CKRenderContext *GetRenderContext() { return m_CKContext->GetPlayerRenderContext(); }

    CKAttributeManager *GetAttributeManager() { return m_AttributeManager; }
    CKBehaviorManager *GetBehaviorManager() { return m_BehaviorManager; }
    CKCollisionManager *GetCollisionManager() { return m_CollisionManager; }
    InputHook *GetInputManager() { return m_Hooks ? m_Hooks->GetInput() : nullptr; }
    CKMessageManager *GetMessageManager() { return m_MessageManager; }
    CKPathManager *GetPathManager() { return m_PathManager; }
    CKParameterManager *GetParameterManager() { return m_ParameterManager; }
    CKRenderManager *GetRenderManager() { return m_RenderManager; }
    CKSoundManager *GetSoundManager() { return m_SoundManager; }
    CKTimeManager *GetTimeManager() { return m_TimeManager; }

    CKDataArray *GetArrayByName(const char *name) {
        return (CKDataArray *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_DATAARRAY);
    }
    CKGroup *GetGroupByName(const char *name) {
        return (CKGroup *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_GROUP);
    }
    CKMaterial *GetMaterialByName(const char *name) {
        return (CKMaterial *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_MATERIAL);
    }
    CKMesh *GetMeshByName(const char *name) {
        return (CKMesh *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_MESH);
    }
    CK2dEntity *Get2dEntityByName(const char *name) {
        return (CK2dEntity *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_2DENTITY);
    }
    CK3dEntity *Get3dEntityByName(const char *name) {
        return (CK3dEntity *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_3DENTITY);
    }
    CK3dObject *Get3dObjectByName(const char *name) {
        return (CK3dObject *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_3DOBJECT);
    }
    CKCamera *GetCameraByName(const char *name) {
        return (CKCamera *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_CAMERA);
    }
    CKTargetCamera *GetTargetCameraByName(const char *name) {
        return (CKTargetCamera *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_TARGETCAMERA);
    }
    CKLight *GetLightByName(const char *name) {
        return (CKLight *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_LIGHT);
    }
    CKTargetLight *GetTargetLightByName(const char *name) {
        return (CKTargetLight *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_TARGETLIGHT);
    }
    CKSound *GetSoundByName(const char *name) {
        return (CKSound *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_SOUND);
    }
    CKTexture *GetTextureByName(const char *name) {
        return (CKTexture *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_TEXTURE);
    }
    CKBehavior *GetScriptByName(const char *name) {
        return (CKBehavior *)m_CKContext->GetObjectByNameAndClass((CKSTRING) name, CKCID_BEHAVIOR);
    }

    void SetIC(CKBeObject *obj, bool hierarchy = false);
    void RestoreIC(CKBeObject *obj, bool hierarchy = false);
    void Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy = false);

    void AddTimer(CKDWORD delay, std::function<void()> callback);
    void AddTimerLoop(CKDWORD delay, std::function<bool()> callback);
    void AddTimer(float delay, std::function<void()> callback);
    void AddTimerLoop(float delay, std::function<bool()> callback);

    void ExitGame();

    BML::ObjectRefs &ObjectRefs() noexcept { return m_ObjectRefs; }
    BML::Behavior::Internal::Sessions &BehaviorSessions() noexcept {
        return m_BehaviorSessions;
    }
    BML::Behavior::Internal::Scripts &BehaviorScripts() noexcept {
        return m_BehaviorScripts;
    }
    BML::Behavior::Internal::Installations &BehaviorInstallations() noexcept {
        return m_BehaviorInstallations;
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
    bool IsIngame() { return m_GamePhase != GamePhase::FrontEnd; }
    bool IsInLevel() const { return m_GamePhase == GamePhase::ActiveLevel || m_GamePhase == GamePhase::PausedLevel; }
    bool IsPaused() { return m_GamePhase == GamePhase::PausedLevel; }
    bool IsPlaying() { return m_GamePhase == GamePhase::ActiveLevel; }

    void OpenModsMenu();
    void CloseModsMenu();
    void OpenMapMenu();
    void CloseMapMenu();

    bool IsCheatEnabled() { return m_CommandContext.IsCheatEnabled(); }
    void EnableCheat(bool enable);

    void SendIngameMessage(const char *msg);
    void ClearIngameMessages();

    float GetSRScore();
    int GetHSScore();
    int GetHUD();
    void SetHUD(int mode);
    void ShowTitle(bool show);
    void ShowFPS(bool show);
    void ShowSRTimer(bool show);
    void StartSRTimer();
    void PauseSRTimer();
    void ResetSRTimer();
    float GetSRTime();

    void SkipRenderForNextTick();

    void RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                          float friction, float elasticity,
                          float mass, const char *collGroup, float linearDamp, float rotDamp, float force,
                          float radius);
    void RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                           const char *collGroup, bool enableColl);
    void RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                           const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                           float linearDamp, float rotDamp, float radius);
    void RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                             const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                             float linearDamp, float rotDamp);
    void RegisterTrafo(const char *modulName);
    void RegisterModul(const char *modulName);

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

    void OnPreStartMenu();
    void OnPostStartMenu();

    void OnExitGame();

    void OnPreLoadLevel();
    void OnPostLoadLevel();

    void OnStartLevel();

    void OnPreResetLevel();
    void OnPostResetLevel();

    void OnPauseLevel();
    void OnUnpauseLevel();

    void OnPreExitLevel();
    void OnPostExitLevel();

    void OnPreNextLevel();
    void OnPostNextLevel();

    void OnDead();

    void OnPreEndLevel();
    void OnPostEndLevel();

    void OnCounterActive();
    void OnCounterInactive();

    void OnBallNavActive();
    void OnBallNavInactive();

    void OnCamNavActive();
    void OnCamNavInactive();

    void OnBallOff();

    void OnPreCheckpointReached();
    void OnPostCheckpointReached();

    void OnLevelFinish();

    void OnGameOver();

    void OnExtraPoint();

    void OnPreSubLife();
    void OnPostSubLife();

    void OnPreLifeUp();
    void OnPostLifeUp();

private:
    void Abandon() noexcept;
    bool CloseApiAccess() noexcept;
    void InitDirectories();
    void InitLogger();
    void ShutdownLogger();
    bool InitHooks();
    bool ShutdownHooks();
    bool GetManagers();


    class ShellDispatcher;
    void WriteShellError(std::string_view message);
    std::wstring GetShellEnvironmentPath() const;
    void LoadShellEnvironment();

    enum class BehaviorRetirement {
        // Runs every step and reports the first failure to the caller.
        Report,
        // Runs every step and logs each failure for the Mod.
        Log,
        // Logs the first failed step and stops there, so the owner stays
        // registered and its unload can be retried.
        Unload,
    };
    BML::Behavior::Internal::Status RetireBehaviorState(
        const std::string &ownerId, BehaviorRetirement mode) noexcept;
    bool CleanupModRegistrations(const std::string &ownerId) noexcept;
    bool CanScheduleTimer() const;
    using ModDataPathKey = std::pair<int, std::string>;
    struct ModDataPathRegistration {
        std::size_t Owners = 0;
        bool Added = false;
    };
    void ReleaseModDataPath(const ModDataPathKey &path);
    std::map<ModDataPathKey, ModDataPathRegistration> m_ModDataPathRegistrations;
    std::unordered_map<const IMod *, std::array<ModDataPathKey, 5>> m_ModDataPaths;
    bool m_Inited = false;
    enum class GamePhase { FrontEnd, Transitioning, ActiveLevel, PausedLevel };
    GamePhase m_GamePhase = GamePhase::FrontEnd;
    BML::ObjectRefs m_ObjectRefs;
    BML::Behavior::Internal::PrototypeCatalog m_BehaviorPrototypes;
    BML::Behavior::Internal::Runtime m_Behaviors;
    BML::Behavior::Internal::Sessions m_BehaviorSessions;
    BML::Behavior::Internal::Installations m_BehaviorInstallations;
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

    struct SDL_IOStream *m_LogStream = nullptr;
    Logger *m_Logger = nullptr;

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

    std::unique_ptr<HookLifecycle> m_Hooks;

    BML::ImcRuntime m_ImcRuntime;
    ConfigStore m_ConfigStore;
    std::vector<std::unique_ptr<Config>> m_DetachedConfigs;

    const std::thread::id m_MainThreadId = std::this_thread::get_id();

    mutable BML::ModInvocationGate m_CommandInvocationGate;
    mutable std::mutex m_CommandMutex;
    mutable std::mutex m_ConfigMutex;
    BML::Api::GuiApi m_GuiApi;
    BML::Api::BuiApi m_BuiApi;
    BML::Api::InputApi m_InputApi;
    BML::Api::TimerApi m_TimerApi;
    BML::Api::ConfigApi m_ConfigApi;
    ModLoader m_Loader;
};

#endif // BML_MODCONTEXT_H
