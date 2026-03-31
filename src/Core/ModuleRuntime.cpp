#include "ModuleRuntime.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "bml_export.h"
#include "FaultTracker.h"
#include "bml_hot_reload.h"
#include "bml_imc.h"
#include "bml_topics.h"

#include "Context.h"
#include "KernelServices.h"
#include "HotReloadCoordinator.h"
#include "ImcBus.h"
#include "Logging.h"
#include "PackageInstallState.h"
#include "PackagePaths.h"
#include "PathUtils.h"
#include "SemanticVersion.h"

namespace BML::Core {
    // Destructor must be defined here where HotReloadCoordinator is complete
    ModuleRuntime::~ModuleRuntime() = default;

    namespace {
        constexpr char kModuleRuntimeLogCategory[] = "module.runtime";

        const char *DefaultModuleExtension() {
#if defined(_WIN32)
            return ".dll";
#elif defined(__APPLE__)
            return ".dylib";
#else
            return ".so";
#endif
        }

        uint16_t ClampVersionComponent(int value) {
            if (value < 0)
                return 0;
            if (value > 0xFFFF)
                return 0xFFFF;
            return static_cast<uint16_t>(value);
        }

        BML_Version ToBmlVersion(const SemanticVersion &version) {
            return bmlMakeVersion(ClampVersionComponent(version.major),
                                  ClampVersionComponent(version.minor),
                                  ClampVersionComponent(version.patch));
        }

        bool IsHotReloadEnvEnabled() {
            auto is_disabled = [](const std::string &value) {
                return value == "0" || value == "false" || value == "off";
            };

#if defined(_WIN32)
            if (const wchar_t *raw = _wgetenv(L"BML_HOT_RELOAD")) {
                std::wstring value(raw);
                for (auto &ch : value) {
                    ch = static_cast<wchar_t>(towlower(ch));
                }

                if (value == L"0" || value == L"false" || value == L"off") {
                    return false;
                }
                return true;
            }
#endif

            const char *raw = std::getenv("BML_HOT_RELOAD");
            if (!raw) {
                return false;
            }

            std::string value(raw);
            for (auto &ch : value) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return !is_disabled(value);
        }

        std::vector<uint8_t> BuildLifecyclePayload(const ModManifest &manifest) {
            const auto &id = manifest.package.id;
            auto version = ToBmlVersion(manifest.package.parsed_version);
            const size_t id_len = id.size();
            std::vector<uint8_t> buffer(sizeof(BML_ModLifecycleWireHeader) + id_len);
            auto *header = reinterpret_cast<BML_ModLifecycleWireHeader *>(buffer.data());
            header->version = version;
            header->id_length = static_cast<uint32_t>(id_len);
            if (id_len != 0) {
                std::memcpy(buffer.data() + sizeof(BML_ModLifecycleWireHeader), id.data(), id_len);
            }
            return buffer;
        }

        void InvalidateRuntimeProvidersForSnapshot(Context &context,
                                                   const std::vector<LoadedModuleSnapshot> &modules) {
            for (const auto &module : modules) {
                if (module.id.empty()) {
                    continue;
                }
                context.InvalidateRuntimeProvider(module.id);
            }
        }

        bool SyncPendingPackages(const std::wstring &mods_dir,
                                 ModuleBootstrapDiagnostics &out_diag,
                                 std::string &out_error) {
            const auto layout = utils::ResolveRuntimeLayoutFromModsDirectory(mods_dir);
            PackageInstaller installer;
            PackageSyncDiagnostics packageDiag;
            if (!installer.SyncPackages(layout.runtime_directory.wstring(), packageDiag, out_error)) {
                out_diag.package_rejections = packageDiag.rejected;
                out_diag.package_warnings = packageDiag.warnings;
                out_diag.package_sync_error = out_error;
                return false;
            }

            out_diag.package_rejections = std::move(packageDiag.rejected);
            out_diag.package_warnings = std::move(packageDiag.warnings);
            out_diag.package_sync_error.clear();
            return true;
        }

