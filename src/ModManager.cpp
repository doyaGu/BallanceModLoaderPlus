#include "ModManager.h"

#include <unordered_set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <io.h>

#include <utf8.h>
#include <oniguruma.h>

#include "BML/InputHook.h"
#include "RenderHook.h"
#include "Overlay.h"
#include "Logger.h"
#include "StringUtils.h"
#include "PathUtils.h"

// Builtin Mods
#include "BMLMod.h"
#include "NewBallTypeMod.h"

ModManager *g_ModManager = nullptr;

ModManager *BML_GetModManager() {
    return g_ModManager;
}

CKContext *BML_GetCKContext() {
    if (!g_ModManager)
        return nullptr;
    return g_ModManager->GetCKContext();
}

CKRenderContext *BML_GetRenderContext() {
    if (!g_ModManager)
        return nullptr;
    return g_ModManager->GetRenderContext();
}

BML::IDataShare *BML_GetDataShare(const char *name) {
    auto *manager = BML_GetModManager();
    if (!manager)
        return nullptr;
    return manager->GetDataShare(name);
}

ModManager::ModManager(CKContext *context) : CKBaseManager(context, MOD_MANAGER_GUID, (CKSTRING) "Mod Manager") {
    m_DataShare = new BML::DataShare("BML");

    g_ModManager = this;
    context->RegisterNewManager(this);
}

ModManager::~ModManager() {
    m_DataShare->Release();

    g_ModManager = nullptr;
}

CKERROR ModManager::OnCKInit() {
    Init();

    return CK_OK;
}

CKERROR ModManager::OnCKEnd() {
    Shutdown();

    return CK_OK;
}

CKERROR ModManager::OnCKPlay() {
    if (m_Context->IsReseted() && m_Context->GetCurrentLevel() != nullptr && !m_RenderContext) {
        m_RenderContext = m_Context->GetPlayerRenderContext();
        m_Logger->Info("Get Render Context pointer 0x%08x", m_RenderContext);

        Overlay::ImGuiInitRenderer(m_Context);
        Overlay::ImGuiContextScope scope;

        LoadMods();
        InitMods();

        Overlay::ImGuiNewFrame();
    }

    return CK_OK;
}

CKERROR ModManager::OnCKReset() {
    if (m_Context->GetCurrentLevel() != nullptr && m_RenderContext) {
        Overlay::ImGuiContextScope scope;
        Overlay::ImGuiEndFrame();

        ShutdownMods();
        UnloadMods();

        Overlay::ImGuiShutdownRenderer(m_Context);

        m_RenderContext = nullptr;
    }

    return CK_OK;
}

CKERROR ModManager::PreProcess() {
    Overlay::ImGuiContextScope scope;

    Overlay::ImGuiNewFrame();

    return CK_OK;
}

CKERROR ModManager::PostProcess() {
    Overlay::ImGuiContextScope scope;

    extern void PhysicsPostProcess();
    PhysicsPostProcess();

    BroadcastCallback(&IMod::OnProcess);

    m_Scheduler.Update(m_TimeManager->GetLastDeltaTimeFree());

    static bool cursorVisibilityChanged = false;
    if (!m_InputHook->GetCursorVisibility()) {
        if (ImGui::GetIO().WantCaptureMouse) {
            m_InputHook->ShowCursor(TRUE);
            cursorVisibilityChanged = true;
        }
    } else if (cursorVisibilityChanged) {
        if (!ImGui::GetIO().WantCaptureMouse) {
            m_InputHook->ShowCursor(FALSE);
            cursorVisibilityChanged = false;
        }
    }

    Overlay::ImGuiRender();

    m_InputHook->Process();

    return CK_OK;
}

CKERROR ModManager::OnPostRender(CKRenderContext *dev) {
    BroadcastCallback(&IMod::OnRender, static_cast<CK_RENDER_FLAGS>(dev->GetCurrentRenderOptions()));

    return CK_OK;
}

CKERROR ModManager::OnPostSpriteRender(CKRenderContext *dev) {
    Overlay::ImGuiOnRender();

    return CK_OK;
}

