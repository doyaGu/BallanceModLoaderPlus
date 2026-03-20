#include "bml_export.h"

#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "Microkernel.h"

BML_BEGIN_CDECLS

BML_API BML_Result bmlAttach(void) {
    // Phase 0: Initialize core only (safe in DllMain)
    // Creates context, registers core APIs
    if (!BML::Core::InitializeCore())
        return BML_RESULT_FAIL;
    
    return BML_RESULT_OK;
}

BML_API BML_Result bmlDiscoverModules(void) {
    // Phase 1: Discover modules (call after bmlAttach)
    // Scans for mods, validates manifests, resolves dependencies
    return BML::Core::DiscoverModules() ? BML_RESULT_OK : BML_RESULT_FAIL;
}

BML_API BML_Result bmlLoadModules(void) {
    // Phase 2: Load discovered modules (call when CKContext is available)
    return BML::Core::LoadDiscoveredModules() ? BML_RESULT_OK : BML_RESULT_FAIL;
}

BML_API BML_Result bmlBootstrap(const BML_BootstrapConfig *config) {
    return BML::Core::Bootstrap(config);
}

BML_API void bmlUpdate(void) {
    BML::Core::UpdateMicrokernel();
}

BML_API void bmlDetach(void) {
    BML::Core::ShutdownMicrokernel();
}

BML_API void bmlShutdown(void) {
    BML::Core::ShutdownMicrokernel();
}

BML_API BML_BootstrapState bmlGetBootstrapState(void) {
    return BML::Core::GetBootstrapState();
}

BML_API void *bmlGetProcAddress(const char *proc_name) {
    if (!proc_name)
        return nullptr;
    return BML::Core::ApiRegistry::Instance().Get(proc_name);
}

BML_API const BML_BootstrapDiagnostics *bmlGetBootstrapDiagnostics(void) {
    return &BML::Core::GetPublicDiagnostics();
}

BML_END_CDECLS

namespace BML::Core {
    void RegisterBootstrapExports() {
        BML_BEGIN_API_REGISTRATION();

        BML_REGISTER_API(bmlGetProcAddress, ::bmlGetProcAddress);
    }
} // namespace BML::Core
