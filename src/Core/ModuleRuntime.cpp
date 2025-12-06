#include "ModuleRuntime.h"

#include <iterator>
#include <chrono>
#include <cwchar>
#include <cwctype>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "bml_export.h"
#include "bml_hot_reload.h"
#include "bml_imc.h"

#include "Context.h"
#include "HotReloadCoordinator.h"
#include "ImcBus.h"
#include "SemanticVersion.h"
#include "Logging.h"

namespace BML::Core {
    // Destructor must be defined here where HotReloadCoordinator is complete
    ModuleRuntime::~ModuleRuntime() = default;

    namespace {
        constexpr char kModuleRuntimeLogCategory[] = "module.runtime";

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
            wchar_t buffer[16];
            DWORD copied = GetEnvironmentVariableW(L"BML_HOT_RELOAD", buffer, static_cast<DWORD>(std::size(buffer)));
            if (copied == 0)
                return false;

            std::wstring value;
            if (copied >= std::size(buffer)) {
                value.resize(copied);
                DWORD second = GetEnvironmentVariableW(L"BML_HOT_RELOAD", value.data(), copied);
                if (second == 0)
                    return false;
                value.resize(second);
            } else {
                value.assign(buffer, buffer + copied);
            }

            if (!value.empty() && value.back() == L'\0')
                value.pop_back();
            for (auto &ch : value)
                ch = static_cast<wchar_t>(towlower(ch));
            if (value == L"0" || value == L"false" || value == L"off")
                return false;
            return true;
        }

        bool IsCacheSubPath(const std::filesystem::path &path) {
            for (const auto &part : path) {
#ifdef _WIN32
                if (_wcsicmp(part.c_str(), L".bp-cache") == 0)
                    return true;
#else
                std::wstring lowered = part.wstring();
                for (auto &ch : lowered)
                    ch = static_cast<wchar_t>(towlower(ch));
                if (lowered == L".bp-cache")
                    return true;
#endif
            }
            return false;
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
    } // namespace
    bool ModuleRuntime::Initialize(const std::wstring &mods_dir, ModuleBootstrapDiagnostics &out_diag) {
        out_diag = {};
        m_HotReloadEnabled = ShouldEnableHotReload();
        m_DiscoveredModsDir = mods_dir;

        ManifestLoadResult manifestResult;
        if (!LoadManifestsFromDirectory(mods_dir, manifestResult)) {
            out_diag.manifest_errors = manifestResult.errors;
            Context::Instance().ClearManifests();
            ApplyDiagnostics(out_diag);
            return false;
        }

        std::vector<ResolvedNode> loadOrder;
        std::vector<DependencyWarning> warnings;
        DependencyResolutionError depError;
        if (!BuildLoadOrder(manifestResult, loadOrder, warnings, depError)) {
            out_diag.manifest_errors = manifestResult.errors;
            out_diag.dependency_error = depError;
            Context::Instance().ClearManifests();
            ApplyDiagnostics(out_diag);
            return false;
        }

        // Log dependency warnings
        for (const auto &warning : warnings) {
            out_diag.dependency_warnings.push_back(warning);
        }

        RecordLoadOrder(loadOrder, out_diag);
        m_DiscoveredOrder = loadOrder;

        auto &ctx = Context::Instance();
        for (auto &manifest : manifestResult.manifests) {
            ctx.RegisterManifest(std::move(manifest));
        }

        std::vector<LoadedModule> loadedModules;
        ModuleLoadError loadError;
        if (!LoadModules(loadOrder,
                         ctx,
                         &bmlGetProcAddress,
                         loadedModules,
                         loadError)) {
            out_diag.load_error = loadError;
            ctx.ShutdownModules();
            ctx.ClearManifests();
            ApplyDiagnostics(out_diag);
            return false;
        }

        for (auto &module : loadedModules) {
            ctx.AddLoadedModule(std::move(module));
        }

        if (m_HotReloadEnabled) {
            EnsureHotReloadCoordinator();
            UpdateHotReloadRegistration();
        } else {
            StopHotReloadCoordinator();
        }

        ApplyDiagnostics(out_diag);
        return true;
    }

