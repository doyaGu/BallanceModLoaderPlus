#include "ModLoader.h"

#include <ctime>
#include <Windows.h>

#include "InputHook.h"
#include "Logger.h"
#include "BMLMod.h"
#include "NewBallTypeMod.h"
#include "ModManager.h"
#include "Util.h"

#include "unzip.h"

bool BModDll::Load() {
    if (!LoadDll())
        return false;
    entry = reinterpret_cast<BModGetBMLEntryFunction>(GetFunctionPtr("BMLEntry"));
    if (!entry)
        return false;
    registerBB = reinterpret_cast<BModRegisterBBFunction>(GetFunctionPtr("RegisterBB"));
    return true;
}

INSTANCE_HANDLE BModDll::LoadDll() {
    VxSharedLibrary shl;
    dllInstance = shl.Load(TOCKSTRING(dllFileName.c_str()));
    return dllInstance;
}

void *BModDll::GetFunctionPtr(const char *functionName) {
    VxSharedLibrary shl;
    shl.Attach(dllInstance);
    return shl.GetFunctionPtr(TOCKSTRING(functionName));
}

ModLoader &ModLoader::GetInstance() {
    static ModLoader instance;
    return instance;
}

ModLoader::ModLoader() {
    VxMakeDirectory("..\\ModLoader");
    VxMakeDirectory("..\\ModLoader\\Config");
    VxMakeDirectory("..\\ModLoader\\Maps");
    VxMakeDirectory("..\\ModLoader\\Mods");

    VxDeleteDirectory("..\\ModLoader\\Cache");
    VxMakeDirectory("..\\ModLoader\\Cache");
    VxMakeDirectory("..\\ModLoader\\Cache\\Maps");
    VxMakeDirectory("..\\ModLoader\\Cache\\Mods");

    m_Logfile = fopen("..\\ModLoader\\ModLoader.log", "w");
    m_Logger = new Logger("ModLoader");
}

ModLoader::~ModLoader() {
    if (IsInitialized())
        Release();
    delete m_InputManager;
}

void ModLoader::Init(CKContext *context) {
    srand((UINT) time(nullptr));

#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
#endif

    m_Logger->Info("Initializing Mod Loader Plus version " BML_VERSION);
    m_Logger->Info("Website: https://github.com/doyaGu/BallanceModLoaderPlus");

#ifdef _DEBUG
    m_Logger->Info("Player.exe Address: 0x%08x", GetModuleHandle("Player.exe"));
    m_Logger->Info("CK2.dll Address: 0x%08x", GetModuleHandle("CK2.dll"));
    m_Logger->Info("VxMath.dll Address: 0x%08x", GetModuleHandle("VxMath.dll"));
#endif

    m_Context = context;

    GetManagers();

    m_ModManager = ModManager::GetManager(m_Context);
    m_Logger->Info("Get Mod Manager pointer 0x%08x", m_ModManager);

    m_Logger->Info("Loading Mod Loader");

    m_Initialized = true;
}

void ModLoader::Release() {
    m_Initialized = false;

    m_Logger->Info("Releasing Mod Loader");

#ifdef _DEBUG
    FreeConsole();
#endif

    m_Logger->Info("Goodbye!");

    delete m_Logger;
    fclose(m_Logfile);
}

void ModLoader::GetManagers() {
    m_AttributeManager = m_Context->GetAttributeManager();
    m_Logger->Info("Get Attribute Manager pointer 0x%08x", m_AttributeManager);

    m_BehaviorManager = m_Context->GetBehaviorManager();
    m_Logger->Info("Get Behavior Manager pointer 0x%08x", m_BehaviorManager);

    m_CollisionManager = (CKCollisionManager *) m_Context->GetManagerByGuid(COLLISION_MANAGER_GUID);
    m_Logger->Info("Get Collision Manager pointer 0x%08x", m_CollisionManager);

    m_InputManager = new InputHook((CKInputManager *) m_Context->GetManagerByGuid(INPUT_MANAGER_GUID));
    m_Logger->Info("Get Input Manager pointer 0x%08x", m_InputManager);

    m_MessageManager = m_Context->GetMessageManager();
    m_Logger->Info("Get Message Manager pointer 0x%08x", m_MessageManager);

    m_PathManager = m_Context->GetPathManager();
    m_Logger->Info("Get Path Manager pointer 0x%08x", m_PathManager);

    m_ParameterManager = m_Context->GetParameterManager();
    m_Logger->Info("Get Parameter Manager pointer 0x%08x", m_ParameterManager);

    m_SoundManager = (CKSoundManager *) m_Context->GetManagerByGuid(SOUND_MANAGER_GUID);
    m_Logger->Info("Get Sound Manager pointer 0x%08x", m_SoundManager);

    m_TimeManager = m_Context->GetTimeManager();
    m_Logger->Info("Get Time Manager pointer 0x%08x", m_TimeManager);
}

