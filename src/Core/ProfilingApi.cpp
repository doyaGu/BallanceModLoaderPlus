#include "ApiRegistration.h"

namespace BML::Core {
    void RegisterProfilingApis(ApiRegistry &) {
        // Zero-handle profiling APIs are available only through the runtime profiling interface.
    }
} // namespace BML::Core
