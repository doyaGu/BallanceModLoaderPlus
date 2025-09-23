#include "Logger.h"

#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "ModContext.h"

Logger *Logger::m_DefaultLogger = nullptr;

Logger *Logger::GetDefault() {
    return m_DefaultLogger;
}

void Logger::SetDefault(Logger *logger) {
    m_DefaultLogger = logger;
}

Logger::Logger(const char *modName) : m_ModName(modName) {}

void Logger::Info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Log("INFO", fmt, args);
    va_end(args);
}

void Logger::Warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Log("WARN", fmt, args);
    va_end(args);
}

void Logger::Error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    Log("ERROR", fmt, args);
    va_end(args);
}

void Logger::Log(const char *level, const char *fmt, va_list args) {
    SYSTEMTIME sys;
    GetLocalTime(&sys);

    FILE *outFiles[] = {
#ifdef _DEBUG
        stdout,
#endif
        BML_GetModContext()->GetLogFile()
    };

    for (FILE *file: outFiles) {
        fprintf(file, "[%02d/%02d/%d %02d:%02d:%02d.%03d] ", sys.wMonth, sys.wDay,
                sys.wYear, sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds);
        fprintf(file, "[%s/%s]: ", m_ModName, level);
        vfprintf(file, fmt, args);
        fputc('\n', file);
        fflush(file);
    }
}