bool ModManager::Init() {
    if (IsInitialized())
        return false;

    InitDirectories();

    InitLogger();

    m_Logger->Info("Initializing Mod Loader Plus version " BML_VERSION);
    m_Logger->Info("Website: https://github.com/doyaGu/BallanceModManagerPlus");

#ifdef _DEBUG
    m_Logger->Info("Player.exe Address: 0x%08x", ::GetModuleHandleA("Player.exe"));
    m_Logger->Info("CK2.dll Address: 0x%08x", ::GetModuleHandleA("CK2.dll"));
    m_Logger->Info("VxMath.dll Address: 0x%08x", ::GetModuleHandleA("VxMath.dll"));
#endif

    OnigEncoding encodings[3] = {ONIG_ENCODING_ASCII, ONIG_ENCODING_UTF8, ONIG_ENCODING_UTF16_LE};
    int err = onig_initialize(encodings, sizeof(encodings) / sizeof(encodings[0]));
    if (err < 0) {
        m_Logger->Error("Failed to initialize regular expression functionality");
        return false;
    }

    if (!GetManagers()) {
        m_Logger->Error("Failed to get managers");
        return false;
    }

    if (!InitHooks()) {
        m_Logger->Error("Failed to initialize hooks");
        return false;
    }

    if (Overlay::ImGuiCreateContext() == nullptr) {
        m_Logger->Error("Failed to create ImGui context");
        return false;
    }

    if (!Overlay::ImGuiInitPlatform(m_Context)) {
        m_Logger->Error("Failed to initialize Win32 platform backend for ImGui");
    }

    m_Initialized = true;
    return true;
}

bool ModManager::Shutdown() {
    if (AreModsLoaded())
        UnloadMods();

    m_Logger->Info("Releasing Mod Loader");

    if (Overlay::GetImGuiContext() != nullptr) {
        Overlay::ImGuiShutdownPlatform(m_Context);
        Overlay::ImGuiDestroyContext();
    }

    ShutdownHooks();

    utils::DeleteDirW(m_TempDir);

    onig_end();

    m_Logger->Info("Goodbye!");

    ShutdownLogger();

    m_Initialized = false;
    return true;
}

void ModManager::LoadMods() {
    if (!IsInitialized() || AreModsLoaded())
        return;

    std::unordered_set<std::string> modSet;

    RegisterBuiltinMods();

    for (auto *mod: m_Mods) {
        const char *id = mod->GetID();
        modSet.emplace(id);
    }

    std::wstring path = m_LoaderDir + L"\\Mods";
    if (utils::DirectoryExistsW(path)) {
        std::vector<std::wstring> modPaths;
        if (ExploreMods(path, modPaths) == 0) {
            m_Logger->Info("No mod is found.");
        }

        for (auto &modPath: modPaths) {
            IMod *mod = LoadMod(modPath);
            if (mod) {
                const char *id = mod->GetID();
                if (modSet.find(id) != modSet.end()) {
                    m_Logger->Warn("Duplicate Mod: %s", id);
                    UnloadMod(id);
                    continue;
                }
                modSet.emplace(id);

                wchar_t drive[4];
                wchar_t dir[MAX_PATH];
                wchar_t wBuf[MAX_PATH];
                char buf[1024];
                _wsplitpath(modPath.c_str(), drive, dir, nullptr, nullptr);
                _snwprintf(wBuf, MAX_PATH, L"%s%s", drive, dir);
                utils::Utf16ToAnsi(wBuf, buf, MAX_PATH);
                AddDataPath(buf);
            }
        }
    }

    m_ModsLoaded = true;
}

void ModManager::UnloadMods() {
    if (!IsInitialized() || !AreModsLoaded())
        return;

    std::vector<std::string> modNames;
    modNames.reserve(m_Mods.size());
    for (auto *mod: m_Mods) {
        modNames.emplace_back(mod->GetID());
    }

    for (auto rit = modNames.rbegin(); rit != modNames.rend(); ++rit) {
        UnloadMod(*rit);
    }

    m_ModsLoaded = false;
}

void ModManager::InitMods() {
    if (!IsInitialized() || !AreModsLoaded() || AreModsInited())
        return;

    for (IMod *mod: m_Mods) {
        m_Logger->Info("Loading Mod %s[%s] v%s by %s",
                       mod->GetID(), mod->GetName(), mod->GetVersion(), mod->GetAuthor());

        FillCallbackMap(mod);
        mod->OnLoad();
    }

    for (Config *config: m_Configs)
        SaveConfig(config);

    std::sort(m_Commands.begin(), m_Commands.end(),
              [](ICommand *a, ICommand *b) { return a->GetName() < b->GetName(); });

    BroadcastCallback(&IMod::OnLoadObject, "base.cmo", false, "", CKCID_3DOBJECT,
                      true, true, true, false, nullptr, nullptr);

    int scriptCnt = m_Context->GetObjectsCountByClassID(CKCID_BEHAVIOR);
    CK_ID *scripts = m_Context->GetObjectsListByClassID(CKCID_BEHAVIOR);
    for (int i = 0; i < scriptCnt; i++) {
        auto *behavior = (CKBehavior *) m_Context->GetObject(scripts[i]);
        if (behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
            BroadcastCallback(&IMod::OnLoadScript, "base.cmo", behavior);
        }
    }

    m_ModsInited = true;
}