void ModLoader::PreloadMods() {
    CKDirectoryParser bmodTraverser("..\\ModLoader\\Mods", "*.bmodp", TRUE);
    for (char *bmodPath = bmodTraverser.GetNextFile(); bmodPath != nullptr; bmodPath = bmodTraverser.GetNextFile()) {
        std::string filename = bmodPath;
        BModDll modDll;
        modDll.dllFileName = filename;
        modDll.dllPath = filename.substr(0, filename.find_last_of('\\'));
        modDll.Load();
        m_ModDlls.push_back(modDll);
    }

    char filename[MAX_PATH];
    CKDirectoryParser zipTraverser("..\\ModLoader\\Mods", "*.zip", TRUE);
    for (char *zipPath = zipTraverser.GetNextFile(); zipPath != nullptr; zipPath = zipTraverser.GetNextFile()) {
        BModDll modDll;

        unzFile zipFile = unzOpen(zipPath);
        unz_file_info zipInfo;

        unzGoToFirstFile(zipFile);
        do {
            unzGetCurrentFileInfo(zipFile, &zipInfo, filename, MAX_PATH, nullptr, 0, nullptr, 0);
            const char *ext = strrchr(filename, '.');
            if (ext && strcmpi(ext, ".bmodp") == 0) {

                std::string path = "..\\ModLoader\\Cache\\Mods\\";
                path.append(CKPathSplitter(zipPath).GetName());
                path.push_back('\\');
                modDll.dllPath = path;
                modDll.dllFileName = path + filename;

                BYTE *buf = new BYTE[8192];
                unzGoToFirstFile(zipFile);
                do {
                    unzGetCurrentFileInfo(zipFile, &zipInfo, filename, MAX_PATH, nullptr, 0, nullptr, 0);
                    std::string fullPath = path + filename;
                    VxCreateFileTree(TOCKSTRING(fullPath.c_str()));
                    if (zipInfo.uncompressed_size != 0) {
                        unzOpenCurrentFile(zipFile);
                        FILE *fp = fopen(fullPath.c_str(), "wb");
                        int err;
                        do {
                            err = unzReadCurrentFile(zipFile, buf, 8192);
                            if (err < 0) {
                                m_Logger->Warn("error %d with zipfile in unzReadCurrentFile\n", err);
                                break;
                            }
                            if (err > 0)
                                if (fwrite(buf, (unsigned) err, 1, fp) != 1) {
                                    m_Logger->Warn("error in writing extracted file\n");
                                    break;
                                }
                        } while (err > 0);
                        fclose(fp);
                        unzCloseCurrentFile(zipFile);
                    }
                } while (unzGoToNextFile(zipFile) == UNZ_OK);
                delete[] buf;

                modDll.Load();
                m_ModDlls.push_back(modDll);
            }
        } while (unzGoToNextFile(zipFile) == UNZ_OK);

        unzClose(zipFile);
    }
}

void ModLoader::RegisterModBBs(XObjectDeclarationArray *reg) {
    for (auto &modDll: m_ModDlls) {
        if (modDll.registerBB)
            modDll.registerBB(reg);
    }
}

Config *ModLoader::GetConfig(IMod *mod) {
    for (Config *cfg: m_Configs) {
        if (cfg->GetMod() == mod)
            return cfg;
    }
    return nullptr;
}

void ModLoader::OpenModsMenu() {
    m_Logger->Info("Open Mods Menu");
    m_BMLMod->ShowModOptions();
}

