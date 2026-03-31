#ifndef BML_CORE_LOGGING_H
#define BML_CORE_LOGGING_H

#include "bml_logging.h"
namespace BML::Core {
    class ApiRegistry;

    void RegisterLoggingApis(ApiRegistry &registry);

    void CoreLog(BML_LogSeverity level, const char *tag, const char *fmt, ...);
    void CoreLogVa(BML_LogSeverity level, const char *tag, const char *fmt, va_list args);

    void LogMessage(BML_Mod owner,
                    BML_LogSeverity level,
                    const char *tag,
                    const char *fmt,
                    ...);
    void LogMessageVa(BML_Mod owner,
                      BML_LogSeverity level,
                      const char *tag,
                      const char *fmt,
                      va_list args);
    void SetLogFilter(BML_Mod owner, BML_LogSeverity minimum_level);

    BML_Result RegisterLogSinkOverride(const BML_LogSinkOverrideDesc *desc);
    BML_Result ClearLogSinkOverride();

    BML_Result AddLogListener(BML_Mod owner, BML_LogListenerFn listener, void *user_data);
    BML_Result RemoveLogListener(BML_Mod owner, BML_LogListenerFn listener);
    void ClearAllLogListeners();
    void RemoveLogListenersForModule(BML_Mod owner);
} // namespace BML::Core

#endif // BML_CORE_LOGGING_H