        void ApplyPackageArchiveMetadata(const std::wstring &mods_dir,
                                         std::vector<std::unique_ptr<ModManifest>> &manifests) {
            const auto layout = utils::ResolveRuntimeLayoutFromModsDirectory(mods_dir);
            const PackagePaths packagePaths = GetPackagePaths(layout.runtime_directory.wstring());

            PackageInstallState state;
            std::string error;
            if (!state.Load(packagePaths.state_path.wstring(), error)) {
                return;
            }

            for (auto &manifest : manifests) {
                if (!manifest) {
                    continue;
                }
                const InstalledRecord *installed = state.FindInstalled(manifest->package.id);
                if (!installed) {
                    continue;
                }
                const PackageRecord *package = state.FindPackageByHash(installed->current_hash);
                if (!package) {
                    continue;
                }
                manifest->source_archive = package->archive_path;
            }
        }
    } // namespace

    bool ModuleRuntime::DiscoverAndValidate(const std::wstring &mods_dir, ModuleBootstrapDiagnostics &out_diag) {
        out_diag = {};
        if (!m_Kernel) {
            ApplyDiagnostics(out_diag);
            return false;
        }
        auto &kernel = *m_Kernel;
        auto &context = *kernel.context;
        m_HotReloadEnabled = ShouldEnableHotReload();
        m_DiscoveredModsDir = mods_dir;
        m_DiscoveredOrder.clear();

        std::string packageError;
        if (!SyncPendingPackages(mods_dir, out_diag, packageError)) {
            context.ClearManifests();
            m_DiscoveredOrder.clear();
            ApplyDiagnostics(out_diag);
            return false;
        }

        ManifestLoadResult manifestResult;
        if (!LoadManifestsFromDirectory(mods_dir, manifestResult)) {
            out_diag.manifest_errors = manifestResult.errors;
            context.ClearManifests();
            m_DiscoveredOrder.clear();
            ApplyDiagnostics(out_diag);
            return false;
        }
        ApplyPackageArchiveMetadata(mods_dir, manifestResult.manifests);

        std::vector<ResolvedNode> loadOrder;
        std::vector<DependencyWarning> warnings;
        DependencyResolutionError depError;
        if (!BuildLoadOrder(manifestResult, loadOrder, warnings, depError)) {
            out_diag.manifest_errors = manifestResult.errors;
            out_diag.dependency_error = depError;
            context.ClearManifests();
            m_DiscoveredOrder.clear();
            ApplyDiagnostics(out_diag);
            return false;
        }

        // Log dependency warnings
        for (const auto &warning : warnings) {
            out_diag.dependency_warnings.push_back(warning);
        }

        FilterDisabledModules(loadOrder);

        RecordLoadOrder(loadOrder, out_diag);

        // LIFETIME: ResolvedNode::manifest pointers reference objects owned by
        // unique_ptrs in manifestResult.manifests. The std::move below transfers
        // ownership to Context without relocating the objects, so the pointers
        // in m_DiscoveredOrder remain valid. Do NOT copy or reallocate manifests
        // between here and LoadDiscovered().
        m_DiscoveredOrder = loadOrder;

        context.ClearManifests();
        for (auto &manifest : manifestResult.manifests) {
            context.RegisterManifest(std::move(manifest));
        }

        ApplyDiagnostics(out_diag);
        return true;
    }

    bool ModuleRuntime::LoadDiscovered(ModuleBootstrapDiagnostics &out_diag,
                                       const BML_Services *services) {
        out_diag = {};
        if (!m_Kernel) {
            ApplyDiagnostics(out_diag);
            return false;
        }
        auto &kernel = *m_Kernel;
        auto &context = *kernel.context;

        if (m_DiscoveredModsDir.empty()) {
            // DiscoverAndValidate() has not run yet.
            ApplyDiagnostics(out_diag);
            return false;
        }
        if (!services) {
            out_diag.load_error.message = "Runtime service bundle is null";
            ApplyDiagnostics(out_diag);
            return false;
        }

        m_Services = services;

        std::vector<LoadedModule> loadedModules;
        ModuleLoadError loadError;

        if (!LoadModules(m_DiscoveredOrder,
                         context,
                         kernel,
                         m_Services,
                         loadedModules,
                         loadError)) {
            out_diag.load_error = loadError;
            context.ShutdownModules(kernel);
            ApplyDiagnostics(out_diag);
            return false;
        }

        out_diag.load_error = loadError;

        for (auto &module : loadedModules) {
            context.AddLoadedModule(std::move(module));
        }

        RecordLoadOrder(m_DiscoveredOrder, out_diag);

        if (m_HotReloadEnabled) {
            EnsureHotReloadCoordinator();
            UpdateHotReloadRegistration();
        } else {
            StopHotReloadCoordinator();
        }
        ApplyDiagnostics(out_diag);
        return true;
    }

