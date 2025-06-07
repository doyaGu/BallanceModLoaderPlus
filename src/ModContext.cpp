#include "ModContext.h"

#include <cstdlib>
#include <unordered_set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <io.h>

#include <utf8.h>
#include <oniguruma.h>

#include "BML/BML.h"
#include "BML/Timer.h"

#include "RenderHook.h"
#include "Overlay.h"
#include "Logger.h"
#include "StringUtils.h"
#include "PathUtils.h"

// Builtin Mods
#include "BMLMod.h"
#include "NewBallTypeMod.h"

extern HMODULE g_DllHandle;

using namespace BML;

ModContext *g_ModContext = nullptr;

ModContext *BML_GetModContext() {
    return g_ModContext;
}

CKContext *BML_GetCKContext() {
    return g_ModContext ? g_ModContext->GetCKContext() : nullptr;
}

CKRenderContext *BML_GetRenderContext() {
    return g_ModContext ? g_ModContext->GetRenderContext() : nullptr;
}

IDataShare *BML_GetDataShare(const char *name) {
    return g_ModContext ? g_ModContext->GetDataShare(name) : nullptr;
}

ModContext::ModContext(CKContext *context) {
    assert(context != nullptr);
    m_CKContext = context;
    m_DataShare = new DataShare("BML");
    g_ModContext = this;
}

ModContext::~ModContext() {
    Shutdown();
    m_DataShare->Release();
    g_ModContext = nullptr;
}

bool ModContext::Init() {
    if (IsInited())
        return true;

    InitDirectories();

    InitLogger();

    m_Logger->Info("Initializing Mod Loader Plus version " BML_VERSION);
    m_Logger->Info("Website: https://github.com/doyaGu/BallanceModContextPlus");

#ifdef _DEBUG
    m_Logger->Info("Player.exe Address: 0x%08x", ::GetModuleHandleA("Player.exe"));
    m_Logger->Info("CK2.dll Address: 0x%08x", ::GetModuleHandleA("CK2.dll"));
    m_Logger->Info("VxMath.dll Address: 0x%08x", ::GetModuleHandleA("VxMath.dll"));
#endif

    OnigEncoding encodings[3] = {ONIG_ENCODING_ASCII, ONIG_ENCODING_UTF8, ONIG_ENCODING_UTF16_LE};
    int err = onig_initialize(encodings, sizeof(encodings) / sizeof(encodings[0]));
    if (err < 0) {
        m_Logger->Error("Failed to initialize regular expression functionality");
        ShutdownLogger();
        return false;
    }

    if (!GetManagers()) {
        m_Logger->Error("Failed to get managers");
        onig_end();
        ShutdownLogger();
        return false;
    }

    if (!InitHooks()) {
        m_Logger->Error("Failed to initialize hooks");
        onig_end();
        ShutdownLogger();
        return false;
    }

    if (Overlay::ImGuiCreateContext() == nullptr) {
        m_Logger->Error("Failed to create ImGui context");
        ShutdownHooks();
        onig_end();
        ShutdownLogger();
        return false;
    }

    if (!Overlay::ImGuiInitPlatform(m_CKContext)) {
        m_Logger->Error("Failed to initialize Win32 platform backend for ImGui");
        Overlay::ImGuiDestroyContext();
        ShutdownHooks();
        onig_end();
        ShutdownLogger();
    }

    SetFlags(BML_INITED);
    return true;
}

void ModContext::Shutdown() {
    if (!IsInited())
        return;

    if (AreModsLoaded())
        UnloadMods();

    m_Logger->Info("Releasing Mod Loader");

    if (Overlay::GetImGuiContext() != nullptr) {
        Overlay::ImGuiShutdownPlatform(m_CKContext);
        Overlay::ImGuiDestroyContext();
    }

    ShutdownHooks();

    m_CKContext = nullptr;

    m_AttributeManager = nullptr;
    m_BehaviorManager = nullptr;
    m_CollisionManager = nullptr;
    m_InputManager = nullptr;
    m_MessageManager = nullptr;
    m_PathManager = nullptr;
    m_ParameterManager = nullptr;
    m_RenderManager = nullptr;
    m_SoundManager = nullptr;
    m_TimeManager = nullptr;

    utils::DeleteDirectoryW(m_TempDir);

    onig_end();

    m_Logger->Info("Goodbye!");

    ShutdownLogger();

    ClearFlags(BML_INITED);
}

