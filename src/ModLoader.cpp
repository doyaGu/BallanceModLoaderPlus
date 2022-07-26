#include "ModLoader.h"

#include <ctime>
#include <Windows.h>

#include "RegisterBB.h"
#include "InputHook.h"
#include "Logger.h"
#include "BMLMod.h"
#include "NewBallTypeMod.h"
#include "ModManager.h"
#include "Util.h"

#include "unzip.h"

extern bool HookObjectLoad();
extern bool UnhookObjectLoad();

extern bool HookPhysicalize();
extern bool UnhookPhysicalize();

bool BModDll::Load() {
    if (LoadDll() == nullptr)
        return false;
    entry = reinterpret_cast<BModGetBMLEntryFunction>(GetFunctionPtr("BMLEntry"));
    if (!entry)
        return false;
    exit = reinterpret_cast<BModGetBMLExitFunction>(GetFunctionPtr("BMLExit"));
    registerBB = reinterpret_cast<BModRegisterBBFunction>(GetFunctionPtr("RegisterBB"));
    return true;
}

INSTANCE_HANDLE BModDll::LoadDll() {
    dllInstance = LoadLibraryEx(dllFileName.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    return dllInstance;
}

void *BModDll::GetFunctionPtr(const char *functionName) const {
    return (void *) ::GetProcAddress((HMODULE) dllInstance, functionName);
}

ModLoader &ModLoader::GetInstance() {
    static ModLoader instance;
    return instance;
}

ModLoader::ModLoader() = default;

ModLoader::~ModLoader() {
    if (IsInitialized())
        Release();
    delete m_InputManager;
}

void ModLoader::Init(CKContext *context) {
    srand((unsigned int) time(nullptr));

    MakeDirectories();

    InitLogger();

    m_Logger->Info("Initializing Mod Loader Plus version " BML_VERSION);
    m_Logger->Info("Website: https://github.com/doyaGu/BallanceModLoaderPlus");

#ifdef _DEBUG
    m_Logger->Info("Player.exe Address: 0x%08x", ::GetModuleHandleA("Player.exe"));
    m_Logger->Info("CK2.dll Address: 0x%08x", ::GetModuleHandleA("CK2.dll"));
    m_Logger->Info("VxMath.dll Address: 0x%08x", ::GetModuleHandleA("VxMath.dll"));
#endif

    m_Context = context;

    GetManagers();

    InitHooks();

    m_Logger->Info("Loading Mod Loader");

    m_Initialized = true;
}

void ModLoader::Release() {
    m_Initialized = false;

    m_Logger->Info("Releasing Mod Loader");

    for (const auto &mod : m_ModDllMap) {
        if (mod.second && mod.second->exit)
            mod.second->exit(mod.first);
    }

    FiniHooks();

    m_Logger->Info("Goodbye!");
    FiniLogger();
}

void ModLoader::PreloadMods() {
    CKDirectoryParser bmodTraverser("..\\ModLoader\\Mods", "*.bmodp", TRUE);
    for (char *bmodPath = bmodTraverser.GetNextFile(); bmodPath != nullptr; bmodPath = bmodTraverser.GetNextFile()) {
        std::string filename = bmodPath;
        BModDll modDll;
        modDll.dllFileName = filename;
        modDll.dllPath = filename.substr(0, filename.find_last_of('\\'));
        if (modDll.Load())
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

                if (modDll.Load())
                    m_ModDlls.push_back(modDll);
            }
        } while (unzGoToNextFile(zipFile) == UNZ_OK);

        unzClose(zipFile);
    }
}

void ModLoader::RegisterBBs(XObjectDeclarationArray *reg) {
    assert(reg != nullptr);

    CreateMessageHooks();
    for (auto &hook: m_MessageHooks) {
        hook->Register(reg);
    }

    for (auto &modDll: m_ModDlls) {
        if (modDll.registerBB)
            modDll.registerBB(reg);
    }
}

