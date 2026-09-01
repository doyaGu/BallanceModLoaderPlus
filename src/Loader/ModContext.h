#ifndef BML_MODCONTEXT_H
#define BML_MODCONTEXT_H

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "BML/BML.h"
#include "BML/IBML.h"
#include "BML/IMod.h"

#include "Config/Config.h"
#include "Config/ConfigStore.h"
#include "Loader/NativeModRegistry.h"
#include "DataShare/DataShare.hpp"
#include "Console/CommandContext.h"
#include "Api/ObjectRefs.h"
#include "HookUtils.h"
#include "Imc/ImcRuntime.h"
#include "Loader/ModInvocationGate.h"
#include "Gameplay/GameSession.h"
#include "UI/GameFontCatalog.h"
#include "Behavior/Runtime.h"
#include "Behavior/Sessions.h"
#include "Behavior/PrototypeCatalog.h"
#include "Api/ExecuteBBAdapter.h"
#include "Behavior/PhysicsForce.h"

// The ids themselves are public, since BML_GetLoaderPath takes them. This name
// stays for the loader's own call sites and for the script binding.
typedef BML_LoaderDirectory DirectoryType;

class ModContext;
class BMLMod;
class NewBallTypeMod;

namespace BML {
#if BML_ENABLE_ANGELSCRIPT
class ScriptMod;
struct ScriptModDefinition;
struct ScriptModLoadCandidate;
class ScriptDevToolsService;
class ScriptModHotReloadService;
struct ScriptModReloadDiagnosticField;
#endif
}

ModContext *BML_GetModContext();
CKContext *BML_GetCKContext();
CKRenderContext *BML_GetRenderContext();

class ModContext final : public IBML {
public:
    enum Flag {
        BML_INITED = 0x00000001,

        BML_MODS_LOADED = 0x00000010,
        BML_MODS_INITED = 0x00000020,
        BML_MODS_SHUTTING_DOWN = 0x00000040,
    };

    explicit ModContext(CKContext *context);

    ModContext(const ModContext &rhs) = delete;
    ModContext(ModContext &&rhs) noexcept = delete;

    ~ModContext() override;

    ModContext &operator=(const ModContext &rhs) = delete;
    ModContext &operator=(ModContext &&rhs) noexcept = delete;

    bool IsInited() const { return AreFlagsSet(BML_INITED); }
    bool Init();
    void Shutdown();

    bool AreModsLoaded() const { return AreFlagsSet(BML_MODS_LOADED); }
    bool LoadMods();
    void UnloadMods();

    bool AreModsInited() const { return AreFlagsSet(BML_MODS_INITED); }
    bool InitMods();
    void ShutdownMods();

    bool AreFlagsSet(int flags) const { return (m_Flags & flags) == flags; }
    void SetFlags(int flags, bool set = true) { m_Flags = set ? m_Flags | flags : m_Flags & ~flags; }
    void ClearFlags(int flags) { m_Flags &= ~flags; }
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
    BML::ScriptDevToolsService *GetScriptDevTools() const { return m_ScriptDevTools.get(); }
#endif

