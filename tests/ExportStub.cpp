// Stub implementations for BML export functions used by tests
// This file should be included INSTEAD of Export.cpp in tests

#include "Core/ApiRegistry.h"
#include "Core/Context.h"
#include "Core/KernelServices.h"
#include "TestKernel.h"
#include "bml_export.h"

#include <new>

namespace BML::Core::Testing {
KernelServices *GetInstalledTestBootstrapKernel() noexcept;
}

struct BML_Runtime_T {
    BML::Core::KernelServices *kernel{nullptr};
    BML_BootstrapDiagnostics diagnostics{};
    const BML_Services *services{nullptr};
};

// Provide implementations without using BML_API to avoid dllimport issues
extern "C" {

BML_API void *bmlGetProcAddress(const char *proc_name) {
    if (!proc_name)
        return nullptr;
    auto *kernel = BML::Core::Testing::GetInstalledTestBootstrapKernel();
    if (!kernel)
        return nullptr;
    if (!kernel->api_registry)
        return nullptr;
    return kernel->api_registry->Get(proc_name);
}

BML_API BML_Result bmlRuntimeCreate(const BML_RuntimeConfig *config, BML_Runtime *out_runtime) {
    (void) config;
    if (!out_runtime) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    auto *runtime = new (std::nothrow) BML_Runtime_T();
    if (!runtime) {
        return BML_RESULT_OUT_OF_MEMORY;
    }

    runtime->kernel = BML::Core::Testing::GetInstalledTestBootstrapKernel();
    if (!runtime->kernel) {
        delete runtime;
        *out_runtime = nullptr;
        return BML_RESULT_INVALID_CONTEXT;
    }
    runtime->services = runtime->kernel->context
        ? &runtime->kernel->context->GetServiceHub()->Interfaces()
        : nullptr;

    *out_runtime = runtime;
    return BML_RESULT_OK;
}

BML_API BML_Result bmlRuntimeDiscoverModules(BML_Runtime runtime) {
    return runtime ? BML_RESULT_OK : BML_RESULT_INVALID_HANDLE;
}

BML_API BML_Result bmlRuntimeLoadModules(BML_Runtime runtime) {
    return runtime ? BML_RESULT_OK : BML_RESULT_INVALID_HANDLE;
}

BML_API void bmlRuntimeUpdate(BML_Runtime runtime) {
    (void) runtime;
}

BML_API const BML_Services *bmlRuntimeGetServices(BML_Runtime runtime) {
    return runtime ? runtime->services : nullptr;
}

BML_API const BML_BootstrapDiagnostics *bmlRuntimeGetBootstrapDiagnostics(BML_Runtime runtime) {
    return runtime ? &runtime->diagnostics : nullptr;
}

BML_API void bmlRuntimeDestroy(BML_Runtime runtime) {
    delete runtime;
}

} // extern "C"