void ModManager::ShutdownMods() {
    if (!IsInitialized() || !AreModsLoaded() || !AreModsInited())
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

    m_ModsInited = false;
}

int ModManager::GetModCount() {
    return (int) m_Mods.size();
}

IMod *ModManager::GetMod(int index) {
    if (index < 0 || index >= (int) m_Mods.size())
        return nullptr;
    return m_Mods[index];
}

IMod *ModManager::FindMod(const char *id) const {
    if (!id)
        return nullptr;

    auto iter = m_ModMap.find(id);
    if (iter == m_ModMap.end())
        return nullptr;
    return iter->second;
}

void ModManager::RegisterCommand(ICommand *cmd) {
    auto it = m_CommandMap.find(cmd->GetName());
    if (it != m_CommandMap.end()) {
        m_Logger->Warn("Command Name Conflict: %s is redefined.", cmd->GetName().c_str());
        return;
    }

    m_CommandMap[cmd->GetName()] = cmd;
    m_Commands.push_back(cmd);

    const auto alias = cmd->GetAlias();
    if (!alias.empty()) {
        it = m_CommandMap.find(alias);
        if (it == m_CommandMap.end())
            m_CommandMap[alias] = cmd;
        else
            m_Logger->Warn("Command Alias Conflict: %s is redefined.", alias.c_str());
    }
}

int ModManager::GetCommandCount() const {
    return (int) m_Commands.size();
}

ICommand *ModManager::GetCommand(int index) const {
    if (index < 0 || index >= (int) m_Commands.size())
        return nullptr;
    return m_Commands[index];
}

ICommand *ModManager::FindCommand(const char *name) const {
    if (!name) return nullptr;

    auto iter = m_CommandMap.find(name);
    if (iter == m_CommandMap.end())
        return nullptr;
    return iter->second;
}