    bool ModuleRuntime::DiscoverAndValidate(const std::wstring &mods_dir, ModuleBootstrapDiagnostics &out_diag) {
        out_diag = {};
        m_HotReloadEnabled = ShouldEnableHotReload();

        ManifestLoadResult manifestResult;
        if (!LoadManifestsFromDirectory(mods_dir, manifestResult)) {
            out_diag.manifest_errors = manifestResult.errors;
            Context::Instance().ClearManifests();
            ApplyDiagnostics(out_diag);
            return false;
        }

        std::vector<ResolvedNode> loadOrder;
        std::vector<DependencyWarning> warnings;
        DependencyResolutionError depError;
        if (!BuildLoadOrder(manifestResult, loadOrder, warnings, depError)) {
            out_diag.manifest_errors = manifestResult.errors;
            out_diag.dependency_error = depError;
            ApplyDiagnostics(out_diag);
            return false;
        }

        // Log dependency warnings
        for (const auto &warning : warnings) {
            out_diag.dependency_warnings.push_back(warning);
        }

        RecordLoadOrder(loadOrder, out_diag);

        // Store for later loading
        m_DiscoveredModsDir = mods_dir;
        m_DiscoveredOrder = std::move(loadOrder);

        // Register manifests with context
        auto &ctx = Context::Instance();
        for (auto &manifest : manifestResult.manifests) {
            ctx.RegisterManifest(std::move(manifest));
        }

        ApplyDiagnostics(out_diag);
        return true;
    }

