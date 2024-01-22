#include "ModLoader.h"

#include <ctime>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <direct.h>
#include <io.h>

#include <zip.h>

#include "BML/InputHook.h"
#include "Logger.h"
#include "StringUtils.h"
#include "PathUtils.h"

// Builtin Mods
#include "BMLMod.h"
#include "NewBallTypeMod.h"

ModLoader &ModLoader::GetInstance() {
    static ModLoader instance;
    return instance;
}

ModLoader::ModLoader() = default;

ModLoader::~ModLoader() {
    Shutdown();
}

void ModLoader::Init(CKContext *context) {
    if (IsInitialized())
        return;

    srand((unsigned int) time(nullptr));

    DetectPlayer();

    InitDirectories();

    InitLogger();

    m_Logger->Info("Initializing Mod Loader Plus version " BML_VERSION);
    m_Logger->Info("Website: https://github.com/doyaGu/BallanceModLoaderPlus");

#ifdef _DEBUG
    m_Logger->Info("Player.exe Address: 0x%08x", ::GetModuleHandleA("Player.exe"));
    m_Logger->Info("CK2.dll Address: 0x%08x", ::GetModuleHandleA("CK2.dll"));
    m_Logger->Info("VxMath.dll Address: 0x%08x", ::GetModuleHandleA("VxMath.dll"));
#endif

    m_CKContext = context;

    GetManagers();

    InitHooks();

    m_Initialized = true;
}

void ModLoader::Shutdown() {
    if (IsInitialized()) {
        m_Logger->Info("Releasing Mod Loader");

        delete m_InputHook;

        ShutdownHooks();

        m_Logger->Info("Goodbye!");
        ShutdownLogger();

        m_Initialized = false;
    }
}

void ModLoader::LoadMods() {
    if (AreModsLoaded())
        return;

    RegisterBuiltinMods();

    std::string path = m_LoaderDir + "\\Mods";
    if (utils::DirectoryExists(path)) {
        std::vector<std::string> mods;
        if (ExploreMods(path, mods) == 0) {
            m_Logger->Info("No mod is found.");
        }

        for (auto &mod: mods) {
            if (LoadMod(mod)) {
                std::string p = utils::RemoveFileName(mod);
                AddDataPath(p.c_str());
            }
        }
    }

    m_ModsLoaded = true;
}

void ModLoader::UnloadMods() {
    if (!AreModsLoaded())
        return;

    for (auto rit = m_Mods.rbegin(); rit != m_Mods.rend(); ++rit) {
        auto *mod = *rit;
        mod->OnUnload();
    }

    for (auto rit = m_Configs.rbegin(); rit != m_Configs.rend(); ++rit) {
        SaveConfig(*rit);
    }

    m_CallbackMap.clear();
    m_Configs.clear();
    m_Commands.clear();
    m_CommandMap.clear();

    std::vector<std::string> modNames;
    modNames.reserve(m_Mods.size());
    for (auto *mod: m_Mods) {
        modNames.emplace_back(mod->GetID());
    }

    for (auto rit = modNames.rbegin(); rit != modNames.rend(); ++rit) {
        UnloadMod(rit->c_str());
    }

    m_ModsLoaded = false;
    m_ModsLoadedOnce = true;
}

int ModLoader::GetModCount() {
    return (int) m_Mods.size();
}

IMod *ModLoader::GetMod(int index) {
    return m_Mods[index];
}

IMod *ModLoader::FindMod(const char *id) const {
    if (!id)
        return nullptr;

    auto iter = m_ModMap.find(id);
    if (iter == m_ModMap.end())
        return nullptr;
    return iter->second;
}

const char *ModLoader::GetDirectory(ModLoader::DirectoryType type) const {
    switch (type) {
        case DIR_WORKING:
            if (m_WorkingDir.empty()) {
                char cwd[MAX_PATH];
                getcwd(cwd, MAX_PATH);
                m_WorkingDir = cwd;
            }
            return m_WorkingDir.c_str();
        case DIR_GAME:
            return m_GameDir.c_str();
        case DIR_LOADER:
            return m_LoaderDir.c_str();
    }

    return nullptr;
}

bool ModLoader::AddConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    bool inserted;
    ConfigMap::iterator it;
    std::tie(it, inserted) = m_ConfigMap.insert({mod->GetID(), config});
    if (!inserted) {
        m_Logger->Error("Can not add duplicate config for %s.", mod->GetID());
        return false;
    }

    LoadConfig(config);
    m_Configs.push_back(config);

    return true;
}

