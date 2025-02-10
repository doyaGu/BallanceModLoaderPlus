#ifndef BML_LOGGER_H
#define BML_LOGGER_H

#include <cstdarg>

#include "BML/ILogger.h"

class BML_EXPORT Logger : public ILogger
{
public:
	static Logger *GetDefault();
	static void SetDefault(Logger *logger);

	explicit Logger(const char *modName);

	void Info(const char *fmt, ...) override;
	void Warn(const char *fmt, ...) override;
	void Error(const char *fmt, ...) override;

private:
	void Log(const char *level, const char *fmt, va_list args);

    const char *m_ModName;

	static Logger *m_DefaultLogger;
};

#endif // BML_LOGGER_H