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
#include "HotReloadMonitor.h"
#include "ImcBus.h"
#include "SemanticVersion.h"

namespace BML::Core {
    // Destructor must be defined here where HotReloadMonitor is complete
    ModuleRuntime::~ModuleRuntime() = default;

    namespace {
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
        hot_reload_enabled_ = ShouldEnableHotReload();
        discovered_mods_dir_ = mods_dir;

        ManifestLoadResult manifestResult;
        if (!LoadManifestsFromDirectory(mods_dir, manifestResult)) {
            out_diag.manifest_errors = manifestResult.errors;
            return false;
        }

        std::vector<ResolvedNode> loadOrder;
        std::vector<DependencyWarning> warnings;
        DependencyResolutionError depError;
        if (!BuildLoadOrder(manifestResult, loadOrder, warnings, depError)) {
            out_diag.manifest_errors = manifestResult.errors;
            out_diag.dependency_error = depError;
            return false;
        }

        // Log dependency warnings
        for (const auto &warning : warnings) {
            out_diag.dependency_warnings.push_back(warning);
        }

        RecordLoadOrder(loadOrder, out_diag);
        discovered_order_ = loadOrder;

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
            return false;
        }

        for (auto &module : loadedModules) {
            ctx.AddLoadedModule(std::move(module));
        }

        if (hot_reload_enabled_) {
            EnsureHotReloadMonitor();
            UpdateHotReloadWatchList();
        } else {
            StopHotReloadMonitor();
        }

