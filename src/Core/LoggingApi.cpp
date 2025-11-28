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
#include "bml_api_ids.h"
#include "bml_capabilities.h"

namespace BML::Core {
    BML_Result BML_API_LoggingGetCaps(BML_LogCaps *out_caps) {
        return GetLoggingCaps(out_caps);
    }

    BML_Result BML_API_RegisterLogSinkOverride(const BML_LogSinkOverrideDesc *desc) {
        return RegisterLogSinkOverride(desc);
    }

    BML_Result BML_API_ClearLogSinkOverride() {
        return ClearLogSinkOverride();
    }

    void RegisterLoggingApis() {
        BML_BEGIN_API_REGISTRATION();

        // Core logging APIs - variadic functions need manual registration with full metadata
        // Note: These are thread-safe (FREE threading model)
        detail::RegisterApiWithMetadata(registry, "bmlLog", BML_API_ID_bmlLog,
                                        reinterpret_cast<void *>(LogMessage), BML_CAP_LOGGING);
        detail::RegisterApiWithMetadata(registry, "bmlLogVa", BML_API_ID_bmlLogVa,
                                        reinterpret_cast<void *>(LogMessageVa), BML_CAP_LOGGING);
        detail::RegisterApiWithMetadata(registry, "bmlSetLogFilter", BML_API_ID_bmlSetLogFilter,
                                        reinterpret_cast<void *>(SetLogFilter), BML_CAP_LOGGING);

        // Guarded APIs with capabilities
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlLoggingGetCaps, "logging", BML_API_LoggingGetCaps, BML_CAP_LOGGING);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlRegisterLogSinkOverride, "logging", BML_API_RegisterLogSinkOverride, BML_CAP_LOGGING);
        BML_REGISTER_API_GUARDED_WITH_CAPS(bmlClearLogSinkOverride, "logging", BML_API_ClearLogSinkOverride, BML_CAP_LOGGING);
    }
} // namespace BML::Core