    void ModuleRuntime::Update() {
        if (m_HotReloadEnabled && m_HotReloadCoordinator) {
            m_HotReloadCoordinator->Update();
        }
    }

    void ModuleRuntime::Shutdown() {
        if (!m_Kernel) {
            return;
        }
        auto &kernel = *m_Kernel;
        auto &context = *kernel.context;
        m_DiagCallback = nullptr;
        StopHotReloadCoordinator();
        const auto loadedModules = context.GetLoadedModuleSnapshot();
        context.ShutdownModules(kernel);
        InvalidateRuntimeProvidersForSnapshot(context, loadedModules);
    }

    void ModuleRuntime::FilterDisabledModules(std::vector<ResolvedNode> &order) const {
        if (!m_Kernel) {
            order.clear();
            return;
        }
        auto &faultTracker = *m_Kernel->fault_tracker;
        auto removed = std::remove_if(order.begin(), order.end(),
            [&faultTracker](const ResolvedNode &node) {
                return faultTracker.IsDisabled(node.id);
            });
        for (auto it = removed; it != order.end(); ++it) {
            CoreLog(BML_LOG_WARN, kModuleRuntimeLogCategory,
                    "Skipping module '%s': disabled by fault tracker",
                    it->id.c_str());
        }
        order.erase(removed, order.end());
    }

    void ModuleRuntime::RecordLoadOrder(const std::vector<ResolvedNode> &order,
                                        ModuleBootstrapDiagnostics &diag) const {
        diag.load_order.clear();
        diag.load_order.reserve(order.size());
        for (const auto &node : order) {
            diag.load_order.push_back(node.id);
        }
    }

    bool ModuleRuntime::ReloadModules(ModuleBootstrapDiagnostics &out_diag) {
        out_diag = {};
        std::unique_lock lock(m_ReloadMutex);
        if (m_ReloadInProgress) {
            out_diag.load_error.message = "Reload already in progress";
            ApplyDiagnostics(out_diag);
            return false;
        }
        m_ReloadInProgress = true;
        lock.unlock();

        bool success = ReloadModulesInternal(out_diag);

        lock.lock();
        m_ReloadInProgress = false;
        lock.unlock();

        if (m_HotReloadEnabled) {
            EnsureHotReloadCoordinator();
            UpdateHotReloadRegistration();
        }

        ApplyDiagnostics(out_diag);
        return success;
    }

    void ModuleRuntime::SetDiagnosticsCallback(std::function<void(const ModuleBootstrapDiagnostics &)> callback) {
        m_DiagCallback = std::move(callback);
    }

#if defined(BML_TEST)
    void ModuleRuntime::TestHandleHotReloadNotify(const std::string &mod_id, ReloadResult result,
                                                  unsigned int version, ReloadFailure failure,
                                                  ReloadRequestKind kind) {
        HandleHotReloadNotify(mod_id, result, version, failure, kind);
    }

    void ModuleRuntime::TestBroadcastLifecycleEventForModule(const char *topic,
                                                             const std::string &mod_id) const {
        BroadcastLifecycleEventForModule(topic, mod_id);
    }

    std::vector<std::string> ModuleRuntime::TestGetLifecycleBroadcastTargets(
            const std::string &mod_id) const {
        std::vector<std::string> result;
        for (const auto &module : CollectLifecycleModulesForBroadcast(mod_id)) {
            result.push_back(module.id);
        }
        return result;
    }
#endif