        return true;
    }

    bool ModuleRuntime::DiscoverAndValidate(const std::wstring &mods_dir, ModuleBootstrapDiagnostics &out_diag) {
        out_diag = {};
        hot_reload_enabled_ = ShouldEnableHotReload();

        ManifestLoadResult manifestResult;
        if (!LoadManifestsFromDirectory(mods_dir, manifestResult)) {
            out_diag.manifest_errors = manifestResult.errors;
            return false;
        }

        std::vector<ResolvedNode> loadOrder;
        std::vector<DependencyWarning> warnings;
        DependencyResolutionError depError;
        if (!BuildLoadOrder(manifestResult, loadOrder, warnings, depError)) {
            out_diag.manifest_errors = manifestResult.errors;
            out_diag.dependency_error = depError;
            return false;
        }

        // Log dependency warnings
        for (const auto &warning : warnings) {
            out_diag.dependency_warnings.push_back(warning);
        }

        RecordLoadOrder(loadOrder, out_diag);

        // Store for later loading
        discovered_mods_dir_ = mods_dir;
        discovered_order_ = std::move(loadOrder);

        // Register manifests with context
        auto &ctx = Context::Instance();
        for (auto &manifest : manifestResult.manifests) {
            ctx.RegisterManifest(std::move(manifest));
        }

        return true;
    }

    bool ModuleRuntime::LoadDiscovered(ModuleBootstrapDiagnostics &out_diag) {
        out_diag = {};

        if (discovered_order_.empty()) {
            // Nothing discovered yet
            return false;
        }

        auto &ctx = Context::Instance();
        std::vector<LoadedModule> loadedModules;
        ModuleLoadError loadError;

        if (!LoadModules(discovered_order_,
                         ctx,
                         &bmlGetProcAddress,
                         loadedModules,
                         loadError)) {
            out_diag.load_error = loadError;
            ctx.ShutdownModules();
            ctx.ClearManifests();
            return false;
        }

        for (auto &module : loadedModules) {
            ctx.AddLoadedModule(std::move(module));
        }

        RecordLoadOrder(discovered_order_, out_diag);

        if (hot_reload_enabled_) {
            EnsureHotReloadMonitor();
            UpdateHotReloadWatchList();
        } else {
            StopHotReloadMonitor();
        }
        return true;
    }

    void ModuleRuntime::Shutdown() {
        diag_callback_ = nullptr;
        StopHotReloadMonitor();
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
        std::unique_lock lock(reload_mutex_);
        if (reload_in_progress_) {
            out_diag.load_error.message = "Reload already in progress";
            return false;
        }
        reload_in_progress_ = true;
        lock.unlock();

        bool success = ReloadModulesInternal(out_diag);

        lock.lock();
        reload_in_progress_ = false;
        lock.unlock();

        if (hot_reload_enabled_) {
            EnsureHotReloadMonitor();
            UpdateHotReloadWatchList();
        }

        ApplyDiagnostics(out_diag);
        return success;
    }

    void ModuleRuntime::SetDiagnosticsCallback(std::function<void(const ModuleBootstrapDiagnostics &)> callback) {
        diag_callback_ = std::move(callback);
    }

    bool ModuleRuntime::ReloadModulesInternal(ModuleBootstrapDiagnostics &out_diag) {
        if (discovered_mods_dir_.empty()) {
            out_diag.load_error.message = "Hot reload requested before discovery";
            return false;
        }

        auto &ctx = Context::Instance();
        BroadcastLifecycleEvent("BML/System/ModUnload", ctx.GetLoadedModules());
        ctx.ShutdownModules();
        ctx.ClearManifests();
        // Note: Extensions are cleaned up per-provider in ShutdownModules()

        ManifestLoadResult manifestResult;
        if (!LoadManifestsFromDirectory(discovered_mods_dir_, manifestResult)) {
            out_diag.manifest_errors = manifestResult.errors;
            return false;
        }

        std::vector<ResolvedNode> loadOrder;
        std::vector<DependencyWarning> warnings;
        DependencyResolutionError depError;
        if (!BuildLoadOrder(manifestResult, loadOrder, warnings, depError)) {
            out_diag.manifest_errors = manifestResult.errors;
            out_diag.dependency_error = depError;
            return false;
        }

        for (const auto &warning : warnings) {
            out_diag.dependency_warnings.push_back(warning);
        }

        RecordLoadOrder(loadOrder, out_diag);
        discovered_order_ = loadOrder;

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
            return false;
        }

        for (auto &module : loadedModules) {
            ctx.AddLoadedModule(std::move(module));
        }

        BroadcastLifecycleEvent("BML/System/ModReload", ctx.GetLoadedModules());
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
            ImcBus::Instance().Publish(topic_id, payload.data(), payload.size(), nullptr);
        }
    }

    void ModuleRuntime::UpdateHotReloadWatchList() {
        if (!hot_reload_enabled_ || !hot_reload_monitor_)
            return;
        hot_reload_monitor_->UpdateWatchList(BuildWatchPaths());
    }

    void ModuleRuntime::EnsureHotReloadMonitor() {
        if (!hot_reload_enabled_)
            return;
        if (!hot_reload_monitor_) {
            hot_reload_monitor_ = std::make_unique<HotReloadMonitor>();
            hot_reload_monitor_->SetCallback([this] { HandleHotReloadTrigger(); });
            hot_reload_monitor_->Start();
        }
    }

    void ModuleRuntime::StopHotReloadMonitor() {
        if (hot_reload_monitor_) {
            hot_reload_monitor_->Stop();
            hot_reload_monitor_.reset();
        }
    }

    void ModuleRuntime::HandleHotReloadTrigger() {
        ModuleBootstrapDiagnostics diag;
        if (ReloadModules(diag)) {
            OutputDebugStringA("[BML hot reload] Modules reloaded\n");
        } else {
            OutputDebugStringA("[BML hot reload] Reload failed\n");
        }
    }

    bool ModuleRuntime::ShouldEnableHotReload() const {
        return IsHotReloadEnvEnabled();
    }

    std::vector<std::wstring> ModuleRuntime::BuildWatchPaths() const {
        std::vector<std::wstring> paths;
        if (!discovered_mods_dir_.empty()) {
            std::filesystem::path mods(discovered_mods_dir_);
            paths.emplace_back(mods.lexically_normal().wstring());
        }

        const auto &manifests = Context::Instance().GetManifests();
        for (const auto &manifest : manifests) {
            if (!manifest)
                continue;
            if (!manifest->directory.empty()) {
                std::filesystem::path dir(manifest->directory);
                if (!IsCacheSubPath(dir)) {
                    paths.emplace_back(dir.lexically_normal().wstring());
                }
            }
            if (!manifest->source_archive.empty()) {
                std::filesystem::path archive(manifest->source_archive);
                paths.emplace_back(archive.lexically_normal().wstring());
            }
        }
        return paths;
    }

    void ModuleRuntime::ApplyDiagnostics(const ModuleBootstrapDiagnostics &diag) const {
        if (diag_callback_)
            diag_callback_(diag);
    }
} // namespace BML::Core