void ModLoader::OnPreStartMenu() {
    m_ModManager->BroadcastMessage("PreStartMenu", &IMod::OnPreStartMenu);
}

void ModLoader::OnPostStartMenu() {
    m_ModManager->BroadcastMessage("PostStartMenu", &IMod::OnPostStartMenu);
}

void ModLoader::OnExitGame() {
    m_ModManager->BroadcastMessage("ExitGame", &IMod::OnExitGame);
}

void ModLoader::OnPreLoadLevel() {
    m_ModManager->BroadcastMessage("PreLoadLevel", &IMod::OnPreLoadLevel);
}

void ModLoader::OnPostLoadLevel() {
    m_ModManager->BroadcastMessage("PostLoadLevel", &IMod::OnPostLoadLevel);
}

void ModLoader::OnStartLevel() {
    m_ModManager->BroadcastMessage("StartLevel", &IMod::OnStartLevel);
    m_Ingame = true;
    m_Paused = false;
}

void ModLoader::OnPreResetLevel() {
    m_ModManager->BroadcastMessage("PreResetLevel", &IMod::OnPreResetLevel);
}

void ModLoader::OnPostResetLevel() {
    m_ModManager->BroadcastMessage("PostResetLevel", &IMod::OnPostResetLevel);
}

void ModLoader::OnPauseLevel() {
    m_ModManager->BroadcastMessage("PauseLevel", &IMod::OnPauseLevel);
    m_Paused = true;
}

void ModLoader::OnUnpauseLevel() {
    m_ModManager->BroadcastMessage("UnpauseLevel", &IMod::OnUnpauseLevel);
    m_Paused = false;
}

void ModLoader::OnPreExitLevel() {
    m_ModManager->BroadcastMessage("PreExitLevel", &IMod::OnPreExitLevel);
}

void ModLoader::OnPostExitLevel() {
    m_ModManager->BroadcastMessage("PostExitLevel", &IMod::OnPostExitLevel);
    m_Ingame = false;
}

void ModLoader::OnPreNextLevel() {
    m_ModManager->BroadcastMessage("PreNextLevel", &IMod::OnPreNextLevel);
}

void ModLoader::OnPostNextLevel() {
    m_ModManager->BroadcastMessage("PostNextLevel", &IMod::OnPostNextLevel);
}

void ModLoader::OnDead() {
    m_ModManager->BroadcastMessage("Dead", &IMod::OnDead);
    m_Ingame = false;
}

void ModLoader::OnPreEndLevel() {
    m_ModManager->BroadcastMessage("PreEndLevel", &IMod::OnPreEndLevel);
}

void ModLoader::OnPostEndLevel() {
    m_ModManager->BroadcastMessage("PostEndLevel", &IMod::OnPostEndLevel);
    m_Ingame = false;
}

void ModLoader::OnCounterActive() {
    m_ModManager->BroadcastMessage("CounterActive", &IMod::OnCounterActive);
}

void ModLoader::OnCounterInactive() {
    m_ModManager->BroadcastMessage("CounterInactive", &IMod::OnCounterInactive);
}

void ModLoader::OnBallNavActive() {
    m_ModManager->BroadcastMessage("BallNavActive", &IMod::OnBallNavActive);
}

void ModLoader::OnBallNavInactive() {
    m_ModManager->BroadcastMessage("BallNavInactive", &IMod::OnBallNavInactive);
}

void ModLoader::OnCamNavActive() {
    m_ModManager->BroadcastMessage("CamNavActive", &IMod::OnCamNavActive);
}

void ModLoader::OnCamNavInactive() {
    m_ModManager->BroadcastMessage("CamNavInactive", &IMod::OnCamNavInactive);
}

void ModLoader::OnBallOff() {
    m_ModManager->BroadcastMessage("BallOff", &IMod::OnBallOff);
}

void ModLoader::OnPreCheckpointReached() {
    m_ModManager->BroadcastMessage("PreCheckpoint", &IMod::OnPreCheckpointReached);
}

void ModLoader::OnPostCheckpointReached() {
    m_ModManager->BroadcastMessage("PostCheckpoint", &IMod::OnPostCheckpointReached);
}