    bool ModuleRuntime::ReloadModulesInternal(ModuleBootstrapDiagnostics &out_diag) {
        if (m_DiscoveredModsDir.empty()) {
            out_diag.load_error.message = "Hot reload requested before discovery";
            ApplyDiagnostics(out_diag);
            return false;
        }

        if (!m_Kernel) {
            ApplyDiagnostics(out_diag);
            return false;
        }
        auto &kernel = *m_Kernel;
        auto &context = *kernel.context;
        if (!m_Services) {
            out_diag.load_error.message = "Runtime service bundle is null";
            ApplyDiagnostics(out_diag);
            return false;
        }
        BroadcastLifecycleEvent(BML_TOPIC_SYSTEM_MOD_UNLOAD, context.GetLoadedModuleSnapshot());
        const auto priorLoadedModules = context.GetLoadedModuleSnapshot();
        context.ShutdownModules(kernel);
        InvalidateRuntimeProvidersForSnapshot(context, priorLoadedModules);
        context.ClearManifests();
        // Note: Extensions are cleaned up per-provider in ShutdownModules()

        std::string packageError;
        if (!SyncPendingPackages(m_DiscoveredModsDir, out_diag, packageError)) {
            m_DiscoveredOrder.clear();
            ApplyDiagnostics(out_diag);
            return false;
        }

        ManifestLoadResult manifestResult;
        if (!LoadManifestsFromDirectory(m_DiscoveredModsDir, manifestResult)) {
            out_diag.manifest_errors = manifestResult.errors;
            m_DiscoveredOrder.clear();
            ApplyDiagnostics(out_diag);
            return false;
        }
        ApplyPackageArchiveMetadata(m_DiscoveredModsDir, manifestResult.manifests);

        std::vector<ResolvedNode> loadOrder;
        std::vector<DependencyWarning> warnings;
        DependencyResolutionError depError;
        if (!BuildLoadOrder(manifestResult, loadOrder, warnings, depError)) {
            out_diag.manifest_errors = manifestResult.errors;
            out_diag.dependency_error = depError;
            m_DiscoveredOrder.clear();
            ApplyDiagnostics(out_diag);
            return false;
        }

        for (const auto &warning : warnings) {
            out_diag.dependency_warnings.push_back(warning);
        }

        FilterDisabledModules(loadOrder);
        RecordLoadOrder(loadOrder, out_diag);
        m_DiscoveredOrder = loadOrder;

        for (auto &manifest : manifestResult.manifests) {
            context.RegisterManifest(std::move(manifest));
        }

        std::vector<LoadedModule> loadedModules;
        ModuleLoadError loadError;
        if (!LoadModules(loadOrder,
                         context,
                         kernel,
                         m_Services,
                         loadedModules,
                         loadError)) {
            out_diag.load_error = loadError;
            context.ShutdownModules(kernel);
            // Note: Extensions cleaned up in ShutdownModules()
            ApplyDiagnostics(out_diag);
            return false;
        }

        out_diag.load_error = loadError;

        for (auto &module : loadedModules) {
            context.AddLoadedModule(std::move(module));
        }

        BroadcastLifecycleEvent(BML_TOPIC_SYSTEM_MOD_RELOAD, context.GetLoadedModuleSnapshot());
        ApplyDiagnostics(out_diag);
        return true;
    }

    void ModuleRuntime::BroadcastLifecycleEvent(const char *topic, const std::vector<LoadedModuleSnapshot> &modules) const {
        if (!topic)
            return;
        if (!m_Kernel) {
            return;
        }

        BML_TopicId topic_id;
        if (ImcGetTopicId(*m_Kernel, topic, &topic_id) != BML_RESULT_OK)
            return;

        for (const auto &module : modules) {
            if (!module.manifest)
                continue;
            auto payload = BuildLifecyclePayload(*module.manifest);
            ImcPublish(*m_Kernel, topic_id, payload.data(), payload.size());
        }
    }

    void ModuleRuntime::BroadcastLifecycleEventForModule(const char *topic,
                                                         const std::string &mod_id) const {
        if (!m_Kernel || mod_id.empty()) {
            return;
        }

        auto modules = CollectLifecycleModulesForBroadcast(mod_id);
        if (!modules.empty()) {
            BroadcastLifecycleEvent(topic, modules);
        }
    }

    std::vector<LoadedModuleSnapshot> ModuleRuntime::CollectLifecycleModulesForBroadcast(
            const std::string &mod_id) const {
        std::vector<LoadedModuleSnapshot> result;
        if (!m_Kernel || mod_id.empty()) {
            return result;
        }

        auto snapshot = m_Kernel->context->GetLoadedModuleSnapshot();
        for (const auto &module : snapshot) {
            if (module.id == mod_id && module.manifest) {
                result.push_back(module);
                break;
            }
        }
        return result;
    }

