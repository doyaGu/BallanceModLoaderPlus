#include "ModContext.h"

#include <unordered_set>
#include <queue>
#include <system_error>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <io.h>

#include <utf8.h>
#include <oniguruma.h>

#include "BML/BML.h"
#include "BML/Timer.h"
#include "ImcEventSnapshot.h"
#include "BuiltinImcApis.h"

#include "RenderHook.h"
#include "Overlay.h"
#include "Logger.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScriptBindings.h"
#include "ScriptDevToolsService.h"
#include "ScriptMod.h"
#include "ScriptModEntryScanner.h"
#include "ScriptModHotReloadService.h"
#include "ScriptModLoader.h"
#include "ScriptModRuntime.h"
#include "ScriptReloadStagingCleanup.h"
#endif
#include "StringUtils.h"
#include "PathUtils.h"

// Builtin Mods
#include "BMLMod.h"
#include "NewBallTypeMod.h"

extern HMODULE g_DllHandle;

using namespace BML;

namespace {
    constexpr wchar_t kLoaderDirectoryName[] = L"ModLoader";
    constexpr wchar_t kTempDirectoryName[] = L"Temp";
    constexpr wchar_t kInstanceDirectoryName[] = L"Instance";
    constexpr wchar_t kPackagesDirectoryName[] = L"Packages";

    std::wstring GetParentDirectory(const std::wstring &path) {
        const std::wstring trimmed = utils::TrimTrailingSeparatorsW(path);
        const size_t separator = trimmed.find_last_of(L"\\/");
        if (separator == std::wstring::npos)
            return {};
        if (separator == 2 && trimmed.size() == 3 && trimmed[1] == L':')
            return {};

        return trimmed.substr(0, separator);
    }

    std::wstring ResolveGameDirectoryFromExecutable(const std::wstring &executablePath) {
        if (executablePath.empty())
            return {};

        return GetParentDirectory(GetParentDirectory(executablePath));
    }

    std::wstring ResolveLoaderDirectory(const std::wstring &gameDirectory) {
        if (gameDirectory.empty())
            return {};

        return utils::CombinePathW(gameDirectory, kLoaderDirectoryName);
    }

    std::wstring BuildFallbackTempDirectory(const std::wstring &baseTempDirectory, unsigned long processId) {
        wchar_t suffix[64] = {};
        _snwprintf(suffix, sizeof(suffix) / sizeof(suffix[0]), L"BML-%lu", processId);
        suffix[(sizeof(suffix) / sizeof(suffix[0])) - 1] = L'\0';
        return utils::CombinePathW(baseTempDirectory, suffix);
    }

    bool PrepareFreshDirectory(const std::wstring &directory) {
        if (directory.empty())
            return false;

        if (utils::FileExistsW(directory) && !utils::DeleteFileW(directory))
            return false;

        if (utils::DirectoryExistsW(directory) && !utils::DeleteDirectoryW(directory))
            return false;

        return utils::CreateFileTreeW(directory);
    }

    std::wstring CreateInstanceTempDirectory(const std::wstring &loaderDirectory, unsigned long processId) {
        std::wstring tempInstanceDirectory = utils::CreateTempFileW(L"BML");
        if (!tempInstanceDirectory.empty()) {
            utils::DeleteFileW(tempInstanceDirectory);
            if (utils::CreateDirectoryW(tempInstanceDirectory))
                return tempInstanceDirectory;

            tempInstanceDirectory.clear();
        }

        const std::wstring fallbackTempDirectory = BuildFallbackTempDirectory(utils::GetTempPathW(), processId);
        if (PrepareFreshDirectory(fallbackTempDirectory))
            return fallbackTempDirectory;

        const std::wstring loaderFallbackDirectory = utils::CombinePathW(
            utils::CombinePathW(loaderDirectory, kTempDirectoryName),
            kInstanceDirectoryName);
        if (utils::CreateFileTreeW(loaderFallbackDirectory))
            return loaderFallbackDirectory;

        return {};
    }

    std::wstring BuildZipExtractionDirectory(const std::wstring &tempDirectory, const std::wstring &archivePath) {
        if (tempDirectory.empty() || archivePath.empty())
            return {};

        std::wstring archiveName = utils::GetFileNameW(archivePath);
        const size_t extension = archiveName.find_last_of(L'.');
        if (extension != std::wstring::npos && extension != 0)
            archiveName.resize(extension);

        if (archiveName.empty())
            return {};

        return utils::CombinePathW(utils::CombinePathW(tempDirectory, kPackagesDirectoryName), archiveName);
    }

}

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

void ModContext::PublishImcEvent(int kind) {
    CaptureBuiltinImcEventNoexcept(*this, [kind](BML::ImcEventSnapshot &snapshot) {
        snapshot.Kind = kind;
    });
}

ModContext::ModContext(CKContext *context) {
    assert(context != nullptr);
    m_ImcRuntime.SetInvocationGate(&m_ModInvocationGate);
    m_CKContext = context;
    m_DataShare = DataShare::GetInstance("BML");
    if (m_DataShare) m_DataShare->AddRef();
#if BML_ENABLE_ANGELSCRIPT
    m_ScriptDevTools = std::make_unique<BML::ScriptDevToolsService>(this);
    m_ScriptHotReload = std::make_unique<BML::ScriptModHotReloadService>(this);
#endif
    g_ModContext = this;
}

ModContext::~ModContext() {
    Shutdown();
    if (m_DataShare) m_DataShare->Release();
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

    if (!Overlay::ImGuiCreateContext()) {
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
        return false;
    }

    SetFlags(BML_INITED);
#if BML_ENABLE_ANGELSCRIPT
    BML_TryRegisterAngelScriptBindings(this);
#endif
    return true;
}