void ModLoader::OnLevelFinish() {
    m_ModManager->BroadcastMessage("LevelFinish", &IMod::OnLevelFinish);
}

void ModLoader::OnGameOver() {
    m_ModManager->BroadcastMessage("GameOver", &IMod::OnGameOver);
}

void ModLoader::OnExtraPoint() {
    m_ModManager->BroadcastMessage("ExtraPoint", &IMod::OnExtraPoint);
}

void ModLoader::OnPreSubLife() {
    m_ModManager->BroadcastMessage("PreSubLife", &IMod::OnPreSubLife);
}

void ModLoader::OnPostSubLife() {
    m_ModManager->BroadcastMessage("PostSubLife", &IMod::OnPostSubLife);
}

void ModLoader::OnPreLifeUp() {
    m_ModManager->BroadcastMessage("PreLifeUp", &IMod::OnPreLifeUp);
}

void ModLoader::OnPostLifeUp() {
    m_ModManager->BroadcastMessage("PostLifeUp", &IMod::OnPostLifeUp);
}

void ModLoader::AddTimer(CKDWORD delay, std::function<void()> callback) {
    m_Timers.emplace_back(delay, callback, m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime());
}

void ModLoader::AddTimerLoop(CKDWORD delay, std::function<bool()> callback) {
    m_Timers.emplace_back(delay, callback, m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime());
}

void ModLoader::AddTimer(float delay, std::function<void()> callback) {
    m_Timers.emplace_back(delay, callback, m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime());
}

void ModLoader::AddTimerLoop(float delay, std::function<bool()> callback) {
    m_Timers.emplace_back(delay, callback, m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime());
}

void ModLoader::SendIngameMessage(const char *msg) {
    m_BMLMod->AddIngameMessage(msg);
}

void ModLoader::RegisterCommand(ICommand *cmd) {
    m_Commands.push_back(cmd);

    auto iter = m_CommandMap.find(cmd->GetName());
    if (iter == m_CommandMap.end())
        m_CommandMap[cmd->GetName()] = cmd;
    else
        m_Logger->Warn("Command Name Conflict: %s is redefined.", cmd->GetName().c_str());

    if (!cmd->GetAlias().empty()) {
        iter = m_CommandMap.find(cmd->GetAlias());
        if (iter == m_CommandMap.end())
            m_CommandMap[cmd->GetAlias()] = cmd;
        else
            m_Logger->Warn("Command Alias Conflict: %s is redefined.", cmd->GetAlias().c_str());
    }
}

ICommand *ModLoader::FindCommand(const std::string &name) {
    auto iter = m_CommandMap.find(name);
    if (iter == m_CommandMap.end())
        return nullptr;
    return iter->second;
}

void ModLoader::ExecuteCommand(const char *cmd, bool force) {
    m_Logger->Info("Execute Command: %s", cmd);
    std::vector<std::string> args = SplitString(cmd, " ");
    ICommand *command = FindCommand(args[0]);
    if (command && (!command->IsCheat() || m_CheatEnabled || force))
        command->Execute(this, args);
    else
        m_BMLMod->AddIngameMessage(("Error: Unknown Command " + args[0]).c_str());
}

std::string ModLoader::TabCompleteCommand(const char *cmd) {
    std::vector<std::string> args = SplitString(cmd, " ");
    std::vector<std::string> res;
    if (args.size() == 1) {
        for (auto &p: m_CommandMap) {
            if (StartWith(p.first, args[0])) {
                if (!p.second->IsCheat() || m_CheatEnabled)
                    res.push_back(p.first);
            }
        }
    } else {
        ICommand *command = FindCommand(args[0]);
        if (command && (!command->IsCheat() || m_CheatEnabled)) {
            for (const std::string &str: command->GetTabCompletion(this, args))
                if (StartWith(str, args[args.size() - 1]))
                    res.push_back(str);
        }
    }

    if (res.empty())
        return cmd;
    if (res.size() == 1) {
        if (args.size() == 1)
            return res[0];
        else {
            std::string str(cmd);
            str = str.substr(0, str.size() - args[args.size() - 1].size());
            str += res[0];
            return str;
        }
    }

    std::string str = res[0];
    for (unsigned int i = 1; i < res.size(); i++)
        str += ", " + res[i];
    m_BMLMod->AddIngameMessage(str.c_str());
    return cmd;
}

