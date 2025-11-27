#ifndef BML_CORE_MICROKERNEL_H
#define BML_CORE_MICROKERNEL_H

#include "bml_errors.h"

#include "ModuleRuntime.h"

namespace BML::Core {
    // Phase 0: Initialize BML Core (context, API registry) - safe in DllMain
    bool InitializeCore();

    // Phase 1: Discover and validate modules (safe to call in DllMain)
    bool DiscoverModules();

    // Phase 2: Load discovered modules (call when CKContext is available)
    bool LoadDiscoveredModules();

    void ShutdownMicrokernel();
    const ModuleBootstrapDiagnostics &GetBootstrapDiagnostics();
    const BML_BootstrapDiagnostics &GetPublicDiagnostics();
} // namespace BML::Core

#endif // BML_CORE_MICROKERNEL_H
