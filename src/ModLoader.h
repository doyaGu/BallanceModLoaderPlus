#ifndef BML_MODLOADER_H
#define BML_MODLOADER_H

#include <cstdio>
#include <functional>
#include <string>
#include <list>
#include <vector>
#include <map>

#include "CKAll.h"

#include "IBML.h"
#include "IMod.h"
#include "Config.h"
#include "Timer.h"
#include "Gui.h"

class ModManager;
class BMLMod;
class NewBallTypeMod;

typedef IMod *(*BModGetBMLEntryFunction)(IBML *);
typedef void (*BModRegisterBBFunction)(XObjectDeclarationArray *);

struct BModDll {
    std::string dllFileName;
    std::string dllPath;
    INSTANCE_HANDLE dllInstance;
    BModGetBMLEntryFunction entry;
    BModRegisterBBFunction registerBB;

    bool Load();

    INSTANCE_HANDLE LoadDll();

    void *GetFunctionPtr(const char *functionName);
};

class ModLoader : public IBML {
    friend class ModManager;
    friend class BMLMod;
    friend class CommandBML;
    friend class CommandHelp;
    friend class CommandCheat;
    friend class CommandClear;
    friend class CommandSector;
    friend class GuiList;
    friend class GuiModOption;
    friend class GuiModMenu;
    friend class GuiModCategory;

public:
    ModLoader();
    ~ModLoader() override;

    void Init(CKContext *context);
    void Release();

    bool IsInitialized() const { return m_Initialized; }
    bool IsReset() const { return m_IsReset; }

    FILE *GetLogFile() { return m_Logfile; }

    void AddConfig(Config *config) { m_Configs.push_back(config); }
    Config *GetConfig(IMod *mod);

    CKContext *GetCKContext() override { return m_Context; }
    CKRenderContext *GetRenderContext() override { return m_RenderContext; }

    void ExitGame() override { m_Exiting = true; }

    CKAttributeManager *GetAttributeManager() override { return m_AttributeManager; }
    CKBehaviorManager *GetBehaviorManager() override { return m_BehaviorManager; }
    CKCollisionManager *GetCollisionManager() override { return m_CollisionManager; }
    InputHook *GetInputManager() override { return m_InputManager; }
    CKMessageManager *GetMessageManager() override { return m_MessageManager; }
    CKPathManager *GetPathManager() override { return m_PathManager; }
    CKParameterManager *GetParameterManager() override { return m_ParameterManager; }
    CKRenderManager *GetRenderManager() override { return m_RenderManager; }
    CKSoundManager *GetSoundManager() override { return m_SoundManager; }
    CKTimeManager *GetTimeManager() override { return m_TimeManager; }

    void OpenModsMenu();
    void OnModifyConfig(IMod *mod, const char *category, const char *key, IProperty *prop) {
        mod->OnModifyConfig(category, key, prop);
    }

    void ExecuteCommand(const char *cmd, bool force = false);
    std::string TabCompleteCommand(const char *cmd);

    void PreloadMods();

    void RegisterModBBs(XObjectDeclarationArray *reg);

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

    void AddTimer(CKDWORD delay, std::function<void()> callback) override;
    void AddTimerLoop(CKDWORD delay, std::function<bool()> callback) override;
    void AddTimer(float delay, std::function<void()> callback) override;
    void AddTimerLoop(float delay, std::function<bool()> callback) override;

    bool IsCheatEnabled() override;
    void EnableCheat(bool enable) override;

    void SendIngameMessage(const char *msg) override;

    void RegisterCommand(ICommand *cmd) override;

    void SetIC(CKBeObject *obj, bool hierarchy = false) override;
    void RestoreIC(CKBeObject *obj, bool hierarchy = false) override;
    void Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy = false) override;

    bool IsIngame() override { return m_Ingame; }
    bool IsPaused() override { return m_Paused; }
    bool IsPlaying() override { return m_Ingame && !m_Paused; }

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

    int GetModCount() override;
    IMod *GetMod(int index) override;

    float GetSRScore() override;
    int GetHSScore() override;

    void SkipRenderForNextTick() override { m_SkipRender = true; }
    bool IsSkipRender() const { return m_SkipRender; }

    void AdjustFrameRate(bool sync = false, float limit = 60.0f) {
        if (sync) {
            m_TimeManager->ChangeLimitOptions(CK_FRAMERATE_SYNC);
        } else if (limit > 0) {
            m_TimeManager->ChangeLimitOptions(CK_FRAMERATE_LIMIT);
            m_TimeManager->SetFrameRateLimit(limit);
        } else {
            m_TimeManager->ChangeLimitOptions(CK_FRAMERATE_FREE);
        }
    }

    void SendMessageBroadcast(const char* msg) {
        m_MessageManager->SendMessageBroadcast(m_MessageManager->AddMessageType(TOCKSTRING(msg)));
    }

    ModManager *GetModManager() { return m_ModManager; }

    static ModLoader &GetInstance();

protected:
    void GetManagers();

    BMLMod *GetBMLMod() { return m_BMLMod; }

    ICommand *FindCommand(const std::string &name);

    bool m_Initialized = false;
    bool m_Exiting = false;
    bool m_IsReset = false;
    bool m_Ingame = false;
    bool m_Paused = false;
    bool m_SkipRender = false;
    bool m_CheatEnabled = false;

    FILE *m_Logfile = nullptr;
    ILogger *m_Logger = nullptr;

    CKContext *m_Context = nullptr;
    CKRenderContext *m_RenderContext = nullptr;

    CKAttributeManager *m_AttributeManager = nullptr;
    CKBehaviorManager *m_BehaviorManager = nullptr;
    CKCollisionManager *m_CollisionManager = nullptr;
    InputHook *m_InputManager = nullptr;
    CKMessageManager *m_MessageManager = nullptr;
    CKPathManager *m_PathManager = nullptr;
    CKParameterManager *m_ParameterManager = nullptr;
    CKRenderManager *m_RenderManager = nullptr;
    CKSoundManager *m_SoundManager = nullptr;
    CKTimeManager *m_TimeManager = nullptr;

    ModManager *m_ModManager = nullptr;

    BMLMod *m_BMLMod = nullptr;
    NewBallTypeMod *m_BallTypeMod = nullptr;

    std::vector<BModDll> m_ModDlls;
    std::vector<Config *> m_Configs;
    std::list<Timer> m_Timers;
    std::vector<ICommand *> m_Commands;
    std::map<std::string, ICommand *> m_CommandMap;
};

#endif // BML_MODLOADER_H