void ModLoader::AddDataPath(const char *path) {
    if (!path) return;

    XString dataPath = path;
    if (!m_PathManager->PathIsAbsolute(dataPath)) {
        char buf[MAX_PATH];
        VxGetCurrentDirectory(buf);
        dataPath.Format("%s\\%s", buf, dataPath.CStr());
    }
    if (dataPath[dataPath.Length() - 1] != '\\')
        dataPath << '\\';

    XString subDataPath1 = dataPath + "3D Entities\\";
    XString subDataPath2 = dataPath + "3D Entities\\PH\\";
    XString texturePath = dataPath + "Textures\\";
    XString soundPath = dataPath + "Sounds\\";
    m_PathManager->AddPath(DATA_PATH_IDX, dataPath);
    m_PathManager->AddPath(DATA_PATH_IDX, subDataPath1);
    m_PathManager->AddPath(DATA_PATH_IDX, subDataPath2);
    m_PathManager->AddPath(BITMAP_PATH_IDX, texturePath);
    m_PathManager->AddPath(SOUND_PATH_IDX, soundPath);
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

void ModLoader::LoadMods() {
    if (!m_Initialized)
        return;

    m_RenderContext = m_Context->GetPlayerRenderContext();
    m_Logger->Info("Get Render Context pointer 0x%08x", m_RenderContext);

    m_RenderManager = m_Context->GetRenderManager();
    m_Logger->Info("Get Render Manager pointer 0x%08x", m_RenderManager);

    ExecuteBB::Init(m_Context);

    m_BMLMod = new BMLMod(this);
    m_Mods.push_back(m_BMLMod);
    m_ModDllMap.insert({m_BMLMod, nullptr});

    m_BallTypeMod = new NewBallTypeMod(this);
    m_Mods.push_back(m_BallTypeMod);
    m_ModDllMap.insert({m_BallTypeMod, nullptr});

    for (auto &modDll: m_ModDlls) {
        if (RegisterMod(modDll) && !modDll.dllPath.empty())
            AddDataPath(modDll.dllPath.c_str());
    }

    for (auto *mod: m_Mods) {
        LoadMod(mod);
    }

    std::sort(m_Commands.begin(), m_Commands.end(),
              [](ICommand *a, ICommand *b) { return a->GetName() < b->GetName(); });

    for (Config *config: m_Configs)
        config->Save();

    BroadcastCallback(&IMod::OnLoadObject, "base.cmo", false, "", CKCID_3DOBJECT, true, true, true, false, nullptr, nullptr);

    int scriptCnt = m_Context->GetObjectsCountByClassID(CKCID_BEHAVIOR);
    CK_ID *scripts = m_Context->GetObjectsListByClassID(CKCID_BEHAVIOR);
    for (int i = 0; i < scriptCnt; i++) {
        auto *behavior = (CKBehavior *) m_Context->GetObject(scripts[i]);
        if (behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
            BroadcastCallback(&IMod::OnLoadScript, "base.cmo", behavior);
        }
    }

    m_IsReset = false;
}

void ModLoader::UnloadMods() {
    BroadcastMessage("OnUnload", &IMod::OnUnload);

    for (Config *config: m_Configs)
        config->Save();
    m_Configs.clear();
}

CKERROR ModLoader::OnPreProcess() {
    BroadcastCallback(&IMod::OnPreProcess);
    return CK_OK;
}

CKERROR ModLoader::OnProcess() {
    m_SkipRender = false;

    for (auto iter = m_Timers.begin(); iter != m_Timers.end();) {
        if (!iter->Process(m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime()))
            iter = m_Timers.erase(iter);
        else
            iter++;
    }

    BroadcastCallback(&IMod::OnProcess);

    if (m_InputManager->IsKeyDown(CKKEY_F) && IsCheatEnabled())
        SkipRenderForNextTick();

    m_InputManager->Process();

    if (m_Exiting) SendMessageBroadcast("Exit Game");
    return CK_OK;
}

CKERROR ModLoader::OnPreRender(CKRenderContext *dev) {
    if (m_SkipRender)
        dev->ChangeCurrentRenderOptions(0, CK_RENDER_DEFAULTSETTINGS);
    else
        dev->ChangeCurrentRenderOptions(CK_RENDER_DEFAULTSETTINGS, 0);
    return CK_OK;
}

CKERROR ModLoader::OnPostRender(CKRenderContext *dev) {
    BroadcastCallback(&IMod::OnRender, static_cast<CK_RENDER_FLAGS>(dev->GetCurrentRenderOptions()));
    return CK_OK;
}

void ModLoader::OnPreStartMenu() {
    BroadcastMessage("PreStartMenu", &IMod::OnPreStartMenu);
}

void ModLoader::OnPostStartMenu() {
    BroadcastMessage("PostStartMenu", &IMod::OnPostStartMenu);
}

void ModLoader::OnExitGame() {
    BroadcastMessage("ExitGame", &IMod::OnExitGame);
}

void ModLoader::OnPreLoadLevel() {
    BroadcastMessage("PreLoadLevel", &IMod::OnPreLoadLevel);
}

void ModLoader::OnPostLoadLevel() {
    BroadcastMessage("PostLoadLevel", &IMod::OnPostLoadLevel);
}

void ModLoader::OnStartLevel() {
    BroadcastMessage("StartLevel", &IMod::OnStartLevel);
    m_Ingame = true;
    m_Paused = false;
}

void ModLoader::OnPreResetLevel() {
    BroadcastMessage("PreResetLevel", &IMod::OnPreResetLevel);
}

void ModLoader::OnPostResetLevel() {
    BroadcastMessage("PostResetLevel", &IMod::OnPostResetLevel);
}

void ModLoader::OnPauseLevel() {
    BroadcastMessage("PauseLevel", &IMod::OnPauseLevel);
    m_Paused = true;
}

void ModLoader::OnUnpauseLevel() {
    BroadcastMessage("UnpauseLevel", &IMod::OnUnpauseLevel);
    m_Paused = false;
}

void ModLoader::OnPreExitLevel() {
    BroadcastMessage("PreExitLevel", &IMod::OnPreExitLevel);
}

void ModLoader::OnPostExitLevel() {
    BroadcastMessage("PostExitLevel", &IMod::OnPostExitLevel);
    m_Ingame = false;
}

void ModLoader::OnPreNextLevel() {
    BroadcastMessage("PreNextLevel", &IMod::OnPreNextLevel);
}

void ModLoader::OnPostNextLevel() {
    BroadcastMessage("PostNextLevel", &IMod::OnPostNextLevel);
}

void ModLoader::OnDead() {
    BroadcastMessage("Dead", &IMod::OnDead);
    m_Ingame = false;
}

void ModLoader::OnPreEndLevel() {
    BroadcastMessage("PreEndLevel", &IMod::OnPreEndLevel);
}

void ModLoader::OnPostEndLevel() {
    BroadcastMessage("PostEndLevel", &IMod::OnPostEndLevel);
    m_Ingame = false;
}

void ModLoader::OnCounterActive() {
    BroadcastMessage("CounterActive", &IMod::OnCounterActive);
}

void ModLoader::OnCounterInactive() {
    BroadcastMessage("CounterInactive", &IMod::OnCounterInactive);
}

void ModLoader::OnBallNavActive() {
    BroadcastMessage("BallNavActive", &IMod::OnBallNavActive);
}

void ModLoader::OnBallNavInactive() {
    BroadcastMessage("BallNavInactive", &IMod::OnBallNavInactive);
}

void ModLoader::OnCamNavActive() {
    BroadcastMessage("CamNavActive", &IMod::OnCamNavActive);
}

void ModLoader::OnCamNavInactive() {
    BroadcastMessage("CamNavInactive", &IMod::OnCamNavInactive);
}

void ModLoader::OnBallOff() {
    BroadcastMessage("BallOff", &IMod::OnBallOff);
}

void ModLoader::OnPreCheckpointReached() {
    BroadcastMessage("PreCheckpoint", &IMod::OnPreCheckpointReached);
}

void ModLoader::OnPostCheckpointReached() {
    BroadcastMessage("PostCheckpoint", &IMod::OnPostCheckpointReached);
}

void ModLoader::OnLevelFinish() {
    BroadcastMessage("LevelFinish", &IMod::OnLevelFinish);
}

void ModLoader::OnGameOver() {
    BroadcastMessage("GameOver", &IMod::OnGameOver);
}

void ModLoader::OnExtraPoint() {
    BroadcastMessage("ExtraPoint", &IMod::OnExtraPoint);
}

void ModLoader::OnPreSubLife() {
    BroadcastMessage("PreSubLife", &IMod::OnPreSubLife);
}

void ModLoader::OnPostSubLife() {
    BroadcastMessage("PostSubLife", &IMod::OnPostSubLife);
}

void ModLoader::OnPreLifeUp() {
    BroadcastMessage("PreLifeUp", &IMod::OnPreLifeUp);
}

void ModLoader::OnPostLifeUp() {
    BroadcastMessage("PostLifeUp", &IMod::OnPostLifeUp);
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

int ModLoader::GetCommandCount() const {
    return m_Commands.size();
}

ICommand *ModLoader::GetCommand(int index) const {
    return m_Commands[index];
}

ICommand *ModLoader::FindCommand(const char *name) const {
    if (!name) return nullptr;

    auto iter = m_CommandMap.find(name);
    if (iter == m_CommandMap.end())
        return nullptr;
    return iter->second;
}

void ModLoader::ExecuteCommand(const char *cmd) {
    m_Logger->Info("Execute Command: %s", cmd);

    std::vector<std::string> args = SplitString(cmd, " ");
    for (auto &ch: args[0])
        ch = static_cast<char>(std::tolower(ch));

    ICommand *command = FindCommand(args[0].c_str());
    if (command && (!command->IsCheat() || m_CheatEnabled)) {
        command->Execute(this, args);
    } else {
        m_BMLMod->AddIngameMessage(("Error: Unknown Command " + args[0]).c_str());
    }
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
        ICommand *command = FindCommand(args[0].c_str());
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
    BroadcastCallback(&IMod::OnCheatEnabled, enable);
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
    return m_Mods.size();
}

IMod *ModLoader::GetMod(int index) {
    return m_Mods[index];
}

float ModLoader::GetSRScore() {
    return m_BMLMod->GetSRScore();
}

int ModLoader::GetHSScore() {
    return m_BMLMod->GetHSScore();
}

void ModLoader::MakeDirectories() {
    VxMakeDirectory("..\\ModLoader");
    VxMakeDirectory("..\\ModLoader\\Config");
    VxMakeDirectory("..\\ModLoader\\Maps");
    VxMakeDirectory("..\\ModLoader\\Mods");

    VxDeleteDirectory("..\\ModLoader\\Cache");
    VxMakeDirectory("..\\ModLoader\\Cache");
    VxMakeDirectory("..\\ModLoader\\Cache\\Maps");
    VxMakeDirectory("..\\ModLoader\\Cache\\Mods");
}

void ModLoader::InitLogger() {
    m_Logfile = fopen("..\\ModLoader\\ModLoader.log", "w");
    m_Logger = new Logger("ModLoader");

#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
#endif
}

void ModLoader::FiniLogger() {
#ifdef _DEBUG
    FreeConsole();
#endif

    delete m_Logger;
    fclose(m_Logfile);
}

void ModLoader::InitHooks() {
    if (HookObjectLoad())
        m_Logger->Info("Hook ObjectLoad Success");
    else
        m_Logger->Info("Hook ObjectLoad Failed");

    if (HookPhysicalize())
        m_Logger->Info("Hook Physicalize Success");
    else
        m_Logger->Info("Hook Physicalize Failed");
}

void ModLoader::FiniHooks() {
    if (UnhookObjectLoad())
        m_Logger->Info("Unhook ObjectLoad Success");
    else
        m_Logger->Info("Unhook ObjectLoad Failed");

    if (UnhookPhysicalize())
        m_Logger->Info("Unhook Physicalize Success");
    else
        m_Logger->Info("Unhook Physicalize Failed");
}

void ModLoader::GetManagers() {
    m_AttributeManager = m_Context->GetAttributeManager();
    m_Logger->Info("Get Attribute Manager pointer 0x%08x", m_AttributeManager);

    m_BehaviorManager = m_Context->GetBehaviorManager();
    m_Logger->Info("Get Behavior Manager pointer 0x%08x", m_BehaviorManager);

    m_CollisionManager = (CKCollisionManager *) m_Context->GetManagerByGuid(COLLISION_MANAGER_GUID);
    m_Logger->Info("Get Collision Manager pointer 0x%08x", m_CollisionManager);

    m_InputManager = new InputHook(m_Context);
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

void ModLoader::CreateMessageHooks() {
#define ADD_GAME_MESSAGE_HOOK(evt, guid) \
    m_MessageHooks.push_back(CreateBuildingBlockHook("BML On" #evt, #evt " Hook.", guid, \
    [](const CKBehaviorContext &behcontext) { \
        ModLoader::GetInstance().On##evt(); \
        return CK_OK; \
    }))

    ADD_GAME_MESSAGE_HOOK(PreStartMenu, BML_ONPRESTARTMENU_GUID);
    ADD_GAME_MESSAGE_HOOK(PostStartMenu, BML_ONPOSTSTARTMENU_GUID);
    ADD_GAME_MESSAGE_HOOK(ExitGame, BML_ONEXITGAME_GUID);
    ADD_GAME_MESSAGE_HOOK(PreLoadLevel, BML_ONPRELOADLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(PostLoadLevel, BML_ONPOSTLOADLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(StartLevel, BML_ONSTARTLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(PreResetLevel, BML_ONPRERESETLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(PostResetLevel, BML_ONPOSTRESETLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(PauseLevel, BML_ONPAUSELEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(UnpauseLevel, BML_ONUNPAUSELEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(PreExitLevel, BML_ONPREEXITLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(PostExitLevel, BML_ONPOSTEXITLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(PreNextLevel, BML_ONPRENEXTLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(PostNextLevel, BML_ONPOSTNEXTLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(Dead, BML_ONDEAD_GUID);
    ADD_GAME_MESSAGE_HOOK(PreEndLevel, BML_ONPREENDLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(PostEndLevel, BML_ONPOSTENDLEVEL_GUID);
    ADD_GAME_MESSAGE_HOOK(CounterActive, BML_ONCOUNTERACTIVE_GUID);
    ADD_GAME_MESSAGE_HOOK(CounterInactive, BML_ONCOUNTERINACTIVE_GUID);
    ADD_GAME_MESSAGE_HOOK(BallNavActive, BML_ONBALLNAVACTIVE_GUID);
    ADD_GAME_MESSAGE_HOOK(BallNavInactive, BML_ONBALLNAVINACTIVE_GUID);
    ADD_GAME_MESSAGE_HOOK(CamNavActive, BML_ONCAMNAVACTIVE_GUID);
    ADD_GAME_MESSAGE_HOOK(CamNavInactive, BML_ONCAMNAVINACTIVE_GUID);
    ADD_GAME_MESSAGE_HOOK(BallOff, BML_ONBALLOFF_GUID);
    ADD_GAME_MESSAGE_HOOK(PreCheckpointReached, BML_ONPRECHECKPOINT_GUID);
    ADD_GAME_MESSAGE_HOOK(PostCheckpointReached, BML_ONPOSTCHECKPOINT_GUID);
    ADD_GAME_MESSAGE_HOOK(LevelFinish, BML_ONLEVELFINISH_GUID);
    ADD_GAME_MESSAGE_HOOK(GameOver, BML_ONGAMEOVER_GUID);
    ADD_GAME_MESSAGE_HOOK(ExtraPoint, BML_ONEXTRAPOINT_GUID);
    ADD_GAME_MESSAGE_HOOK(PreSubLife, BML_ONPRESUBLIFE_GUID);
    ADD_GAME_MESSAGE_HOOK(PostSubLife, BML_ONPOSTSUBLIFE_GUID);
    ADD_GAME_MESSAGE_HOOK(PreLifeUp, BML_ONPRELIFEUP_GUID);
    ADD_GAME_MESSAGE_HOOK(PostLifeUp, BML_ONPOSTLIFEUP_GUID);

    m_MessageHooks.push_back(CreateBuildingBlockHook("BML ModsMenu", "Show BML Mods Menu.", BML_MODSMENU_GUID,
                                                     [](const CKBehaviorContext &behcontext) {
                                                         ModLoader::GetInstance().OpenModsMenu();
                                                         return CK_OK;
                                                     }));
#undef ADD_GAME_MESSAGE_HOOK
}

bool ModLoader::RegisterMod(BModDll &modDll) {
    IMod *mod = modDll.entry(this);
    BMLVersion curVer;
    BMLVersion reqVer = mod->GetBMLVersion();
    if (curVer < reqVer) {
        m_Logger->Warn("Mod %s[%s] requires BML %d.%d.%d", mod->GetID(), mod->GetName(), reqVer.major, reqVer.minor, reqVer.build);
        m_ModDlls.erase(std::find_if(m_ModDlls.begin(), m_ModDlls.end(),
                                     [modDll](const BModDll &md) { return md.dllInstance == modDll.dllInstance; }));
        return false;
    }
    m_Mods.push_back(mod);
    m_ModDllMap.insert({mod, &modDll});
    return true;
}

void ModLoader::LoadMod(IMod *mod) {
    m_Logger->Info("Loading Mod %s[%s] v%s by %s", mod->GetID(), mod->GetName(), mod->GetVersion(), mod->GetAuthor());
    FillCallbackMap(mod);
    mod->OnLoad();
}

void ModLoader::FillCallbackMap(IMod *mod) {
    static class BlankMod : IMod {
    public:
        explicit BlankMod(IBML *bml) : IMod(bml) {}

        const char *GetID() override { return ""; }
        const char *GetVersion() override { return ""; }
        const char *GetName() override { return ""; }
        const char *GetAuthor() override { return ""; }
        const char *GetDescription() override { return ""; }
        DECLARE_BML_VERSION;
    } blank(this);

    void **vtable[2] = {
        *reinterpret_cast<void ***>(&blank),
        *reinterpret_cast<void ***>(mod)};

    int index = 0;
#define CHECK_V_FUNC(IDX, FUNC)                             \
    do {                                                    \
        auto idx = IDX;                                     \
        if (vtable[0][idx] != vtable[1][idx])               \
            m_CallbackMap[func_addr(FUNC)].push_back(mod);  \
    } while(0)

    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreStartMenu);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostStartMenu);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnExitGame);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreLoadLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostLoadLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnStartLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreResetLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostResetLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPauseLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnUnpauseLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreExitLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostExitLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreNextLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostNextLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnDead);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreEndLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostEndLevel);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnCounterActive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnCounterInactive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnBallNavActive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnBallNavInactive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnCamNavActive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnCamNavInactive);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnBallOff);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreCheckpointReached);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostCheckpointReached);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnLevelFinish);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnGameOver);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnExtraPoint);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreSubLife);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostSubLife);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPreLifeUp);
    CHECK_V_FUNC(index++, &IMessageReceiver::OnPostLifeUp);

    index += 7;

    CHECK_V_FUNC(index++, &IMod::OnLoad);
    CHECK_V_FUNC(index++, &IMod::OnUnload);
    CHECK_V_FUNC(index++, &IMod::OnModifyConfig);
    CHECK_V_FUNC(index++, &IMod::OnLoadObject);
    CHECK_V_FUNC(index++, &IMod::OnLoadScript);
    CHECK_V_FUNC(index++, &IMod::OnProcess);
    CHECK_V_FUNC(index++, &IMod::OnRender);
    CHECK_V_FUNC(index++, &IMod::OnCheatEnabled);

    CHECK_V_FUNC(index++, &IMod::OnPhysicalize);
    CHECK_V_FUNC(index++, &IMod::OnUnphysicalize);

    if (mod->GetBMLVersion() >= BMLVersion(0, 2, 1)) {
        CHECK_V_FUNC(index++, &IMod::OnPreProcess);
    }

#undef CHECK_V_FUNC
}