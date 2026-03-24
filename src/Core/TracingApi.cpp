#include "KernelServices.h"
#include "ApiRegistration.h"

namespace BML::Core {
    void ResetTracingStateForKernel(KernelServices *kernel) {
        (void) kernel;
    }

    void RegisterTracingApis(ApiRegistry &) {
        // Zero-handle tracing globals are available only through runtime-owned services.
    }
} // namespace BML::Core
