#include "Logging/Logger.h"

Logger *Logger::m_DefaultLogger = nullptr;
Logger *Logger::GetDefault() { static Logger logger("Test"); return &logger; }
void Logger::SetDefault(Logger *logger) { m_DefaultLogger = logger; }
Logger::Logger(const char *name) : m_ModName(name) {}
void Logger::Info(const char *, ...) {}
void Logger::Warn(const char *, ...) {}
void Logger::Error(const char *, ...) {}
