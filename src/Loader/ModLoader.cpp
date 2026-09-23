#include "Loader/ModLoader.h"
#include "Loader/ModContext.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <io.h>
#include <queue>
#include <stdexcept>
#include <system_error>
#include <unordered_set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "BML/Timer.h"
#include "Logging/Logger.h"
#include "Loader/LegacyModVersion.h"
#include "Mods/BMLMod.h"
#include "Mods/NewBallTypeMod.h"
#include "PathUtils.h"
#include "StringUtils.h"
#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptDevToolsService.h"
#include "AngelScript/ScriptMod.h"
#include "AngelScript/ScriptModEntryScanner.h"
#include "AngelScript/ScriptModHotReloadService.h"
#include "AngelScript/ScriptModLoader.h"
#include "AngelScript/ScriptModRuntime.h"
#include "AngelScript/ScriptReloadStagingCleanup.h"
#endif

using namespace BML;

namespace {
    constexpr wchar_t PACKAGES_DIRECTORY_NAME[] = L"Packages";

    HMODULE ModuleFromAddress(const void *address) {
        if (!address)
            return nullptr;

        HMODULE module = nullptr;
        if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  reinterpret_cast<LPCSTR>(address),
                                  &module)) {
            return nullptr;
        }
        return module;
    }

    std::wstring ModuleDirectory(HMODULE module) {
        if (!module)
            return {};

        std::wstring path(260, L'\0');
        for (;;) {
            const DWORD written = ::GetModuleFileNameW(
                module, path.data(), static_cast<DWORD>(path.size()));
            if (written == 0)
                return {};
            if (written < path.size()) {
                path.resize(written);
                return utils::GetParentDirectoryW(path);
            }
            if (path.size() >= 32768)
                return {};
            path.resize(path.size() * 2);
        }
    }

    std::wstring ModuleDirectoryFromAddress(const void *address) {
        return ModuleDirectory(ModuleFromAddress(address));
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

    std::wstring BuildZipExtractionDirectory(const std::wstring &tempDirectory, const std::wstring &archivePath) {
        if (tempDirectory.empty() || archivePath.empty())
            return {};

        std::wstring archiveName = utils::GetFileNameW(archivePath);
        const size_t extension = archiveName.find_last_of(L'.');
        if (extension != std::wstring::npos && extension != 0)
            archiveName.resize(extension);

        if (archiveName.empty())
            return {};

        return utils::CombinePathW(utils::CombinePathW(tempDirectory, PACKAGES_DIRECTORY_NAME), archiveName);
    }

}

ModLoader::ModLoader(ModContext &context) : m_Context(context) {
#if BML_ENABLE_ANGELSCRIPT
    m_ScriptDevTools = std::make_unique<BML::ScriptDevToolsService>(&m_Context);
    m_ScriptHotReload = std::make_unique<BML::ScriptModHotReloadService>(&m_Context, *m_ScriptDevTools);
    m_ScriptDevTools->SetHotReload(*m_ScriptHotReload);
#endif
}

ModLoader::~ModLoader() {
    (void) Stop();
}

bool ModLoader::Start() {
    if (AreModsInited())
        return true;
    if (!m_Context.IsInited() || !m_Context.IsMainThread())
        return false;
    if (!m_Mods.empty()) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("ModLoader cannot start while registrations from an earlier run remain.");
        return false;
    }

    m_ShuttingDown = false;

    if ((!AreModsLoaded() && !LoadMods()) || !InitMods()) {
        (void) Stop();
        return false;
    }
    return true;
}

bool ModLoader::Stop() {
    if (!m_Context.IsMainThread()) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("ModLoader::Stop must run on the game thread.");
        return false;
    }
    if (m_ModInvocationGate.IsCallActiveOnCurrentThread()) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("ModLoader::Stop cannot unload Mods from a Mod callback.");
        return false;
    }

    try {
        if (!m_ModsInited && !m_ShuttingDown &&
            (m_ModsLoaded || !m_Mods.empty() || m_BMLMod || m_BallTypeMod)) {
            RollbackModActivation();
        }
        ShutdownMods();
        m_ShuttingDown = true;
        UnloadMods();
    } catch (const std::exception &e) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("ModLoader::Stop failed: %s", e.what());
        return false;
    } catch (...) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("ModLoader::Stop failed with an unknown exception.");
        return false;
    }
    return !m_ModsLoaded && !m_ModsInited && m_Mods.empty() &&
           m_RejectedNativeDlls.empty() && !m_BMLMod && !m_BallTypeMod;
}

void ModLoader::Abandon() noexcept {
    m_ShuttingDown = true;
#if BML_ENABLE_ANGELSCRIPT
    try {
        if (m_ScriptHotReload)
            m_ScriptHotReload->Stop();
    } catch (...) {
    }
#endif
}

void ModLoader::LogCallbackFailure(IMod *mod, const char *reason) const {
    if (!m_Context.GetLogger())
        return;
    if (reason)
        m_Context.GetLogger()->Error("Exception in mod %s callback: %s", mod->GetID(), reason);
    else
        m_Context.GetLogger()->Error("Unknown exception in mod %s callback", mod->GetID());
}

void ModLoader::LogImGuiRecovery(IMod *mod) const {
    if (m_Context.GetLogger())
        m_Context.GetLogger()->Warn("Recovered unbalanced ImGui state after mod %s callback", mod->GetID());
}

void ModLoader::LogMessage(const char *message) const {
    if (m_Context.GetLogger())
        m_Context.GetLogger()->Info("On Message %s", message);
}

void ModLoader::DispatchConfigChange(IMod *mod, const char *category, const char *key, IProperty *property) {
    try {
        ModInvocation invocation(this, mod);
        mod->OnModifyConfig(category, key, property);
    } catch (const std::exception &e) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("Exception in mod %s config callback: %s", mod->GetID(), e.what());
    } catch (...) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("Unknown exception in mod %s config callback", mod->GetID());
    }
}

std::wstring ModLoader::GetModRootDirectory(const void *callerAddress, const char *modId) const {
    if (!modId || !*modId)
        return ModuleDirectoryFromAddress(callerAddress);

    std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
    IMod *mod = FindModLocked(modId);
    if (!mod)
        return {};

    const std::shared_ptr<void> nativeDll = m_NativeModRegistry.FindDllForMod(modId);
    if (nativeDll)
        return ModuleDirectory(static_cast<HMODULE>(nativeDll.get()));

#if BML_ENABLE_ANGELSCRIPT
    for (const auto &scriptMod : m_ScriptMods) {
        if (scriptMod.get() == mod)
            return scriptMod->GetEntry().RootDirectory;
    }
#endif

    if (mod == static_cast<IMod *>(m_BMLMod) || mod == static_cast<IMod *>(m_BallTypeMod))
        return ModuleDirectoryFromAddress(reinterpret_cast<const void *>(&BML_GetModContext));
    return {};
}

#if BML_ENABLE_ANGELSCRIPT
void ModLoader::ProcessScriptState() {
    ProcessScriptModFailureCleanup();
    if (m_ScriptDevTools)
        m_ScriptDevTools->ProcessActions();
    if (m_ScriptHotReload)
        m_ScriptHotReload->Process();
    ProcessScriptModQueuedCallbacks();
}
#endif


