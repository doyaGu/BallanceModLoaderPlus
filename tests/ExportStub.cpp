// Stub implementations for BML export functions used by tests
// This file should be included INSTEAD of Export.cpp in tests

#include "Core/ApiRegistry.h"
#include "bml_export.h"

// Provide implementations without using BML_API to avoid dllimport issues
extern "C" {

void *bmlGetProcAddress(const char *proc_name) {
    if (!proc_name)
        return nullptr;
    return BML::Core::ApiRegistry::Instance().Get(proc_name);
}

void *bmlGetProcAddressById(BML_ApiId api_id) {
    return BML::Core::ApiRegistry::Instance().GetById(api_id);
}

int bmlGetApiId(const char *proc_name, BML_ApiId *out_id) {
    if (!proc_name || !out_id)
        return 0;
    return BML::Core::ApiRegistry::Instance().GetApiId(proc_name, out_id) ? 1 : 0;
}

} // extern "C"