bool ModLoader::RemoveConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    auto it = m_ConfigMap.find(mod->GetID());
    if (it != m_ConfigMap.end()) {
        SaveConfig(config);
        m_Configs.erase(std::remove(m_Configs.begin(), m_Configs.end(), it->second), m_Configs.end());
    }

    return true;
}

Config *ModLoader::GetConfig(IMod *mod) {
    if (!mod)
        return nullptr;

    auto it = m_ConfigMap.find(mod->GetID());
    if (it == m_ConfigMap.end())
        return nullptr;
    return it->second;
}

bool ModLoader::LoadConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    std::string configPath = GetDirectory(DIR_LOADER);
    configPath.append("\\Config\\").append(mod->GetID()).append(".cfg");
    return config->Load(configPath.c_str());
}

bool ModLoader::SaveConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    std::string configPath = GetDirectory(DIR_LOADER);
    configPath.append("\\Config\\").append(mod->GetID()).append(".cfg");
    return config->Save(configPath.c_str());
}

void ModLoader::RegisterCommand(ICommand *cmd) {
    auto it = m_CommandMap.find(cmd->GetName());
    if (it != m_CommandMap.end()) {
        m_Logger->Warn("Command Name Conflict: %s is redefined.", cmd->GetName().c_str());
        return;
    }

    m_CommandMap[cmd->GetName()] = cmd;
    m_Commands.push_back(cmd);

    if (!cmd->GetAlias().empty()) {
        it = m_CommandMap.find(cmd->GetAlias());
        if (it == m_CommandMap.end())
            m_CommandMap[cmd->GetAlias()] = cmd;
        else
            m_Logger->Warn("Command Alias Conflict: %s is redefined.", cmd->GetAlias().c_str());
    }
}

int ModLoader::GetCommandCount() const {
    return (int) m_Commands.size();
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

    auto args = utils::SplitString(cmd, " ");
    for (auto &ch: args[0])
        ch = static_cast<char>(std::tolower(ch));

    ICommand *command = FindCommand(args[0].c_str());
    if (!command) {
        m_BMLMod->AddIngameMessage(("Error: Unknown Command " + args[0]).c_str());
        return;
    }

    if (command->IsCheat() && !m_CheatEnabled) {
        m_BMLMod->AddIngameMessage(("Error: Can not execute cheat command " + args[0]).c_str());
        return;
    }

    BroadcastCallback(&IMod::OnPreCommandExecute, command, args);
    command->Execute(this, args);
    BroadcastCallback(&IMod::OnPostCommandExecute, command, args);
}

std::string ModLoader::TabCompleteCommand(const char *cmd) {
    auto args = utils::SplitString(cmd, " ");
    std::vector<std::string> res;
    if (args.size() == 1) {
        for (auto &p: m_CommandMap) {
            if (utils::StringStartsWith(p.first, args[0])) {
                if (!p.second->IsCheat() || m_CheatEnabled)
                    res.push_back(p.first);
            }
        }
    } else {
        ICommand *command = FindCommand(args[0].c_str());
        if (command && (!command->IsCheat() || m_CheatEnabled)) {
            for (const std::string &str: command->GetTabCompletion(this, args))
                if (utils::StringStartsWith(str, args[args.size() - 1]))
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

void ModLoader::SetIC(CKBeObject *obj, bool hierarchy) {
    m_CKContext->GetCurrentScene()->SetObjectInitialValue(obj, CKSaveObjectState(obj));

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
    CKStateChunk *chunk = m_CKContext->GetCurrentScene()->GetObjectInitialValue(obj);
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

void ModLoader::OpenModsMenu() {
    m_Logger->Info("Open Mods Menu");
    m_BMLMod->ShowModOptions();
}

bool ModLoader::IsCheatEnabled() {
    return m_CheatEnabled;
}

void ModLoader::EnableCheat(bool enable) {
    m_CheatEnabled = enable;
    m_BMLMod->ShowCheatBanner(enable);
    BroadcastCallback(&IMod::OnCheatEnabled, enable);
}

void ModLoader::SendIngameMessage(const char *msg) {
    m_BMLMod->AddIngameMessage(msg);
}

float ModLoader::GetSRScore() {
    return m_BMLMod->GetSRScore();
}

int ModLoader::GetHSScore() {
    return m_BMLMod->GetHSScore();
}

void ModLoader::RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                                 float friction, float elasticity, float mass, const char *collGroup,
                                 float linearDamp, float rotDamp, float force, float radius) {
    m_BallTypeMod->RegisterBallType(ballFile, ballId, ballName, objName, friction, elasticity,
                                    mass, collGroup, linearDamp, rotDamp, force, radius);
}

void ModLoader::RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                                  const char *collGroup, bool enableColl) {
    m_BallTypeMod->RegisterFloorType(floorName, friction, elasticity, mass, collGroup, enableColl);
}

void ModLoader::RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                  const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                  float linearDamp, float rotDamp, float radius) {
    m_BallTypeMod->RegisterModulBall(modulName, fixed, friction, elasticity, mass, collGroup,
                                     frozen, enableColl, calcMassCenter, linearDamp, rotDamp, radius);
}

void ModLoader::RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                    const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                    float linearDamp, float rotDamp) {
    m_BallTypeMod->RegisterModulConvex(modulName, fixed, friction, elasticity, mass, collGroup,
                                       frozen, enableColl, calcMassCenter, linearDamp, rotDamp);
}