void ModContext::Shutdown() {
    if (!IsInited())
        return;

    if (AreModsLoaded()) {
#if BML_ENABLE_ANGELSCRIPT
        if (m_ScriptHotReload)
            m_ScriptHotReload->Stop();
#endif
        ShutdownMods();
        UnloadMods();
    }

    m_ImcRuntime.Shutdown();

#if BML_ENABLE_ANGELSCRIPT
    BML_UnregisterAngelScriptBindings(this);
    if (m_ScriptDevTools)
        m_ScriptDevTools->Hide();
#endif

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
#if BML_ENABLE_ANGELSCRIPT
            CleanupStaleScriptReloadArtifacts(path, m_Logger);
#endif
            std::vector<std::wstring> modPaths;
            ExploreMods(path, modPaths);

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

#if BML_ENABLE_ANGELSCRIPT
            std::vector<BML::ScriptModLoadCandidate> scriptModCandidates;
            if (AreAngelScriptBindingsRegistered()) {
                ExploreScriptMods(path, scriptModCandidates);
                for (auto &scriptCandidate : scriptModCandidates) {
                    IMod *mod = LoadScriptMod(scriptCandidate);
                    if (mod) {
                        const char *id = mod->GetID();
                        if (modSet.find(id) != modSet.end()) {
                            m_Logger->Warn("Duplicate Mod: %s", id);
                            UnloadMod(id);
                            continue;
                        }
                        modSet.emplace(id);
                        loadedMods.push_back(mod);

                        std::string ansiPath = utils::Utf16ToAnsi(scriptCandidate.RootDirectory);
                        AddDataPath(ansiPath.c_str());
                    }
                }
            }
#endif

            if (modPaths.empty()
#if BML_ENABLE_ANGELSCRIPT
                && scriptModCandidates.empty()
#endif
            ) {
                m_Logger->Info("No mod is found.");
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
    if (!IsMainThread()) {
        if (m_Logger)
            m_Logger->Error("UnloadMods must run on the game thread.");
        return;
    }

#if BML_ENABLE_ANGELSCRIPT
    if (m_ScriptHotReload)
        m_ScriptHotReload->Stop();
#endif

    if (AreModsInited())
        ShutdownMods();

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

#if BML_ENABLE_ANGELSCRIPT
    m_ScriptMods.clear();
#endif

    m_ModDependencies.clear();

    ClearFlags(BML_MODS_LOADED);
}

bool ModContext::InitMods() {
    if (!IsInited() || !AreModsLoaded() || AreModsInited())
        return false;

    ClearFlags(BML_MODS_SHUTTING_DOWN);

    if (!ResolveDependencies())
        return false;

    for (IMod *mod : m_Mods) {
        m_Logger->Info("Loading Mod %s[%s] v%s by %s",
                       mod->GetID(), mod->GetName(), mod->GetVersion(), mod->GetAuthor());

        std::string dependencyDiagnostic;
        if (EvaluateDependencies(mod, &dependencyDiagnostic) == 0) {
            m_Logger->Error("Cannot initialize Mod %s: %s",
                            mod->GetID(),
                            dependencyDiagnostic.empty()
                                ? "dependencies are not satisfied."
                                : dependencyDiagnostic.c_str());
            continue; // Skip this mod but continue loading others
        }

        FillCallbackMap(mod);
        mod->OnLoad();
    }

#if BML_ENABLE_ANGELSCRIPT
    int scriptLoadedCount = 0;
    int scriptFailedCount = 0;
    BML::ScriptMod *firstFailedScript = nullptr;
    for (IMod *mod : m_Mods) {
        auto *scriptMod = dynamic_cast<BML::ScriptMod *>(mod);
        if (!scriptMod)
            continue;
        if (scriptMod->IsFailed()) {
            ++scriptFailedCount;
            if (!firstFailedScript)
                firstFailedScript = scriptMod;
        } else if (scriptMod->IsLoaded()) {
            ++scriptLoadedCount;
        }
    }
    if (scriptLoadedCount > 0 || scriptFailedCount > 0) {
        m_Logger->Info("BML script mod summary: loaded=%d failed=%d",
                       scriptLoadedCount,
                       scriptFailedCount);
        if (firstFailedScript) {
            m_Logger->Warn("First failed script mod %s: %s",
                           firstFailedScript->GetID(),
                           firstFailedScript->GetLastDiagnostic().c_str());
        }
    }
#endif

    for (Config *config : m_Configs)
        SaveConfig(config);

    m_CommandContext.SortCommands();

    OnLoadGame();

#if BML_ENABLE_ANGELSCRIPT
    if (m_ScriptHotReload)
        m_ScriptHotReload->Start();
#endif

    SetFlags(BML_MODS_INITED);
    return true;
}

void ModContext::ShutdownMods() {
    if (!IsInited() || !AreModsLoaded() || !AreModsInited())
        return;
    if (!IsMainThread()) {
        if (m_Logger)
            m_Logger->Error("ShutdownMods must run on the game thread.");
        return;
    }

    SetFlags(BML_MODS_SHUTTING_DOWN);
    auto invocationLock = LockModInvocation();

#if BML_ENABLE_ANGELSCRIPT
    if (m_ScriptHotReload)
        m_ScriptHotReload->Stop();
#endif

    for (auto rit = m_Mods.rbegin(); rit != m_Mods.rend(); ++rit) {
        auto *mod = *rit;
        try {
            mod->OnUnload();
        } catch (const std::exception &e) {
            if (m_Logger)
                m_Logger->Error("Exception in mod %s unload callback: %s", mod->GetID(), e.what());
        } catch (...) {
            if (m_Logger)
                m_Logger->Error("Unknown exception in mod %s unload callback", mod->GetID());
        }
    }

    Timer::CancelAll();
    if (m_TimeManager) {
        Timer::ProcessAll(m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime() / 1000.0f);
    } else {
        Timer::ProcessAll(0, 0.0f);
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
    std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
    return (int) m_Mods.size();
}

IMod *ModContext::GetMod(int index) {
    std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
    if (index < 0 || index >= (int) m_Mods.size())
        return nullptr;
    return m_Mods[index];
}

IMod *ModContext::FindMod(const char *id) const {
    if (!id)
        return nullptr;

    std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
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
        std::lock_guard<std::mutex> lock(m_Mutex);
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
        std::lock_guard<std::mutex> lock(m_Mutex);
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
    return EvaluateDependencies(mod, nullptr);
}

int ModContext::EvaluateDependencies(IMod *mod, std::string *diagnostic) const {
    if (diagnostic)
        diagnostic->clear();
    if (!mod) {
        if (diagnostic)
            *diagnostic = "the Mod pointer is null.";
        return 0;
    }

    auto invocationLock = LockModInvocation();

    // A relaxed semantic version parser:
    // - Reads up to three numeric parts.
    // - "1.2"     -> (1,2,0)
    // - "1.2.3-x" -> (1,2,3)  (suffix ignored)
    auto parseRelaxed = [](const char *s) -> BMLVersion {
        if (!s || !*s) return BMLVersion(0, 0, 0);
        int parts[3] = {0, 0, 0};
        int n = 0;
        const char *p = s;

        // Scan digits, allow arbitrary separators/suffix after a part
        while (*p && n < 3) {
            // Skip non-digits until we find a digit
            while (*p && (*p < '0' || *p > '9')) ++p;
            if (!*p) break;

            // Accumulate one integer part
            int val = 0;
            while (*p >= '0' && *p <= '9') {
                val = val * 10 + (*p - '0');
                ++p;
            }
            parts[n++] = val;

            // Skip until next digit or end (tolerate '.', '-', '+', etc.)
            while (*p && (*p < '0' || *p > '9')) ++p;
        }
        return BMLVersion(parts[0], parts[1], parts[2]);
    };

    try {
        struct DependencySnapshot {
            std::string Id;
            BMLVersion MinVersion;
            bool Optional = false;
            IMod *Mod = nullptr;
        };

        std::vector<DependencySnapshot> dependencies;
        {
            std::lock_guard<std::mutex> dependencyLock(m_Mutex);
            const auto it = m_ModDependencies.find(mod);
            if (it == m_ModDependencies.end())
                return 1;
            dependencies.reserve(it->second.size());
            for (const auto &dep : it->second) {
                if (dep.id && *dep.id)
                    dependencies.push_back({dep.id, dep.minVersion, dep.optional != 0, nullptr});
            }
        }

        /* The invocation gate keeps registered mod objects alive while the
         * registry lock is released. Never call a virtual mod method while
         * either registry mutex is held. */
        {
            std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
            for (auto &dependency : dependencies) {
                const auto found = m_ModMap.find(dependency.Id);
                if (found != m_ModMap.end())
                    dependency.Mod = found->second;
            }
        }

        for (const auto &dependency : dependencies) {
            IMod *depMod = dependency.Mod;
            if (!depMod) {
                if (!dependency.Optional) {
                    if (diagnostic)
                        *diagnostic = "required dependency '" + dependency.Id + "' is not installed.";
                    return 0;
                }
                continue;
            }
#if BML_ENABLE_ANGELSCRIPT
            if (BML::IsFailedScriptMod(depMod)) {
                if (!dependency.Optional) {
                    if (diagnostic)
                        *diagnostic = "required dependency '" + dependency.Id + "' failed to load.";
                    return 0;
                }
                continue;
            }
#endif
            const char *verStr = depMod->GetVersion();
            BMLVersion have = parseRelaxed(verStr);

            // If version is older than required and it's not optional -> not satisfied
            if (have < dependency.MinVersion) {
                if (!dependency.Optional) {
                    if (diagnostic) {
                        *diagnostic = "required dependency '" + dependency.Id + "' is version '" +
                                      (verStr ? verStr : "") + "'; version " +
                                      dependency.MinVersion.ToString() + " or newer is required.";
                    }
                    return 0;
                }
            }
        }
        return 1;
    } catch (const std::exception &e) {
        if (diagnostic)
            *diagnostic = std::string("dependency check raised an exception: ") + e.what();
        return 0;
    } catch (...) {
        if (diagnostic)
            *diagnostic = "dependency check raised an unknown exception.";
        return 0;
    }
}

int ModContext::GetDependencyCount(IMod *mod) const {
    if (!mod) {
        return -1;
    }

    try {
        std::lock_guard<std::mutex> lock(m_Mutex);
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
    if (!mod || index < 0 || (dependencyId && idSize <= 0)) {
        return BML_ERROR_FAIL;
    }

    try {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_ModDependencies.find(mod);
        if (it == m_ModDependencies.end() || index >= static_cast<int>(it->second.size())) {
            return BML_ERROR_NOT_FOUND;
        }

        const auto &dep = it->second[index];

        if (dependencyId && idSize > 0 && dep.id) {
            size_t depIdLen = strlen(dep.id);
            size_t copyLen = std::min(static_cast<size_t>(idSize - 1), depIdLen);

            if (copyLen > 0) {
                memcpy(dependencyId, dep.id, copyLen);
            }
            dependencyId[copyLen] = '\0'; // Guaranteed null termination
        } else if (dependencyId && idSize > 0) {
            dependencyId[0] = '\0'; // Empty string if no ID
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
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_ModDependencies.erase(mod);
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

void ModContext::RegisterCommand(ICommand *cmd) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_CommandContext.RegisterCommand(cmd)) {
        return;
    }

    if (!cmd) {
        m_Logger->Error("Failed to register a null command.");
        return;
    }

    m_Logger->Error(
        "Failed to register command '%s'. Command names are case-insensitive and must be valid UTF-8 tokens without spaces.",
        cmd->GetName().c_str());
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

    try {
        CaptureBuiltinImcEventNoexcept(*this, [&](BML::ImcEventSnapshot &event) {
            event.Kind = BML_EVENT_COMMAND_PRE;
            event.Command = args[0];
            event.CommandArgs = args;
        });
        BroadcastCallback(&IMod::OnPreCommandExecute, command, args);
        command->Execute(this, args);
        CaptureBuiltinImcEventNoexcept(*this, [&](BML::ImcEventSnapshot &event) {
            event.Kind = BML_EVENT_COMMAND_POST;
            event.Command = args[0];
            event.CommandArgs = args;
        });
        BroadcastCallback(&IMod::OnPostCommandExecute, command, args);
    } catch (const std::exception &e) {
        m_Logger->Error("Exception executing command '%s': %s", cmd, e.what());
        m_BMLMod->AddIngameMessage(("Error: Command failed - " + std::string(e.what())).c_str());
    } catch (...) {
        m_Logger->Error("Unknown exception executing command '%s'", cmd);
        m_BMLMod->AddIngameMessage("Error: Command failed with unknown exception");
    }
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
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (!config)
        return false;

    IMod *mod = config->GetMod();
    if (!mod)
        return false;

    auto it = m_ConfigMap.find(mod->GetID());
    if (it != m_ConfigMap.end()) {
        SaveConfig(config);
        m_Configs.erase(std::remove(m_Configs.begin(), m_Configs.end(), it->second), m_Configs.end());
        m_ConfigMap.erase(it);
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

BML_DataShare *ModContext::GetDataShare(const char *name) {
    if (!name || !*name)
        return reinterpret_cast<BML_DataShare *>(m_DataShare);
    return reinterpret_cast<BML_DataShare *>(BML::DataShare::GetInstance(name));
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
    if (!obj)
        return;

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
    if (!CanScheduleTimer())
        return;

    Delay(static_cast<size_t>(delay), callback, m_TimeManager->GetMainTickCount());
}

void ModContext::AddTimerLoop(CKDWORD delay, std::function<bool()> callback) {
    if (!CanScheduleTimer())
        return;

    Interval(static_cast<size_t>(delay), callback, m_TimeManager->GetMainTickCount());
}

void ModContext::AddTimer(float delay, std::function<void()> callback) {
    if (!CanScheduleTimer())
        return;

    Delay(delay / 1000.0f, callback, m_TimeManager->GetAbsoluteTime() / 1000.0f);
}

void ModContext::AddTimerLoop(float delay, std::function<bool()> callback) {
    if (!CanScheduleTimer())
        return;

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
    if (m_BMLMod)
        m_BMLMod->OpenModsMenu();
}

void ModContext::CloseModsMenu() {
    if (m_BMLMod)
        m_BMLMod->CloseModsMenu();
}

void ModContext::OpenMapMenu() {
    if (m_BMLMod)
        m_BMLMod->OpenMapMenu();
}

void ModContext::CloseMapMenu() {
    if (m_BMLMod)
        m_BMLMod->CloseMapMenu();
}

void ModContext::EnableCheat(bool enable) {
    if (m_RuntimeState.SetCheatEnabled(enable)) {
        CaptureBuiltinImcEventNoexcept(*this, [&](BML::ImcEventSnapshot &event) {
            event.Kind = BML_EVENT_CHEAT_CHANGED;
            event.CheatEnabled = enable;
        });
        BroadcastCallback(&IMod::OnCheatEnabled, enable);
    }
}

void ModContext::SendIngameMessage(const char *msg) {
    if (m_BMLMod)
        m_BMLMod->AddIngameMessage(msg ? msg : "");
}

void ModContext::ClearIngameMessages() {
    if (m_BMLMod)
        m_BMLMod->ClearIngameMessages();
}

float ModContext::GetSRScore() {
    return m_BMLMod ? m_BMLMod->GetSRTime() : 0.0f;
}

int ModContext::GetHSScore() {
    return m_BMLMod ? m_BMLMod->GetHSScore() : 0;
}

int ModContext::GetHUD() {
    return m_BMLMod ? m_BMLMod->GetHUD() : 0;
}

void ModContext::SetHUD(int mode) {
    if (m_BMLMod)
        m_BMLMod->SetHUD(mode);
}

void ModContext::ShowTitle(bool show) {
    if (m_BMLMod)
        m_BMLMod->ShowTitle(show);
}

void ModContext::ShowFPS(bool show) {
    if (m_BMLMod)
        m_BMLMod->ShowFPS(show);
}

void ModContext::ShowSRTimer(bool show) {
    if (m_BMLMod)
        m_BMLMod->ShowSRTimer(show);
}

void ModContext::StartSRTimer() {
    if (m_BMLMod)
        m_BMLMod->StartSRTimer();
}

void ModContext::PauseSRTimer() {
    if (m_BMLMod)
        m_BMLMod->PauseSRTimer();
}

void ModContext::ResetSRTimer() {
    if (m_BMLMod)
        m_BMLMod->ResetSRTimer();
}

float ModContext::GetSRTime() {
    return m_BMLMod ? m_BMLMod->GetSRTime() : 0.0f;
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
    if (!IsInited() || !m_TimeManager)
        return;

#if BML_ENABLE_ANGELSCRIPT
    BML_TryRegisterAngelScriptBindings(this);
    ProcessScriptModFailureCleanup();
    if (m_ScriptDevTools)
        m_ScriptDevTools->ProcessActions();
    if (m_ScriptHotReload)
        m_ScriptHotReload->Process();
    ProcessScriptModQueuedCallbacks();
#endif
    m_ImcRuntime.Pump();
    Timer::ProcessAll(m_TimeManager->GetMainTickCount(), m_TimeManager->GetAbsoluteTime() / 1000.0f);
    BroadcastCallback(&IMod::OnProcess);
}

void ModContext::OnRender(CKRenderContext *dev) {
    if (!IsInited() || !dev)
        return;

    BroadcastCallback(&IMod::OnRender, static_cast<CK_RENDER_FLAGS>(dev->GetCurrentRenderOptions()));
}

void ModContext::OnLoadGame() {
    CaptureBuiltinImcEventNoexcept(*this, [](BML::ImcEventSnapshot &event) {
        event.Kind = BML_EVENT_LOAD_OBJECT;
        event.Filename = "base.cmo";
        event.AddToScene = true;
        event.ReuseMeshes = true;
        event.ReuseMaterials = true;
        event.FilterClass = CKCID_3DOBJECT;
    });
    BroadcastCallback(&IMod::OnLoadObject, "base.cmo", false, "", CKCID_3DOBJECT,
                      true, true, true, false, nullptr, nullptr);

    int scriptCnt = m_CKContext->GetObjectsCountByClassID(CKCID_BEHAVIOR);
    CK_ID *scripts = m_CKContext->GetObjectsListByClassID(CKCID_BEHAVIOR);
    for (int i = 0; i < scriptCnt; i++) {
        auto *behavior = (CKBehavior *) m_CKContext->GetObject(scripts[i]);
        if (behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
            CaptureBuiltinImcEventNoexcept(*this, [&](BML::ImcEventSnapshot &event) {
                event.Kind = BML_EVENT_LOAD_SCRIPT;
                event.Filename = "base.cmo";
                event.Script = MakeBuiltinObjectRef(*this, behavior);
            });
            BroadcastCallback(&IMod::OnLoadScript, "base.cmo", behavior);
        }
    }
}

void ModContext::OnPreStartMenu() {
    PublishImcEvent(BML_EVENT_PRE_START_MENU);
    BroadcastMessage("PreStartMenu", &IMod::OnPreStartMenu);
}

void ModContext::OnPostStartMenu() {
    PublishImcEvent(BML_EVENT_POST_START_MENU);
    BroadcastMessage("PostStartMenu", &IMod::OnPostStartMenu);
}

void ModContext::OnExitGame() {
    PublishImcEvent(BML_EVENT_EXIT_GAME);
    BroadcastMessage("ExitGame", &IMod::OnExitGame);
}

void ModContext::OnPreLoadLevel() {
    PublishImcEvent(BML_EVENT_PRE_LOAD_LEVEL);
    BroadcastMessage("PreLoadLevel", &IMod::OnPreLoadLevel);
}

void ModContext::OnPostLoadLevel() {
    PublishImcEvent(BML_EVENT_POST_LOAD_LEVEL);
    BroadcastMessage("PostLoadLevel", &IMod::OnPostLoadLevel);
}

void ModContext::OnStartLevel() {
    PublishImcEvent(BML_EVENT_START_LEVEL);
    BroadcastMessage("StartLevel", &IMod::OnStartLevel);
    m_RuntimeState.Apply(BML::RuntimeStateTransition::EnterLevel);
}

void ModContext::OnPreResetLevel() {
    PublishImcEvent(BML_EVENT_PRE_RESET_LEVEL);
    BroadcastMessage("PreResetLevel", &IMod::OnPreResetLevel);
    m_RuntimeState.Apply(BML::RuntimeStateTransition::LeaveLevel);
}

void ModContext::OnPostResetLevel() {
    PublishImcEvent(BML_EVENT_POST_RESET_LEVEL);
    BroadcastMessage("PostResetLevel", &IMod::OnPostResetLevel);
}

void ModContext::OnPauseLevel() {
    PublishImcEvent(BML_EVENT_PAUSE_LEVEL);
    BroadcastMessage("PauseLevel", &IMod::OnPauseLevel);
    m_RuntimeState.Apply(BML::RuntimeStateTransition::Pause);
}

void ModContext::OnUnpauseLevel() {
    PublishImcEvent(BML_EVENT_UNPAUSE_LEVEL);
    BroadcastMessage("UnpauseLevel", &IMod::OnUnpauseLevel);
    m_RuntimeState.Apply(BML::RuntimeStateTransition::Resume);
}

void ModContext::OnPreExitLevel() {
    PublishImcEvent(BML_EVENT_PRE_EXIT_LEVEL);
    BroadcastMessage("PreExitLevel", &IMod::OnPreExitLevel);
}

void ModContext::OnPostExitLevel() {
    PublishImcEvent(BML_EVENT_POST_EXIT_LEVEL);
    BroadcastMessage("PostExitLevel", &IMod::OnPostExitLevel);
    m_RuntimeState.Apply(BML::RuntimeStateTransition::LeaveGame);
}

void ModContext::OnPreNextLevel() {
    PublishImcEvent(BML_EVENT_PRE_NEXT_LEVEL);
    BroadcastMessage("PreNextLevel", &IMod::OnPreNextLevel);
}

void ModContext::OnPostNextLevel() {
    PublishImcEvent(BML_EVENT_POST_NEXT_LEVEL);
    BroadcastMessage("PostNextLevel", &IMod::OnPostNextLevel);
    m_RuntimeState.Apply(BML::RuntimeStateTransition::LeaveLevel);
}

void ModContext::OnDead() {
    PublishImcEvent(BML_EVENT_DEAD);
    BroadcastMessage("Dead", &IMod::OnDead);
    m_RuntimeState.Apply(BML::RuntimeStateTransition::LeaveGame);
}

void ModContext::OnPreEndLevel() {
    PublishImcEvent(BML_EVENT_PRE_END_LEVEL);
    BroadcastMessage("PreEndLevel", &IMod::OnPreEndLevel);
}

void ModContext::OnPostEndLevel() {
    PublishImcEvent(BML_EVENT_POST_END_LEVEL);
    BroadcastMessage("PostEndLevel", &IMod::OnPostEndLevel);
    m_RuntimeState.Apply(BML::RuntimeStateTransition::LeaveGame);
}

void ModContext::OnCounterActive() {
    PublishImcEvent(BML_EVENT_COUNTER_ACTIVE);
    BroadcastMessage("CounterActive", &IMod::OnCounterActive);
}

void ModContext::OnCounterInactive() {
    PublishImcEvent(BML_EVENT_COUNTER_INACTIVE);
    BroadcastMessage("CounterInactive", &IMod::OnCounterInactive);
}

void ModContext::OnBallNavActive() {
    PublishImcEvent(BML_EVENT_BALL_NAV_ACTIVE);
    BroadcastMessage("BallNavActive", &IMod::OnBallNavActive);
}

void ModContext::OnBallNavInactive() {
    PublishImcEvent(BML_EVENT_BALL_NAV_INACTIVE);
    BroadcastMessage("BallNavInactive", &IMod::OnBallNavInactive);
}

void ModContext::OnCamNavActive() {
    PublishImcEvent(BML_EVENT_CAM_NAV_ACTIVE);
    BroadcastMessage("CamNavActive", &IMod::OnCamNavActive);
}

void ModContext::OnCamNavInactive() {
    PublishImcEvent(BML_EVENT_CAM_NAV_INACTIVE);
    BroadcastMessage("CamNavInactive", &IMod::OnCamNavInactive);
}

void ModContext::OnBallOff() {
    PublishImcEvent(BML_EVENT_BALL_OFF);
    BroadcastMessage("BallOff", &IMod::OnBallOff);
}

void ModContext::OnPreCheckpointReached() {
    PublishImcEvent(BML_EVENT_PRE_CHECKPOINT_REACHED);
    BroadcastMessage("PreCheckpoint", &IMod::OnPreCheckpointReached);
}

void ModContext::OnPostCheckpointReached() {
    PublishImcEvent(BML_EVENT_POST_CHECKPOINT_REACHED);
    BroadcastMessage("PostCheckpoint", &IMod::OnPostCheckpointReached);
}

void ModContext::OnLevelFinish() {
    PublishImcEvent(BML_EVENT_LEVEL_FINISH);
    BroadcastMessage("LevelFinish", &IMod::OnLevelFinish);
    m_RuntimeState.Apply(BML::RuntimeStateTransition::LeaveLevel);
}

void ModContext::OnGameOver() {
    PublishImcEvent(BML_EVENT_GAME_OVER);
    BroadcastMessage("GameOver", &IMod::OnGameOver);
}

void ModContext::OnExtraPoint() {
    PublishImcEvent(BML_EVENT_EXTRA_POINT);
    BroadcastMessage("ExtraPoint", &IMod::OnExtraPoint);
}

void ModContext::OnPreSubLife() {
    PublishImcEvent(BML_EVENT_PRE_SUB_LIFE);
    BroadcastMessage("PreSubLife", &IMod::OnPreSubLife);
}

void ModContext::OnPostSubLife() {
    PublishImcEvent(BML_EVENT_POST_SUB_LIFE);
    BroadcastMessage("PostSubLife", &IMod::OnPostSubLife);
}

void ModContext::OnPreLifeUp() {
    PublishImcEvent(BML_EVENT_PRE_LIFE_UP);
    BroadcastMessage("PreLifeUp", &IMod::OnPreLifeUp);
}

void ModContext::OnPostLifeUp() {
    PublishImcEvent(BML_EVENT_POST_LIFE_UP);
    BroadcastMessage("PostLifeUp", &IMod::OnPostLifeUp);
}

void ModContext::InitDirectories() {
    wchar_t path[MAX_PATH];

    // Set up working directory
    _wgetcwd(path, MAX_PATH);
    path[MAX_PATH - 1] = '\0';
    m_WorkingDir = path;
    m_WorkingDirUtf8 = utils::ToString(m_WorkingDir);

    // Set up game directory
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    path[MAX_PATH - 1] = '\0';
    m_GameDir = ResolveGameDirectoryFromExecutable(path);
    m_GameDirUtf8 = utils::ToString(m_GameDir);

    // Set up loader directory
    m_LoaderDir = ResolveLoaderDirectory(m_GameDir);
    utils::CreateFileTreeW(m_LoaderDir);
    m_LoaderDirUtf8 = utils::ToString(m_LoaderDir);

    // Set up temp directory
    m_TempDir = CreateInstanceTempDirectory(
        m_LoaderDir,
        static_cast<unsigned long>(::GetCurrentProcessId()));
    m_TempDirUtf8 = utils::ToString(m_TempDir);

    // Set up config directory
    m_ConfigDir = m_LoaderDir + L"\\Configs";
    utils::CreateFileTreeW(m_ConfigDir);
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
    if (m_Logfile)
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
    m_InputHook = nullptr;

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
            const std::wstring ext = utils::GetExtensionW(fullPath);

            if (_wcsicmp(ext.c_str(), L".zip") == 0) {
                const std::wstring dest = BuildZipExtractionDirectory(m_TempDir, fullPath);

                if (dest.empty() || !PrepareFreshDirectory(dest)) {
                    m_Logger->Error("Failed to create temp extraction directory: %s", utils::Utf16ToAnsi(dest).c_str());
                    continue;
                }

                if (utils::ExtractZipW(fullPath, dest)) {
                    ExploreMods(dest, mods);
                } else {
                    m_Logger->Error("Failed to extract zip file: %s", utils::Utf16ToAnsi(fullPath).c_str());
                }
            } else if (_wcsicmp(ext.c_str(), L".bmodp") == 0) {
                mods.push_back(fullPath);
            }
        }
    } while (_wfindnext(handle, &fileinfo) == 0);

    _findclose(handle);

    return mods.size();
}

#if BML_ENABLE_ANGELSCRIPT
size_t ModContext::ExploreScriptMods(const std::wstring &path, std::vector<BML::ScriptModLoadCandidate> &candidates) {
    const size_t before = candidates.size();
    BML::FindScriptModCandidates(path, candidates);

    if (path.empty() || !utils::DirectoryExistsW(path))
        return candidates.size() - before;

    const std::wstring pattern = path + L"\\*.zip";
    _wfinddata_t fileinfo = {};
    auto handle = _wfindfirst(pattern.c_str(), &fileinfo);
    if (handle == -1)
        return candidates.size() - before;

    do {
        if ((fileinfo.attrib & _A_SUBDIR) != 0)
            continue;

        const std::wstring zipPath = path + L"\\" + fileinfo.name;
        const std::wstring dest = BuildZipExtractionDirectory(m_TempDir, zipPath);
        if (dest.empty() || !PrepareFreshDirectory(dest)) {
            m_Logger->Error("Failed to create script package extraction directory: %s", utils::Utf16ToAnsi(dest).c_str());
            continue;
        }

        if (!utils::ExtractZipW(zipPath, dest)) {
            m_Logger->Error("Failed to extract script package zip: %s", utils::Utf16ToAnsi(zipPath).c_str());
            continue;
        }

        std::vector<BML::ScriptModLoadCandidate> packageCandidates;
        std::vector<std::wstring> rootEntries = BML::FindScriptModEntryPaths(dest);
        if (!rootEntries.empty()) {
            BML::ScriptModLoadCandidate candidate = BML::MakeDirectoryScriptModCandidate(
                dest,
                BML::ScriptModEntrySourceKind::ZipPackage,
                zipPath);
            candidate.EntryPaths = std::move(rootEntries);
            packageCandidates.push_back(std::move(candidate));
        }

        std::vector<BML::ScriptModLoadCandidate> nestedCandidates;
        BML::FindScriptModCandidates(dest, nestedCandidates);
        for (auto &candidate : nestedCandidates) {
            if (candidate.SourceKind == BML::ScriptModEntrySourceKind::Directory)
                packageCandidates.push_back(std::move(candidate));
        }

        if (packageCandidates.size() != 1 || packageCandidates.front().EntryPaths.size() != 1) {
            BML::ScriptModLoadCandidate candidate = BML::MakeDirectoryScriptModCandidate(
                dest,
                BML::ScriptModEntrySourceKind::ZipPackage,
                zipPath);
            candidate.EntryPaths.clear();
            for (const auto &packageCandidate : packageCandidates) {
                candidate.EntryPaths.insert(candidate.EntryPaths.end(),
                                            packageCandidate.EntryPaths.begin(),
                                            packageCandidate.EntryPaths.end());
            }
            candidates.push_back(std::move(candidate));
        } else {
            packageCandidates.front().SourceKind = BML::ScriptModEntrySourceKind::ZipPackage;
            packageCandidates.front().SourcePath = zipPath;
            candidates.push_back(std::move(packageCandidates.front()));
        }
    } while (_wfindnext(handle, &fileinfo) == 0);

    _findclose(handle);
    return candidates.size() - before;
}
#endif

std::shared_ptr<void> ModContext::LoadLib(const wchar_t *path) {
    if (!path || path[0] == '\0')
        return nullptr;

    std::shared_ptr<void> dllHandlePtr;

    HMODULE dllHandle = ::LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dllHandle) {
        const DWORD error = ::GetLastError();
        const std::string message = std::system_category().message(static_cast<int>(error));
        m_Logger->Error("Failed to load native Mod DLL %s: Windows error %lu (%s).",
                        utils::Utf16ToAnsi(path).c_str(),
                        static_cast<unsigned long>(error),
                        message.c_str());
        return nullptr;
    }

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

    std::vector<IMod *> mods = it->second;
    for (auto *mod : mods) {
        auto handleIt = m_ModToDllHandleMap.find(mod);
        std::shared_ptr<void> modDllHandle;
        if (handleIt != m_ModToDllHandleMap.end())
            modDllHandle = handleIt->second;

        UnregisterMod(mod, modDllHandle);
    }

    m_DllHandleToModsMap.erase(dllHandle);
    m_DllHandleMap.erase(dllHandle);
    return true;
}

IMod *ModContext::LoadMod(const std::wstring &path) {
    wchar_t filename[MAX_PATH];
    _wsplitpath(path.c_str(), nullptr, nullptr, filename, nullptr);

    auto dllHandle = LoadLib(path.c_str());
    if (!dllHandle)
        return nullptr;

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

#if BML_ENABLE_ANGELSCRIPT
IMod *ModContext::LoadScriptMod(const BML::ScriptModLoadCandidate &candidate) {
    BML::ScriptModLoader loader;
    BML::ScriptModLoadResult loadResult = loader.Load(this, GetCKContext(), candidate);
    auto &scriptMod = loadResult.Mod;
    if (!scriptMod) {
        m_Logger->Error("Script Mod could not be loaded due to allocation failure.");
        return nullptr;
    }

    IMod *mod = scriptMod.get();
    if (!RegisterMod(mod))
        return nullptr;

    RegisterScriptModDependencies(mod, loadResult.Definition);
    m_ScriptMods.push_back(std::move(scriptMod));
    if (m_ScriptHotReload)
        m_ScriptHotReload->RegisterMod(static_cast<BML::ScriptMod *>(mod));
    return mod;
}

void ModContext::RegisterScriptModDependencies(IMod *mod, const BML::ScriptModDefinition &definition) {
    if (!mod)
        return;

    for (const auto &dependency : definition.Dependencies) {
        if (dependency.Id.empty())
            continue;

        if (dependency.Optional) {
            RegisterOptionalDependency(mod,
                                       dependency.Id.c_str(),
                                       dependency.MinVersion.major,
                                       dependency.MinVersion.minor,
                                       dependency.MinVersion.patch);
        } else {
            RegisterDependency(mod,
                               dependency.Id.c_str(),
                               dependency.MinVersion.major,
                               dependency.MinVersion.minor,
                               dependency.MinVersion.patch);
        }
    }
}

void ModContext::ProcessScriptModFailureCleanup() {
    for (const auto &scriptMod : m_ScriptMods) {
        if (scriptMod)
            scriptMod->ProcessFailureCleanup();
    }
}

void ModContext::ProcessScriptModQueuedCallbacks() {
    for (const auto &scriptMod : m_ScriptMods) {
        if (scriptMod)
            scriptMod->ProcessQueuedScriptServiceCallbacks();
    }
}

bool ModContext::ValidateScriptModReloadDependencies(const BML::ScriptMod *mod,
                                                     const BML::ScriptModDefinition &candidate,
                                                     std::string &diagnostic,
                                                     std::vector<BML::ScriptModReloadDiagnosticField> *fields) const {
    auto invocationLock = LockModInvocation();
    std::lock_guard<std::mutex> dependencyLock(m_Mutex);
    std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
    auto addField = [&](const std::string &key, const std::string &value) {
        if (fields)
            fields->push_back({key, value});
    };
    auto addDependencyBoundary = [&](const BML::ScriptModDependency &dependency,
                                     const char *action) {
        addField("boundary", "dependency_graph");
        addField("cascade", "false");
        addField("dependency", dependency.Id);
        addField("action", action ? action : "restart_or_reload_dependency");
    };

    if (!mod) {
        diagnostic = "Script mod reload target is missing.";
        addField("boundary", "reload_target");
        addField("action", "restart_required");
        return false;
    }

    const char *currentId = const_cast<BML::ScriptMod *>(mod)->GetID();
    if (candidate.Id.empty()) {
        diagnostic = "Script mod reload candidate has an empty id.";
        addField("boundary", "mod_identity");
        addField("action", "fix_metadata");
        return false;
    }
    if (currentId && candidate.Id != currentId) {
        auto existing = m_ModMap.find(candidate.Id);
        if (existing != m_ModMap.end() && existing->second != mod) {
            diagnostic = "Script mod failed-load recovery id '" + candidate.Id + "' conflicts with an already registered mod.";
            addField("boundary", "mod_identity");
            addField("conflict", candidate.Id);
            addField("action", "restart_required");
            return false;
        }
    }

    for (const auto &dependency : candidate.Dependencies) {
        if (dependency.Id.empty())
            continue;
        auto dependencyIt = m_ModMap.find(dependency.Id);
        if (dependencyIt == m_ModMap.end()) {
            if (!dependency.Optional) {
                diagnostic = "Script mod reload dependency '" + dependency.Id + "' is missing. "
                             "Hot reload only refreshes already registered script mods; it does not discover or load new dependency graph nodes. "
                             "Restart after adding dependencies.";
                addDependencyBoundary(dependency, "restart_after_adding_dependency");
                return false;
            }
            continue;
        }

        IMod *dependencyMod = dependencyIt->second;
        if (!dependencyMod) {
            if (!dependency.Optional) {
                diagnostic = "Script mod reload dependency '" + dependency.Id + "' is unavailable.";
                addDependencyBoundary(dependency, "restart_or_reload_dependency");
                return false;
            }
            continue;
        }
#if BML_ENABLE_ANGELSCRIPT
        if (BML::IsFailedScriptMod(dependencyMod)) {
            if (!dependency.Optional) {
                diagnostic = "Script mod reload dependency '" + dependency.Id + "' is failed. "
                             "Hot reload does not repair or cascade reload required dependencies; fix and reload the dependency first, or restart.";
                addDependencyBoundary(dependency, "fix_and_reload_dependency_or_restart");
                return false;
            }
            continue;
        }
#endif
        const BMLVersion have = BML::ParseBmlVersion(dependencyMod->GetVersion() ? dependencyMod->GetVersion() : "0.0.0");
        if (have < dependency.MinVersion && !dependency.Optional) {
            diagnostic = "Script mod reload dependency '" + dependency.Id + "' is older than required. "
                         "Hot reload does not cascade reload dependencies; update/reload that dependency first, or restart.";
            addDependencyBoundary(dependency, "update_or_reload_dependency_or_restart");
            return false;
        }
    }

    const BMLVersion candidateVersion = BML::ParseBmlVersion(candidate.Version);
    for (const auto &entry : m_ModDependencies) {
        IMod *dependent = entry.first;
        if (!dependent || dependent == mod)
            continue;

        for (const auto &dependency : entry.second) {
            if (!dependency.id || std::string(dependency.id) != candidate.Id)
                continue;
            if (!dependency.optional && candidateVersion < dependency.minVersion) {
                diagnostic = "Script mod reload version would no longer satisfy dependent mod '";
                diagnostic += dependent->GetID() ? dependent->GetID() : "";
                diagnostic += "'. Hot reload does not cascade reload dependent mods; restart or reload dependent mods explicitly.";
                addField("boundary", "dependent_compatibility");
                addField("cascade", "false");
                addField("dependent", dependent->GetID() ? dependent->GetID() : "");
                addField("action", "restart_or_reload_dependents_explicitly");
                return false;
            }
        }
    }

    return true;
}

bool ModContext::PromoteFailedScriptModPlaceholder(BML::ScriptMod *mod,
                                                   const std::string &oldId,
                                                   const BML::ScriptModDefinition &candidate,
                                                   std::string &diagnostic) {
    if (!mod) {
        diagnostic = "Script mod failed-load recovery target is missing.";
        return false;
    }
    if (!mod->IsFailedPlaceholder()) {
        diagnostic = "Script mod id changes require restart after the mod has loaded once.";
        return false;
    }
    if (oldId.empty() || candidate.Id.empty()) {
        diagnostic = "Script mod failed-load recovery requires non-empty old and new ids.";
        return false;
    }

    {
        std::unique_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        auto oldIt = m_ModMap.find(oldId);
        if (oldIt == m_ModMap.end() || oldIt->second != mod) {
            diagnostic = "Script mod failed-load recovery lost its placeholder registration.";
            return false;
        }

        auto newIt = m_ModMap.find(candidate.Id);
        if (newIt != m_ModMap.end() && newIt->second != mod) {
            diagnostic = "Script mod failed-load recovery id '" + candidate.Id + "' conflicts with an already registered mod.";
            return false;
        }

        if (candidate.Id != oldId) {
            m_ModMap.erase(oldIt);
            m_ModMap.emplace(candidate.Id, mod);
        }
    }

    ClearDependencies(mod);
    RegisterScriptModDependencies(mod, candidate);
    return true;
}

void ModContext::RestoreFailedScriptModPlaceholder(BML::ScriptMod *mod,
                                                   const std::string &currentId,
                                                   const BML::ScriptModDefinition &oldDefinition) {
    if (!mod)
        return;

    {
        std::unique_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        if (!currentId.empty()) {
            auto currentIt = m_ModMap.find(currentId);
            if (currentIt != m_ModMap.end() && currentIt->second == mod)
                m_ModMap.erase(currentIt);
        }

        if (!oldDefinition.Id.empty())
            m_ModMap[oldDefinition.Id] = mod;
    }

    ClearDependencies(mod);
    RegisterScriptModDependencies(mod, oldDefinition);
}

bool ModContext::QueueScriptModReload(const std::string &id,
                                      const BML::ScriptModReloadOptions &options,
                                      std::string &message) {
    if (!m_ScriptHotReload) {
        message = "Script hot reload service is unavailable.";
        return false;
    }
    return m_ScriptHotReload->QueueReload(id, options, message);
}

bool ModContext::QueueScriptLibraryReload(const std::string &id,
                                          const std::string &version,
                                          const BML::ScriptModReloadOptions &options,
                                          std::string &message) {
    if (!m_ScriptHotReload) {
        message = "Script hot reload service is unavailable.";
        return false;
    }
    return m_ScriptHotReload->QueueReloadLibrary(id, version, options, message);
}

size_t ModContext::QueueAllScriptModReloads(const BML::ScriptModReloadOptions &options) {
    return m_ScriptHotReload ? m_ScriptHotReload->QueueReloadAll(options) : 0;
}

bool ModContext::SetScriptHotReloadAutomatic(bool enabled) {
    return m_ScriptHotReload && m_ScriptHotReload->SetAutomaticEnabled(enabled);
}

bool ModContext::SetScriptHotReloadWatching(bool enabled) {
    return SetScriptHotReloadAutomatic(enabled);
}

std::string ModContext::GetScriptHotReloadStatus() const {
    return m_ScriptHotReload ? m_ScriptHotReload->GetStatus() : "script hot reload: unavailable";
}

void ModContext::RenderScriptDevToolsPanel() {
    if (m_ScriptDevTools)
        m_ScriptDevTools->RenderPanel();
}

void ModContext::PublishScriptDevLogEvent(const char *level, const char *endpoint, const std::string &message) {
    if (m_ScriptDevTools)
        m_ScriptDevTools->PublishLogLine(level, endpoint, message);
}

void ModContext::PublishScriptDevDiagnostic(BML::ScriptDevEventSeverity severity,
                                            const std::string &code,
                                            const std::string &modId,
                                            const BML::ScriptDiagnostic &diagnostic) {
    if (m_ScriptDevTools)
        m_ScriptDevTools->PublishDiagnostic(severity, code, modId, diagnostic);
}
#endif

bool ModContext::UnloadMod(const std::string &id) {
    if (!IsMainThread()) {
        if (m_Logger)
            m_Logger->Error("UnloadMod must run on the game thread.");
        return false;
    }
    IMod *mod = nullptr;
    std::shared_ptr<void> dllHandle;
    {
        std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        auto it = m_ModMap.find(id);
        if (it == m_ModMap.end())
            return false;
        mod = it->second;
        auto dit = m_ModToDllHandleMap.find(mod);
        if (dit != m_ModToDllHandleMap.end())
            dllHandle = dit->second;
    }

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
    // Allow registering built-in mods that don't come from a DLL (dllHandle can be null).
    if (!mod) {
        m_Logger->Error("Mod registration failed: the Mod pointer is null.");
        return false;
    }

    const char *modId = mod->GetID();
    if (!modId || !*modId) {
        m_Logger->Error("Mod registration failed: GetID() returned an empty id.");
        return false;
    }

    BMLVersion curVer;
    BMLVersion reqVer = mod->GetBMLVersion();
    if (curVer < reqVer) {
        m_Logger->Warn("Mod %s[%s] requires BML %d.%d.%d", mod->GetID(), mod->GetName(),
                       reqVer.major, reqVer.minor, reqVer.patch);
        return false;
    }

    if (m_ModInvocationGate.IsCallActiveOnCurrentThread()) {
        m_Logger->Error("Mod %s cannot be registered from an active Mod callback.", modId);
        return false;
    }
    auto invocationLock = m_ModInvocationGate.LockMutation();
    std::unique_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);

    // Reject duplicates
    if (m_ModMap.find(modId) != m_ModMap.end()) {
        m_Logger->Error("Mod registration failed: duplicate id %s.", modId);
        return false;
    }

    // Record the mod in our registries
    m_Mods.push_back(mod);
    m_ModMap.emplace(modId, mod);

    // If there is a real DLL handle, wire up the handle <-> mod mappings
    if (dllHandle) {
        m_ModToDllHandleMap[mod] = dllHandle;

        void *raw = dllHandle.get();
        m_DllHandleToModsMap[raw].push_back(mod);

        // Keep a weak reference so we can check liveness without owning it
        m_DllHandleMap[raw] = dllHandle;
    }

    return true;
}

std::string ModContext::GetNativeImcOwnerId(
    const void *callerAddress, const char *requestedOwnerId) const {
    if (!callerAddress)
        return {};

    HMODULE callerModule = nullptr;
    if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCSTR>(callerAddress),
                              &callerModule)) {
        return {};
    }

    std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);

    HMODULE bmlModule = nullptr;
    if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCSTR>(&BML_GetModContext),
                             &bmlModule) &&
        callerModule == bmlModule) {
        if (!requestedOwnerId || !*requestedOwnerId || !m_BMLMod)
            return {};
        const auto requested = m_ModMap.find(requestedOwnerId);
        return requested != m_ModMap.end() && requested->second == m_BMLMod
                   ? requested->first
                   : std::string();
    }

    const auto owners = m_DllHandleToModsMap.find(callerModule);
    if (owners == m_DllHandleToModsMap.end())
        return {};

    if (requestedOwnerId && *requestedOwnerId) {
        const auto requested = m_ModMap.find(requestedOwnerId);
        if (requested == m_ModMap.end() || !requested->second ||
            std::find(owners->second.begin(), owners->second.end(),
                      requested->second) == owners->second.end())
            return {};
        return requested->first;
    }

    if (owners->second.size() != 1 || !owners->second.front())
        return {};

    IMod *owner = owners->second.front();
    const auto id = std::find_if(m_ModMap.begin(), m_ModMap.end(), [owner](const auto &entry) {
        return entry.second == owner;
    });
    return id == m_ModMap.end() ? std::string() : id->first;
}