    void ModuleRuntime::UpdateHotReloadRegistration() {
        if (!m_HotReloadEnabled || !m_HotReloadCoordinator)
            return;
        if (!m_Kernel) {
            return;
        }
        auto &context = *m_Kernel->context;

        // Unregister all existing modules first
        auto registered = m_HotReloadCoordinator->GetRegisteredModules();
        for (const auto &id : registered) {
            m_HotReloadCoordinator->UnregisterModule(id);
        }

        // Register loaded modules for hot reload
        auto manifests = context.GetManifestSnapshot();
        for (const auto &manifest : manifests) {

            // Build DLL path from manifest
            std::wstring dll_path;
            if (!manifest.package.entry.empty()) {
                std::filesystem::path entry_path(manifest.package.entry);
                if (entry_path.is_relative()) {
                    std::filesystem::path base(manifest.directory);
                    entry_path = base / entry_path;
                }
                dll_path = entry_path.lexically_normal().wstring();
            } else if (!manifest.package.id.empty()) {
                std::filesystem::path base(manifest.directory);
                dll_path = (base / (manifest.package.id + DefaultModuleExtension())).lexically_normal().wstring();
            }

            if (dll_path.empty())
                continue;

            HotReloadModuleEntry entry;
            entry.id = manifest.package.id;
            entry.dll_path = dll_path;
            entry.watch_path = std::filesystem::path(manifest.directory).lexically_normal().wstring();
            entry.manifest = manifest;
            // Script modules need recursive watching for subdirectory includes.
            // Use case-insensitive comparison since Windows paths can vary.
            std::filesystem::path ep(dll_path);
            auto ext = ep.extension().string();
            bool is_native_dll = false;
            if (ext.size() == 4) {
                is_native_dll = (ext[0] == '.' &&
                                 (ext[1] == 'd' || ext[1] == 'D') &&
                                 (ext[2] == 'l' || ext[2] == 'L') &&
                                 (ext[3] == 'l' || ext[3] == 'L'));
            }
            entry.watch_recursive = !is_native_dll;

            m_HotReloadCoordinator->RegisterModule(entry);
        }
    }

    void ModuleRuntime::EnsureHotReloadCoordinator() {
        if (!m_HotReloadEnabled)
            return;
        if (!m_HotReloadCoordinator) {
            auto &context = *m_Kernel->context;
            m_HotReloadCoordinator = std::make_unique<HotReloadCoordinator>(context, *m_Kernel);
            m_HotReloadCoordinator->SetServices(m_Services);

            HotReloadSettings settings;
            settings.enabled = true;
            settings.debounce = std::chrono::milliseconds(500);
            settings.temp_directory = GetHotReloadTempDirectory();
            m_HotReloadCoordinator->Configure(settings);

            m_HotReloadCoordinator->SetNotifyCallback(
                [this](const std::string &mod_id, ReloadResult result,
                       unsigned int version, ReloadFailure failure,
                       ReloadRequestKind kind) {
                    HandleHotReloadNotify(mod_id, result, version, failure, kind);
                });

            m_HotReloadCoordinator->Start();
        }
    }

    void ModuleRuntime::StopHotReloadCoordinator() {
        if (m_HotReloadCoordinator) {
            m_HotReloadCoordinator->Stop();
            m_HotReloadCoordinator.reset();
        }
    }

    bool ModuleRuntime::ReloadSingleModule(const std::string &mod_id,
                                            ModuleBootstrapDiagnostics &out_diag) {
        if (!m_Kernel || !m_Kernel->context || !m_HotReloadCoordinator) {
            return false;
        }
        auto &context = *m_Kernel->context;

        // Verify the module exists in the loaded snapshot
        bool found = false;
        for (const auto &m : context.GetLoadedModuleSnapshot()) {
            if (m.id == mod_id) {
                found = true;
                break;
            }
        }
        if (!found) {
            CoreLog(BML_LOG_WARN, kModuleRuntimeLogCategory,
                    "Targeted reload: module '%s' not found in loaded modules", mod_id.c_str());
            return false;
        }

        // The ReloadableModuleSlot has already performed the DLL swap
        // (UnloadCurrent -> LoadVersion) including:
        //   - PrepareModuleForDetach gate (checks dependencies)
        //   - DETACH entrypoint call
        //   - CleanupModuleKernelState
        //   - FreeLibrary old DLL
        //   - CopyDllToTemp -> LoadLibrary new DLL
        //   - ATTACH entrypoint call
        //
        // We just need to synchronize Context's loaded-module record
        // with the slot's new DLL handle and entrypoint.

        HMODULE new_handle = nullptr;
        PFN_BML_ModEntrypoint new_entrypoint = nullptr;
        if (!m_HotReloadCoordinator->GetSlotModuleInfo(mod_id, &new_handle, &new_entrypoint)) {
            CoreLog(BML_LOG_WARN, kModuleRuntimeLogCategory,
                    "Targeted reload: slot for '%s' not loaded after reload", mod_id.c_str());
            return false;
        }

        if (!context.UpdateLoadedModule(mod_id, new_handle, new_entrypoint)) {
            CoreLog(BML_LOG_WARN, kModuleRuntimeLogCategory,
                    "Targeted reload: failed to update Context for '%s'", mod_id.c_str());
            return false;
        }

        BroadcastLifecycleEventForModule(BML_TOPIC_SYSTEM_MOD_RELOAD, mod_id);
        CoreLog(BML_LOG_INFO, kModuleRuntimeLogCategory,
                "Targeted hot reload of '%s' succeeded (version %u)",
                mod_id.c_str(), m_HotReloadCoordinator->GetModuleVersion(mod_id));
        return true;
    }