void ModLoader::RegisterTrafo(const char *modulName) {
    m_BallTypeMod->RegisterTrafo(modulName);
}

void ModLoader::RegisterModul(const char *modulName) {
    m_BallTypeMod->RegisterModul(modulName);
}

void ModLoader::SkipRenderForNextTick() {
    m_RenderContext->ChangeCurrentRenderOptions(0, CK_RENDER_DEFAULTSETTINGS);
    AddTimer(1ul, [this]() { m_RenderContext->ChangeCurrentRenderOptions(CK_RENDER_DEFAULTSETTINGS, 0); });
}

void ModLoader::OnCKPostReset() {
    if (!IsInitialized() || AreModsLoaded() || AreModsLoadedOnce())
        return;

    if (m_CKContext->GetCurrentLevel() == nullptr)
        return;

    m_RenderManager = m_CKContext->GetRenderManager();
    m_Logger->Info("Get Render Manager pointer 0x%08x", m_RenderManager);

    m_RenderContext = m_CKContext->GetPlayerRenderContext();
    m_Logger->Info("Get Render Context pointer 0x%08x", m_RenderContext);

    LoadMods();

    for (IMod *mod: m_Mods) {
        m_Logger->Info("Loading Mod %s[%s] v%s by %s",
                       mod->GetID(), mod->GetName(), mod->GetVersion(), mod->GetAuthor());

        FillCallbackMap(mod);
        mod->OnLoad();
    }

    std::sort(m_Commands.begin(), m_Commands.end(),
              [](ICommand *a, ICommand *b) { return a->GetName() < b->GetName(); });

    for (Config *config: m_Configs)
        SaveConfig(config);

    BroadcastCallback(&IMod::OnLoadObject, "base.cmo", false, "", CKCID_3DOBJECT,
                      true, true, true, false, nullptr, nullptr);

    int scriptCnt = m_CKContext->GetObjectsCountByClassID(CKCID_BEHAVIOR);
    CK_ID *scripts = m_CKContext->GetObjectsListByClassID(CKCID_BEHAVIOR);
    for (int i = 0; i < scriptCnt; i++) {
        auto *behavior = (CKBehavior *) m_CKContext->GetObject(scripts[i]);
        if (behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
            BroadcastCallback(&IMod::OnLoadScript, "base.cmo", behavior);
        }
    }
}

void ModLoader::OnCKReset() {
    if (!IsInitialized() || !AreModsLoaded())
        return;

    UnloadMods();
}

void ModLoader::PostProcess() {
    extern void PhysicsPostProcess();
    PhysicsPostProcess();

    for (auto iter = m_Timers.begin(); iter != m_Timers.end();) {
        if (!iter->Process(m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime()))
            iter = m_Timers.erase(iter);
        else
            iter++;
    }

    BroadcastCallback(&IMod::OnProcess);

    if (m_Exiting)
        m_MessageManager->SendMessageBroadcast(m_MessageManager->AddMessageType(TOCKSTRING("Exit Game")));

    m_InputHook->Process();
}

void ModLoader::OnPostRender(CKRenderContext *dev) {
    BroadcastCallback(&IMod::OnRender, static_cast<CK_RENDER_FLAGS>(dev->GetCurrentRenderOptions()));
}

void ModLoader::OnPostSpriteRender(CKRenderContext *dev) {}

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
    m_InLevel = true;
    m_Paused = false;
}

void ModLoader::OnPreResetLevel() {
    BroadcastMessage("PreResetLevel", &IMod::OnPreResetLevel);
    m_InLevel = false;
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
    m_InLevel = false;
}