    int GetModCount() override;
    IMod *GetMod(int index) override;
    IMod *FindMod(const char *id) const override;
    std::string GetNativeImcOwnerId(
        const void *callerAddress,
        const char *requestedOwnerId = nullptr) const;
    std::string GetNativeModOwnerId(
        const void *callerAddress,
        const char *requestedOwnerId = nullptr) const;
    BML::ModInvocationGate::CallLock LockModInvocation() const { return m_ModInvocationGate.LockCall(); }
    bool IsModInvocationActiveOnCurrentThread() const {
        return m_ModInvocationGate.IsCallActiveOnCurrentThread();
    }
    bool IsMainThread() const { return std::this_thread::get_id() == m_MainThreadId; }
    BML::ImcRuntime &GetImcRuntime() { return m_ImcRuntime; }
    const BML::ImcRuntime &GetImcRuntime() const { return m_ImcRuntime; }
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
    std::vector<BML::CommandContext::CommandInfo> GetCommandSnapshot() const;
    bool GetCommandInfo(int index, BML::CommandContext::CommandInfo &info) const;
    bool FindCommandInfo(const char *name, BML::CommandContext::CommandInfo &info) const;
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
    BML::Behavior::Runtime &Behaviors() noexcept { return m_Behaviors; }
    const BML::Behavior::Runtime &Behaviors() const noexcept { return m_Behaviors; }
    BML::Behavior::Sessions &BehaviorSessions() noexcept {
        return m_BehaviorSessions;
    }
    BML::Behavior::PrototypeCatalog &BehaviorPrototypes() noexcept {
        return m_BehaviorPrototypes;
    }
    BML::ExecuteBBAdapter &ExecuteBB() noexcept { return m_ExecuteBB; }
    BML::Behavior::PhysicsForce::Sessions &PhysicsForce() noexcept {
        return m_PhysicsForce;
    }
    void VirtoolsObjectsToBeDeleted(const CK_ID *ids, int count);
    void ProcessVirtoolsFrame();
    void ResetVirtoolsWorld();
    BML::GameFontCatalog &GetGameFonts() noexcept { return m_GameFonts; }
    const BML::GameFontCatalog &GetGameFonts() const noexcept { return m_GameFonts; }
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
        auto invocationLock = LockModInvocation();
        std::vector<IMod *> mods;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_CallbackMap.find(utils::TypeErase(callback));
            if (it != m_CallbackMap.end())
                mods = it->second;
        }
        for (IMod *mod : mods) {
            try {
                (mod->*callback)(std::forward<Args>(args)...);
            } catch (const std::exception &e) {
                if (m_Logger)
                    m_Logger->Error("Exception in mod %s callback: %s", mod->GetID(), e.what());
            } catch (...) {
                if (m_Logger)
                    m_Logger->Error("Unknown exception in mod %s callback", mod->GetID());
            }
        }
    }

    template<typename T>
    std::enable_if_t<std::is_member_function_pointer<T>::value, void> BroadcastMessage(const char *msg, T func) {
        m_Logger->Info("On Message %s", msg);
        BroadcastCallback(func);
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

    size_t ExploreMods(const std::wstring &path, std::vector<std::wstring> &mods);
#if BML_ENABLE_ANGELSCRIPT
    struct ModDependencySnapshot {
        std::string Id;
        BMLVersion MinVersion;
        bool Optional = false;
    };

    struct RegisteredModSnapshot {
        const IMod *Identity = nullptr;
        std::string Id;
        std::string Version;
        bool Failed = false;
        std::vector<ModDependencySnapshot> Dependencies;
    };

    std::vector<RegisteredModSnapshot> SnapshotModRegistry() const;
    size_t ExploreScriptMods(const std::wstring &path, std::vector<BML::ScriptModLoadCandidate> &candidates);
#endif

    std::shared_ptr<void> LoadLib(const wchar_t *path);
    bool UnloadLib(void *dllHandle);
    void DestroyNativeMod(void *dllHandle, IMod *mod, const char *modLabel) noexcept;

    IMod *LoadMod(const std::wstring &path);
#if BML_ENABLE_ANGELSCRIPT
    IMod *LoadScriptMod(const BML::ScriptModLoadCandidate &candidate);
    void RegisterScriptModDependencies(IMod *mod, const BML::ScriptModDefinition &definition);
    void ProcessScriptModQueuedCallbacks();
    void ProcessScriptModFailureCleanup();
#endif
    bool UnloadMod(const std::string &id);

    void RegisterBuiltinMods();

    bool RegisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle = nullptr);
    bool UnregisterMod(IMod *mod);
    IMod *FindModLocked(const std::string &id) const;
    bool RegisterOwnedCommand(const void *registrar, ICommand *command);
    BML::CommandContext::UnregisterResult UnregisterOwnedCommand(
        const void *registrar, const char *name);

    int EvaluateDependencies(IMod *mod, std::string *diagnostic) const;
    int EvaluateActivationDependencies(IMod *mod, std::string *diagnostic) const;
    bool ResolveDependencies();

    void FillCallbackMap(IMod *mod);
    void SnapshotConfigMetadata();
    void FlushConfigChanges(bool saveAll = false, bool dispatchNotifications = true);
    void DeactivateActiveMods(bool dispatchPendingNotifications);
    void RollbackModActivation();

    void AddDataPath(const char *path);
    bool CanScheduleTimer() const;
    int m_Flags = 0;
    BML::GameSession m_GameSession;
    BML::ObjectRefs m_ObjectRefs;
    BML::Behavior::PrototypeCatalog m_BehaviorPrototypes;
    BML::Behavior::Runtime m_Behaviors;
    BML::Behavior::Sessions m_BehaviorSessions;
    BML::Behavior::PhysicsForce::Sessions m_PhysicsForce;
    BML::ExecuteBBAdapter m_ExecuteBB;
    BML::GameFontCatalog m_GameFonts;
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
    BML::DataShare *m_DataShare = nullptr;

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
    BMLMod *m_BMLMod = nullptr;
    NewBallTypeMod *m_BallTypeMod = nullptr;
#if BML_ENABLE_ANGELSCRIPT
    std::vector<std::unique_ptr<BML::ScriptMod>> m_ScriptMods;
    std::unique_ptr<BML::ScriptDevToolsService> m_ScriptDevTools;
    std::unique_ptr<BML::ScriptModHotReloadService> m_ScriptHotReload;
#endif

    NativeModRegistry m_NativeModRegistry;

    std::vector<IMod *> m_Mods;
    std::vector<IMod *> m_ActiveMods;
    std::unordered_map<std::string, size_t> m_ModIndex;

    std::unordered_map<IMod*, std::vector<ModDependency>> m_ModDependencies;

    ConfigStore m_ConfigStore;

    std::unordered_map<void *, std::vector<IMod *>> m_CallbackMap;

    const std::thread::id m_MainThreadId = std::this_thread::get_id();

    // Mod synchronization protocol:
    // 1. Acquire the invocation gate before either registry lock when a task
    //    needs both object lifetime protection and registry state.
    // 2. When both mutex families are needed, acquire m_Mutex before
    //    m_ModRegistryMutex and release them in reverse order.
    // 3. Never call a Mod virtual method while m_Mutex or
    //    m_ModRegistryMutex is held. Copy the required state, unlock, and then
    //    invoke external code while the invocation gate keeps objects alive.
    //
    // m_ModRegistryMutex protects the Mod/DLL owner registries and their
    // indices. m_Mutex protects dependencies, configs, commands, and callbacks.
    mutable std::shared_mutex m_ModRegistryMutex;
    mutable BML::ModInvocationGate m_ModInvocationGate;
    mutable BML::ModInvocationGate m_CommandInvocationGate;
    mutable std::mutex m_Mutex;
};

#endif // BML_MODCONTEXT_H
