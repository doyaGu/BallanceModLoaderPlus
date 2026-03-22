// Stub implementations for BML export functions used by tests
// This file should be included INSTEAD of Export.cpp in tests

#include "Core/ApiRegistry.h"
#include "Core/KernelServices.h"
#include "bml_export.h"

// Provide implementations without using BML_API to avoid dllimport issues
extern "C" {

BML_API void *bmlGetProcAddress(const char *proc_name) {
    if (!proc_name)
        return nullptr;
    auto &kernel = BML::Core::Kernel();
    if (!kernel.api_registry)
        return nullptr;
    return kernel.api_registry->Get(proc_name);
}

} // extern "C"