bool ModContext::LoadMods() {
    if (!IsInited() || AreModsLoaded())
        return false;

    std::unordered_set<std::string> modSet;
    std::vector<IMod *> loadedMods;
    bool success = true;

    try {
        RegisterBuiltinMods();

        for (auto *mod : m_Mods) {
            const char *id = mod->GetID();
            modSet.emplace(id);
        }

        std::wstring path = m_LoaderDir + L"\\Mods";
        if (utils::DirectoryExistsW(path)) {
            std::vector<std::wstring> modPaths;
            if (ExploreMods(path, modPaths) == 0) {
                m_Logger->Info("No mod is found.");
            }

            for (auto &modPath : modPaths) {
                IMod *mod = LoadMod(modPath);
                if (mod) {
                    const char *id = mod->GetID();
                    if (modSet.find(id) != modSet.end()) {
                        m_Logger->Warn("Duplicate Mod: %s", id);
                        UnloadMod(id);
                        continue;
                    }
                    modSet.emplace(id);
                    loadedMods.push_back(mod);

                    auto [drive, dir] = utils::GetDriveAndDirectoryW(modPath);
                    std::wstring drivePath = drive + dir;
                    std::string ansiPath = utils::Utf16ToAnsi(drivePath);
                    AddDataPath(ansiPath.c_str());
                }
            }
        }

        SetFlags(BML_MODS_LOADED);
    } catch (const std::exception &e) {
        m_Logger->Error("Exception during mod loading: %s", e.what());
        success = false;

        // Rollback loaded mods if there was an error
        for (auto *mod : loadedMods) {
            UnloadMod(mod->GetID());
        }
    }

    return success;
}

void ModContext::UnloadMods() {
    if (!IsInited() || !AreModsLoaded())
        return;

    std::vector<std::string> modNames;
    modNames.reserve(m_Mods.size());
    for (auto *mod : m_Mods) {
        modNames.emplace_back(mod->GetID());
    }

    for (auto rit = modNames.rbegin(); rit != modNames.rend(); ++rit) {
        UnloadMod(*rit);
    }

    delete m_BallTypeMod;
    m_BallTypeMod = nullptr;

    delete m_BMLMod;
    m_BMLMod = nullptr;

    m_ModDependencies.clear();
    m_DependencyGraph.clear();

    ClearFlags(BML_MODS_LOADED);
}

bool ModContext::InitMods() {
    if (!IsInited() || !AreModsLoaded() || AreModsInited())
        return false;

    if (!ResolveDependencies()) {
        m_Logger->Error("Failed to resolve mod dependencies");
        return false;
    }

    for (IMod *mod : m_Mods) {
        m_Logger->Info("Loading Mod %s[%s] v%s by %s",
                       mod->GetID(), mod->GetName(), mod->GetVersion(), mod->GetAuthor());

        if (GetDependencyCount(mod) > 0 && CheckDependencies(mod) == 0) {
            m_Logger->Error("Dependencies not satisfied for mod %s", mod->GetID());
            continue; // Skip this mod but continue loading others
        }

        FillCallbackMap(mod);
        mod->OnLoad();
    }

    for (Config *config : m_Configs)
        SaveConfig(config);

    m_CommandContext.SortCommands();

    OnLoadGame();

    SetFlags(BML_MODS_INITED);
    return true;
}

void ModContext::ShutdownMods() {
    if (!IsInited() || !AreModsLoaded() || !AreModsInited())
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
    m_CommandContext.ClearCommands();

    ClearFlags(BML_MODS_INITED);
}

int ModContext::GetModCount() {
    return (int) m_Mods.size();
}

IMod *ModContext::GetMod(int index) {
    if (index < 0 || index >= (int) m_Mods.size())
        return nullptr;
    return m_Mods[index];
}

IMod *ModContext::FindMod(const char *id) const {
    if (!id)
        return nullptr;

    auto iter = m_ModMap.find(id);
    if (iter == m_ModMap.end())
        return nullptr;
    return iter->second;
}

