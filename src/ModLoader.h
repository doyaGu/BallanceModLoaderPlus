#ifndef BML_MODLOADER_H
#define BML_MODLOADER_H

#include <cstdio>
#include <functional>
#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <memory>

#include "CKAll.h"

#include "IBML.h"
#include "IMod.h"
#include "Config.h"
#include "Timer.h"
#include "Gui.h"

class BMLMod;
class NewBallTypeMod;

template<typename T>
void *FuncToAddr(T func) {
    return *reinterpret_cast<void **>(&func);
}

class ModLoader : public IBML {
public:
    typedef enum DirectoryType {
        DIR_WORKING = 0,
        DIR_GAME = 1,
        DIR_LOADER = 2,
    } DirectoryType;

    static ModLoader &GetInstance();

    ModLoader();
    ~ModLoader() override;

    bool IsInitialized() const { return m_Initialized; }
    bool AreModsLoaded() const { return m_ModsLoaded; }

    void Init(CKContext *context);
    void Shutdown();

    void LoadMods();
    void UnloadMods();

    int GetModCount() override;
    IMod *GetMod(int index) override;
    IMod *FindMod(const char *id) const override;

    ILogger *GetLogger() {return m_Logger; }
    FILE *GetLogFile() { return m_Logfile; }

    const char *GetDirectory(DirectoryType type) const;

    void AddConfig(Config *config) { m_Configs.push_back(config); }
    Config *GetConfig(IMod *mod);

    CKContext *GetCKContext() override { return m_Context; }
    CKRenderContext *GetRenderContext() override { return m_RenderContext; }

    void ExitGame() override { m_Exiting = true; }

    bool IsIngame() override { return m_Ingame; }
    bool IsPaused() override { return m_Paused; }
    bool IsPlaying() override { return m_Ingame && !m_Paused; }
    bool IsInLevel() { return m_InLevel && !m_Paused; }
    bool IsOriginalPlayer() const { return m_IsOriginalPlayer; }

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

    void RegisterCommand(ICommand *cmd) override;

    int GetCommandCount() const override;
    ICommand *GetCommand(int index) const override;
    ICommand *FindCommand(const char *name) const override;

    void ExecuteCommand(const char *cmd) override;
    std::string TabCompleteCommand(const char *cmd);

    void AddTimer(CKDWORD delay, std::function<void()> callback) override;
    void AddTimerLoop(CKDWORD delay, std::function<bool()> callback) override;
    void AddTimer(float delay, std::function<void()> callback) override;
    void AddTimerLoop(float delay, std::function<bool()> callback) override;

