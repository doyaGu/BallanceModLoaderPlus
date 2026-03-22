#ifndef BML_CORE_LOGGING_H
#define BML_CORE_LOGGING_H

#include "bml_logging.h"
namespace BML::Core {
    void RegisterLoggingApis();

    void CoreLog(BML_LogSeverity level, const char *tag, const char *fmt, ...);
    void CoreLogVa(BML_LogSeverity level, const char *tag, const char *fmt, va_list args);

    void LogMessage(BML_Mod owner,
                    BML_Context ctx,
                    BML_LogSeverity level,
                    const char *tag,
                    const char *fmt,
                    ...);
    void LogMessageVa(BML_Mod owner,
                      BML_Context ctx,
                      BML_LogSeverity level,
                      const char *tag,
                      const char *fmt,
                      va_list args);
    void SetLogFilter(BML_Mod owner, BML_LogSeverity minimum_level);

    BML_Result RegisterLogSinkOverride(const BML_LogSinkOverrideDesc *desc);
    BML_Result ClearLogSinkOverride();
} // namespace BML::Core

#endif // BML_CORE_LOGGING_H
