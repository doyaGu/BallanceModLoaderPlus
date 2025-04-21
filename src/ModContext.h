#ifndef BML_MODCONTEXT_H
#define BML_MODCONTEXT_H

#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BML/IBML.h"
#include "BML/IMod.h"

#include "Config.h"
#include "DataShare.h"
#include "CommandContext.h"
#include "HookUtils.h"

typedef enum DirectoryType {
    BML_DIR_WORKING = 0,
    BML_DIR_TEMP = 1,
    BML_DIR_GAME = 2,
    BML_DIR_LOADER = 3,
    BML_DIR_CONFIG = 4,
} DirectoryType;

class ModContext;
class BMLMod;
class NewBallTypeMod;

ModContext *BML_GetModContext();
CKContext *BML_GetCKContext();
CKRenderContext *BML_GetRenderContext();

class ModContext final : public IBML {
public:
    enum Flag {
        BML_INITED = 0x00000001,

        BML_MODS_LOADED = 0x00000010,
        BML_MODS_INITED = 0x00000020,

        BML_INGAME = 0x00000100,
        BML_INLEVEL = 0x00000200,
        BML_PAUSED = 0x00000400,
        BML_CHEAT = 0x00000800,
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
    void ModifyFlags(int add, int remove) { m_Flags = (m_Flags | add) & ~remove; }
    void SetFlags(int flags, bool set = true) { m_Flags = set ? m_Flags | flags : m_Flags & ~flags; }
    void ClearFlags(int flags) { m_Flags &= ~flags; }

    int GetModCount() override;
    IMod *GetMod(int index) override;
    IMod *FindMod(const char *id) const override;

    int RegisterDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) override;
    int RegisterOptionalDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) override;
    int CheckDependencies(IMod *mod) const override;
    int GetDependencyCount(IMod *mod) const override;
    int GetDependencyInfo(IMod *mod, int index, char *dependencyId, int idSize,
                          int *major, int *minor, int *patch, int *optional) const override;
    int ClearDependencies(IMod *mod) override;

    void RegisterCommand(ICommand *cmd) override;
    int GetCommandCount() const override;
    ICommand *GetCommand(int index) const override;
    ICommand *FindCommand(const char *name) const override;
    void ExecuteCommand(const char *cmd) override;

    bool AddConfig(Config *config);
    bool RemoveConfig(Config *config);
    Config *GetConfig(IMod *mod);
    bool LoadConfig(Config *config);
    bool SaveConfig(Config *config);

    ILogger *GetLogger() const {return m_Logger; }
    FILE *GetLogFile() const { return m_Logfile; }

    const wchar_t *GetDirectory(DirectoryType type);
    const char *GetDirectoryUtf8(DirectoryType type);

    BML::CommandContext &GetCommandContext() { return m_CommandContext; }
    BML::IDataShare *GetDataShare(const char *name = nullptr);

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

    bool IsIngame() override { return AreFlagsSet(BML_INGAME); }
    bool IsInLevel() const { return AreFlagsSet(BML_INLEVEL) && !AreFlagsSet(BML_PAUSED); }
    bool IsPaused() override { return AreFlagsSet(BML_PAUSED); }
    bool IsPlaying() override { return AreFlagsSet(BML_INGAME) && !AreFlagsSet(BML_PAUSED); }

    void OpenModsMenu();

    bool IsCheatEnabled() override { return AreFlagsSet(BML_CHEAT); }
    void EnableCheat(bool enable) override;

    void SendIngameMessage(const char *msg) override;

    float GetSRScore() override;
    int GetHSScore() override;

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
        for (IMod *mod: m_CallbackMap[utils::TypeErase(callback)]) {
            (mod->*callback)(std::forward<Args>(args)...);
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

    std::shared_ptr<void> LoadLib(const wchar_t *path);
    bool UnloadLib(void *dllHandle);

    IMod *LoadMod(const std::wstring &path);
    bool UnloadMod(const std::string &id);

    void RegisterBuiltinMods();

    bool RegisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle = nullptr);
    bool UnregisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle = nullptr);

    bool ResolveDependencies();
    bool HasCircularDependencies(const std::string& modId, std::unordered_set<std::string>& visited, std::unordered_set<std::string>& inProgress);
    std::vector<IMod*> SortModsByDependencies();

    void FillCallbackMap(IMod *mod);

    void AddDataPath(const char *path);

    int m_Flags = 0;

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

    BMLMod *m_BMLMod = nullptr;
    NewBallTypeMod *m_BallTypeMod = nullptr;

    typedef std::unordered_map<IMod *, std::shared_ptr<void>> ModToDllHandleMap;
    ModToDllHandleMap m_ModToDllHandleMap;

    typedef std::unordered_map<void *, std::vector<IMod *>> DllHandleToModsMap;
    DllHandleToModsMap m_DllHandleToModsMap;

    typedef std::unordered_map<void *, std::weak_ptr<void>> DllHandleMap;
    DllHandleMap m_DllHandleMap;

    std::vector<IMod *> m_Mods;
    typedef std::unordered_map<std::string, IMod *> ModMap;
    ModMap m_ModMap;

    std::unordered_map<IMod*, std::vector<ModDependency>> m_ModDependencies;
    std::unordered_map<std::string, std::vector<std::string>> m_DependencyGraph;

    std::vector<Config *> m_Configs;
    typedef std::unordered_map<std::string, Config *> ConfigMap;
    ConfigMap m_ConfigMap;

    std::unordered_map<void *, std::vector<IMod *>> m_CallbackMap;

    mutable std::mutex m_Mutex;
};

#endif // BML_MODCONTEXT_H