void ModManager::ExecuteCommand(const char *cmd) {
    if (!cmd || cmd[0] == '\0')
        return;

    size_t size = utf8size(cmd);
    char *buf = new char[size + 1];
    utf8ncpy(buf, cmd, size);

    std::vector<std::string> args;

    char *lp = &buf[0];
    char *rp = lp;
    char *end = lp + size;
    utf8_int32_t cp, temp;
    utf8codepoint(rp, &cp);
    while (rp != end) {
        if (utf8codepointsize(*rp) == 1 && std::isspace(*rp) || *rp == '\0') {
            size_t len = rp - lp;
            if (len != 0) {
                char bk = *rp;
                *rp = '\0';
                args.emplace_back(lp);
                *rp = bk;
            }

            if (*rp != '\0') {
                while (utf8codepointsize(*rp) == 1 && std::isspace(*rp))
                    ++rp;
                --rp;
            }

            lp = utf8codepoint(rp, &temp);
        }

        rp = utf8codepoint(rp, &cp);
    }

    delete[] buf;

    m_Logger->Info("Execute Command: %s", cmd);

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

bool ModManager::AddConfig(Config *config) {
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

bool ModManager::RemoveConfig(Config *config) {
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

Config *ModManager::GetConfig(IMod *mod) {
    if (!mod)
        return nullptr;

    auto it = m_ConfigMap.find(mod->GetID());
    if (it == m_ConfigMap.end())
        return nullptr;
    return it->second;
}

bool ModManager::LoadConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    wchar_t buf[256];
    utils::AnsiToUtf16(mod->GetID(), buf, 256);
    std::wstring configPath = m_LoaderDir;
    configPath.append(L"\\Configs\\").append(buf).append(L".cfg");
    return config->Load(configPath.c_str());
}

bool ModManager::SaveConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    wchar_t buf[256];
    utils::AnsiToUtf16(mod->GetID(), buf, 256);
    std::wstring configPath = m_LoaderDir;
    configPath.append(L"\\Configs\\").append(buf).append(L".cfg");
    return config->Save(configPath.c_str());
}

BML::IDataShare *ModManager::GetDataShare(const char *name) {
    if (!name)
        return m_DataShare;
    return BML::DataShare::GetInstance(name);
}

const wchar_t *ModManager::GetDirectory(DirectoryType type) {
    switch (type) {
        case BML_DIR_WORKING:
            return m_WorkingDir.c_str();
        case BML_DIR_TEMP:
            return m_TempDir.c_str();
        case BML_DIR_GAME:
            return m_GameDir.c_str();
        case BML_DIR_LOADER:
            return m_LoaderDir.c_str();
        case BML_DIR_CONFIG:
            return m_ConfigDir.c_str();
        default:
            break;
    }

    return nullptr;
}

const char *ModManager::GetDirectoryUtf8(DirectoryType type) {
    switch (type) {
        case BML_DIR_WORKING:
            return m_WorkingDirUtf8.c_str();
        case BML_DIR_TEMP:
            return m_TempDirUtf8.c_str();
        case BML_DIR_GAME:
            return m_GameDirUtf8.c_str();
        case BML_DIR_LOADER:
            return m_LoaderDirUtf8.c_str();
        case BML_DIR_CONFIG:
            return m_ConfigDirUtf8.c_str();
        default:
            break;
    }

    return nullptr;
}

void ModManager::SetIC(CKBeObject *obj, bool hierarchy) {
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

void ModManager::RestoreIC(CKBeObject *obj, bool hierarchy) {
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

void ModManager::Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy) {
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

void ModManager::AddTimer(CKDWORD delay, std::function<void()> callback) {
    Spawn(m_Scheduler,
                 [](CKDWORD delay, std::function<void()> callback) -> Task {
                     co_await WaitForFrames(static_cast<int>(delay));
                     callback();
                     co_return;
                 },
                 delay, std::move(callback));
}

void ModManager::AddTimerLoop(CKDWORD delay, std::function<bool()> callback) {
    Spawn(m_Scheduler,
                 [](CKDWORD delay, std::function<bool()> callback) -> Task {
                     while (true) {
                         co_await WaitForFrames(static_cast<int>(delay));
                         if (!callback())
                             break;
                     }
                     co_return;
                 },
                 delay, std::move(callback));
}

void ModManager::AddTimer(float delay, std::function<void()> callback) {
    Spawn(m_Scheduler,
                 [](float delay, std::function<void()> callback) -> Task {
                     co_await WaitForMilliseconds(delay);
                     callback();
                     co_return;
                 },
                 delay, std::move(callback));
}

void ModManager::AddTimerLoop(float delay, std::function<bool()> callback) {
    Spawn(m_Scheduler,
                 [](float delay, std::function<bool()> callback) -> Task {
                     while (true) {
                         co_await WaitForMilliseconds(delay);
                         if (!callback())
                             break;
                     }
                     co_return;
                 },
                 delay, std::move(callback));
}

void ModManager::ExitGame() {
    AddTimer(1ul, [this]() {
        OnExitGame();
        ::PostMessage((HWND) m_Context->GetMainWindow(), 0x5FA, 0, 0);
    });
}

void ModManager::OpenModsMenu() {
    m_Logger->Info("Open Mods Menu");
    m_BMLMod->OpenModsMenu();
}

bool ModManager::IsCheatEnabled() {
    return m_CheatEnabled;
}

void ModManager::EnableCheat(bool enable) {
    m_CheatEnabled = enable;
    BroadcastCallback(&IMod::OnCheatEnabled, enable);
}

void ModManager::SendIngameMessage(const char *msg) {
    m_BMLMod->AddIngameMessage(msg);
}

float ModManager::GetSRScore() {
    return m_BMLMod->GetSRScore();
}

int ModManager::GetHSScore() {
    return m_BMLMod->GetHSScore();
}

void ModManager::SkipRenderForNextTick() {
    RenderHook::DisableRender(true);
    AddTimer(1ul, []() { RenderHook::DisableRender(false); });
}

void ModManager::RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                                  float friction, float elasticity, float mass, const char *collGroup,
                                  float linearDamp, float rotDamp, float force, float radius) {
    m_BallTypeMod->RegisterBallType(ballFile, ballId, ballName, objName, friction, elasticity,
                                    mass, collGroup, linearDamp, rotDamp, force, radius);
}

void ModManager::RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                                   const char *collGroup, bool enableColl) {
    m_BallTypeMod->RegisterFloorType(floorName, friction, elasticity, mass, collGroup, enableColl);
}

void ModManager::RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                   const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                   float linearDamp, float rotDamp, float radius) {
    m_BallTypeMod->RegisterModulBall(modulName, fixed, friction, elasticity, mass, collGroup,
                                     frozen, enableColl, calcMassCenter, linearDamp, rotDamp, radius);
}

void ModManager::RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                     const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                     float linearDamp, float rotDamp) {
    m_BallTypeMod->RegisterModulConvex(modulName, fixed, friction, elasticity, mass, collGroup,
                                       frozen, enableColl, calcMassCenter, linearDamp, rotDamp);
}

