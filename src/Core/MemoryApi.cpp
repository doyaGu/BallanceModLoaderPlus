#include "ApiRegistration.h"

namespace BML::Core {
    void RegisterMemoryApis(ApiRegistry &) {
        // Zero-handle memory APIs are available only through the runtime memory interface.
    }
} // namespace BML::Core
