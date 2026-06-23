#include "Logger.h"

#include <cstdio>
#include <string>
#include <vector>

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

    auto *ctx = BML_GetModContext();
    FILE *logFile = ctx ? ctx->GetLogFile() : nullptr;

    std::string message;
    if (fmt) {
        va_list sizeArgs;
        va_copy(sizeArgs, args);
        const int length = _vscprintf(fmt, sizeArgs);
        va_end(sizeArgs);
        if (length >= 0) {
            std::vector<char> buffer(static_cast<size_t>(length) + 1);
            va_list formatArgs;
            va_copy(formatArgs, args);
            vsnprintf_s(buffer.data(), buffer.size(), _TRUNCATE, fmt, formatArgs);
            va_end(formatArgs);
            message.assign(buffer.data());
        }
    }

    FILE *outFiles[] = {
#ifdef _DEBUG
        stdout,
#endif
        logFile
    };

    for (FILE *file: outFiles) {
        if (!file) continue;

        fprintf(file, "[%02d/%02d/%d %02d:%02d:%02d.%03d] ", sys.wMonth, sys.wDay,
                sys.wYear, sys.wHour, sys.wMinute, sys.wSecond, sys.wMilliseconds);
        fprintf(file, "[%s/%s]: ", m_ModName, level);
        fputs(message.c_str(), file);
        fputc('\n', file);
        fflush(file);
    }

#if BML_ENABLE_ANGELSCRIPT
    if (ctx)
        ctx->PublishScriptDevLogEvent(level, m_ModName, message);
#endif
}
