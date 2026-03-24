#include "Logging.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <vector>

#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "Context.h"

namespace BML::Core {
    BML_Result BML_API_RegisterLogSinkOverride(BML_Mod /*owner*/,
                                               const BML_LogSinkOverrideDesc *desc) {
        return RegisterLogSinkOverride(desc);
    }

    BML_Result BML_API_ClearLogSinkOverride(BML_Mod /*owner*/) {
        return ClearLogSinkOverride();
    }

    void RegisterLoggingApis(ApiRegistry &apiRegistry) {
        BML_BEGIN_API_REGISTRATION(apiRegistry);

        // Core logging APIs - variadic functions need manual registration
        detail::RegisterApi(registry, "bmlLog", reinterpret_cast<void *>(LogMessage));
        detail::RegisterApi(registry, "bmlLogVa", reinterpret_cast<void *>(LogMessageVa));
        detail::RegisterApi(registry, "bmlSetLogFilter", reinterpret_cast<void *>(SetLogFilter));

        // Guarded APIs
        BML_REGISTER_API_GUARDED(bmlRegisterLogSinkOverride, "logging", BML_API_RegisterLogSinkOverride);
        BML_REGISTER_API_GUARDED(bmlClearLogSinkOverride, "logging", BML_API_ClearLogSinkOverride);
    }
} // namespace BML::Core
