#include "Core/Logging.h"

#include <cstdarg>

namespace BML::Core {

void RegisterLoggingApis() {}

void CoreLog(BML_LogSeverity, const char *, const char *, ...) {}

void CoreLogVa(BML_LogSeverity, const char *, const char *, va_list) {}

BML_Result RegisterLogSinkOverride(const BML_LogSinkOverrideDesc *) {
    return BML_RESULT_OK;
}

BML_Result ClearLogSinkOverride() {
    return BML_RESULT_OK;
}

} // namespace BML::Core