bool ModLoader::LoadMods() {
    if (!m_Context.IsInited() || AreModsLoaded())
        return false;

    std::unordered_set<std::string> modSet;
    try {
        if (!RegisterBuiltinMods())
            return false;

        for (auto *mod : m_Mods) {
            const char *id = mod->GetID();
            modSet.emplace(id);
        }

        std::wstring path = std::wstring(m_Context.GetDirectory(BML_DIR_LOADER)) + L"\\Mods";
        if (utils::DirectoryExistsW(path)) {
#if BML_ENABLE_ANGELSCRIPT
            CleanupStaleScriptReloadArtifacts(path, m_Context.GetLogger());
#endif
            std::vector<std::wstring> modPaths;
            ExploreMods(path, modPaths);

            for (auto &modPath : modPaths) {
                IMod *mod = LoadMod(modPath);
                if (mod) {
                    const char *id = mod->GetID();
                    if (modSet.find(id) != modSet.end()) {
                        m_Context.GetLogger()->Warn("Duplicate Mod: %s", id);
                        UnregisterMod(mod);
                        continue;
                    }
                    modSet.emplace(id);

                    auto [drive, dir] = utils::GetDriveAndDirectoryW(modPath);
                    std::wstring drivePath = drive + dir;
                    std::string ansiPath = utils::Utf16ToAnsi(drivePath);
                    m_Context.AddDataPath(ansiPath.c_str());
                }
            }

#if BML_ENABLE_ANGELSCRIPT
            std::vector<BML::ScriptModLoadCandidate> scriptModCandidates;
            if (m_Context.AreAngelScriptBindingsRegistered()) {
                ExploreScriptMods(path, scriptModCandidates);
                for (auto &scriptCandidate : scriptModCandidates) {
                    IMod *mod = LoadScriptMod(scriptCandidate);
                    if (mod) {
                        const char *id = mod->GetID();
                        if (modSet.find(id) != modSet.end()) {
                            m_Context.GetLogger()->Warn("Duplicate Mod: %s", id);
                            UnregisterMod(mod);
                            continue;
                        }
                        modSet.emplace(id);

                        std::string ansiPath = utils::Utf16ToAnsi(scriptCandidate.RootDirectory);
                        m_Context.AddDataPath(ansiPath.c_str());
                    }
                }
            }
#endif

            if (modPaths.empty()
#if BML_ENABLE_ANGELSCRIPT
                && scriptModCandidates.empty()
#endif
            ) {
                m_Context.GetLogger()->Info("No mod is found.");
            }
        }

        m_ModsLoaded = true;
        return true;
    } catch (const std::exception &e) {
        m_Context.GetLogger()->Error("Exception during mod loading: %s", e.what());
    } catch (...) {
        m_Context.GetLogger()->Error("Unknown exception during mod loading.");
    }
    return false;
}

void ModLoader::UnloadMods() {
    if (!m_Context.IsInited() && m_Mods.empty() && m_RejectedNativeDlls.empty() &&
        !m_BMLMod && !m_BallTypeMod)
        return;
    if (!m_Context.IsMainThread()) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("UnloadMods must run on the game thread.");
        return;
    }

#if BML_ENABLE_ANGELSCRIPT
    if (m_ScriptHotReload)
        m_ScriptHotReload->Stop();
#endif

    if (AreModsInited())
        ShutdownMods();

    std::vector<std::string> modNames(m_Mods.size());
    {
        std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
        for (const auto &entry : m_ModIndex) {
            if (entry.second < modNames.size())
                modNames[entry.second] = entry.first;
        }
    }

    for (auto rit = modNames.rbegin(); rit != modNames.rend(); ++rit) {
        if (!rit->empty())
            UnloadMod(*rit);
    }

    for (auto it = m_RejectedNativeDlls.begin(); it != m_RejectedNativeDlls.end();) {
        if (m_Context.UnregisterNativeCommands(it->get()))
            it = m_RejectedNativeDlls.erase(it);
        else
            ++it;
    }

    if (!m_Mods.empty() || !m_RejectedNativeDlls.empty()) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("ModLoader stopped with %zu registered Mod(s) and %zu rejected DLL(s); their code remains loaded for a safe retry.",
                                      m_Mods.size(), m_RejectedNativeDlls.size());
        return;
    }

    delete m_BallTypeMod;
    m_BallTypeMod = nullptr;

    delete m_BMLMod;
    m_BMLMod = nullptr;

#if BML_ENABLE_ANGELSCRIPT
    m_ScriptMods.clear();
#endif

    {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        m_ModDependencies.clear();
    }

    m_ModsLoaded = false;
}

bool ModLoader::InitMods() {
    if (!m_Context.IsInited() || !AreModsLoaded() || AreModsInited())
        return false;

    m_ShuttingDown = false;
    m_ActiveMods.clear();
    try {
        m_ActiveMods.reserve(m_Mods.size());
    } catch (...) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("Cannot initialize Mods: failed to prepare activation state.");
        return false;
    }

    const char *activatingModId = nullptr;
    try {
        if (!ResolveDependencies())
            return false;

        for (IMod *mod : m_Mods) {
            activatingModId = mod->GetID();
            m_Context.GetLogger()->Info("Loading Mod %s[%s] v%s by %s",
                           activatingModId, mod->GetName(), mod->GetVersion(), mod->GetAuthor());

            std::string dependencyDiagnostic;
            if (EvaluateActivationDependencies(mod, &dependencyDiagnostic) == 0) {
                m_Context.GetLogger()->Error("Cannot initialize Mod %s: %s",
                                activatingModId,
                                dependencyDiagnostic.empty()
                                    ? "dependencies are not satisfied."
                                    : dependencyDiagnostic.c_str());
                activatingModId = nullptr;
                continue; // Skip this mod but continue loading others
            }

            // Preserve the legacy behavior that lets a mod receive callbacks
            // which it triggers from its own OnLoad implementation.
            FillCallbackMap(mod);
            {
                ModInvocation invocation(this, mod);
                mod->OnLoad();
            }
            // Capacity was reserved before activation, so recording success
            // cannot fail after a mod has completed OnLoad.
            m_ActiveMods.push_back(mod);
            activatingModId = nullptr;
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
            m_Context.GetLogger()->Info("BML script mod summary: loaded=%d failed=%d",
                           scriptLoadedCount,
                           scriptFailedCount);
            if (firstFailedScript) {
                m_Context.GetLogger()->Warn("First failed script mod %s: %s",
                               firstFailedScript->GetID(),
                               firstFailedScript->GetLastDiagnostic().c_str());
                std::string message = "[script] load failed: ";
                message += firstFailedScript->GetID();
                message += "; script diag ";
                message += firstFailedScript->GetID();
                m_Context.SendIngameMessage(message.c_str());
            }
        }
#endif

        m_Context.FlushConfigChanges(true);

        m_Context.OnLoadGame();

#if BML_ENABLE_ANGELSCRIPT
        if (m_ScriptHotReload)
            m_ScriptHotReload->Start();
#endif

        m_ModsInited = true;
        return true;
    } catch (const std::exception &e) {
        if (m_Context.GetLogger()) {
            if (activatingModId)
                m_Context.GetLogger()->Error("Cannot initialize Mod %s: activation raised an exception: %s",
                                activatingModId, e.what());
            else
                m_Context.GetLogger()->Error("Cannot complete Mod initialization: %s", e.what());
        }
    } catch (...) {
        if (m_Context.GetLogger()) {
            if (activatingModId)
                m_Context.GetLogger()->Error("Cannot initialize Mod %s: activation raised an unknown exception.",
                                activatingModId);
            else
                m_Context.GetLogger()->Error("Cannot complete Mod initialization: unknown exception.");
        }
    }

    RollbackModActivation();
    return false;
}