void ModLoader::OnPreNextLevel() {
    BroadcastMessage("PreNextLevel", &IMod::OnPreNextLevel);
}

void ModLoader::OnPostNextLevel() {
    BroadcastMessage("PostNextLevel", &IMod::OnPostNextLevel);
    m_InLevel = false;
}

void ModLoader::OnDead() {
    BroadcastMessage("Dead", &IMod::OnDead);
    m_Ingame = false;
    m_InLevel = false;
}

void ModLoader::OnPreEndLevel() {
    BroadcastMessage("PreEndLevel", &IMod::OnPreEndLevel);
}

void ModLoader::OnPostEndLevel() {
    BroadcastMessage("PostEndLevel", &IMod::OnPostEndLevel);
    m_Ingame = false;
    m_InLevel = false;
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
    m_InLevel = false;
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

void ModLoader::DetectPlayer() {
    FILE *fp = fopen("Player.exe", "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        rewind(fp);

        m_IsOriginalPlayer = size == 155648;
        fclose(fp);
    }
}

void ModLoader::InitDirectories() {
    char path[MAX_PATH];
    {
        wchar_t szPath[MAX_PATH];
        wchar_t drive[4];
        wchar_t dir[MAX_PATH];
        ::GetModuleFileNameW(nullptr, szPath, MAX_PATH);
        _wsplitpath(szPath, drive, dir, nullptr, nullptr);
        _snwprintf(szPath, MAX_PATH, L"%s%s", drive, dir);
        size_t len = wcslen(szPath);
        szPath[len - 1] = '\0';
        wchar_t *s = wcsrchr(szPath, '\\');
        *s = '\0';
        utils::Utf16ToAnsi(szPath, path, MAX_PATH);
    }

    // Set up game directory
    m_GameDir = path;

    // Set up loader directory
    m_LoaderDir = m_GameDir + "\\ModLoader";
    if (!utils::DirectoryExists(m_LoaderDir)) {
        utils::CreateDir(m_LoaderDir);
    }

    if (!utils::DirectoryExists(m_LoaderDir)) {
        utils::CreateDir(m_LoaderDir);
    }

    // Set up configs directory
    std::string configPath = m_LoaderDir + "\\Config";
    if (!utils::DirectoryExists(configPath)) {
        utils::CreateDir(configPath);
    }

    // Set up cache directory
    std::string cachePath = m_LoaderDir + "\\Cache";
    if (!utils::DirectoryExists(cachePath)) {
        utils::CreateDir(cachePath);
    } else {
        VxDeleteDirectory((CKSTRING)cachePath.c_str());
    }
}

void ModLoader::InitLogger() {
    std::string logfilePath = m_LoaderDir + "\\ModLoader.log";
    m_Logfile = fopen(logfilePath.c_str(), "w");
    m_Logger = new Logger("ModLoader");

#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
#endif
}

void ModLoader::ShutdownLogger() {
#ifdef _DEBUG
    FreeConsole();
#endif

    delete m_Logger;
    fclose(m_Logfile);
}

void ModLoader::InitHooks() {
    extern bool HookObjectLoad();
    extern bool HookPhysicalize();

    if (HookObjectLoad())
        m_Logger->Info("Hook ObjectLoad Success");
    else
        m_Logger->Info("Hook ObjectLoad Failed");

    if (HookPhysicalize())
        m_Logger->Info("Hook Physicalize Success");
    else
        m_Logger->Info("Hook Physicalize Failed");
}

void ModLoader::ShutdownHooks() {
    extern bool UnhookObjectLoad();
    extern bool UnhookPhysicalize();

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
    m_AttributeManager = m_CKContext->GetAttributeManager();
    m_Logger->Info("Get Attribute Manager pointer 0x%08x", m_AttributeManager);

    m_BehaviorManager = m_CKContext->GetBehaviorManager();
    m_Logger->Info("Get Behavior Manager pointer 0x%08x", m_BehaviorManager);

    m_CollisionManager = (CKCollisionManager *) m_CKContext->GetManagerByGuid(COLLISION_MANAGER_GUID);
    m_Logger->Info("Get Collision Manager pointer 0x%08x", m_CollisionManager);

    m_InputHook = new InputHook((CKInputManager *)m_CKContext->GetManagerByGuid(INPUT_MANAGER_GUID));
    m_Logger->Info("Get Input Manager pointer 0x%08x", m_InputHook);

    m_MessageManager = m_CKContext->GetMessageManager();
    m_Logger->Info("Get Message Manager pointer 0x%08x", m_MessageManager);

    m_PathManager = m_CKContext->GetPathManager();
    m_Logger->Info("Get Path Manager pointer 0x%08x", m_PathManager);

    m_ParameterManager = m_CKContext->GetParameterManager();
    m_Logger->Info("Get Parameter Manager pointer 0x%08x", m_ParameterManager);

    m_SoundManager = (CKSoundManager *) m_CKContext->GetManagerByGuid(SOUND_MANAGER_GUID);
    m_Logger->Info("Get Sound Manager pointer 0x%08x", m_SoundManager);

    m_TimeManager = m_CKContext->GetTimeManager();
    m_Logger->Info("Get Time Manager pointer 0x%08x", m_TimeManager);
}

size_t ModLoader::ExploreMods(const std::string &path, std::vector<std::string> &mods) {
    if (!utils::DirectoryExists(path))
        return 0;

    std::string p = path + (utils::HasTrailingPathSeparator(path) ? "*" : "\\*");
    _finddata_t fileinfo = {};
    auto handle = _findfirst(p.c_str(), &fileinfo);
    if (handle == -1)
        return 0;

    do {
        if ((fileinfo.attrib & _A_SUBDIR)) {
            if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
                ExploreMods(p.assign(path).append("\\").append(fileinfo.name), mods);
        } else {
            std::string filename = path + "\\" + fileinfo.name;
            if (utils::StringEndsWithCaseInsensitive(filename, ".zip")) {
                std::string cachePath = GetDirectory(DIR_LOADER);
                std::string name = utils::GetFileName(filename);
                cachePath.append("\\Cache\\Mods\\").append(name);
                if (zip_extract(filename.c_str(), cachePath.c_str(), nullptr, nullptr) == 0)
                    ExploreMods(cachePath, mods);
            } else if (utils::StringEndsWithCaseInsensitive(filename, ".bmodp")) {
                // Add mod filename.
                mods.push_back(filename);
            }
        }
    } while (_findnext(handle, &fileinfo) == 0);

    _findclose(handle);

    return mods.size();
}

std::shared_ptr<void> ModLoader::LoadLib(const std::string &path) {
    if (path.empty())
        return nullptr;

    std::shared_ptr<void> dllHandlePtr;

    HMODULE dllHandle = ::LoadLibraryEx(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dllHandle)
        return nullptr;

    bool inserted;
    DllHandleMap::iterator it;
    std::tie(it, inserted) = m_DllHandleMap.insert({dllHandle, std::weak_ptr<void>()});
    if (!inserted) {
        dllHandlePtr = it->second.lock();
        if (dllHandlePtr) {
            ::FreeLibrary(dllHandle);
        }
    }

    if (!dllHandlePtr) {
        dllHandlePtr = std::shared_ptr<void>(dllHandle, [](void *ptr) {
            ::FreeLibrary(reinterpret_cast<HMODULE>(ptr));
        });
        it->second = dllHandlePtr;
    }

    return dllHandlePtr;
}

bool ModLoader::UnloadLib(void *dllHandle) {
    auto it = m_DllHandleToModsMap.find(dllHandle);
    if (it == m_DllHandleToModsMap.end())
        return false;

    for (auto *mod: it->second) {
        UnregisterMod(mod, m_ModToDllHandleMap[mod]);
    }
    return true;
}

bool ModLoader::LoadMod(const std::string &filename) {
    auto modName = utils::GetFileName(filename);
    auto dllHandle = LoadLib(filename);
    if (!dllHandle) {
        m_Logger->Error("Failed to load %s.", modName.c_str());
        return false;
    }

    constexpr const char *ENTRY_SYMBOL = "BMLEntry";
    typedef IMod *(*BMLEntryFunc)(IBML *);

    auto func = reinterpret_cast<BMLEntryFunc>(::GetProcAddress(reinterpret_cast<HMODULE>(dllHandle.get()),
                                                                ENTRY_SYMBOL));
    if (!func) {
        m_Logger->Error("%s does not export the required symbol: %s.", filename.c_str(), ENTRY_SYMBOL);
        return false;
    }

    IMod *mod = func(this);
    if (!mod) {
        m_Logger->Error("No mod could be registered, %s will be unloaded.", modName.c_str());
        UnloadLib(dllHandle.get());
        return false;
    }

    if (!RegisterMod(mod, dllHandle))
        return false;

    return true;
}

bool ModLoader::UnloadMod(const std::string &id) {
    auto it = m_ModMap.find(id);
    if (it == m_ModMap.end())
        return false;

    IMod *mod = it->second;
    auto dit = m_ModToDllHandleMap.find(mod);
    if (dit == m_ModToDllHandleMap.end())
        return false;

    auto &dllHandle = dit->second;

    if (!UnregisterMod(mod, dllHandle)) {
        m_Logger->Error("Failed to unload mod %s.", id.c_str());
        return false;
    }

    return true;
}

void ModLoader::RegisterBuiltinMods() {
    m_BMLMod = new BMLMod(this);
    RegisterMod(m_BMLMod, nullptr);

    m_BallTypeMod = new NewBallTypeMod(this);
    RegisterMod(m_BallTypeMod, nullptr);
}

bool ModLoader::RegisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle) {
    if (!mod)
        return false;

    BMLVersion curVer;
    BMLVersion reqVer = mod->GetBMLVersion();
    if (curVer < reqVer) {
        m_Logger->Warn("Mod %s[%s] requires BML %d.%d.%d", mod->GetID(), mod->GetName(),
                       reqVer.major, reqVer.minor, reqVer.build);
        return false;
    }

    bool inserted;
    ModMap::iterator it;
    std::tie(it, inserted) = m_ModMap.insert({mod->GetID(), mod});
    if (!inserted) {
        m_Logger->Error("Mod %s has already been registered.", mod->GetID());
        return false;
    }

    m_Mods.push_back(mod);

    auto mods = m_DllHandleToModsMap[dllHandle.get()];
    mods.push_back(mod);

    m_ModToDllHandleMap.insert({mod, dllHandle});

    return true;
}