void ModManager::RegisterTrafo(const char *modulName) {
    m_BallTypeMod->RegisterTrafo(modulName);
}

void ModManager::RegisterModul(const char *modulName) {
    m_BallTypeMod->RegisterModul(modulName);
}

void ModManager::OnPreStartMenu() {
    BroadcastMessage("PreStartMenu", &IMod::OnPreStartMenu);
}

void ModManager::OnPostStartMenu() {
    BroadcastMessage("PostStartMenu", &IMod::OnPostStartMenu);
}

void ModManager::OnExitGame() {
    BroadcastMessage("ExitGame", &IMod::OnExitGame);
}

void ModManager::OnPreLoadLevel() {
    BroadcastMessage("PreLoadLevel", &IMod::OnPreLoadLevel);
}

void ModManager::OnPostLoadLevel() {
    BroadcastMessage("PostLoadLevel", &IMod::OnPostLoadLevel);
}

void ModManager::OnStartLevel() {
    BroadcastMessage("StartLevel", &IMod::OnStartLevel);
    m_Ingame = true;
    m_InLevel = true;
    m_Paused = false;
}

void ModManager::OnPreResetLevel() {
    BroadcastMessage("PreResetLevel", &IMod::OnPreResetLevel);
    m_InLevel = false;
}

void ModManager::OnPostResetLevel() {
    BroadcastMessage("PostResetLevel", &IMod::OnPostResetLevel);
}

void ModManager::OnPauseLevel() {
    BroadcastMessage("PauseLevel", &IMod::OnPauseLevel);
    m_Paused = true;
}

void ModManager::OnUnpauseLevel() {
    BroadcastMessage("UnpauseLevel", &IMod::OnUnpauseLevel);
    m_Paused = false;
}

void ModManager::OnPreExitLevel() {
    BroadcastMessage("PreExitLevel", &IMod::OnPreExitLevel);
}

void ModManager::OnPostExitLevel() {
    BroadcastMessage("PostExitLevel", &IMod::OnPostExitLevel);
    m_Ingame = false;
    m_InLevel = false;
}

void ModManager::OnPreNextLevel() {
    BroadcastMessage("PreNextLevel", &IMod::OnPreNextLevel);
}

void ModManager::OnPostNextLevel() {
    BroadcastMessage("PostNextLevel", &IMod::OnPostNextLevel);
    m_InLevel = false;
}

void ModManager::OnDead() {
    BroadcastMessage("Dead", &IMod::OnDead);
    m_Ingame = false;
    m_InLevel = false;
}

void ModManager::OnPreEndLevel() {
    BroadcastMessage("PreEndLevel", &IMod::OnPreEndLevel);
}

void ModManager::OnPostEndLevel() {
    BroadcastMessage("PostEndLevel", &IMod::OnPostEndLevel);
    m_Ingame = false;
    m_InLevel = false;
}

void ModManager::OnCounterActive() {
    BroadcastMessage("CounterActive", &IMod::OnCounterActive);
}

void ModManager::OnCounterInactive() {
    BroadcastMessage("CounterInactive", &IMod::OnCounterInactive);
}

void ModManager::OnBallNavActive() {
    BroadcastMessage("BallNavActive", &IMod::OnBallNavActive);
}

void ModManager::OnBallNavInactive() {
    BroadcastMessage("BallNavInactive", &IMod::OnBallNavInactive);
}

void ModManager::OnCamNavActive() {
    BroadcastMessage("CamNavActive", &IMod::OnCamNavActive);
}

void ModManager::OnCamNavInactive() {
    BroadcastMessage("CamNavInactive", &IMod::OnCamNavInactive);
}

void ModManager::OnBallOff() {
    BroadcastMessage("BallOff", &IMod::OnBallOff);
}

void ModManager::OnPreCheckpointReached() {
    BroadcastMessage("PreCheckpoint", &IMod::OnPreCheckpointReached);
}

void ModManager::OnPostCheckpointReached() {
    BroadcastMessage("PostCheckpoint", &IMod::OnPostCheckpointReached);
}

void ModManager::OnLevelFinish() {
    BroadcastMessage("LevelFinish", &IMod::OnLevelFinish);
    m_InLevel = false;
}

void ModManager::OnGameOver() {
    BroadcastMessage("GameOver", &IMod::OnGameOver);
}

void ModManager::OnExtraPoint() {
    BroadcastMessage("ExtraPoint", &IMod::OnExtraPoint);
}

void ModManager::OnPreSubLife() {
    BroadcastMessage("PreSubLife", &IMod::OnPreSubLife);
}