void ModLoader::ShutdownMods() {
    if (!m_Context.IsInited() || !AreModsLoaded() || !AreModsInited())
        return;
    if (!m_Context.IsMainThread()) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("ShutdownMods must run on the game thread.");
        return;
    }

    // End loader-owned UI sessions while every contributed callback and owner
    // DLL is still alive. The shutdown close releases input without returning
    // to Menu_Options, whose script is itself about to be deactivated.
    if (m_BMLMod)
        m_BMLMod->CloseModsMenuForShutdown();

    m_ShuttingDown = true;

#if BML_ENABLE_ANGELSCRIPT
    if (m_ScriptHotReload)
        m_ScriptHotReload->Stop();
#endif

    // Wait for concurrent callbacks before unloading their owning Mod objects.
    auto invocationLock = m_ModInvocationGate.LockMutation();
    DeactivateActiveMods(true);

    m_ModsInited = false;
}

void ModLoader::DeactivateActiveMods(bool dispatchPendingNotifications) {
    // A normal shutdown still owes Mods notifications queued by the last frame.
    // Deliver them before the first OnUnload so teardown remains the final callback.
    // Activation rollback deliberately suppresses callbacks from incomplete OnLoad
    // implementations and only persists their final values below.
    m_Context.SnapshotConfigMetadata();
    if (dispatchPendingNotifications)
        m_Context.FlushConfigChanges();

    for (auto rit = m_ActiveMods.rbegin(); rit != m_ActiveMods.rend(); ++rit) {
        IMod *mod = *rit;
        try {
            ModInvocation invocation(this, mod);
            mod->OnUnload();
        } catch (const std::exception &e) {
            if (m_Context.GetLogger())
                m_Context.GetLogger()->Error("Exception in a Mod unload callback: %s", e.what());
        } catch (...) {
            if (m_Context.GetLogger())
                m_Context.GetLogger()->Error("Unknown exception in a Mod unload callback.");
        }
        try {
            const char *ownerId = mod ? mod->GetID() : nullptr;
            if (ownerId && ownerId[0] != '\0')
                m_Context.RetireModBehaviorState(ownerId);
        } catch (...) {
            if (m_Context.GetLogger())
                m_Context.GetLogger()->Error("Failed to resolve a Mod id during Behavior cleanup.");
        }
    }

    // OnUnload may change configuration, but it is the Mod's final callback. Drain
    // those notifications without dispatching them, and persist the final values
    // without calling back into Mod metadata.
    m_Context.FlushConfigChanges(true, false);

    for (auto rit = m_ActiveMods.rbegin(); rit != m_ActiveMods.rend(); ++rit) {
        IMod *mod = *rit;
        try {
            const char *ownerId = mod ? mod->GetID() : nullptr;
            if (ownerId && ownerId[0] != '\0')
                m_Context.CleanupModState(ownerId);
        } catch (...) {
            if (m_Context.GetLogger())
                m_Context.GetLogger()->Error("Failed to resolve a Mod id during owner-state cleanup.");
        }
    }

    try {
        Timer::CancelAll();
        if (CKTimeManager *timeManager = m_Context.GetTimeManager()) {
            Timer::ProcessAll(timeManager->GetMainTickCount(), timeManager->GetAbsoluteTime() / 1000.0f);
        } else {
            Timer::ProcessAll(0, 0.0f);
        }
    } catch (...) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("Failed to clean Mod timers during shutdown.");
    }

    {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        m_CallbackMap.clear();
    }
    m_Context.ClearLegacyCommands();
    m_ActiveMods.clear();
}

void ModLoader::RollbackModActivation() {
    m_ShuttingDown = true;
#if BML_ENABLE_ANGELSCRIPT
    if (m_ScriptHotReload)
        m_ScriptHotReload->Stop();
#endif
    auto invocationLock = m_ModInvocationGate.LockMutation();
    DeactivateActiveMods(false);

    // A throwing OnLoad may publish owner-scoped state before the Mod becomes
    // active. Retire every owner because only completed activations reached the
    // active list cleaned by DeactivateActiveMods.
    for (IMod *mod : m_Mods) {
        try {
            const char *ownerId = mod ? mod->GetID() : nullptr;
            if (ownerId && ownerId[0] != '\0')
                m_Context.CleanupModState(ownerId);
        } catch (...) {
            if (m_Context.GetLogger())
                m_Context.GetLogger()->Error("Failed to resolve a Mod id during activation rollback.");
        }
    }
    m_ModsInited = false;
}

int ModLoader::GetModCount() {
    std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
    return (int) m_Mods.size();
}

IMod *ModLoader::GetMod(int index) {
    std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
    if (index < 0 || index >= (int) m_Mods.size())
        return nullptr;
    return m_Mods[index];
}

std::uint64_t ModLoader::GetModGeneration(const IMod *mod) const {
    if (!mod)
        return 0;

    std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
    const auto generation = m_ModGenerations.find(mod);
    return generation == m_ModGenerations.end() ? 0 : generation->second;
}

std::uint64_t ModLoader::GetModRegistryRevision() const {
    std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
    return m_ModRegistryRevision;
}

IMod *ModLoader::FindMod(const char *id) const {
    if (!id)
        return nullptr;

    std::shared_lock<std::shared_mutex> lock(m_ModRegistryMutex);
    return FindModLocked(id);
}

IMod *ModLoader::FindModLocked(const std::string &id) const {
    const auto entry = m_ModIndex.find(id);
    return entry != m_ModIndex.end() && entry->second < m_Mods.size()
        ? m_Mods[entry->second]
        : nullptr;
}