bool ModLoader::IsCheatEnabled() {
    return m_CheatEnabled;
}

void ModLoader::EnableCheat(bool enable) {
    m_CheatEnabled = enable;
    m_BMLMod->ShowCheatBanner(enable);
    m_ModManager->BroadcastCallback(&IMod::OnCheatEnabled,std::bind(&IMod::OnCheatEnabled, std::placeholders::_1, enable));
}

void ModLoader::SetIC(CKBeObject *obj, bool hierarchy) {
    m_Context->GetCurrentScene()->SetObjectInitialValue(obj, CKSaveObjectState(obj));

    if (hierarchy) {
        if (CKIsChildClassOf(obj, CKCID_2DENTITY)) {
            auto *entity = (CK2dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                SetIC(entity->GetChild(i), true);
        }
        if (CKIsChildClassOf(obj, CKCID_3DENTITY)) {
            auto *entity = (CK3dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                SetIC(entity->GetChild(i), true);
        }
    }
}

void ModLoader::RestoreIC(CKBeObject *obj, bool hierarchy) {
    CKStateChunk *chunk = m_Context->GetCurrentScene()->GetObjectInitialValue(obj);
    if (chunk)
        CKReadObjectState(obj, chunk);

    if (hierarchy) {
        if (CKIsChildClassOf(obj, CKCID_2DENTITY)) {
            auto *entity = (CK2dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                RestoreIC(entity->GetChild(i), true);
        }
        if (CKIsChildClassOf(obj, CKCID_3DENTITY)) {
            auto *entity = (CK3dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                RestoreIC(entity->GetChild(i), true);
        }
    }
}

void ModLoader::Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy) {
    obj->Show(show);

    if (hierarchy) {
        if (CKIsChildClassOf(obj, CKCID_2DENTITY)) {
            auto *entity = (CK2dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                Show(entity->GetChild(i), show, true);
        }
        if (CKIsChildClassOf(obj, CKCID_3DENTITY)) {
            auto *entity = (CK3dEntity *) obj;
            for (int i = 0; i < entity->GetChildrenCount(); i++)
                Show(entity->GetChild(i), show, true);
        }
    }
}

void ModLoader::RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                                 float friction, float elasticity,
                                 float mass, const char *collGroup, float linearDamp, float rotDamp, float force,
                                 float radius) {
    m_BallTypeMod->RegisterBallType(ballFile, ballId, ballName, objName, friction, elasticity,
                                    mass, collGroup, linearDamp, rotDamp, force, radius);
}

void ModLoader::RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                                  const char *collGroup, bool enableColl) {
    m_BallTypeMod->RegisterFloorType(floorName, friction, elasticity, mass, collGroup, enableColl);
}

void ModLoader::RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                  const char *collGroup,
                                  bool frozen, bool enableColl, bool calcMassCenter, float linearDamp, float rotDamp,
                                  float radius) {
    m_BallTypeMod->RegisterModulBall(modulName, fixed, friction, elasticity, mass, collGroup,
                                     frozen, enableColl, calcMassCenter, linearDamp, rotDamp, radius);
}

void ModLoader::RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                    const char *collGroup,
                                    bool frozen, bool enableColl, bool calcMassCenter, float linearDamp,
                                    float rotDamp) {
    m_BallTypeMod->RegisterModulConvex(modulName, fixed, friction, elasticity, mass, collGroup,
                                       frozen, enableColl, calcMassCenter, linearDamp, rotDamp);
}

void ModLoader::RegisterTrafo(const char *modulName) {
    m_BallTypeMod->RegisterTrafo(modulName);
}

void ModLoader::RegisterModul(const char *modulName) {
    m_BallTypeMod->RegisterModul(modulName);
}

int ModLoader::GetModCount() {
    return m_ModManager->GetModCount();
}

IMod *ModLoader::GetMod(int index) {
    return m_ModManager->GetMod(index);
}

float ModLoader::GetSRScore() {
    return m_BMLMod->GetSRScore();
}

int ModLoader::GetHSScore() {
    return m_BMLMod->GetHSScore();
}