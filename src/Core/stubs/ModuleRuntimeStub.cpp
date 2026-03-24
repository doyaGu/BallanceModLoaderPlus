#include "ModuleRuntime.h"

#include <utility>

namespace BML::Core {
    ModuleRuntime::~ModuleRuntime() = default;

    bool ModuleRuntime::DiscoverAndValidate(const std::wstring &mods_dir, ModuleBootstrapDiagnostics &out_diag) {
        out_diag = {};
        m_DiscoveredModsDir = mods_dir;
        m_DiscoveredOrder.clear();
        return true;
    }

    bool ModuleRuntime::LoadDiscovered(ModuleBootstrapDiagnostics &out_diag,
                                       const BML_Services * /*services*/) {
        out_diag = {};
        return true;
    }

    bool ModuleRuntime::ReloadModules(ModuleBootstrapDiagnostics &out_diag) {
        out_diag = {};
        out_diag.load_error.message = "Hot reload is not supported on this platform";
        return false;
    }

    void ModuleRuntime::Update() {
    }

    void ModuleRuntime::Shutdown() {
    }

    void ModuleRuntime::SetDiagnosticsCallback(std::function<void(const ModuleBootstrapDiagnostics &)> callback) {
        m_DiagCallback = std::move(callback);
    }
} // namespace BML::Core