int ModLoader::RegisterDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) {
    if (!mod || !dependencyId) {
        return BML_ERROR_FAIL;
    }

    try {
        std::lock_guard<std::mutex> lock(m_StateMutex);
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

int ModLoader::RegisterOptionalDependency(IMod *mod, const char *dependencyId, int major, int minor, int patch) {
    if (!mod || !dependencyId) {
        return BML_ERROR_FAIL;
    }

    try {
        std::lock_guard<std::mutex> lock(m_StateMutex);
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

int ModLoader::CheckDependencies(IMod *mod) const {
    return EvaluateDependencies(mod, nullptr);
}

int ModLoader::EvaluateDependencies(IMod *mod, std::string *diagnostic) const {
    if (diagnostic)
        diagnostic->clear();
    if (!mod) {
        if (diagnostic)
            *diagnostic = "the Mod pointer is null.";
        return 0;
    }

    auto invocationLock = LockModInvocation();

    try {
        struct DependencySnapshot {
            std::string Id;
            BMLVersion MinVersion;
            bool Optional = false;
            IMod *Mod = nullptr;
        };

        std::vector<DependencySnapshot> dependencies;
        {
            std::lock_guard<std::mutex> dependencyLock(m_StateMutex);
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
                dependency.Mod = FindModLocked(dependency.Id);
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
            BMLVersion have = BML::ParseLegacyModVersion(verStr);

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

int ModLoader::EvaluateActivationDependencies(IMod *mod, std::string *diagnostic) const {
    if (EvaluateDependencies(mod, diagnostic) == 0)
        return 0;

    std::lock_guard<std::mutex> dependencyLock(m_StateMutex);
    const auto dependencies = m_ModDependencies.find(mod);
    if (dependencies == m_ModDependencies.end())
        return 1;

    // InitMods runs on the game thread after dependency ordering is fixed, so
    // the Mod registry and activation vector cannot change during this pass.
    for (const ModDependency &dependency : dependencies->second) {
        if (dependency.optional || !dependency.id || !*dependency.id)
            continue;
        IMod *registered = FindModLocked(dependency.id);
        if (!registered ||
            std::find(m_ActiveMods.begin(), m_ActiveMods.end(), registered) == m_ActiveMods.end()) {
            if (diagnostic)
                *diagnostic = "required dependency '" + std::string(dependency.id) + "' did not initialize.";
            return 0;
        }
    }
    return 1;
}

int ModLoader::GetDependencyCount(IMod *mod) const {
    if (!mod) {
        return -1;
    }

    try {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        auto it = m_ModDependencies.find(mod);
        if (it == m_ModDependencies.end()) {
            return 0;
        }

        return static_cast<int>(it->second.size());
    } catch (...) {
        return -1;
    }
}

int ModLoader::GetDependencyInfo(IMod *mod, int index, char *dependencyId, int idSize,
                                  int *major, int *minor, int *patch, int *optional) const {
    if (!mod || index < 0 || (dependencyId && idSize <= 0)) {
        return BML_ERROR_FAIL;
    }

    try {
        std::lock_guard<std::mutex> lock(m_StateMutex);
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

int ModLoader::ClearDependencies(IMod *mod) {
    if (!mod) {
        return BML_ERROR_FAIL;
    }

    try {
        std::lock_guard<std::mutex> lock(m_StateMutex);
        m_ModDependencies.erase(mod);
        return BML_OK;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

size_t ModLoader::ExploreMods(const std::wstring &path, std::vector<std::wstring> &mods) {
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
                const std::wstring dest = BuildZipExtractionDirectory(m_Context.GetDirectory(BML_DIR_TEMP), fullPath);

                if (dest.empty() || !PrepareFreshDirectory(dest)) {
                    m_Context.GetLogger()->Error("Failed to create temp extraction directory: %s", utils::Utf16ToAnsi(dest).c_str());
                    continue;
                }

                if (utils::ExtractZipW(fullPath, dest)) {
                    ExploreMods(dest, mods);
                } else {
                    m_Context.GetLogger()->Error("Failed to extract zip file: %s", utils::Utf16ToAnsi(fullPath).c_str());
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
size_t ModLoader::ExploreScriptMods(const std::wstring &path, std::vector<BML::ScriptModLoadCandidate> &candidates) {
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
        const std::wstring dest = BuildZipExtractionDirectory(m_Context.GetDirectory(BML_DIR_TEMP), zipPath);
        if (dest.empty() || !PrepareFreshDirectory(dest)) {
            m_Context.GetLogger()->Error("Failed to create script package extraction directory: %s", utils::Utf16ToAnsi(dest).c_str());
            continue;
        }

        if (!utils::ExtractZipW(zipPath, dest)) {
            m_Context.GetLogger()->Error("Failed to extract script package zip: %s", utils::Utf16ToAnsi(zipPath).c_str());
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

std::shared_ptr<void> ModLoader::LoadLib(const wchar_t *path) {
    if (!path || path[0] == '\0')
        return nullptr;

    HMODULE dllHandle = ::LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!dllHandle) {
        const DWORD error = ::GetLastError();
        const std::string message = std::system_category().message(static_cast<int>(error));
        m_Context.GetLogger()->Error("Failed to load native Mod DLL %s: Windows error %lu (%s).",
                        utils::Utf16ToAnsi(path).c_str(),
                        static_cast<unsigned long>(error),
                        message.c_str());
        return nullptr;
    }

    return std::shared_ptr<void>(dllHandle, [](void *ptr) {
        ::FreeLibrary(static_cast<HMODULE>(ptr));
    });
}

bool ModLoader::UnloadLib(void *dllHandle) {
    std::vector<std::string> modIds;
    {
        std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        modIds = m_NativeModRegistry.SnapshotMods(dllHandle);
        if (modIds.empty())
            return false;
    }

    bool success = true;
    for (const std::string &modId : modIds)
        success = UnloadMod(modId) && success;
    return success;
}

void ModLoader::DestroyNativeMod(void *dllHandle, IMod *mod, const char *modLabel) noexcept {
    if (!dllHandle || !mod)
        return;

    constexpr const char *EXIT_SYMBOL = "BMLExit";
    typedef void (BML_CDECL *BMLExitFunc)(IMod *);

    try {
        auto func = reinterpret_cast<BMLExitFunc>(
            ::GetProcAddress(static_cast<HMODULE>(dllHandle), EXIT_SYMBOL));
        if (!func) {
            if (m_Context.GetLogger()) {
                m_Context.GetLogger()->Warn(
                    "Native Mod %s does not export %s; its instance cannot be destroyed safely.",
                    modLabel ? modLabel : "<unknown>", EXIT_SYMBOL);
            }
            return;
        }

        try {
            func(mod);
        } catch (const std::exception &e) {
            if (m_Context.GetLogger()) {
                m_Context.GetLogger()->Error("Exception in %s for native Mod %s: %s",
                                EXIT_SYMBOL, modLabel ? modLabel : "<unknown>", e.what());
            }
        } catch (...) {
            if (m_Context.GetLogger()) {
                m_Context.GetLogger()->Error("Unknown exception in %s for native Mod %s.",
                                EXIT_SYMBOL, modLabel ? modLabel : "<unknown>");
            }
        }
    } catch (...) {
        // Cleanup must not prevent the loader from releasing the DLL handle.
    }
}

void ModLoader::CleanupRejectedNativeEntry(const std::shared_ptr<void> &dllHandle) {
    if (!m_Context.UnregisterNativeCommands(dllHandle.get())) {
        // LoadMod reserves this slot before calling BMLEntry. Keep the DLL
        // alive until command callbacks can be detached on a later Stop.
        m_RejectedNativeDlls.push_back(dllHandle);
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("Cannot detach commands from a rejected native Mod DLL yet; retaining the DLL.");
    }
}

IMod *ModLoader::LoadMod(const std::wstring &path) {
    const std::string modPath = utils::Utf16ToAnsi(path);

    auto dllHandle = LoadLib(path.c_str());
    if (!dllHandle)
        return nullptr;

    constexpr const char *ENTRY_SYMBOL = "BMLEntry";
    typedef IMod *(BML_CDECL *BMLEntryFunc)(IBML *);

    auto func = reinterpret_cast<BMLEntryFunc>(::GetProcAddress(static_cast<HMODULE>(dllHandle.get()), ENTRY_SYMBOL));
    if (!func) {
        m_Context.GetLogger()->Error("Native Mod DLL %s does not export the required symbol: %s.",
                        modPath.c_str(), ENTRY_SYMBOL);
        return nullptr;
    }

    m_RejectedNativeDlls.reserve(m_RejectedNativeDlls.size() + 1);

    auto *bml = static_cast<IBML *>(&m_Context);
    IMod *mod = nullptr;
    try {
        mod = func(bml);
    } catch (const std::exception &e) {
        m_Context.GetLogger()->Error("Exception in %s for native Mod DLL %s: %s",
                        ENTRY_SYMBOL, modPath.c_str(), e.what());
        CleanupRejectedNativeEntry(dllHandle);
        return nullptr;
    } catch (...) {
        m_Context.GetLogger()->Error("Unknown exception in %s for native Mod DLL %s.",
                        ENTRY_SYMBOL, modPath.c_str());
        CleanupRejectedNativeEntry(dllHandle);
        return nullptr;
    }

    if (!mod) {
        m_Context.GetLogger()->Error("%s returned null for native Mod DLL %s; the DLL will be unloaded.",
                        ENTRY_SYMBOL, modPath.c_str());
        CleanupRejectedNativeEntry(dllHandle);
        return nullptr;
    }

    bool registered = false;
    try {
        registered = RegisterMod(mod, dllHandle);
    } catch (const std::exception &e) {
        m_Context.GetLogger()->Error("Exception while registering native Mod DLL %s: %s",
                                   modPath.c_str(), e.what());
    } catch (...) {
        m_Context.GetLogger()->Error("Unknown exception while registering native Mod DLL %s.",
                                   modPath.c_str());
    }
    if (!registered) {
        CleanupRejectedNativeEntry(dllHandle);
        if (Config *config = m_Context.GetConfig(mod))
            m_Context.RemoveConfig(config);
        DestroyNativeMod(dllHandle.get(), mod, modPath.c_str());
        return nullptr;
    }

    return mod;
}

#if BML_ENABLE_ANGELSCRIPT
IMod *ModLoader::LoadScriptMod(const BML::ScriptModLoadCandidate &candidate) {
    BML::ScriptModLoadResult loadResult = BML::LoadScriptMod(&m_Context, m_Context.GetCKContext(), candidate);
    auto &scriptMod = loadResult.Mod;
    if (!scriptMod) {
        m_Context.GetLogger()->Error("Script Mod could not be loaded due to allocation failure.");
        return nullptr;
    }

    m_ScriptMods.push_back(std::move(scriptMod));
    IMod *mod = m_ScriptMods.back().get();
    if (!RegisterMod(mod)) {
        if (Config *config = m_Context.GetConfig(mod))
            m_Context.RemoveConfig(config);
        m_ScriptMods.pop_back();
        return nullptr;
    }

    RegisterScriptModDependencies(mod, loadResult.Definition);
    if (m_ScriptHotReload)
        m_ScriptHotReload->RegisterMod(static_cast<BML::ScriptMod *>(mod));
    return mod;
}
#endif

#if BML_ENABLE_ANGELSCRIPT
void ModLoader::RegisterScriptModDependencies(IMod *mod, const BML::ScriptModDefinition &definition) {
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
#endif

#if BML_ENABLE_ANGELSCRIPT
void ModLoader::ProcessScriptModFailureCleanup() {
    for (const auto &scriptMod : m_ScriptMods) {
        if (scriptMod)
            scriptMod->ProcessFailureCleanup();
    }
}
#endif

#if BML_ENABLE_ANGELSCRIPT
void ModLoader::ProcessScriptModQueuedCallbacks() {
    for (const auto &scriptMod : m_ScriptMods) {
        if (scriptMod)
            scriptMod->ProcessQueuedScriptServiceCallbacks();
    }
}
#endif

#if BML_ENABLE_ANGELSCRIPT
std::vector<ModLoader::RegisteredModSnapshot> ModLoader::SnapshotModRegistry() const {
    struct PendingModSnapshot {
        IMod *Mod = nullptr;
        std::vector<ModDependencySnapshot> Dependencies;
    };

    auto invocationLock = LockModInvocation();
    std::vector<PendingModSnapshot> pending;
    {
        std::lock_guard<std::mutex> dependencyLock(m_StateMutex);
        std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        pending.reserve(m_Mods.size());
        for (IMod *registered : m_Mods) {
            PendingModSnapshot item;
            item.Mod = registered;

            const auto dependencies = m_ModDependencies.find(registered);
            if (dependencies != m_ModDependencies.end()) {
                item.Dependencies.reserve(dependencies->second.size());
                for (const ModDependency &dependency : dependencies->second) {
                    if (dependency.id && *dependency.id) {
                        item.Dependencies.push_back({
                            dependency.id,
                            dependency.minVersion,
                            dependency.optional != 0,
                        });
                    }
                }
            }
            pending.push_back(std::move(item));
        }
    }

    std::vector<RegisteredModSnapshot> snapshot;
    snapshot.reserve(pending.size());
    for (auto &item : pending) {
        if (!item.Mod)
            continue;

        const char *id = item.Mod->GetID();
        const char *version = item.Mod->GetVersion();
        snapshot.push_back({
            item.Mod,
            id ? id : "",
            version ? version : "",
            BML::IsFailedScriptMod(item.Mod),
            std::move(item.Dependencies),
        });
    }
    return snapshot;
}
#endif

#if BML_ENABLE_ANGELSCRIPT
bool ModLoader::ValidateScriptModReloadDependencies(const BML::ScriptMod *mod,
                                                     const BML::ScriptModDefinition &candidate,
                                                     std::string &diagnostic,
                                                     std::vector<BML::ScriptModReloadDiagnosticField> *fields) const {
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

    const std::vector<RegisteredModSnapshot> registry = SnapshotModRegistry();
    const auto findById = [&registry](const std::string &id) {
        return std::find_if(registry.begin(), registry.end(), [&id](const RegisteredModSnapshot &entry) {
            return entry.Id == id;
        });
    };
    const auto current = std::find_if(
        registry.begin(), registry.end(), [mod](const RegisteredModSnapshot &entry) {
            return entry.Identity == mod;
        });
    if (current == registry.end()) {
        diagnostic = "Script mod reload target is not registered.";
        addField("boundary", "reload_target");
        addField("action", "restart_required");
        return false;
    }

    if (candidate.Id.empty()) {
        diagnostic = "Script mod reload candidate has an empty id.";
        addField("boundary", "mod_identity");
        addField("action", "fix_metadata");
        return false;
    }
    if (candidate.Id != current->Id) {
        const auto existing = findById(candidate.Id);
        if (existing != registry.end() && existing->Identity != mod) {
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
        const auto dependencyIt = findById(dependency.Id);
        if (dependencyIt == registry.end()) {
            if (!dependency.Optional) {
                diagnostic = "Script mod reload dependency '" + dependency.Id + "' is missing. "
                             "Hot reload only refreshes already registered script mods; it does not discover or load new dependency graph nodes. "
                             "Restart after adding dependencies.";
                addDependencyBoundary(dependency, "restart_after_adding_dependency");
                return false;
            }
            continue;
        }

        if (dependencyIt->Failed) {
            if (!dependency.Optional) {
                diagnostic = "Script mod reload dependency '" + dependency.Id + "' is failed. "
                             "Hot reload does not repair or cascade reload required dependencies; fix and reload the dependency first, or restart.";
                addDependencyBoundary(dependency, "fix_and_reload_dependency_or_restart");
                return false;
            }
            continue;
        }
        const BMLVersion have = BML::ParseLegacyModVersion(dependencyIt->Version.c_str());
        if (have < dependency.MinVersion && !dependency.Optional) {
            diagnostic = "Script mod reload dependency '" + dependency.Id + "' is older than required. "
                         "Hot reload does not cascade reload dependencies; update/reload that dependency first, or restart.";
            addDependencyBoundary(dependency, "update_or_reload_dependency_or_restart");
            return false;
        }
    }

    BMLVersion candidateVersion(0, 0, 0);
    if (!BML::ParseScriptModVersion(candidate.Version, candidateVersion)) {
        diagnostic = "Script mod reload candidate has an invalid version.";
        addField("boundary", "mod_version");
        addField("action", "fix_metadata");
        return false;
    }
    for (const RegisteredModSnapshot &dependent : registry) {
        if (dependent.Identity == mod)
            continue;

        for (const ModDependencySnapshot &dependency : dependent.Dependencies) {
            if (dependency.Id != candidate.Id)
                continue;
            if (!dependency.Optional && candidateVersion < dependency.MinVersion) {
                diagnostic = "Script mod reload version would no longer satisfy dependent mod '";
                diagnostic += dependent.Id;
                diagnostic += "'. Hot reload does not cascade reload dependent mods; restart or reload dependent mods explicitly.";
                addField("boundary", "dependent_compatibility");
                addField("cascade", "false");
                addField("dependent", dependent.Id);
                addField("action", "restart_or_reload_dependents_explicitly");
                return false;
            }
        }
    }

    return true;
}
#endif

#if BML_ENABLE_ANGELSCRIPT
bool ModLoader::PromoteFailedScriptModPlaceholder(BML::ScriptMod *mod,
                                                   const std::string &oldId,
                                                   const BML::ScriptModDefinition &candidate,
                                                   std::string &diagnostic) {
    if (m_ModInvocationGate.IsCallActiveOnCurrentThread()) {
        diagnostic = "Script mod failed-load recovery cannot change owners during a Mod callback.";
        return false;
    }
    auto invocationLock = m_ModInvocationGate.LockMutation();
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

    const bool changedId = candidate.Id != oldId;
    {
        std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        const auto oldIt = m_ModIndex.find(oldId);
        if (oldIt == m_ModIndex.end() || oldIt->second >= m_Mods.size() ||
            m_Mods[oldIt->second] != mod) {
            diagnostic = "Script mod failed-load recovery lost its placeholder registration.";
            return false;
        }
        if (changedId && m_ModIndex.find(candidate.Id) != m_ModIndex.end()) {
            diagnostic = "Script mod failed-load recovery id '" + candidate.Id + "' conflicts with an already registered mod.";
            return false;
        }
    }
    if (changedId && !m_Context.RegisterModOwner(candidate.Id)) {
        diagnostic = "Script mod failed-load recovery could not register its new owner.";
        return false;
    }

    bool promoted = false;
    try {
        std::unique_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        const auto oldIt = m_ModIndex.find(oldId);
        const auto newIt = m_ModIndex.find(candidate.Id);
        if (oldIt == m_ModIndex.end() || oldIt->second >= m_Mods.size() ||
            m_Mods[oldIt->second] != mod) {
            diagnostic = "Script mod failed-load recovery lost its placeholder registration.";
        } else if (changedId && newIt != m_ModIndex.end()) {
            diagnostic = "Script mod failed-load recovery id '" + candidate.Id + "' conflicts with an already registered mod.";
        } else {
            if (changedId) {
                const std::size_t index = oldIt->second;
                m_ModIndex.emplace(candidate.Id, index);
                m_ModIndex.erase(oldId);
                ++m_ModRegistryRevision;
            }
            promoted = true;
        }
    } catch (const std::exception &e) {
        diagnostic = std::string("Script mod failed-load recovery could not update its registration: ") + e.what();
    } catch (...) {
        diagnostic = "Script mod failed-load recovery could not update its registration.";
    }
    if (!promoted) {
        if (changedId)
            m_Context.RetireFailedModOwner(candidate.Id);
        return false;
    }

    if (changedId)
        m_Context.RetireFailedModOwner(oldId);

    ClearDependencies(mod);
    RegisterScriptModDependencies(mod, candidate);
    return true;
}
#endif

#if BML_ENABLE_ANGELSCRIPT
void ModLoader::RestoreFailedScriptModPlaceholder(BML::ScriptMod *mod,
                                                   const std::string &currentId,
                                                   const BML::ScriptModDefinition &oldDefinition) {
    if (m_ModInvocationGate.IsCallActiveOnCurrentThread()) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("Script mod failed-load recovery cannot restore owners during a Mod callback.");
        return;
    }
    auto invocationLock = m_ModInvocationGate.LockMutation();
    if (!mod)
        return;

    const bool changedId = currentId != oldDefinition.Id;
    {
        std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        const auto current = m_ModIndex.find(currentId);
        if (current == m_ModIndex.end() || current->second >= m_Mods.size() ||
            m_Mods[current->second] != mod)
            return;
        const auto old = m_ModIndex.find(oldDefinition.Id);
        if (changedId && old != m_ModIndex.end())
            return;
    }
    if (changedId && !m_Context.RegisterModOwner(oldDefinition.Id)) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("Script mod failed-load recovery could not restore owner %s.",
                                      oldDefinition.Id.c_str());
        return;
    }

    bool restored = false;
    try {
        std::unique_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        const auto current = m_ModIndex.find(currentId);
        if (current != m_ModIndex.end() && current->second < m_Mods.size() &&
            m_Mods[current->second] == mod) {
            if (changedId) {
                const std::size_t index = current->second;
                const auto inserted = m_ModIndex.emplace(oldDefinition.Id, index);
                if (inserted.second) {
                    m_ModIndex.erase(currentId);
                    ++m_ModRegistryRevision;
                    restored = true;
                }
            } else {
                restored = true;
            }
        }
    } catch (...) {
    }
    if (!restored) {
        if (changedId)
            m_Context.RetireFailedModOwner(oldDefinition.Id);
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("Script mod failed-load recovery could not restore its registration.");
        return;
    }

    if (changedId)
        m_Context.RetireFailedModOwner(currentId);

    ClearDependencies(mod);
    RegisterScriptModDependencies(mod, oldDefinition);
}
#endif

bool ModLoader::UnloadMod(const std::string &id) {
    if (!m_Context.IsMainThread()) {
        if (m_Context.GetLogger())
            m_Context.GetLogger()->Error("UnloadMod must run on the game thread.");
        return false;
    }
    IMod *mod = nullptr;
    {
        std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        mod = FindModLocked(id);
        if (!mod)
            return false;
    }

    if (!UnregisterMod(mod)) {
        m_Context.GetLogger()->Error("Failed to unload mod %s.", id.c_str());
        return false;
    }

    return true;
}

bool ModLoader::RegisterBuiltinMods() {
    m_BMLMod = new BMLMod(&m_Context);
    if (!RegisterMod(m_BMLMod))
        return false;

    m_BallTypeMod = new NewBallTypeMod(&m_Context);
    return RegisterMod(m_BallTypeMod);
}

bool ModLoader::RegisterMod(IMod *mod, const std::shared_ptr<void> &dllHandle) {
    // Allow registering built-in mods that don't come from a DLL (dllHandle can be null).
    if (!mod) {
        m_Context.GetLogger()->Error("Mod registration failed: the Mod pointer is null.");
        return false;
    }

    std::string modId;
    try {
        const char *reportedId = mod->GetID();
        if (!reportedId || !*reportedId) {
            m_Context.GetLogger()->Error("Mod registration failed: GetID() returned an empty id.");
            return false;
        }
        modId = reportedId;

        BMLVersion curVer;
        BMLVersion reqVer = mod->GetBMLVersion();
        if (curVer < reqVer) {
            m_Context.GetLogger()->Warn("Mod %s[%s] requires BML %d.%d.%d", modId.c_str(), mod->GetName(),
                           reqVer.major, reqVer.minor, reqVer.patch);
            return false;
        }
    } catch (const std::exception &e) {
        m_Context.GetLogger()->Error("Mod registration failed for %s: %s",
                        modId.empty() ? "<unknown>" : modId.c_str(), e.what());
        return false;
    } catch (...) {
        m_Context.GetLogger()->Error("Mod registration failed for %s: unknown exception.",
                        modId.empty() ? "<unknown>" : modId.c_str());
        return false;
    }

    if (m_ModInvocationGate.IsCallActiveOnCurrentThread()) {
        m_Context.GetLogger()->Error("Mod %s cannot be registered from an active Mod callback.", modId.c_str());
        return false;
    }
    auto invocationLock = m_ModInvocationGate.LockMutation();
    {
        std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        if (m_ModIndex.find(modId) != m_ModIndex.end()) {
            m_Context.GetLogger()->Error("Mod registration failed: duplicate id %s.", modId.c_str());
            return false;
        }
        if (std::find(m_Mods.begin(), m_Mods.end(), mod) != m_Mods.end()) {
            m_Context.GetLogger()->Error("Mod registration failed: the Mod pointer is already registered.");
            return false;
        }
    }

    if (!m_Context.RegisterModOwner(modId))
        return false;

    const char *registryError = nullptr;
    std::exception_ptr registryException;
    bool registered = false;
    {
        std::unique_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        bool appended = false;
        bool indexed = false;
        bool generated = false;
        bool nativeRegistered = false;
        try {
            if (m_ModIndex.find(modId) != m_ModIndex.end()) {
                registryError = "duplicate id";
            } else if (std::find(m_Mods.begin(), m_Mods.end(), mod) != m_Mods.end()) {
                registryError = "duplicate Mod pointer";
            } else {
                m_Mods.push_back(mod);
                appended = true;

                indexed = m_ModIndex.emplace(modId, m_Mods.size() - 1).second;
                if (!indexed) {
                    registryError = "inconsistent id index";
                } else {
                    generated = m_ModGenerations.emplace(mod, m_NextModGeneration).second;
                    if (!generated) {
                        registryError = "inconsistent generation index";
                    } else if (dllHandle) {
                        nativeRegistered = m_NativeModRegistry.Add(dllHandle, modId);
                        if (!nativeRegistered)
                            registryError = "inconsistent native DLL ownership";
                    }
                }

                if (!registryError) {
                    ++m_NextModGeneration;
                    ++m_ModRegistryRevision;
                    registered = true;
                }
            }
        } catch (...) {
            registryException = std::current_exception();
        }

        if (!registered) {
            if (nativeRegistered)
                (void) m_NativeModRegistry.Remove(modId);
            if (generated)
                m_ModGenerations.erase(mod);
            if (indexed)
                m_ModIndex.erase(modId);
            if (appended)
                m_Mods.pop_back();
        }
    }

    if (registered)
        return true;

    m_Context.RetireFailedModOwner(modId);

    if (registryException) {
        try {
            std::rethrow_exception(registryException);
        } catch (const std::exception &e) {
            m_Context.GetLogger()->Error("Mod registration failed for %s: %s", modId.c_str(), e.what());
        } catch (...) {
            m_Context.GetLogger()->Error("Mod registration failed for %s: unknown registry exception.",
                            modId.c_str());
        }
    } else {
        m_Context.GetLogger()->Error("Mod registration failed for %s: %s.", modId.c_str(),
                        registryError ? registryError : "registry update failed");
    }
    return false;
}

std::string ModLoader::GetNativeImcOwnerId(
    const void *callerAddress, const char *requestedOwnerId) const {
    return GetNativeModOwnerId(callerAddress, requestedOwnerId);
}

std::string ModLoader::GetNativeModOwnerId(
    const void *callerAddress, const char *requestedOwnerId) const {
    if (!callerAddress)
        return {};

    HMODULE callerModule = ModuleFromAddress(callerAddress);
    if (!callerModule)
        return {};

    std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);

    HMODULE const bmlModule = ModuleFromAddress(&BML_GetModContext);
    if (bmlModule && callerModule == bmlModule) {
        if (!requestedOwnerId || !*requestedOwnerId)
            return {};
        const auto requested = m_ModIndex.find(requestedOwnerId);
        if (requested == m_ModIndex.end() ||
            requested->second >= m_Mods.size())
            return {};
        IMod *owner = m_Mods[requested->second];
        IMod *invoked = ModInvocation::Current(this);
#if BML_ENABLE_ANGELSCRIPT
        // Script calls enter through BMLPlus.dll too. Hot-reload and queued
        // service callbacks are not necessarily inside the Loader's native
        // Mod broadcast scope, but ScriptModRuntime still carries the exact
        // physical Script Mod whose code is executing.
        if (!invoked)
            invoked = BML::ScriptModRuntime::GetCurrentScriptMod();
#endif
        if (owner == invoked || (owner == m_BMLMod && !invoked))
            return requested->first;
        return {};
    }

    if (requestedOwnerId && *requestedOwnerId) {
        const auto requested = m_ModIndex.find(requestedOwnerId);
        if (requested == m_ModIndex.end() || requested->second >= m_Mods.size() ||
            !m_NativeModRegistry.Owns(callerModule, requested->first))
            return {};
        return requested->first;
    }

    const std::string ownerId = m_NativeModRegistry.GetUniqueModId(callerModule);
    if (ownerId.empty())
        return {};

    const auto id = m_ModIndex.find(ownerId);
    return id == m_ModIndex.end() || id->second >= m_Mods.size()
        ? std::string()
        : id->first;
}

bool ModLoader::NativeModOwnsAddress(
    const std::string &ownerId, const void *address) const {
    if (ownerId.empty() || !address)
        return false;

    HMODULE module = ModuleFromAddress(address);
    if (!module)
        return false;

    std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
    return m_NativeModRegistry.Owns(module, ownerId);
}

bool ModLoader::UnregisterMod(IMod *mod) {
    if (!mod) {
        return false;
    }
    if (!m_Context.IsMainThread())
        return false;

    if (m_ModInvocationGate.IsCallActiveOnCurrentThread())
        return false;
    auto invocationLock = m_ModInvocationGate.LockMutation();

    try {
        std::string modIdCopy;
        {
            std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
            const auto registered = std::find(m_Mods.begin(), m_Mods.end(), mod);
            if (registered == m_Mods.end())
                return false;
            const size_t position = static_cast<size_t>(std::distance(m_Mods.begin(), registered));
            const auto id = std::find_if(m_ModIndex.begin(), m_ModIndex.end(), [position](const auto &entry) {
                return entry.second == position;
            });
            if (id == m_ModIndex.end())
                return false;
            modIdCopy = id->first;
        }
        if (!m_Context.PrepareModUnload(modIdCopy))
            return false;
#if BML_ENABLE_ANGELSCRIPT
        if (m_ScriptHotReload) {
            if (auto *scriptMod = dynamic_cast<BML::ScriptMod *>(mod))
                m_ScriptHotReload->UnregisterMod(scriptMod);
        }
#endif
        std::shared_ptr<void> ownedDllHandle;
        {
            std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
            ownedDllHandle = m_NativeModRegistry.FindDllForMod(modIdCopy);
        }

        // Config persistence needs the live Mod id, so detach and destroy the
        // loader-owned config before the Mod instance or its DLL goes away.
        if (Config *config = m_Context.GetConfig(mod); config && !m_Context.RemoveConfig(config)) {
            if (m_Context.GetLogger())
                m_Context.GetLogger()->Error("Failed to detach config before unloading mod %s.", modIdCopy.c_str());
            return false;
        }

        void *rawDllHandle = ownedDllHandle.get();
        if (!m_Context.UnregisterNativeCommands(rawDllHandle))
            return false;
        {
            std::lock_guard<std::mutex> lock(m_StateMutex);
            std::unique_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);

            if (ownedDllHandle && !m_NativeModRegistry.Remove(modIdCopy))
                throw std::logic_error("native Mod registry lost its DLL association");

            // Remove from callback map to prevent dangling pointer in BroadcastCallback
            for (auto &kv : m_CallbackMap) {
                auto &vec = kv.second;
                vec.erase(std::remove(vec.begin(), vec.end(), mod), vec.end());
            }

            const auto registeredId = m_ModIndex.find(modIdCopy);
            if (registeredId == m_ModIndex.end() || registeredId->second >= m_Mods.size() ||
                m_Mods[registeredId->second] != mod)
                throw std::logic_error("Mod index lost its registry entry");

            const size_t removedIndex = registeredId->second;
            m_Mods.erase(m_Mods.begin() + static_cast<std::ptrdiff_t>(removedIndex));
            m_ModIndex.erase(registeredId);
            m_ModGenerations.erase(mod);
            ++m_ModRegistryRevision;
            for (auto &entry : m_ModIndex) {
                if (entry.second > removedIndex)
                    --entry.second;
            }

            m_ActiveMods.erase(std::remove(m_ActiveMods.begin(), m_ActiveMods.end(), mod),
                               m_ActiveMods.end());

            m_ModDependencies.erase(mod);
        }

        invocationLock.unlock();
        DestroyNativeMod(rawDllHandle, mod, modIdCopy.c_str());

        return true;
    } catch (...) {
        return false;
    }
}

bool ModLoader::ResolveDependencies() {
    struct OrderDependency {
        std::string Id;
        BMLVersion MinVersion;
        bool Optional = false;
    };

    auto invocationLock = LockModInvocation();
    std::vector<IMod *> mods;
    std::unordered_map<IMod *, std::vector<OrderDependency>> dependencies;
    {
        std::lock_guard<std::mutex> stateLock(m_StateMutex);
        std::shared_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        mods = m_Mods;
        dependencies.reserve(m_ModDependencies.size());
        for (const auto &entry : m_ModDependencies) {
            auto &snapshot = dependencies[entry.first];
            snapshot.reserve(entry.second.size());
            for (const ModDependency &dependency : entry.second) {
                if (dependency.id && *dependency.id)
                    snapshot.push_back({dependency.id, dependency.minVersion, dependency.optional != 0});
            }
        }
    }

    // Build a stable position map to keep deterministic ordering for nodes with the same in-degree
    std::unordered_map<std::string, size_t> pos;
    std::unordered_map<std::string, IMod *> modMap;
    std::unordered_map<IMod *, std::string> idsByMod;
    pos.reserve(mods.size());
    modMap.reserve(mods.size());
    idsByMod.reserve(mods.size());

    for (size_t i = 0; i < mods.size(); ++i) {
        IMod *m = mods[i];
        const char *reportedId = m ? m->GetID() : nullptr;
        if (!reportedId || !*reportedId) {
            if (m_Context.GetLogger())
                m_Context.GetLogger()->Error("Cannot resolve Mod dependencies: registry entry %zu has no valid Mod id.", i);
            return false;
        }
        std::string id = reportedId;
        pos[id] = i;
        modMap[id] = m;
        idsByMod[m] = id;
    }

    // adj: dependency -> [dependents]
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_map<std::string, int> inDegree;

    // Initialize in-degrees to 0 for all known mods
    for (auto &kv : modMap) inDegree[kv.first] = 0;

    // Collect edges and compute in-degree as "number of (present) dependencies" for each mod
    for (IMod *m : mods) {
        const std::string &mid = idsByMod.at(m);
        auto it = dependencies.find(m);
        if (it == dependencies.end()) continue;

        std::unordered_set<std::string> seen; // deduplicate per mod
        for (const auto &dep : it->second) {
            const std::string &depId = dep.Id;
            if (!seen.insert(depId).second) continue; // skip duplicate dependency

            auto depInSet = modMap.find(depId);
            if (depInSet == modMap.end()) {
                if (!dep.Optional) {
                    if (m_Context.GetLogger()) {
                        m_Context.GetLogger()->Error(
                            "Cannot initialize Mod %s: required dependency '%s' version %s or newer is not installed.",
                            mid.c_str(), depId.c_str(), dep.MinVersion.ToString().c_str());
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
    for (IMod *m : mods) {
        const std::string &id = idsByMod.at(m);
        if (inDegree[id] == 0) q.push(id);
    }

    std::vector<IMod *> sorted;
    sorted.reserve(mods.size());

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
    if (sorted.size() != mods.size()) {
        std::string affected;
        for (IMod *mod : mods) {
            const std::string &id = idsByMod.at(mod);
            const auto entry = inDegree.find(id);
            if (entry == inDegree.end() || entry->second == 0)
                continue;
            if (!affected.empty())
                affected += ", ";
            affected += "'";
            affected += id;
            affected += "'";
        }
        if (m_Context.GetLogger()) {
            m_Context.GetLogger()->Error(
                "Cannot resolve Mod dependencies: a dependency cycle involves or blocks %s.",
                affected.empty() ? "one or more Mods" : affected.c_str());
        }
        return false;
    }

    // Build the replacement index before changing either published container.
    // A failed allocation or inconsistent id leaves the current registry intact.
    std::unordered_map<std::string, size_t> sortedIndex;
    sortedIndex.reserve(sorted.size());
    for (size_t i = 0; i < sorted.size(); ++i) {
        const auto inserted = sortedIndex.emplace(idsByMod.at(sorted[i]), i);
        if (!inserted.second)
            throw std::logic_error("duplicate Mod id in dependency order");
    }

    {
        std::unique_lock<std::shared_mutex> registryLock(m_ModRegistryMutex);
        m_Mods.swap(sorted);
        m_ModIndex.swap(sortedIndex);
    }
    return true;
}

void ModLoader::FillCallbackMap(IMod *mod) {
    std::lock_guard<std::mutex> lock(m_StateMutex);

    class BlankMod : IMod {
    public:
        explicit BlankMod(IBML *bml) : IMod(bml) {}

        const char *GetID() override { return ""; }
        const char *GetVersion() override { return ""; }
        const char *GetName() override { return ""; }
        const char *GetAuthor() override { return ""; }
        const char *GetDescription() override { return ""; }
        DECLARE_BML_VERSION;
    } blank(&m_Context);

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
