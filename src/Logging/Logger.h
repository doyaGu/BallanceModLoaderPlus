#ifndef BML_LOGGER_H
#define BML_LOGGER_H

#include <cstdarg>
#include <string>

class Logger {
public:
	static Logger *GetDefault();
	static void SetDefault(Logger *logger);

	explicit Logger(const char *modName);

        void Info(const char *fmt, ...);
        void Warn(const char *fmt, ...);
        void Error(const char *fmt, ...);

    private:
        void Log(const char *level, const char *fmt, va_list args);

        std::string m_ModName;

        static Logger *m_DefaultLogger;
};

#endif // BML_LOGGER_H
