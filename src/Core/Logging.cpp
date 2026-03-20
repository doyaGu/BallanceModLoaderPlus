#include "Logging.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "ModHandle.h"

#include "bml_logging.h"

#if defined(_MSC_VER)
#define BML_SAFE_FPRINTF(stream, ...) ::fprintf_s(stream, __VA_ARGS__)
#else
#define BML_SAFE_FPRINTF(stream, ...) std::fprintf(stream, __VA_ARGS__)
#endif

namespace BML::Core {
    namespace {
        std::mutex g_LogMutex;
        std::atomic<int> g_GlobalMinimumSeverity{BML_LOG_INFO};
        constexpr uint32_t kAllSeverityMask =
            BML_LOG_SEVERITY_MASK(BML_LOG_TRACE) |
            BML_LOG_SEVERITY_MASK(BML_LOG_DEBUG) |
            BML_LOG_SEVERITY_MASK(BML_LOG_INFO) |
            BML_LOG_SEVERITY_MASK(BML_LOG_WARN) |
            BML_LOG_SEVERITY_MASK(BML_LOG_ERROR) |
            BML_LOG_SEVERITY_MASK(BML_LOG_FATAL);

        std::shared_mutex g_LogSinkMutex;
        BML_LogSinkOverrideDesc g_LogSinkOverride{};
        bool g_LogSinkOverrideActive{false};
        std::atomic<uint32_t> g_LogSinkDispatches{0};
        std::mutex g_LogSinkDrainMutex;
        std::condition_variable g_LogSinkDrainCv;

        const char *SeverityToString(BML_LogSeverity level) {
            switch (level) {
            case BML_LOG_TRACE:
                return "TRACE";
            case BML_LOG_DEBUG:
                return "DEBUG";
            case BML_LOG_INFO:
                return "INFO";
            case BML_LOG_WARN:
                return "WARN";
            case BML_LOG_ERROR:
                return "ERROR";
            case BML_LOG_FATAL:
                return "FATAL";
            default:
                return "UNK";
            }
        }

        int ClampSeverity(int value) {
            return std::clamp(value,
                              static_cast<int>(BML_LOG_TRACE),
                              static_cast<int>(BML_LOG_FATAL));
        }

        std::string FormatBody(const char *fmt, va_list args) {
            if (!fmt)
                return {};

            va_list argsCopy;
            va_copy(argsCopy, args);
#if defined(_MSC_VER)
            int required = _vscprintf(fmt, argsCopy);
#else
            int required = vsnprintf(nullptr, 0, fmt, argsCopy);
#endif
            va_end(argsCopy);
            if (required <= 0)
                return {};

            std::vector<char> buffer(static_cast<size_t>(required) + 1);
            va_copy(argsCopy, args);
            vsnprintf(buffer.data(), buffer.size(), fmt, argsCopy);
            va_end(argsCopy);
            return std::string(buffer.data(), static_cast<size_t>(required));
        }

        std::string BuildLine(BML_Mod_T *mod,
                              BML_LogSeverity level,
                              const char *tag,
                              const std::string &message) {
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % std::chrono::milliseconds(1000);

            std::tm local_tm{};
#if defined(_WIN32)
            localtime_s(&local_tm, &now_time_t);
#else
            localtime_r(&now_time_t, &local_tm);
#endif
            char timestamp[64];
            int written = snprintf(timestamp,
                                   sizeof(timestamp),
                                   "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                                   static_cast<unsigned>(local_tm.tm_year + 1900),
                                   static_cast<unsigned>(local_tm.tm_mon + 1),
                                   static_cast<unsigned>(local_tm.tm_mday),
                                   static_cast<unsigned>(local_tm.tm_hour),
                                   static_cast<unsigned>(local_tm.tm_min),
                                   static_cast<unsigned>(local_tm.tm_sec),
                                   static_cast<unsigned>(millis.count()));
            if (written < 0) {
                timestamp[0] = '\0';
            }

            std::ostringstream oss;
            oss << "[" << timestamp << "]";
            oss << "[" << (mod ? mod->id.c_str() : "bml.core") << "]";
            oss << "[" << SeverityToString(level) << "]";
            if (tag && *tag) {
                oss << "[" << tag << "]";
            }
            if (!message.empty()) {
                oss << ' ' << message;
            }
            return oss.str();
        }

