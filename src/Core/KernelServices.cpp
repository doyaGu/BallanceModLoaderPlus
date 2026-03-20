#include "KernelServices.h"

namespace BML::Core {

namespace {
    KernelServices *g_Kernel = nullptr;
}

KernelServices *GetKernelOrNull() noexcept {
    return g_Kernel;
}

void InstallKernel(KernelServices *kernel) noexcept {
    g_Kernel = kernel;
}

// NOTE: ~KernelServices() is defined in Microkernel.cpp, not here.
// This keeps KernelServices.cpp free of L0 subsystem link deps,
// which is critical for test targets that cherry-pick source files.

} // namespace BML::Core