void ModManager::OnPostSubLife() {
    BroadcastMessage("PostSubLife", &IMod::OnPostSubLife);
}

void ModManager::OnPreLifeUp() {
    BroadcastMessage("PreLifeUp", &IMod::OnPreLifeUp);
}

void ModManager::OnPostLifeUp() {
    BroadcastMessage("PostLifeUp", &IMod::OnPostLifeUp);
}

void ModManager::InitDirectories() {
    char buf[1024];

    wchar_t path[MAX_PATH];
    wchar_t drive[4];
    wchar_t dir[MAX_PATH];

    // Set up working directory
    _wgetcwd(path, MAX_PATH);
    path[MAX_PATH - 1] = '\0';
    m_WorkingDir = path;

    utils::Utf16ToUtf8(m_WorkingDir.c_str(), buf, sizeof(buf));
    m_WorkingDirUtf8 = buf;

    // Set up game directory
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    _wsplitpath(path, drive, dir, nullptr, nullptr);
    _snwprintf(path, MAX_PATH, L"%s%s", drive, dir);
    path[MAX_PATH - 1] = '\0';
    size_t len = wcslen(path);
    path[len - 1] = '\0';
    wchar_t *s = wcsrchr(path, '\\');
    *s = '\0';
    m_GameDir = path;

    utils::Utf16ToUtf8(m_GameDir.c_str(), buf, sizeof(buf));
    m_GameDirUtf8 = buf;

    // Set up loader directory
    m_LoaderDir = m_GameDir + L"\\ModLoader";
    if (!utils::DirectoryExistsW(m_LoaderDir)) {
        utils::CreateDirW(m_LoaderDir);
    }

    utils::Utf16ToUtf8(m_LoaderDir.c_str(), buf, sizeof(buf));
    m_LoaderDirUtf8 = buf;

    // Set up temp directory
    ::GetTempPathW(MAX_PATH, path);
    wcsncat(path, L"BML", MAX_PATH);
    m_TempDir = path;
    if (!utils::DirectoryExistsW(m_TempDir)) {
        utils::CreateDirW(m_TempDir);
    }

    utils::Utf16ToUtf8(m_TempDir.c_str(), buf, sizeof(buf));
    m_TempDirUtf8 = buf;

    // Set up config directory
    m_ConfigDir = m_LoaderDir + L"\\Configs";
    if (!utils::DirectoryExistsW(m_ConfigDir)) {
        utils::CreateDirW(m_ConfigDir);
    }

    utils::Utf16ToUtf8(m_ConfigDir.c_str(), buf, sizeof(buf));
    m_ConfigDirUtf8 = buf;
}

void ModManager::InitLogger() {
    std::wstring logfilePath = m_LoaderDir + L"\\ModLoader.log";
    m_Logfile = _wfopen(logfilePath.c_str(), L"w");
    m_Logger = new Logger("ModLoader");

#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
#endif
}

void ModManager::ShutdownLogger() {
#ifdef _DEBUG
    FreeConsole();
#endif

    delete m_Logger;
    fclose(m_Logfile);
}

extern bool HookObjectLoad();
extern bool HookPhysicalize();

bool ModManager::InitHooks() {
    bool result = true;

    m_InputHook = new InputHook(m_InputManager);

    if (HookObjectLoad()) {
        m_Logger->Info("Hook ObjectLoad Success");
    } else {
        m_Logger->Info("Hook ObjectLoad Failed");
        result = false;
    }

    if (HookPhysicalize()) {
        m_Logger->Info("Hook Physicalize Success");
    } else {
        m_Logger->Info("Hook Physicalize Failed");
        result = false;
    }

    return result;
}

extern bool UnhookObjectLoad();
extern bool UnhookPhysicalize();

bool ModManager::ShutdownHooks() {
    bool result = true;

    delete m_InputHook;

    if (UnhookObjectLoad()) {
        m_Logger->Info("Unhook ObjectLoad Success");
    } else {
        m_Logger->Info("Unhook ObjectLoad Failed");
        result = false;
    }

    if (UnhookPhysicalize()) {
        m_Logger->Info("Unhook Physicalize Success");
    } else {
        m_Logger->Info("Unhook Physicalize Failed");
        result = false;
    }

    return result;
}

