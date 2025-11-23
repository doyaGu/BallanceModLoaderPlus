#ifndef BML_ILOGGER_H
#define BML_ILOGGER_H

#include "BML/Defines.h"

class BML_EXPORT ILogger {
public:
	virtual void Info(const char* fmt, ...) = 0;
	virtual void Warn(const char* fmt, ...) = 0;
	virtual void Error(const char* fmt, ...) = 0;

    virtual ~ILogger() = default;
};

#endif // BML_ILOGGER_H