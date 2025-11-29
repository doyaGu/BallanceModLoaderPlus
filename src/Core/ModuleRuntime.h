#ifndef BML_CORE_MODULE_RUNTIME_H
#define BML_CORE_MODULE_RUNTIME_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "HotReloadCoordinator.h"
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
        ~ModuleRuntime(); // Defined in .cpp where HotReloadCoordinator is complete
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
        void UpdateHotReloadRegistration();
        void EnsureHotReloadCoordinator();
        void StopHotReloadCoordinator();
        void HandleHotReloadNotify(const std::string &mod_id, ReloadResult result,
                                   unsigned int version, ReloadFailure failure);
        bool ShouldEnableHotReload() const;
        std::wstring GetHotReloadTempDirectory() const;
        void ApplyDiagnostics(const ModuleBootstrapDiagnostics &diag) const;

        // State for two-phase loading
        std::wstring m_DiscoveredModsDir;
        std::vector<ResolvedNode> m_DiscoveredOrder;

        bool m_HotReloadEnabled{false};
        std::unique_ptr<HotReloadCoordinator> m_HotReloadCoordinator;
        std::function<void(const ModuleBootstrapDiagnostics &)> m_DiagCallback;
        mutable std::mutex m_ReloadMutex;
        bool m_ReloadInProgress{false};
    };
} // namespace BML::Core

#endif // BML_CORE_MODULE_RUNTIME_H