bool ModLoader::UnregisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle) {
    if (!mod)
        return false;

    auto it = m_ModMap.find(mod->GetID());
    if (it == m_ModMap.end())
        return false;
    m_ModMap.erase(it);

    auto oit = std::find(m_Mods.begin(), m_Mods.end(), mod);
    if (oit != m_Mods.end())
        m_Mods.erase(oit);

    constexpr const char *EXIT_SYMBOL = "BMLExit";
    typedef void (*BMLExitFunc)(IMod *);

    auto func = reinterpret_cast<BMLExitFunc>(::GetProcAddress(reinterpret_cast<HMODULE>(dllHandle.get()),
                                                               EXIT_SYMBOL));
    if (func)
        func(mod);

    auto mit = m_DllHandleToModsMap.find(dllHandle.get());
    if (mit != m_DllHandleToModsMap.end()) {
        auto &mods = mit->second;
        mods.erase(std::remove(mods.begin(), mods.end(), mod), mods.end());
    }

    auto dit = m_ModToDllHandleMap.find(mod);
    if (dit != m_ModToDllHandleMap.end()) {
        m_ModToDllHandleMap.erase(dit);
    }

    return true;
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
            m_CallbackMap[FuncToAddr(FUNC)].push_back(mod);  \
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

    CHECK_V_FUNC(index++, &IMod::OnPreCommandExecute);
    CHECK_V_FUNC(index++, &IMod::OnPostCommandExecute);

#undef CHECK_V_FUNC
}