bool ModManager::GetManagers() {
    m_AttributeManager = m_Context->GetAttributeManager();
    if (m_AttributeManager) {
        m_Logger->Info("Get Attribute Manager pointer 0x%08x", m_AttributeManager);
    } else {
        m_Logger->Info("Failed to get Attribute Manager");
        return false;
    }

    m_BehaviorManager = m_Context->GetBehaviorManager();
    if (m_BehaviorManager) {
        m_Logger->Info("Get Behavior Manager pointer 0x%08x", m_BehaviorManager);
    } else {
        m_Logger->Info("Failed to get Behavior Manager");
        return false;
    }

    m_CollisionManager = (CKCollisionManager *) m_Context->GetManagerByGuid(COLLISION_MANAGER_GUID);
    if (m_CollisionManager) {
        m_Logger->Info("Get Collision Manager pointer 0x%08x", m_CollisionManager);
    } else {
        m_Logger->Info("Failed to get Collision Manager");
        return false;
    }

    m_InputManager = (CKInputManager *) m_Context->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (m_InputManager) {
        m_Logger->Info("Get Input Manager pointer 0x%08x", m_InputManager);
    } else {
        m_Logger->Info("Failed to get Input Manager");
        return false;
    }

    m_MessageManager = m_Context->GetMessageManager();
    if (m_MessageManager) {
        m_Logger->Info("Get Message Manager pointer 0x%08x", m_MessageManager);
    } else {
        m_Logger->Info("Failed to get Message Manager");
        return false;
    }

    m_PathManager = m_Context->GetPathManager();
    if (m_PathManager) {
        m_Logger->Info("Get Path Manager pointer 0x%08x", m_PathManager);
    } else {
        m_Logger->Info("Failed to get Path Manager");
        return false;
    }

    m_ParameterManager = m_Context->GetParameterManager();
    if (m_ParameterManager) {
        m_Logger->Info("Get Parameter Manager pointer 0x%08x", m_ParameterManager);
    } else {
        m_Logger->Info("Failed to get Parameter Manager");
        return false;
    }

    m_RenderManager = m_Context->GetRenderManager();
    if (m_RenderManager) {
        m_Logger->Info("Get Render Manager pointer 0x%08x", m_RenderManager);
    } else {
        m_Logger->Info("Failed to get Render Manager");
        return false;
    }

    m_SoundManager = (CKSoundManager *) m_Context->GetManagerByGuid(SOUND_MANAGER_GUID);
    if (m_SoundManager) {
        m_Logger->Info("Get Sound Manager pointer 0x%08x", m_SoundManager);
    } else {
        m_Logger->Info("Failed to get Sound Manager");
        return false;
    }

    m_TimeManager = m_Context->GetTimeManager();
    if (m_TimeManager) {
        m_Logger->Info("Get Time Manager pointer 0x%08x", m_TimeManager);
    } else {
        m_Logger->Info("Failed to get Time Manager");
        return false;
    }

    return true;
}

size_t ModManager::ExploreMods(const std::wstring &path, std::vector<std::wstring> &mods) {
    if (path.empty() || !utils::DirectoryExistsW(path))
        return 0;

    std::wstring p = path + L"\\*";

    _wfinddata_t fileinfo = {};
    auto handle = _wfindfirst(p.c_str(), &fileinfo);
    if (handle == -1)
        return 0;

    do {
        if ((fileinfo.attrib & _A_SUBDIR) == 0) {
            std::wstring fullPath = path + L"\\" + fileinfo.name;
            wchar_t filename[MAX_PATH];
            wchar_t ext[32];
            _wsplitpath(fileinfo.name, nullptr, nullptr, filename, ext);

            if (_wcsicmp(ext, L".zip") == 0) {
                std::wstring dest = m_TempDir;
                dest.append(L"\\Mods\\").append(filename);
                utils::ExtractZipW(fullPath, dest);
                ExploreMods(dest, mods);
            } else if (_wcsicmp(ext, L".bmodp") == 0) {
                mods.push_back(fullPath);
            }
        }
    } while (_wfindnext(handle, &fileinfo) == 0);

    _findclose(handle);

    return mods.size();
}

std::shared_ptr<void> ModManager::LoadLib(const wchar_t *path) {
    if (!path || path[0] == '\0')
        return nullptr;

    std::shared_ptr<void> dllHandlePtr;

    HMODULE dllHandle = ::LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
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
            ::FreeLibrary(static_cast<HMODULE>(ptr));
        });
        it->second = dllHandlePtr;
    }

    return dllHandlePtr;
}

bool ModManager::UnloadLib(void *dllHandle) {
    auto it = m_DllHandleToModsMap.find(dllHandle);
    if (it == m_DllHandleToModsMap.end())
        return false;

    for (auto *mod: it->second) {
        UnregisterMod(mod, m_ModToDllHandleMap[mod]);
    }
    return true;
}