    bool ModuleRuntime::LoadDiscovered(ModuleBootstrapDiagnostics &out_diag) {
        out_diag = {};

        if (m_DiscoveredOrder.empty()) {
            // Nothing discovered yet
            ApplyDiagnostics(out_diag);
            return false;
        }

        auto &ctx = Context::Instance();
        std::vector<LoadedModule> loadedModules;
        ModuleLoadError loadError;

        if (!LoadModules(m_DiscoveredOrder,
                         ctx,
                         &bmlGetProcAddress,
                         loadedModules,
                         loadError)) {
            out_diag.load_error = loadError;
            ctx.ShutdownModules();
            ctx.ClearManifests();
            ApplyDiagnostics(out_diag);
            return false;
        }

        for (auto &module : loadedModules) {
            ctx.AddLoadedModule(std::move(module));
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

    void ModuleRuntime::Shutdown() {
        m_DiagCallback = nullptr;
        StopHotReloadCoordinator();
        Context::Instance().ShutdownModules();
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

    bool ModuleRuntime::ReloadModulesInternal(ModuleBootstrapDiagnostics &out_diag) {
        if (m_DiscoveredModsDir.empty()) {
            out_diag.load_error.message = "Hot reload requested before discovery";
            ApplyDiagnostics(out_diag);
            return false;
        }

        auto &ctx = Context::Instance();
        BroadcastLifecycleEvent("BML/System/ModUnload", ctx.GetLoadedModules());
        ctx.ShutdownModules();
        ctx.ClearManifests();
        // Note: Extensions are cleaned up per-provider in ShutdownModules()

        ManifestLoadResult manifestResult;
        if (!LoadManifestsFromDirectory(m_DiscoveredModsDir, manifestResult)) {
            out_diag.manifest_errors = manifestResult.errors;
            ApplyDiagnostics(out_diag);
            return false;
        }

        std::vector<ResolvedNode> loadOrder;
        std::vector<DependencyWarning> warnings;
        DependencyResolutionError depError;
        if (!BuildLoadOrder(manifestResult, loadOrder, warnings, depError)) {
            out_diag.manifest_errors = manifestResult.errors;
            out_diag.dependency_error = depError;
            ApplyDiagnostics(out_diag);
            return false;
        }

        for (const auto &warning : warnings) {
            out_diag.dependency_warnings.push_back(warning);
        }

        RecordLoadOrder(loadOrder, out_diag);
        m_DiscoveredOrder = loadOrder;

        for (auto &manifest : manifestResult.manifests) {
            ctx.RegisterManifest(std::move(manifest));
        }

        std::vector<LoadedModule> loadedModules;
        ModuleLoadError loadError;
        if (!LoadModules(loadOrder,
                         ctx,
                         &bmlGetProcAddress,
                         loadedModules,
                         loadError)) {
            out_diag.load_error = loadError;
            ctx.ShutdownModules();
            ctx.ClearManifests();
            // Note: Extensions cleaned up in ShutdownModules()
            ApplyDiagnostics(out_diag);
            return false;
        }

        for (auto &module : loadedModules) {
            ctx.AddLoadedModule(std::move(module));
        }

        BroadcastLifecycleEvent("BML/System/ModReload", ctx.GetLoadedModules());
        ApplyDiagnostics(out_diag);
        return true;
    }

    void ModuleRuntime::BroadcastLifecycleEvent(const char *topic, const std::vector<LoadedModule> &modules) const {
        if (!topic)
            return;

        BML_TopicId topic_id;
        if (ImcBus::Instance().GetTopicId(topic, &topic_id) != BML_RESULT_OK)
            return;

        for (const auto &module : modules) {
            if (!module.manifest)
                continue;
            auto payload = BuildLifecyclePayload(*module.manifest);
            ImcBus::Instance().Publish(topic_id, payload.data(), payload.size());
        }
    }

    void ModuleRuntime::UpdateHotReloadRegistration() {
        if (!m_HotReloadEnabled || !m_HotReloadCoordinator)
            return;

        // Unregister all existing modules first
        auto registered = m_HotReloadCoordinator->GetRegisteredModules();
        for (const auto &id : registered) {
            m_HotReloadCoordinator->UnregisterModule(id);
        }

        // Register loaded modules for hot reload
        const auto &manifests = Context::Instance().GetManifests();
        for (const auto &manifest : manifests) {
            if (!manifest)
                continue;

            // Build DLL path from manifest
            std::wstring dll_path;
            if (!manifest->package.entry.empty()) {
                std::filesystem::path entry_path(manifest->package.entry);
                if (entry_path.is_relative()) {
                    std::filesystem::path base(manifest->directory);
                    entry_path = base / entry_path;
                }
                dll_path = entry_path.lexically_normal().wstring();
            } else if (!manifest->package.id.empty()) {
                std::filesystem::path base(manifest->directory);
                dll_path = (base / (manifest->package.id + ".dll")).lexically_normal().wstring();
            }

            if (dll_path.empty())
                continue;

            // Skip if in cache directory
            if (IsCacheSubPath(std::filesystem::path(dll_path)))
                continue;

            HotReloadModuleEntry entry;
            entry.id = manifest->package.id;
            entry.dll_path = dll_path;
            entry.watch_path = std::filesystem::path(manifest->directory).lexically_normal().wstring();
            entry.manifest = manifest.get();

            m_HotReloadCoordinator->RegisterModule(entry);
        }
    }

    void ModuleRuntime::EnsureHotReloadCoordinator() {
        if (!m_HotReloadEnabled)
            return;
        if (!m_HotReloadCoordinator) {
            m_HotReloadCoordinator = std::make_unique<HotReloadCoordinator>(Context::Instance());

            HotReloadSettings settings;
            settings.enabled = true;
            settings.debounce = std::chrono::milliseconds(500);
            settings.temp_directory = GetHotReloadTempDirectory();
            m_HotReloadCoordinator->Configure(settings);

            m_HotReloadCoordinator->SetNotifyCallback(
                [this](const std::string &mod_id, ReloadResult result,
                       unsigned int version, ReloadFailure failure) {
                    HandleHotReloadNotify(mod_id, result, version, failure);
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

    void ModuleRuntime::HandleHotReloadNotify(const std::string &mod_id, ReloadResult result,
                                              unsigned int version, ReloadFailure failure) {
        // For now, we still do a full reload when any module changes
        // In the future, this could be optimized to reload only the changed module
        if (result == ReloadResult::Success || result == ReloadResult::RolledBack) {
            CoreLog(BML_LOG_INFO, kModuleRuntimeLogCategory,
                    "Hot reload notification: mod '%s' version %u, result=%d",
                    mod_id.c_str(), version, static_cast<int>(result));

            // Trigger full reload for now (matching old behavior)
            ModuleBootstrapDiagnostics diag;
            if (ReloadModules(diag)) {
                CoreLog(BML_LOG_INFO, kModuleRuntimeLogCategory, "Hot reload succeeded");
            } else {
                CoreLog(BML_LOG_ERROR, kModuleRuntimeLogCategory, "Hot reload failed");
            }
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
        // Use system temp directory with BML subfolder
        wchar_t temp_path[MAX_PATH];
        DWORD len = GetTempPathW(MAX_PATH, temp_path);
        if (len == 0 || len >= MAX_PATH) {
            return L"";
        }
        std::filesystem::path temp_dir(temp_path);
        temp_dir /= L"BML_HotReload";
        return temp_dir.wstring();
    }

    void ModuleRuntime::ApplyDiagnostics(const ModuleBootstrapDiagnostics &diag) const {
        if (m_DiagCallback)
            m_DiagCallback(diag);
    }
} // namespace BML::Core
