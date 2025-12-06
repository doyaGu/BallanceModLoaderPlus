#include "Logger.h"

#include <cstdio>
#include <mutex>
#include <shared_mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#if defined(_MSC_VER)
#define BML_SAFE_FPRINTF(stream, ...) ::fprintf_s(stream, __VA_ARGS__)
#define BML_SAFE_VFPRINTF(stream, format, args) ::vfprintf_s(stream, format, args)
#else
#define BML_SAFE_FPRINTF(stream, ...) std::fprintf(stream, __VA_ARGS__)
#define BML_SAFE_VFPRINTF(stream, format, args) std::vfprintf(stream, format, args)
#endif

#include "ModContext.h"

static std::shared_mutex g_DefaultLoggerMutex;
static std::mutex g_LogWriteMutex;

Logger *Logger::m_DefaultLogger = nullptr;

Logger *Logger::GetDefault() {
    std::shared_lock lock(g_DefaultLoggerMutex);
    return m_DefaultLogger;
}

void Logger::SetDefault(Logger *logger) {
    std::unique_lock lock(g_DefaultLoggerMutex);
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
    if (!level || !fmt)
        return;

    SYSTEMTIME sys;
    GetLocalTime(&sys);

    FILE *outFiles[] = {
#ifdef _DEBUG
        stdout,
#endif
        BML_GetModContext()->GetLogFile()
    };

    std::lock_guard lock(g_LogWriteMutex);
    for (FILE *file : outFiles) {
        if (!file)
            continue;

        BML_SAFE_FPRINTF(file,
                 "[%02d/%02d/%d %02d:%02d:%02d.%03d] ",
                 sys.wMonth,
                 sys.wDay,
                 sys.wYear,
                 sys.wHour,
                 sys.wMinute,
                 sys.wSecond,
                 sys.wMilliseconds);
        BML_SAFE_FPRINTF(file, "[%s/%s]: ", m_ModName, level);

        va_list argsCopy;
        va_copy(argsCopy, args);
        BML_SAFE_VFPRINTF(file, fmt, argsCopy);
        va_end(argsCopy);

        fputc('\n', file);
        fflush(file);
    }
}

#undef BML_SAFE_FPRINTF
#undef BML_SAFE_VFPRINTF