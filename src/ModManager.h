#ifndef BML_MODMANAGER_H
#define BML_MODMANAGER_H

#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "CKBaseManager.h"
#include "CKContext.h"

#include "BML/IBML.h"
#include "BML/IMod.h"
#include "BML/Scheduler.h"
#include "DataShare.h"
#include "Config.h"
#include "HookUtils.h"

#ifndef MOD_MANAGER_GUID
#define MOD_MANAGER_GUID CKGUID(0x32a40332, 0x3bf12a51)
#endif

class ModManager;
class BMLMod;
class NewBallTypeMod;

typedef enum DirectoryType {
    BML_DIR_WORKING = 0,
    BML_DIR_TEMP = 1,
    BML_DIR_GAME = 2,
    BML_DIR_LOADER = 3,
    BML_DIR_CONFIG = 4,
} DirectoryType;

ModManager *BML_GetModManager();
CKContext *BML_GetCKContext();
CKRenderContext *BML_GetRenderContext();

class ModManager : public CKBaseManager, public IBML {
public:
    explicit ModManager(CKContext *context);
    ~ModManager() override;

    CKERROR OnCKInit() override;
    CKERROR OnCKEnd() override;

    CKERROR OnCKPlay() override;
    CKERROR OnCKReset() override;

    CKERROR PreProcess() override;
    CKERROR PostProcess() override;

    CKERROR OnPostRender(CKRenderContext *dev) override;
    CKERROR OnPostSpriteRender(CKRenderContext *dev) override;

    CKDWORD GetValidFunctionsMask() override {
        return CKMANAGER_FUNC_OnCKInit |
               CKMANAGER_FUNC_OnCKEnd |
               CKMANAGER_FUNC_OnCKPlay |
               CKMANAGER_FUNC_OnCKReset |
               CKMANAGER_FUNC_PreProcess |
               CKMANAGER_FUNC_PostProcess |
               CKMANAGER_FUNC_OnPostRender |
               CKMANAGER_FUNC_OnPostSpriteRender;
    }

    int GetFunctionPriority(CKMANAGER_FUNCTIONS Function) override {
        if (Function == CKMANAGER_FUNC_PreProcess)
            return -10000; // Low Priority
        else if (Function == CKMANAGER_FUNC_PostProcess)
            return 10000; // High Priority
        else
            return 0;
    }

    bool IsInitialized() const { return m_Initialized; }
    bool AreModsLoaded() const { return m_ModsLoaded; }
    bool AreModsInited() const { return m_ModsInited; }

    bool Init();
    bool Shutdown();

    void LoadMods();
    void UnloadMods();

    void InitMods();
    void ShutdownMods();

    int GetModCount() override;
    IMod *GetMod(int index) override;
    IMod *FindMod(const char *id) const override;

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

    BML::IDataShare *GetDataShare(const char *name = nullptr);

    const wchar_t *GetDirectory(DirectoryType type);
    const char *GetDirectoryUtf8(DirectoryType type);

    CKContext *GetCKContext() override { return m_Context; }
    CKRenderContext *GetRenderContext() override { return m_RenderContext; }

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
        return (CKDataArray *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_DATAARRAY);
    }
    CKGroup *GetGroupByName(const char *name) override {
        return (CKGroup *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_GROUP);
    }
    CKMaterial *GetMaterialByName(const char *name) override {
        return (CKMaterial *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_MATERIAL);
    }
    CKMesh *GetMeshByName(const char *name) override {
        return (CKMesh *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_MESH);
    }
    CK2dEntity *Get2dEntityByName(const char *name) override {
        return (CK2dEntity *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_2DENTITY);
    }
    CK3dEntity *Get3dEntityByName(const char *name) override {
        return (CK3dEntity *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_3DENTITY);
    }
    CK3dObject *Get3dObjectByName(const char *name) override {
        return (CK3dObject *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_3DOBJECT);
    }
    CKCamera *GetCameraByName(const char *name) override {
        return (CKCamera *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_CAMERA);
    }
    CKTargetCamera *GetTargetCameraByName(const char *name) override {
        return (CKTargetCamera *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_TARGETCAMERA);
    }
    CKLight *GetLightByName(const char *name) override {
        return (CKLight *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_LIGHT);
    }
    CKTargetLight *GetTargetLightByName(const char *name) override {
        return (CKTargetLight *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_TARGETLIGHT);
    }
    CKSound *GetSoundByName(const char *name) override {
        return (CKSound *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_SOUND);
    }
    CKTexture *GetTextureByName(const char *name) override {
        return (CKTexture *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_TEXTURE);
    }
    CKBehavior *GetScriptByName(const char *name) override {
        return (CKBehavior *)m_Context->GetObjectByNameAndClass((CKSTRING) name, CKCID_BEHAVIOR);
    }

    void SetIC(CKBeObject *obj, bool hierarchy) override;
    void RestoreIC(CKBeObject *obj, bool hierarchy) override;
    void Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy) override;

    void AddTimer(CKDWORD delay, std::function<void()> callback) override;
    void AddTimerLoop(CKDWORD delay, std::function<bool()> callback) override;
    void AddTimer(float delay, std::function<void()> callback) override;
    void AddTimerLoop(float delay, std::function<bool()> callback) override;

    Scheduler &GetScheduler() { return m_Scheduler; }

    void ExitGame() override;

    bool IsIngame() override { return m_Ingame; }
    bool IsPaused() override { return m_Paused; }
    bool IsPlaying() override { return m_Ingame && !m_Paused; }
    bool IsInLevel() const { return m_InLevel && !m_Paused; }

    void OpenModsMenu();

    bool IsCheatEnabled() override;
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

    static void OnModifyConfig(IMod *mod, const char *category, const char *key, IProperty *prop) {
        mod->OnModifyConfig(category, key, prop);
    }

    static ModManager *GetManager(CKContext *context) {
        return (ModManager *)context->GetManagerByGuid(MOD_MANAGER_GUID);
    }

protected:
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

    bool RegisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle);
    bool UnregisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle);

    void FillCallbackMap(IMod *mod);

    void AddDataPath(const char *path);

    bool m_Initialized = false;
    bool m_ModsLoaded = false;
    bool m_ModsInited = false;

    bool m_Ingame = false;
    bool m_InLevel = false;
    bool m_Paused = false;
    bool m_CheatEnabled = false;

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

    BML::DataShare *m_DataShare = nullptr;

    FILE *m_Logfile = nullptr;
    ILogger *m_Logger = nullptr;

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

    CKRenderContext *m_RenderContext = nullptr;
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

    std::vector<ICommand *> m_Commands;
    typedef std::unordered_map<std::string, ICommand *> CommandMap;
    CommandMap m_CommandMap;

    std::vector<Config *> m_Configs;
    typedef std::unordered_map<std::string, Config *> ConfigMap;
    ConfigMap m_ConfigMap;

    Scheduler m_Scheduler;

    std::unordered_map<void *, std::vector<IMod *>> m_CallbackMap;
};

#endif // BML_MODMANAGER_H