int ModContext::RegisterDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) {
    if (!mod || !dependencyId) {
        return BML_ERROR_FAIL;
    }

    try {
        ModDependency dep;
        dep.id = BML_Strdup(dependencyId);
        dep.minVersion = BMLVersion(major, minor, patch);
        dep.optional = false;

        m_ModDependencies[mod].push_back(dep);
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int ModContext::RegisterOptionalDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) {
    if (!mod || !dependencyId) {
        return BML_ERROR_FAIL;
    }

    try {
        ModDependency dep;
        dep.id = BML_Strdup(dependencyId);
        dep.minVersion = BMLVersion(major, minor, patch);
        dep.optional = true;

        m_ModDependencies[mod].push_back(dep);
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int ModContext::CheckDependencies(IMod *mod) const {
    if (!mod) {
        return 0; // Not satisfied
    }

    try {
        auto it = m_ModDependencies.find(mod);
        if (it == m_ModDependencies.end() || it->second.empty()) {
            return 1; // No dependencies = satisfied
        }

        for (const auto &dep : it->second) {
            IMod *depMod = FindMod(dep.id);

            // Check if mod exists
            if (!depMod) {
                if (dep.optional) {
                    // Optional dependency is missing, but that's okay
                    continue;
                }
                // Required dependency is missing
                return 0;
            }

            // Check version
            BMLVersion modVersion;
            if (std::string(depMod->GetVersion()).find_first_not_of("0123456789.") == std::string::npos) {
                // Parse version if it's in the format "x.y.z"
                sscanf(depMod->GetVersion(), "%d.%d.%d", &modVersion.major, &modVersion.minor, &modVersion.patch);
            }

            if (modVersion < dep.minVersion) {
                if (dep.optional) {
                    // Optional dependency version is too old, but that's okay
                    continue;
                }
                // Required dependency version is too old
                return 0;
            }
        }

        return 1; // All dependencies satisfied
    } catch (...) {
        return 0; // Error, consider not satisfied
    }
}

int ModContext::GetDependencyCount(IMod *mod) const {
    if (!mod) {
        return -1;
    }

    try {
        auto it = m_ModDependencies.find(mod);
        if (it == m_ModDependencies.end()) {
            return 0;
        }

        return static_cast<int>(it->second.size());
    } catch (...) {
        return -1;
    }
}

int ModContext::GetDependencyInfo(IMod *mod, int index, char *dependencyId, int idSize,
                                  int *major, int *minor, int *patch, int *optional) const {
    if (!mod || index < 0) {
        return BML_ERROR_FAIL;
    }

    try {
        auto it = m_ModDependencies.find(mod);
        if (it == m_ModDependencies.end() || index >= static_cast<int>(it->second.size())) {
            return BML_ERROR_NOT_FOUND;
        }

        const auto &dep = it->second[index];

        if (dependencyId && idSize > 0) {
            strncpy(dependencyId, dep.id, idSize - 1);
            dependencyId[idSize - 1] = '\0';
        }

        if (major) *major = dep.minVersion.major;
        if (minor) *minor = dep.minVersion.minor;
        if (patch) *patch = dep.minVersion.patch;
        if (optional) *optional = dep.optional ? 1 : 0;

        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int ModContext::ClearDependencies(IMod *mod) {
    if (!mod) {
        return BML_ERROR_FAIL;
    }

    try {
        m_ModDependencies.erase(mod);
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

void ModContext::RegisterCommand(ICommand *cmd) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_CommandContext.RegisterCommand(cmd);
}

int ModContext::GetCommandCount() const {
    return static_cast<int>(m_CommandContext.GetCommandCount());
}

ICommand *ModContext::GetCommand(int index) const {
    return m_CommandContext.GetCommandByIndex(index);
}

ICommand *ModContext::FindCommand(const char *name) const {
    return m_CommandContext.GetCommandByName(name);
}

void ModContext::ExecuteCommand(const char *cmd) {
    if (!cmd || cmd[0] == '\0')
        return;

    const auto args = CommandContext::ParseCommandLine(cmd);
    if (args.empty()) {
        m_BMLMod->AddIngameMessage("Error: Empty command");
        return;
    }

    ICommand *command = FindCommand(args[0].c_str());
    if (!command) {
        m_BMLMod->AddIngameMessage(("Error: Unknown Command " + args[0]).c_str());
        return;
    }

    if (command->IsCheat() && !IsCheatEnabled()) {
        m_BMLMod->AddIngameMessage(("Error: Can not execute cheat command " + args[0]).c_str());
        return;
    }

    m_Logger->Info("Execute Command: %s", cmd);

    BroadcastCallback(&IMod::OnPreCommandExecute, command, args);
    command->Execute(this, args);
    BroadcastCallback(&IMod::OnPostCommandExecute, command, args);
}

bool ModContext::AddConfig(Config *config) {
    std::lock_guard<std::mutex> lock(m_Mutex);

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

bool ModContext::RemoveConfig(Config *config) {
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

Config *ModContext::GetConfig(IMod *mod) {
    if (!mod)
        return nullptr;

    auto it = m_ConfigMap.find(mod->GetID());
    if (it == m_ConfigMap.end())
        return nullptr;
    return it->second;
}

bool ModContext::LoadConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    std::wstring configPath = m_LoaderDir;
    configPath.append(L"\\Configs\\").append(utils::ToWString(mod->GetID())).append(L".cfg");
    return config->Load(configPath.c_str());
}

bool ModContext::SaveConfig(Config *config) {
    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    std::wstring configPath = m_LoaderDir;
    configPath.append(L"\\Configs\\").append(utils::ToWString(mod->GetID())).append(L".cfg");
    return config->Save(configPath.c_str());
}

const wchar_t *ModContext::GetDirectory(DirectoryType type) {
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

const char *ModContext::GetDirectoryUtf8(DirectoryType type) {
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

BML::IDataShare *ModContext::GetDataShare(const char *name) {
    if (!name)
        return m_DataShare;
    return BML::DataShare::GetInstance(name);
}

void ModContext::SetIC(CKBeObject *obj, bool hierarchy) {
    if (!obj)
        return;

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

void ModContext::RestoreIC(CKBeObject *obj, bool hierarchy) {
    if (!obj)
        return;

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

void ModContext::Show(CKBeObject *obj, CK_OBJECT_SHOWOPTION show, bool hierarchy) {
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

void ModContext::AddTimer(CKDWORD delay, std::function<void()> callback) {
    Delay(static_cast<size_t>(delay), callback, m_TimeManager->GetMainTickCount());
}

void ModContext::AddTimerLoop(CKDWORD delay, std::function<bool()> callback) {
    Interval(static_cast<size_t>(delay), callback, m_TimeManager->GetMainTickCount());
}

void ModContext::AddTimer(float delay, std::function<void()> callback) {
    Delay(delay / 1000.0f, callback, m_TimeManager->GetAbsoluteTime() / 1000.0f);
}

void ModContext::AddTimerLoop(float delay, std::function<bool()> callback) {
    Interval(delay / 1000.0f, callback, m_TimeManager->GetAbsoluteTime() / 1000.0f);
}

void ModContext::ExitGame() {
    OnExitGame();
    AddTimer(1ul, [this]() {
        ::PostMessage((HWND) m_CKContext->GetMainWindow(), 0x5FA, 0, 0);
    });
}

void ModContext::OpenModsMenu() {
    m_Logger->Info("Open Mods Menu");
    m_BMLMod->OpenModsMenu();
}

void ModContext::EnableCheat(bool enable) {
    if (AreFlagsSet(BML_CHEAT) != enable) {
        SetFlags(BML_CHEAT, enable);
        BroadcastCallback(&IMod::OnCheatEnabled, enable);
    }
}

void ModContext::SendIngameMessage(const char *msg) {
    m_CommandContext.Output(msg);
}

float ModContext::GetSRScore() {
    return m_BMLMod->GetSRScore();
}

int ModContext::GetHSScore() {
    return m_BMLMod->GetHSScore();
}

void ModContext::SkipRenderForNextTick() {
    RenderHook::DisableRender(true);
    AddTimer(1ul, []() { RenderHook::DisableRender(false); });
}

void ModContext::RegisterBallType(const char *ballFile, const char *ballId, const char *ballName, const char *objName,
                                  float friction, float elasticity, float mass, const char *collGroup,
                                  float linearDamp, float rotDamp, float force, float radius) {
    m_BallTypeMod->RegisterBallType(ballFile, ballId, ballName, objName, friction, elasticity,
                                    mass, collGroup, linearDamp, rotDamp, force, radius);
}

void ModContext::RegisterFloorType(const char *floorName, float friction, float elasticity, float mass,
                                   const char *collGroup, bool enableColl) {
    m_BallTypeMod->RegisterFloorType(floorName, friction, elasticity, mass, collGroup, enableColl);
}

void ModContext::RegisterModulBall(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                   const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                   float linearDamp, float rotDamp, float radius) {
    m_BallTypeMod->RegisterModulBall(modulName, fixed, friction, elasticity, mass, collGroup,
                                     frozen, enableColl, calcMassCenter, linearDamp, rotDamp, radius);
}

void ModContext::RegisterModulConvex(const char *modulName, bool fixed, float friction, float elasticity, float mass,
                                     const char *collGroup, bool frozen, bool enableColl, bool calcMassCenter,
                                     float linearDamp, float rotDamp) {
    m_BallTypeMod->RegisterModulConvex(modulName, fixed, friction, elasticity, mass, collGroup,
                                       frozen, enableColl, calcMassCenter, linearDamp, rotDamp);
}

void ModContext::RegisterTrafo(const char *modulName) {
    m_BallTypeMod->RegisterTrafo(modulName);
}

void ModContext::RegisterModul(const char *modulName) {
    m_BallTypeMod->RegisterModul(modulName);
}

void ModContext::OnProcess() {
    Timer::ProcessAll(m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime() / 1000.0f);
    BroadcastCallback(&IMod::OnProcess);
}

void ModContext::OnRender(CKRenderContext *dev) {
    BroadcastCallback(&IMod::OnRender, static_cast<CK_RENDER_FLAGS>(dev->GetCurrentRenderOptions()));
}

void ModContext::OnLoadGame() {
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

void ModContext::OnPreStartMenu() {
    BroadcastMessage("PreStartMenu", &IMod::OnPreStartMenu);
}

void ModContext::OnPostStartMenu() {
    BroadcastMessage("PostStartMenu", &IMod::OnPostStartMenu);
}

void ModContext::OnExitGame() {
    BroadcastMessage("ExitGame", &IMod::OnExitGame);
}

void ModContext::OnPreLoadLevel() {
    BroadcastMessage("PreLoadLevel", &IMod::OnPreLoadLevel);
}

void ModContext::OnPostLoadLevel() {
    BroadcastMessage("PostLoadLevel", &IMod::OnPostLoadLevel);
}

void ModContext::OnStartLevel() {
    BroadcastMessage("StartLevel", &IMod::OnStartLevel);
    ModifyFlags(BML_INGAME | BML_INLEVEL, BML_PAUSED);
}

void ModContext::OnPreResetLevel() {
    BroadcastMessage("PreResetLevel", &IMod::OnPreResetLevel);
    ClearFlags(BML_INLEVEL);
}

void ModContext::OnPostResetLevel() {
    BroadcastMessage("PostResetLevel", &IMod::OnPostResetLevel);
}

void ModContext::OnPauseLevel() {
    BroadcastMessage("PauseLevel", &IMod::OnPauseLevel);
    SetFlags(BML_PAUSED);
}

void ModContext::OnUnpauseLevel() {
    BroadcastMessage("UnpauseLevel", &IMod::OnUnpauseLevel);
    ClearFlags(BML_PAUSED);
}

void ModContext::OnPreExitLevel() {
    BroadcastMessage("PreExitLevel", &IMod::OnPreExitLevel);
}

void ModContext::OnPostExitLevel() {
    BroadcastMessage("PostExitLevel", &IMod::OnPostExitLevel);
    ClearFlags(BML_INGAME | BML_INLEVEL);
}

void ModContext::OnPreNextLevel() {
    BroadcastMessage("PreNextLevel", &IMod::OnPreNextLevel);
}

void ModContext::OnPostNextLevel() {
    BroadcastMessage("PostNextLevel", &IMod::OnPostNextLevel);
    ClearFlags(BML_INLEVEL);
}

void ModContext::OnDead() {
    BroadcastMessage("Dead", &IMod::OnDead);
    ClearFlags(BML_INGAME | BML_INLEVEL);
}

void ModContext::OnPreEndLevel() {
    BroadcastMessage("PreEndLevel", &IMod::OnPreEndLevel);
}

void ModContext::OnPostEndLevel() {
    BroadcastMessage("PostEndLevel", &IMod::OnPostEndLevel);
    ClearFlags(BML_INGAME | BML_INLEVEL);
}

void ModContext::OnCounterActive() {
    BroadcastMessage("CounterActive", &IMod::OnCounterActive);
}

void ModContext::OnCounterInactive() {
    BroadcastMessage("CounterInactive", &IMod::OnCounterInactive);
}

void ModContext::OnBallNavActive() {
    BroadcastMessage("BallNavActive", &IMod::OnBallNavActive);
}

void ModContext::OnBallNavInactive() {
    BroadcastMessage("BallNavInactive", &IMod::OnBallNavInactive);
}

void ModContext::OnCamNavActive() {
    BroadcastMessage("CamNavActive", &IMod::OnCamNavActive);
}

void ModContext::OnCamNavInactive() {
    BroadcastMessage("CamNavInactive", &IMod::OnCamNavInactive);
}

void ModContext::OnBallOff() {
    BroadcastMessage("BallOff", &IMod::OnBallOff);
}

void ModContext::OnPreCheckpointReached() {
    BroadcastMessage("PreCheckpoint", &IMod::OnPreCheckpointReached);
}

void ModContext::OnPostCheckpointReached() {
    BroadcastMessage("PostCheckpoint", &IMod::OnPostCheckpointReached);
}

void ModContext::OnLevelFinish() {
    BroadcastMessage("LevelFinish", &IMod::OnLevelFinish);
    ClearFlags(BML_INLEVEL);
}

void ModContext::OnGameOver() {
    BroadcastMessage("GameOver", &IMod::OnGameOver);
}

void ModContext::OnExtraPoint() {
    BroadcastMessage("ExtraPoint", &IMod::OnExtraPoint);
}

void ModContext::OnPreSubLife() {
    BroadcastMessage("PreSubLife", &IMod::OnPreSubLife);
}

void ModContext::OnPostSubLife() {
    BroadcastMessage("PostSubLife", &IMod::OnPostSubLife);
}

void ModContext::OnPreLifeUp() {
    BroadcastMessage("PreLifeUp", &IMod::OnPreLifeUp);
}

void ModContext::OnPostLifeUp() {
    BroadcastMessage("PostLifeUp", &IMod::OnPostLifeUp);
}

void ModContext::InitDirectories() {
    wchar_t path[MAX_PATH];
    wchar_t drive[4];
    wchar_t dir[MAX_PATH];

    // Set up working directory
    _wgetcwd(path, MAX_PATH);
    path[MAX_PATH - 1] = '\0';
    m_WorkingDir = path;
    m_WorkingDirUtf8 = utils::ToString(m_WorkingDir);

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
    m_GameDirUtf8 = utils::ToString(m_GameDir);

    // Set up loader directory
    m_LoaderDir = m_GameDir + L"\\ModLoader";
    if (!utils::DirectoryExistsW(m_LoaderDir)) {
        utils::CreateDirectoryW(m_LoaderDir);
    }
    m_LoaderDirUtf8 = utils::ToString(m_LoaderDir);

    // Set up temp directory
    ::GetTempPathW(MAX_PATH, path);
    wcsncat(path, L"BML", MAX_PATH);
    m_TempDir = path;
    if (!utils::DirectoryExistsW(m_TempDir)) {
        utils::CreateDirectoryW(m_TempDir);
    }
    m_TempDirUtf8 = utils::ToString(m_TempDir);

    // Set up config directory
    m_ConfigDir = m_LoaderDir + L"\\Configs";
    if (!utils::DirectoryExistsW(m_ConfigDir)) {
        utils::CreateDirectoryW(m_ConfigDir);
    }
    m_ConfigDirUtf8 = utils::ToString(m_ConfigDir);
}

void ModContext::InitLogger() {
    std::wstring logfilePath = m_LoaderDir + L"\\ModLoader.log";
    m_Logfile = _wfopen(logfilePath.c_str(), L"w");
    auto *logger = new Logger("ModLoader");
    Logger::SetDefault(logger);
    m_Logger = logger;

#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
#endif
}

void ModContext::ShutdownLogger() {
#ifdef _DEBUG
    FreeConsole();
#endif

    Logger::SetDefault(nullptr);
    delete m_Logger;
    fclose(m_Logfile);
}

extern bool HookObjectLoad();
extern bool HookPhysicalize();

extern bool UnhookObjectLoad();
extern bool UnhookPhysicalize();

bool ModContext::InitHooks() {
    bool result = true;

    m_InputHook = new InputHook(m_InputManager);
    if (!m_InputHook) {
        m_Logger->Error("Failed to create InputHook");
        return false;
    }

    bool objectLoadHookSuccess = HookObjectLoad();
    if (objectLoadHookSuccess) {
        m_Logger->Info("Hook ObjectLoad Success");
    } else {
        m_Logger->Error("Hook ObjectLoad Failed");
        result = false;
    }

    bool physicalizeHookSuccess = HookPhysicalize();
    if (physicalizeHookSuccess) {
        m_Logger->Info("Hook Physicalize Success");
    } else {
        m_Logger->Error("Hook Physicalize Failed");
        result = false;
    }

    if (!result) {
        if (objectLoadHookSuccess) UnhookObjectLoad();
        if (physicalizeHookSuccess) UnhookPhysicalize();
        delete m_InputHook;
        m_InputHook = nullptr;
    }

    return result;
}

bool ModContext::ShutdownHooks() {
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

bool ModContext::GetManagers() {
    m_AttributeManager = m_CKContext->GetAttributeManager();
    if (m_AttributeManager) {
        m_Logger->Info("Get Attribute Manager pointer 0x%08x", m_AttributeManager);
    } else {
        m_Logger->Info("Failed to get Attribute Manager");
        return false;
    }

    m_BehaviorManager = m_CKContext->GetBehaviorManager();
    if (m_BehaviorManager) {
        m_Logger->Info("Get Behavior Manager pointer 0x%08x", m_BehaviorManager);
    } else {
        m_Logger->Info("Failed to get Behavior Manager");
        return false;
    }

    m_CollisionManager = (CKCollisionManager *) m_CKContext->GetManagerByGuid(COLLISION_MANAGER_GUID);
    if (m_CollisionManager) {
        m_Logger->Info("Get Collision Manager pointer 0x%08x", m_CollisionManager);
    } else {
        m_Logger->Info("Failed to get Collision Manager");
        return false;
    }

    m_InputManager = (CKInputManager *) m_CKContext->GetManagerByGuid(INPUT_MANAGER_GUID);
    if (m_InputManager) {
        m_Logger->Info("Get Input Manager pointer 0x%08x", m_InputManager);
    } else {
        m_Logger->Info("Failed to get Input Manager");
        return false;
    }

    m_MessageManager = m_CKContext->GetMessageManager();
    if (m_MessageManager) {
        m_Logger->Info("Get Message Manager pointer 0x%08x", m_MessageManager);
    } else {
        m_Logger->Info("Failed to get Message Manager");
        return false;
    }

    m_PathManager = m_CKContext->GetPathManager();
    if (m_PathManager) {
        m_Logger->Info("Get Path Manager pointer 0x%08x", m_PathManager);
    } else {
        m_Logger->Info("Failed to get Path Manager");
        return false;
    }

    m_ParameterManager = m_CKContext->GetParameterManager();
    if (m_ParameterManager) {
        m_Logger->Info("Get Parameter Manager pointer 0x%08x", m_ParameterManager);
    } else {
        m_Logger->Info("Failed to get Parameter Manager");
        return false;
    }

    m_RenderManager = m_CKContext->GetRenderManager();
    if (m_RenderManager) {
        m_Logger->Info("Get Render Manager pointer 0x%08x", m_RenderManager);
    } else {
        m_Logger->Info("Failed to get Render Manager");
        return false;
    }

    m_SoundManager = (CKSoundManager *) m_CKContext->GetManagerByGuid(SOUND_MANAGER_GUID);
    if (m_SoundManager) {
        m_Logger->Info("Get Sound Manager pointer 0x%08x", m_SoundManager);
    } else {
        m_Logger->Info("Failed to get Sound Manager");
        return false;
    }

    m_TimeManager = m_CKContext->GetTimeManager();
    if (m_TimeManager) {
        m_Logger->Info("Get Time Manager pointer 0x%08x", m_TimeManager);
    } else {
        m_Logger->Info("Failed to get Time Manager");
        return false;
    }

    return true;
}

size_t ModContext::ExploreMods(const std::wstring &path, std::vector<std::wstring> &mods) {
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

                if (!utils::DirectoryExistsW(dest)) {
                    utils::CreateDirectoryW(dest);
                }

                if (utils::ExtractZipW(fullPath, dest)) {
                    ExploreMods(dest, mods);
                } else {
                    m_Logger->Error("Failed to extract zip file: %s", utils::Utf16ToAnsi(fullPath).c_str());
                }
            } else if (_wcsicmp(ext, L".bmodp") == 0) {
                mods.push_back(fullPath);
            }
        }
    } while (_wfindnext(handle, &fileinfo) == 0);

    _findclose(handle);

    return mods.size();
}

std::shared_ptr<void> ModContext::LoadLib(const wchar_t *path) {
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

bool ModContext::UnloadLib(void *dllHandle) {
    auto it = m_DllHandleToModsMap.find(dllHandle);
    if (it == m_DllHandleToModsMap.end())
        return false;

    for (auto *mod : it->second) {
        UnregisterMod(mod, m_ModToDllHandleMap[mod]);
    }

    m_DllHandleToModsMap.erase(it);
    return true;
}

IMod *ModContext::LoadMod(const std::wstring &path) {
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

bool ModContext::UnloadMod(const std::string &id) {
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

void ModContext::RegisterBuiltinMods() {
    m_BMLMod = new BMLMod(this);
    RegisterMod(m_BMLMod);

    m_BallTypeMod = new NewBallTypeMod(this);
    RegisterMod(m_BallTypeMod);
}

bool ModContext::RegisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle) {
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

bool ModContext::UnregisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle) {
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

bool ModContext::ResolveDependencies() {
    // Check if there are any dependencies to resolve
    bool hasDependencies = false;
    for (auto *mod : m_Mods) {
        auto it = m_ModDependencies.find(mod);
        if (it != m_ModDependencies.end() && !it->second.empty()) {
            hasDependencies = true;
            break;
        }
    }

    // If no dependencies, keep original order
    if (!hasDependencies) {
        return true;
    }

    // Build dependency graph
    m_DependencyGraph.clear();
    std::unordered_set<std::string> involvedInDependencies;

    for (auto *mod : m_Mods) {
        std::string modId = mod->GetID();

        auto it = m_ModDependencies.find(mod);
        if (it != m_ModDependencies.end() && !it->second.empty()) {
            std::vector<std::string> dependsOn;
            for (const auto &dep : it->second) {
                // Mark both this mod and its dependency as involved in dependency relationships
                involvedInDependencies.insert(modId);
                involvedInDependencies.insert(dep.id);

                if (!dep.optional) {
                    dependsOn.emplace_back(dep.id);
                }
            }

            m_DependencyGraph[modId] = dependsOn;
        } else {
            // Still add to graph for dependency resolution, but mark as having no dependencies
            m_DependencyGraph[modId] = {};
        }
    }

    // Check for circular dependencies
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> inProgress;

    for (const auto &pair : m_DependencyGraph) {
        const auto &modId = pair.first;
        if (visited.find(modId) == visited.end()) {
            if (HasCircularDependencies(modId, visited, inProgress)) {
                m_Logger->Error("Circular dependency detected involving mod %s", modId.c_str());
                return false;
            }
        }
    }

    // Preserve original order for mods not involved in dependencies
    std::vector<IMod *> newOrder;
    std::unordered_set<std::string> processedMods;

    // First add mods that aren't involved in any dependency relationships
    for (auto *mod : m_Mods) {
        std::string modId = mod->GetID();
        if (involvedInDependencies.find(modId) == involvedInDependencies.end()) {
            newOrder.push_back(mod);
            processedMods.insert(modId);
        }
    }

    // Then sort and add the mods involved in dependency relationships
    std::vector<IMod *> dependencyMods;
    for (auto *mod : m_Mods) {
        std::string modId = mod->GetID();
        if (processedMods.find(modId) == processedMods.end()) {
            dependencyMods.push_back(mod);
        }
    }

    if (!dependencyMods.empty()) {
        // Sort mods with dependencies
        std::unordered_map<std::string, IMod *> modMap;
        for (auto *mod : dependencyMods) {
            modMap[mod->GetID()] = mod;
        }

        std::vector<IMod *> sortedDependencyMods;
        std::unordered_set<std::string> visitedForSort;

        // Define a DFS function to traverse the dependency graph
        std::function<void(const std::string &)> dfs = [&](const std::string &modId) {
            if (visitedForSort.find(modId) != visitedForSort.end() ||
                processedMods.find(modId) != processedMods.end()) {
                return;
            }

            visitedForSort.insert(modId);

            // Visit all dependencies first
            auto it = m_DependencyGraph.find(modId);
            if (it != m_DependencyGraph.end()) {
                const auto &dependencies = it->second;
                for (const auto &depId : dependencies) {
                    if (m_DependencyGraph.find(depId) != m_DependencyGraph.end()) {
                        dfs(depId);
                    }
                }
            }

            // Add the mod after all its dependencies
            auto modIt = modMap.find(modId);
            if (modIt != modMap.end()) {
                sortedDependencyMods.push_back(modIt->second);
            }
        };

        // Process all dependency mods
        for (auto *mod : dependencyMods) {
            dfs(mod->GetID());
        }

        // Add the sorted dependency mods to the new order
        newOrder.insert(newOrder.end(), sortedDependencyMods.begin(), sortedDependencyMods.end());
    }

    // Check if we got all mods
    if (newOrder.size() != m_Mods.size()) {
        m_Logger->Error("Failed to sort mods by dependencies - mod count mismatch");
        return false;
    }

    // Update mod order
    m_Logger->Info("Reordering mods based on dependencies");
    m_Mods = newOrder;

    return true;
}

bool ModContext::HasCircularDependencies(const std::string &modId,
                                         std::unordered_set<std::string> &visited,
                                         std::unordered_set<std::string> &inProgress) {
    visited.insert(modId);
    inProgress.insert(modId);

    auto it = m_DependencyGraph.find(modId);
    if (it == m_DependencyGraph.end()) {
        inProgress.erase(modId);
        return false;
    }

    const auto &dependencies = it->second;
    for (const auto &depId : dependencies) {
        if (inProgress.find(depId) != inProgress.end()) {
            return true; // Circular dependency found
        }

        if (visited.find(depId) == visited.end()) {
            if (HasCircularDependencies(depId, visited, inProgress)) {
                return true;
            }
        }
    }

    inProgress.erase(modId);
    return false;
}

std::vector<IMod *> ModContext::SortModsByDependencies() {
    std::vector<IMod *> result;
    std::unordered_set<std::string> visited;
    std::unordered_map<std::string, IMod *> modMap;

    // Build mod map
    for (auto *mod : m_Mods) {
        modMap[mod->GetID()] = mod;
    }

    // Define a DFS function to traverse the dependency graph
    std::function<void(const std::string &)> dfs = [&](const std::string &modId) {
        if (visited.find(modId) != visited.end()) {
            return;
        }

        visited.insert(modId);

        // Visit all dependencies first
        auto it = m_DependencyGraph.find(modId);
        if (it != m_DependencyGraph.end()) {
            const auto &dependencies = it->second;
            for (const auto &depId : dependencies) {
                if (m_DependencyGraph.find(depId) != m_DependencyGraph.end()) {
                    dfs(depId);
                }
            }
        }

        // Add the mod after all its dependencies
        auto modIt = modMap.find(modId);
        if (modIt != modMap.end()) {
            result.push_back(modIt->second);
        }
    };

    // Process all mods
    for (const auto &pair: m_DependencyGraph) {
        dfs(pair.first);
    }

    return result;
}

void ModContext::FillCallbackMap(IMod *mod) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    class BlankMod : IMod {
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

void ModContext::AddDataPath(const char *path) {
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
