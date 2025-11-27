#ifndef BML_CORE_MODULE_RUNTIME_H
#define BML_CORE_MODULE_RUNTIME_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "HotReloadMonitor.h"
#include "ModuleDiscovery.h"
#include "ModuleLoader.h"

namespace BML::Core {
    struct ModuleBootstrapDiagnostics {
        std::vector<ManifestParseError> manifest_errors;
        DependencyResolutionError dependency_error;
        std::vector<DependencyWarning> dependency_warnings;
        ModuleLoadError load_error;
        std::vector<std::string> load_order;
    };

    class ModuleRuntime {
    public:
        ModuleRuntime() = default;
        ~ModuleRuntime(); // Defined in .cpp where HotReloadMonitor is complete
        ModuleRuntime(const ModuleRuntime &) = delete;
        ModuleRuntime &operator=(const ModuleRuntime &) = delete;

        bool Initialize(const std::wstring &mods_dir, ModuleBootstrapDiagnostics &out_diag);

        // Phase 1: Discover and validate modules without loading DLLs
        bool DiscoverAndValidate(const std::wstring &mods_dir, ModuleBootstrapDiagnostics &out_diag);

        // Phase 2: Load previously discovered modules
        bool LoadDiscovered(ModuleBootstrapDiagnostics &out_diag);

        bool ReloadModules(ModuleBootstrapDiagnostics &out_diag);

        void Shutdown();

        void SetDiagnosticsCallback(std::function<void(const ModuleBootstrapDiagnostics &)> callback);

    private:
        void RecordLoadOrder(const std::vector<ResolvedNode> &order, ModuleBootstrapDiagnostics &diag) const;
        bool ReloadModulesInternal(ModuleBootstrapDiagnostics &out_diag);
        void BroadcastLifecycleEvent(const char *topic, const std::vector<LoadedModule> &modules) const;
        void UpdateHotReloadWatchList();
        void EnsureHotReloadMonitor();
        void StopHotReloadMonitor();
        void HandleHotReloadTrigger();
        bool ShouldEnableHotReload() const;
        std::vector<std::wstring> BuildWatchPaths() const;
        void ApplyDiagnostics(const ModuleBootstrapDiagnostics &diag) const;

        // State for two-phase loading
        std::wstring discovered_mods_dir_;
        std::vector<ResolvedNode> discovered_order_;

        bool hot_reload_enabled_{false};
        std::unique_ptr<HotReloadMonitor> hot_reload_monitor_;
        std::function<void(const ModuleBootstrapDiagnostics &)> diag_callback_;
        mutable std::mutex reload_mutex_;
        bool reload_in_progress_{false};
    };
} // namespace BML::Core

#endif // BML_CORE_MODULE_RUNTIME_H