    void SetIC(CKBeObject *obj, bool hierarchy = false) override;
    void RestoreIC(CKBeObject *obj, bool hierarchy = false) override;
    void Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy = false) override;

    CKDataArray *GetArrayByName(const char *name) override {
        return (CKDataArray *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_DATAARRAY);
    }
    CKGroup *GetGroupByName(const char *name) override {
        return (CKGroup *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_GROUP);
    }
    CKMaterial *GetMaterialByName(const char *name) override {
        return (CKMaterial *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_MATERIAL);
    }
    CKMesh *GetMeshByName(const char *name) override {
        return (CKMesh *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_MESH);
    }
    CK2dEntity *Get2dEntityByName(const char *name) override {
        return (CK2dEntity *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_2DENTITY);
    }
    CK3dEntity *Get3dEntityByName(const char *name) override {
        return (CK3dEntity *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_3DENTITY);
    }
    CK3dObject *Get3dObjectByName(const char *name) override {
        return (CK3dObject *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_3DOBJECT);
    }
    CKCamera *GetCameraByName(const char *name) override {
        return (CKCamera *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_CAMERA);
    }
    CKTargetCamera *GetTargetCameraByName(const char *name) override {
        return (CKTargetCamera *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_TARGETCAMERA);
    }
    CKLight *GetLightByName(const char *name) override {
        return (CKLight *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_LIGHT);
    }
    CKTargetLight *GetTargetLightByName(const char *name) override {
        return (CKTargetLight *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_TARGETLIGHT);
    }
    CKSound *GetSoundByName(const char *name) override {
        return (CKSound *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_SOUND);
    }
    CKTexture *GetTextureByName(const char *name) override {
        return (CKTexture *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_TEXTURE);
    }
    CKBehavior *GetScriptByName(const char *name) override {
        return (CKBehavior *)m_Context->GetObjectByNameAndClass(TOCKSTRING(name), CKCID_BEHAVIOR);
    }

    BMLMod *GetBMLMod() { return m_BMLMod; }

    void OpenModsMenu();

    bool IsCheatEnabled() override;
    void EnableCheat(bool enable) override;

    void SendIngameMessage(const char *msg) override;

    float GetSRScore() override;
    int GetHSScore() override;

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

    void SkipRenderForNextTick() override;

    void OnModifyConfig(IMod *mod, const char *category, const char *key, IProperty *prop) {
        mod->OnModifyConfig(category, key, prop);
    }

    template<typename T, typename... Args>
    std::enable_if_t<std::is_member_function_pointer<T>::value, void> BroadcastCallback(T callback, Args&&... args) {
        for (IMod *mod: m_CallbackMap[FuncToAddr(callback)]) {
            (mod->*callback)(std::forward<Args>(args)...);
        }
    }

    template<typename T>
    std::enable_if_t<std::is_member_function_pointer<T>::value, void> BroadcastMessage(const char *msg, T func) {
        m_Logger->Info("On Message %s", msg);
        BroadcastCallback(func);
    }

    CKERROR OnCKInit(CKContext *context);
    CKERROR OnCKEnd();

    CKERROR OnCKPostReset();
    CKERROR PreClearAll();

    CKERROR PreProcess();
    CKERROR PostProcess();

    CKERROR OnPostRender(CKRenderContext *dev);
    CKERROR OnPostSpriteRender(CKRenderContext *dev);

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

protected:
    void DetectPlayer();
    void InitDirectories();
    void InitLogger();
    void ShutdownLogger();
    void InitHooks();
    void ShutdownHooks();
    void GetManagers();

    size_t ExploreMods(const std::string &path, std::vector<std::string> &mods);

    bool Unzip(const std::string &zipfile, const std::string &dest);

    std::shared_ptr<void> LoadLib(const std::string &path);
    bool UnloadLib(void *dllHandle);

    bool LoadMod(const std::string &filename);
    bool UnloadMod(const std::string &id);

    void RegisterBuiltinMods();

    bool RegisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle);
    bool UnregisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle);

    void FillCallbackMap(IMod *mod);

    void AddDataPath(const char *path);

    bool m_Initialized = false;
    bool m_ModsLoaded = false;

    bool m_Exiting = false;
    bool m_Ingame = false;
    bool m_InLevel = false;
    bool m_Paused = false;
    bool m_IsOriginalPlayer = false;
    bool m_CheatEnabled = false;

    mutable std::string m_WorkingDir;
    std::string m_GameDir;
    std::string m_LoaderDir;

    FILE *m_Logfile = nullptr;
    ILogger *m_Logger = nullptr;

    CKContext *m_Context = nullptr;
    CKRenderContext *m_RenderContext = nullptr;

    CKAttributeManager *m_AttributeManager = nullptr;
    CKBehaviorManager *m_BehaviorManager = nullptr;
    CKCollisionManager *m_CollisionManager = nullptr;
    InputHook *m_InputHook = nullptr;
    CKMessageManager *m_MessageManager = nullptr;
    CKPathManager *m_PathManager = nullptr;
    CKParameterManager *m_ParameterManager = nullptr;
    CKRenderManager *m_RenderManager = nullptr;
    CKSoundManager *m_SoundManager = nullptr;
    CKTimeManager *m_TimeManager = nullptr;

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
    std::list<Timer> m_Timers;

    std::unordered_map<void *, std::vector<IMod *>> m_CallbackMap;
};

#endif // BML_MODLOADER_H