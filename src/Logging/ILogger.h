// Private printf-style logger used by loader features and script bindings.
// Native Mods log UTF-8 text through their explicit Mod Context.
#ifndef BML_ILOGGER_H
#define BML_ILOGGER_H

#include "BML/Defines.h"

class ILogger {
public:
    virtual void Info(const char *fmt, ...) = 0;
    virtual void Warn(const char *fmt, ...) = 0;
    virtual void Error(const char *fmt, ...) = 0;

    virtual ~ILogger() = default;
};

#endif // BML_ILOGGER_H