#include "bml_export.h"

#include "ApiRegistry.h"
#include "Microkernel.h"

using BML::Core::ApiRegistry;

extern "C" BML_API BML_Result bmlAttach(void) {
    // Phase 0: Initialize core only (safe in DllMain)
    // Creates context, registers core APIs
    if (!BML::Core::InitializeCore())
        return BML_RESULT_FAIL;
    
    return BML_RESULT_OK;
}

extern "C" BML_API BML_Result bmlDiscoverModules(void) {
    // Phase 1: Discover modules (call after bmlAttach)
    // Scans for mods, validates manifests, resolves dependencies
    return BML::Core::DiscoverModules() ? BML_RESULT_OK : BML_RESULT_FAIL;
}

extern "C" BML_API BML_Result bmlLoadModules(void) {
    // Phase 2: Load discovered modules (call when CKContext is available)
    return BML::Core::LoadDiscoveredModules() ? BML_RESULT_OK : BML_RESULT_FAIL;
}

extern "C" BML_API void bmlDetach(void) {
    BML::Core::ShutdownMicrokernel();
}

extern "C" BML_API void *bmlGetProcAddress(const char *proc_name) {
    if (!proc_name)
        return nullptr;
    return ApiRegistry::Instance().Get(proc_name);
}

// ========================================================================
// ID-Based Fast Path (v0.4.1+)
// ========================================================================

extern "C" BML_API void *bmlGetProcAddressById(BML_ApiId api_id) {
    return ApiRegistry::Instance().GetById(api_id);
}

extern "C" BML_API int bmlGetApiId(const char *proc_name, BML_ApiId *out_id) {
    if (!proc_name || !out_id)
        return 0;
    return ApiRegistry::Instance().GetApiId(proc_name, out_id) ? 1 : 0;
}

extern "C" BML_API const BML_BootstrapDiagnostics *bmlGetBootstrapDiagnostics(void) {
    return &BML::Core::GetPublicDiagnostics();
}