bool ModContext::UnregisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle) {
    if (!mod) {
        return false;
    }
    if (!IsMainThread())
        return false;

    if (m_ModInvocationGate.IsCallActiveOnCurrentThread())
        return false;
    auto invocationLock = m_ModInvocationGate.LockMutation();

    try {
        {
            std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
            if (std::find(m_Mods.begin(), m_Mods.end(), mod) == m_Mods.end())
                return false;
        }
        const char *modId = mod->GetID();
        if (!modId) {
            return false;
        }
        const std::string modIdCopy = modId;
        m_ImcRuntime.CleanupOwner(modIdCopy);
#if BML_ENABLE_ANGELSCRIPT
        if (m_ScriptHotReload) {
            if (auto *scriptMod = dynamic_cast<BML::ScriptMod *>(mod))
                m_ScriptHotReload->UnregisterMod(scriptMod);
        }
#endif
        std::shared_ptr<void> ownedDllHandle = dllHandle;
        if (!ownedDllHandle) {
            std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
            const auto handleIt = m_ModToDllHandleMap.find(mod);
            if (handleIt != m_ModToDllHandleMap.end())
                ownedDllHandle = handleIt->second;
        }

        void *rawDllHandle = ownedDllHandle.get();
        bool dllHandleStillHasMods = false;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            std::unique_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);

            // Remove from callback map to prevent dangling pointer in BroadcastCallback
            for (auto &kv : m_CallbackMap) {
                auto &vec = kv.second;
                vec.erase(std::remove(vec.begin(), vec.end(), mod), vec.end());
            }

            // Remove from mod map
            auto it = m_ModMap.find(modId);
            if (it != m_ModMap.end()) {
                m_ModMap.erase(it);
            }

            // Remove from mod vector
            auto oit = std::find(m_Mods.begin(), m_Mods.end(), mod);
            if (oit != m_Mods.end()) {
                m_Mods.erase(oit);
            }

            m_ModDependencies.erase(mod);

            m_ModToDllHandleMap.erase(mod);

            if (rawDllHandle) {
                auto mit = m_DllHandleToModsMap.find(rawDllHandle);
                if (mit != m_DllHandleToModsMap.end()) {
                    auto &mods = mit->second;
                    mods.erase(std::remove(mods.begin(), mods.end(), mod), mods.end());
                    dllHandleStillHasMods = !mods.empty();
                    if (mods.empty())
                        m_DllHandleToModsMap.erase(mit);
                }
                if (!dllHandleStillHasMods)
                    m_DllHandleMap.erase(rawDllHandle);
            }
        }

        invocationLock.unlock();
        if (rawDllHandle) {
            // Call BMLExit function safely
            constexpr const char *EXIT_SYMBOL = "BMLExit";
            typedef void (*BMLExitFunc)(IMod *);

            try {
                auto func = reinterpret_cast<BMLExitFunc>(::GetProcAddress(static_cast<HMODULE>(rawDllHandle), EXIT_SYMBOL));
                if (func) {
                    func(mod);
                }
            } catch (...) {
                // Continue cleanup even if BMLExit fails
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool ModContext::ResolveDependencies() {
    // Build a stable position map to keep deterministic ordering for nodes with the same in-degree
    std::unordered_map<std::string, size_t> pos;
    std::unordered_map<std::string, IMod *> modMap;
    pos.reserve(m_Mods.size());
    modMap.reserve(m_Mods.size());

    for (size_t i = 0; i < m_Mods.size(); ++i) {
        IMod *m = m_Mods[i];
        if (!m || !m->GetID() || !*m->GetID()) {
            if (m_Logger)
                m_Logger->Error("Cannot resolve Mod dependencies: registry entry %zu has no valid Mod id.", i);
            return false;
        }
        std::string id = m->GetID();
        pos[id] = i;
        modMap[id] = m;
    }

    // adj: dependency -> [dependents]
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, int> inDegree;

    // Initialize in-degrees to 0 for all known mods
    for (auto &kv : modMap) inDegree[kv.first] = 0;

    // Collect edges and compute in-degree as "number of (present) dependencies" for each mod
    for (IMod *m : m_Mods) {
        const std::string mid = m->GetID();
        auto it = m_ModDependencies.find(m);
        if (it == m_ModDependencies.end()) continue;

        std::unordered_set<std::string> seen; // deduplicate per mod
        for (const auto &dep : it->second) {
            if (!dep.id || !*dep.id) continue;
            const std::string depId(dep.id);
            if (!seen.insert(depId).second) continue; // skip duplicate dependency

            auto depInSet = modMap.find(depId);
            if (depInSet == modMap.end()) {
                if (!dep.optional) {
                    if (m_Logger) {
                        m_Logger->Error(
                            "Cannot initialize Mod %s: required dependency '%s' version %s or newer is not installed.",
                            mid.c_str(), depId.c_str(), dep.minVersion.ToString().c_str());
                    }
                    return false;
                }
                continue;
            }

            adj[depId].push_back(mid);               // dep -> dependent
            ++inDegree[mid];
        }
    }

    // Make adjacency stable (respect original m_Mods order for deterministic results)
    for (auto &kv : adj) {
        auto &v = kv.second;
        std::stable_sort(v.begin(), v.end(), [&](const std::string &a, const std::string &b) {
            return pos[a] < pos[b];
        });
    }

    // Kahn's algorithm with stable seeding by original order
    std::queue<std::string> q;
    // Seed queue with all nodes with inDegree == 0 in original order
    for (IMod *m : m_Mods) {
        const std::string id = m->GetID();
        if (inDegree[id] == 0) q.push(id);
    }

    std::vector<IMod *> sorted;
    sorted.reserve(m_Mods.size());

    while (!q.empty()) {
        std::string cur = q.front();
        q.pop();
        sorted.push_back(modMap[cur]);

        auto ait = adj.find(cur);
        if (ait == adj.end()) continue;

        for (const std::string &nxt : ait->second) {
            if (--inDegree[nxt] == 0) q.push(nxt);
        }
    }

    // If not all nodes were processed, a cycle exists. Nodes remaining with a
    // non-zero in-degree either belong to the cycle or depend on one.
    if (sorted.size() != m_Mods.size()) {
        std::string affected;
        for (IMod *mod : m_Mods) {
            const char *id = mod ? mod->GetID() : nullptr;
            const auto entry = id ? inDegree.find(id) : inDegree.end();
            if (entry == inDegree.end() || entry->second == 0)
                continue;
            if (!affected.empty())
                affected += ", ";
            affected += "'";
            affected += id;
            affected += "'";
        }
        if (m_Logger) {
            m_Logger->Error(
                "Cannot resolve Mod dependencies: a dependency cycle involves or blocks %s.",
                affected.empty() ? "one or more Mods" : affected.c_str());
        }
        return false;
    }

    m_Mods.swap(sorted);
    return true;
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

bool ModContext::CanScheduleTimer() const {
    return IsInited() && m_TimeManager != nullptr && !AreFlagsSet(BML_MODS_SHUTTING_DOWN);
}