        void WriteLineLocked(BML_Mod_T *mod, const std::string &line) {
#ifndef NDEBUG
            std::string debugLine = line;
            debugLine.push_back('\n');
#endif
            std::scoped_lock lock(g_LogMutex);
            FILE *target = nullptr;
            if (mod && mod->log_file) {
                target = mod->log_file.get();
            }
            if (!target) {
                target = stdout;
            }
            if (target) {
                BML_SAFE_FPRINTF(target, "%s\n", line.c_str());
                fflush(target);
            }
#ifndef NDEBUG
            OutputDebugStringA(debugLine.c_str());
#endif
        }

        bool TryDispatchOverride(BML_Mod_T *mod,
                                 BML_LogSeverity level,
                                 const char *tag,
                                 const std::string &body,
                                 const std::string &formatted) {
            BML_LogSinkOverrideDesc desc{};
            bool dispatch_registered = false;
            {
                std::shared_lock lock(g_LogSinkMutex);
                if (!g_LogSinkOverrideActive)
                    return false;
                desc = g_LogSinkOverride;
                if (!desc.dispatch)
                    return false;
                g_LogSinkDispatches.fetch_add(1, std::memory_order_acq_rel);
                dispatch_registered = true;
            }

            BML_LogMessageInfo info{};
            info.struct_size = sizeof(BML_LogMessageInfo);
            info.api_version = bmlGetApiVersion();
            info.mod = mod;
            info.mod_id = mod ? mod->id.c_str() : nullptr;
            info.severity = level;
            info.tag = tag;
            info.message = body.c_str();
            info.formatted_line = formatted.c_str();

            auto finish_dispatch = [&]() {
                if (!dispatch_registered)
                    return;
                if (g_LogSinkDispatches.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    std::lock_guard<std::mutex> lock(g_LogSinkDrainMutex);
                    g_LogSinkDrainCv.notify_all();
                }
            };

            // Exception isolation: failing override should not crash the logger
            try {
                desc.dispatch(Context::Instance().GetHandle(), &info, desc.user_data);
            } catch (const std::exception &ex) {
                // Log to debug output since we can't use the logger recursively
#ifndef NDEBUG
                std::string msg = "[BML Logging] Override dispatch exception: ";
                msg += ex.what();
                msg += "\n";
                OutputDebugStringA(msg.c_str());
#else
                (void)ex;
#endif
                // Fall through to default logging
                finish_dispatch();
                return false;
            } catch (...) {
#ifndef NDEBUG
                OutputDebugStringA("[BML Logging] Override dispatch threw unknown exception\n");
#endif
                finish_dispatch();
                return false;
            }

            bool suppress_default =
                (desc.flags & BML_LOG_SINK_OVERRIDE_SUPPRESS_DEFAULT) != 0;
            finish_dispatch();
            return suppress_default;
        }

        BML_Mod_T *ResolveModFromCaller(void *caller) {
            auto &ctx = Context::Instance();
            if (auto current = Context::GetCurrentModule()) {
                if (auto *handle = ctx.ResolveModHandle(current)) {
                    return handle;
                }
            }
            if (!caller)
                return nullptr;

#if defined(_WIN32)
            HMODULE module = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   reinterpret_cast<LPCSTR>(caller),
                                   &module)) {
                if (auto handle = ctx.GetModHandleByModule(module)) {
                    return ctx.ResolveModHandle(handle);
                }
            }
#else
            (void) caller;
#endif
            return nullptr;
        }

        bool ShouldLog(BML_Mod_T *mod, BML_LogSeverity level) {
            int threshold = mod
                                ? mod->minimum_severity.load(std::memory_order_relaxed)
                                : g_GlobalMinimumSeverity.load(std::memory_order_relaxed);
            return static_cast<int>(level) >= ClampSeverity(threshold);
        }

        void SetMinimumSeverity(void *caller, BML_LogSeverity level) {
            int clamped = ClampSeverity(static_cast<int>(level));
            BML_Mod_T *mod = ResolveModFromCaller(caller);
            if (mod) {
                mod->minimum_severity.store(clamped, std::memory_order_relaxed);
            } else {
                g_GlobalMinimumSeverity.store(clamped, std::memory_order_relaxed);
            }
        }

        void LogMessageInternal(void *caller,
                                BML_Context ctx,
                                BML_LogSeverity level,
                                const char *tag,
                                const char *fmt,
                                va_list args) {
            (void) ctx;
            BML_Mod_T *mod = ResolveModFromCaller(caller);
            if (!ShouldLog(mod, level))
                return;

            std::string body = FormatBody(fmt, args);
            std::string line = BuildLine(mod, level, tag, body);
            if (TryDispatchOverride(mod, level, tag, body, line))
                return;
            WriteLineLocked(mod, line);
        }
    } // namespace

    void CoreLog(BML_LogSeverity level, const char *tag, const char *fmt, ...) {
        if (!fmt)
            return;
        va_list args;
        va_start(args, fmt);
        CoreLogVa(level, tag, fmt, args);
        va_end(args);
    }

    void CoreLogVa(BML_LogSeverity level, const char *tag, const char *fmt, va_list args) {
        if (!fmt)
            return;
        va_list argsCopy;
        va_copy(argsCopy, args);
        LogMessageInternal(nullptr, nullptr, level, tag, fmt, argsCopy);
        va_end(argsCopy);
    }

