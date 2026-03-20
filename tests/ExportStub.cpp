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

} // extern "C"
