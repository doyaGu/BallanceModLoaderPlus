#include "Logging/Logger.h"

#include <cstdio>
#include <string>
#include <vector>

#if BML_ENABLE_ANGELSCRIPT
#include "AngelScript/ScriptDevToolsService.h"
#endif

#include <SDL3/SDL_time.h>

#include "Loader/ModContext.h"

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
    auto *ctx = BML_GetModContext();
    SDL_DateTime sys{};
    SDL_Time now{};
    SDL_GetCurrentTime(&now);
    SDL_TimeToDateTime(now, &sys, true);

    std::string message;
    if (fmt) {
        va_list sizeArgs;
        va_copy(sizeArgs, args);
        const int length = std::vsnprintf(nullptr, 0, fmt, sizeArgs);
        va_end(sizeArgs);
        if (length >= 0) {
            std::vector<char> buffer(static_cast<size_t>(length) + 1);
            va_list formatArgs;
            va_copy(formatArgs, args);
            std::vsnprintf(buffer.data(), buffer.size(), fmt, formatArgs);
            va_end(formatArgs);
            message.assign(buffer.data());
        }
    }

    char timestamp[64];
    std::snprintf(timestamp, sizeof(timestamp), "[%02d/%02d/%d %02d:%02d:%02d.%03d] ",
                  sys.month, sys.day, sys.year, sys.hour, sys.minute, sys.second, sys.nanosecond / 1000000);
    if (ctx) ctx->WriteLogLine(std::string(timestamp) + "[" + m_ModName + "/" + level + "]: " + message);

#if BML_ENABLE_ANGELSCRIPT
    if (ctx) {
        if (auto *devTools = ctx->GetScriptDevTools())
            devTools->PublishLogLine(level, m_ModName, message);
    }
#endif
}