#if defined(_MSC_VER)
#define BML_CALLER_ADDRESS() _ReturnAddress()
#else
#define BML_CALLER_ADDRESS() __builtin_return_address(0)
#endif

    void LogMessage(BML_Context ctx, BML_LogSeverity level, const char *tag, const char *fmt, ...) {
        if (!fmt)
            return;
        va_list args;
        va_start(args, fmt);
        LogMessageInternal(BML_CALLER_ADDRESS(), ctx, level, tag, fmt, args);
        va_end(args);
    }

    void LogMessageVa(BML_Context ctx, BML_LogSeverity level, const char *tag, const char *fmt, va_list args) {
        if (!fmt)
            return;
        va_list argsCopy;
        va_copy(argsCopy, args);
        LogMessageInternal(BML_CALLER_ADDRESS(), ctx, level, tag, fmt, argsCopy);
        va_end(argsCopy);
    }

    void SetLogFilter(BML_LogSeverity minimum_level) {
        SetMinimumSeverity(BML_CALLER_ADDRESS(), minimum_level);
    }

#undef BML_CALLER_ADDRESS


    BML_Result RegisterLogSinkOverride(const BML_LogSinkOverrideDesc *desc) {
        if (!desc || desc->struct_size < sizeof(BML_LogSinkOverrideDesc) || !desc->dispatch) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        BML_LogSinkOverrideDesc copy = *desc;
        copy.struct_size = sizeof(BML_LogSinkOverrideDesc);

        std::unique_lock lock(g_LogSinkMutex);
        if (g_LogSinkOverrideActive) {
            return BML_RESULT_ALREADY_EXISTS;
        }
        g_LogSinkOverride = copy;
        g_LogSinkOverrideActive = true;
        return BML_RESULT_OK;
    }

    BML_Result ClearLogSinkOverride() {
        std::unique_lock lock(g_LogSinkMutex);
        if (!g_LogSinkOverrideActive) {
            return BML_RESULT_NOT_FOUND;
        }
        auto shutdown = g_LogSinkOverride.on_shutdown;
        void *user_data = g_LogSinkOverride.user_data;
        g_LogSinkOverride = {}; // Clear the descriptor
        g_LogSinkOverrideActive = false;
        lock.unlock();

        {
            std::unique_lock<std::mutex> drain_lock(g_LogSinkDrainMutex);
            g_LogSinkDrainCv.wait(drain_lock, [] {
                return g_LogSinkDispatches.load(std::memory_order_acquire) == 0;
            });
        }

        // Call shutdown callback outside the lock with exception safety
        if (shutdown) {
            try {
                shutdown(user_data);
            } catch (const std::exception &ex) {
#ifndef NDEBUG
                std::string msg = "[BML Logging] Shutdown callback exception: ";
                msg += ex.what();
                msg += "\n";
                OutputDebugStringA(msg.c_str());
#endif
                (void)ex;
            } catch (...) {
#ifndef NDEBUG
                OutputDebugStringA("[BML Logging] Shutdown callback threw unknown exception\n");
#endif
            }
        }
        return BML_RESULT_OK;
    }
} // namespace BML::Core

#undef BML_SAFE_FPRINTF
