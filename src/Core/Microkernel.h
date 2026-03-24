#ifndef BML_CORE_MICROKERNEL_H
#define BML_CORE_MICROKERNEL_H

#include <string>

#include "bml_bootstrap.h"
#include "bml_errors.h"
#include "bml_export.h"

#include "ModuleRuntime.h"

namespace BML::Core {
    struct RuntimeState;
    struct KernelServices;

    RuntimeState *CreateRuntimeState();
    void DestroyRuntimeState(RuntimeState *state);
    KernelServices *GetRuntimeKernel(RuntimeState &state) noexcept;

    // Phase 0: Initialize BML Core (context, API registry) - safe in DllMain
    bool InitializeCore(RuntimeState &state);

    // Phase 1: Discover and validate modules (safe to call in DllMain)
    bool DiscoverModules(RuntimeState &state);
    bool DiscoverModulesInDirectory(RuntimeState &state, const std::wstring &mods_dir);

    // Phase 2: Load discovered modules (call when CKContext is available)
    bool LoadDiscoveredModules(RuntimeState &state,
                               const BML_Services *services);

    // Main-loop update hook for deferred runtime tasks (e.g. hot reload processing)
    void UpdateMicrokernel(RuntimeState &state);

    void ShutdownMicrokernel(RuntimeState &state);
    const ModuleBootstrapDiagnostics &GetBootstrapDiagnostics(RuntimeState &state);
    const BML_BootstrapDiagnostics &GetPublicDiagnostics(RuntimeState &state);
} // namespace BML::Core

#endif // BML_CORE_MICROKERNEL_H