    void ModuleRuntime::HandleHotReloadNotify(const std::string &mod_id, ReloadResult result,
                                              unsigned int version, ReloadFailure failure,
                                              ReloadRequestKind kind) {
        if (kind == ReloadRequestKind::FullRuntime) {
            ModuleBootstrapDiagnostics diag;
            if (ReloadModules(diag)) {
                CoreLog(BML_LOG_INFO, kModuleRuntimeLogCategory,
                        "Full hot reload succeeded for '%s'", mod_id.c_str());
            } else {
                CoreLog(BML_LOG_ERROR, kModuleRuntimeLogCategory,
                        "Full hot reload failed for '%s'", mod_id.c_str());
            }
            return;
        }

        if (result == ReloadResult::Success) {
            CoreLog(BML_LOG_INFO, kModuleRuntimeLogCategory,
                    "Hot reload notification: mod '%s' version %u, result=%d",
                    mod_id.c_str(), version, static_cast<int>(result));

            // Try targeted single-module reload first (only updates Context state)
            ModuleBootstrapDiagnostics diag;
            if (ReloadSingleModule(mod_id, diag)) {
                // Targeted reload succeeded - other modules were not disturbed
            } else {
                // Fall back to full reload (e.g. module not found, Context sync failed)
                CoreLog(BML_LOG_WARN, kModuleRuntimeLogCategory,
                        "Targeted reload of '%s' failed, falling back to full reload",
                        mod_id.c_str());
                if (ReloadModules(diag)) {
                    CoreLog(BML_LOG_INFO, kModuleRuntimeLogCategory,
                            "Full hot reload succeeded");
                } else {
                    CoreLog(BML_LOG_ERROR, kModuleRuntimeLogCategory,
                            "Full hot reload failed");
                }
            }
        } else if (result == ReloadResult::UnloadBlocked) {
            // Module has active dependencies that prevent targeted reload.
            // Fall back to full reload which tears down everything.
            CoreLog(BML_LOG_WARN, kModuleRuntimeLogCategory,
                    "Module '%s' has active dependencies, falling back to full reload",
                    mod_id.c_str());
            ModuleBootstrapDiagnostics diag;
            if (!ReloadModules(diag)) {
                CoreLog(BML_LOG_ERROR, kModuleRuntimeLogCategory,
                        "Full reload fallback also failed for '%s'", mod_id.c_str());
            }
        } else if (result == ReloadResult::RolledBack) {
            CoreLog(BML_LOG_WARN, kModuleRuntimeLogCategory,
                    "Hot reload for mod '%s' rolled back to version %u; keeping recovered module active",
                    mod_id.c_str(), version);
        } else if (result != ReloadResult::NoChange) {
            CoreLog(BML_LOG_WARN, kModuleRuntimeLogCategory,
                    "Hot reload failed for mod '%s': result=%d, failure=%d",
                    mod_id.c_str(), static_cast<int>(result), static_cast<int>(failure));
        }
    }

    bool ModuleRuntime::ShouldEnableHotReload() const {
        return IsHotReloadEnvEnabled();
    }

    std::wstring ModuleRuntime::GetHotReloadTempDirectory() const {
        std::error_code ec;
        auto temp_dir = std::filesystem::temp_directory_path(ec);
        if (ec) {
            return {};
        }
        temp_dir /= L"BML_HotReload";
        return temp_dir.wstring();
    }

    void ModuleRuntime::ApplyDiagnostics(const ModuleBootstrapDiagnostics &diag) const {
        if (m_DiagCallback)
            m_DiagCallback(diag);
    }

    void ModuleRuntime::BindKernel(KernelServices &kernel) noexcept {
        m_Kernel = &kernel;
    }
} // namespace BML::Core