IMod *ModManager::LoadMod(const std::wstring &path) {
    wchar_t filename[MAX_PATH];
    _wsplitpath(path.c_str(), nullptr, nullptr, filename, nullptr);

    auto dllHandle = LoadLib(path.c_str());
    if (!dllHandle) {
        m_Logger->Error("Failed to load %s.", filename);
        return nullptr;
    }

    constexpr const char *ENTRY_SYMBOL = "BMLEntry";
    typedef IMod *(*BMLEntryFunc)(IBML *);

    auto func = reinterpret_cast<BMLEntryFunc>(::GetProcAddress(static_cast<HMODULE>(dllHandle.get()), ENTRY_SYMBOL));
    if (!func) {
        m_Logger->Error("%s does not export the required symbol: %s.", filename, ENTRY_SYMBOL);
        return nullptr;
    }

    auto *bml = static_cast<IBML *>(this);
    IMod *mod = func(bml);
    if (!mod) {
        m_Logger->Error("No mod could be registered, %s will be unloaded.", filename);
        UnloadLib(dllHandle.get());
        return nullptr;
    }

    if (!RegisterMod(mod, dllHandle))
        return nullptr;

    return mod;
}

bool ModManager::UnloadMod(const std::string &id) {
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

void ModManager::RegisterBuiltinMods() {
    m_BMLMod = new BMLMod(this);
    RegisterMod(m_BMLMod, nullptr);

    m_BallTypeMod = new NewBallTypeMod(this);
    RegisterMod(m_BallTypeMod, nullptr);
}

bool ModManager::RegisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle) {
    if (!mod)
        return false;

    BMLVersion curVer;
    BMLVersion reqVer = mod->GetBMLVersion();
    if (curVer < reqVer) {
        m_Logger->Warn("Mod %s[%s] requires BML %d.%d.%d", mod->GetID(), mod->GetName(),
                       reqVer.major, reqVer.minor, reqVer.patch);
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

bool ModManager::UnregisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle) {
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

    auto func = reinterpret_cast<BMLExitFunc>(::GetProcAddress(static_cast<HMODULE>(dllHandle.get()), EXIT_SYMBOL));
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

void ModManager::FillCallbackMap(IMod *mod) {
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
        *reinterpret_cast<void ***>(mod)
    };

    int index = 0;
#define CHECK_V_FUNC(IDX, FUNC)                             \
    do {                                                    \
        auto idx = IDX;                                     \
        if (vtable[0][idx] != vtable[1][idx])               \
            m_CallbackMap[utils::TypeErase(FUNC)].push_back(mod);  \
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

void ModManager::AddDataPath(const char *path) {
    if (!path || path[0] == '\0')
        return;

    XString dataPath = path;
    if (!m_PathManager->PathIsAbsolute(dataPath)) {
        char buf[MAX_PATH];
        VxGetCurrentDirectory(buf);
        dataPath.Format("%s\\%s", buf, dataPath.CStr());
    }
    if (dataPath[dataPath.Length() - 1] != '\\')
        dataPath << '\\';

    if (utils::DirectoryExistsA(dataPath.CStr()) &&
        m_PathManager->GetPathIndex(DATA_PATH_IDX, dataPath) == -1) {
        m_PathManager->AddPath(DATA_PATH_IDX, dataPath);

        XString subDataPath1 = dataPath + "3D Entities\\";
        if (utils::DirectoryExistsA(subDataPath1.CStr()) &&
            m_PathManager->GetPathIndex(DATA_PATH_IDX, subDataPath1) == -1) {
            m_PathManager->AddPath(DATA_PATH_IDX, subDataPath1);
        }

        XString subDataPath2 = dataPath + "3D Entities\\PH\\";
        if (utils::DirectoryExistsA(subDataPath2.CStr()) &&
            m_PathManager->GetPathIndex(DATA_PATH_IDX, subDataPath2) == -1) {
            m_PathManager->AddPath(DATA_PATH_IDX, subDataPath2);
        }
    }

    XString texturePath = dataPath + "Textures\\";
    if (utils::DirectoryExistsA(texturePath.CStr()) &&
        m_PathManager->GetPathIndex(BITMAP_PATH_IDX, texturePath) == -1) {
        m_PathManager->AddPath(BITMAP_PATH_IDX, texturePath);
    }

    XString soundPath = dataPath + "Sounds\\";
    if (utils::DirectoryExistsA(soundPath.CStr()) &&
        m_PathManager->GetPathIndex(SOUND_PATH_IDX, soundPath) == -1) {
        m_PathManager->AddPath(SOUND_PATH_IDX, soundPath);
    }
}