void ModLoader::AddDataPath(const char *path) {
    if (!path)
        return;

    XString dataPath = path;
    if (!m_PathManager->PathIsAbsolute(dataPath)) {
        char buf[MAX_PATH];
        VxGetCurrentDirectory(buf);
        dataPath.Format("%s\\%s", buf, dataPath.CStr());
    }
    if (dataPath[dataPath.Length() - 1] != '\\')
        dataPath << '\\';

    m_PathManager->AddPath(DATA_PATH_IDX, dataPath);

    XString subDataPath1 = dataPath + "3D Entities\\";
    XString subDataPath2 = dataPath + "3D Entities\\PH\\";
    XString texturePath = dataPath + "Textures\\";
    XString soundPath = dataPath + "Sounds\\";

    if (utils::DirectoryExists(subDataPath1.CStr()))
        m_PathManager->AddPath(DATA_PATH_IDX, subDataPath1);
    if (utils::DirectoryExists(subDataPath2.CStr()))
        m_PathManager->AddPath(DATA_PATH_IDX, subDataPath2);
    if (utils::DirectoryExists(texturePath.CStr()))
        m_PathManager->AddPath(BITMAP_PATH_IDX, texturePath);
    if (utils::DirectoryExists(soundPath.CStr()))
        m_PathManager->AddPath(SOUND_PATH_IDX, soundPath);
}