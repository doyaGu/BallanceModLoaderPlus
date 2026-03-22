/**
 * @file bml_logger.hpp
 * @brief BML C++ Logging Wrapper
 * 
 * Provides convenient logging interface with severity levels.
 */

#ifndef BML_LOGGER_HPP
#define BML_LOGGER_HPP

#include "bml_builtin_interfaces.h"
#include "bml_logging.h"
#include "bml_context.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <cstdarg>
#include <optional>

namespace bml {
    namespace detail {
        template <typename MemberT>
        constexpr bool HasLoggingMember(const BML_CoreLoggingInterface *iface, size_t offset) noexcept {
            return iface != nullptr && iface->header.struct_size >= offset + sizeof(MemberT);
        }
    } // namespace detail

#define BML_CORE_LOGGING_HAS_MEMBER(iface, member) \
    (::bml::detail::HasLoggingMember<decltype(((BML_CoreLoggingInterface *) 0)->member)>( \
        (iface), offsetof(BML_CoreLoggingInterface, member)) && (iface)->member != nullptr)

    // ============================================================================
    // Log Level Enum
    // ============================================================================

    /**
     * @brief C++ enum class for log severity levels
     */
    enum class LogLevel {
        Trace = BML_LOG_TRACE,
        Debug = BML_LOG_DEBUG,
        Info  = BML_LOG_INFO,
        Warn  = BML_LOG_WARN,
        Error = BML_LOG_ERROR,
        Fatal = BML_LOG_FATAL
    };

    /**
     * @brief Set the minimum log severity filter
     * @param level Minimum severity level to log
     */
    inline void SetLogFilter(LogLevel level, const BML_CoreLoggingInterface *loggingInterface = nullptr) {
        if (loggingInterface && loggingInterface->SetLogFilter) {
            loggingInterface->SetLogFilter(static_cast<BML_LogSeverity>(level));
        }
    }

    inline void SetLogFilter(BML_Mod owner,
                             LogLevel level,
                             const BML_CoreLoggingInterface *loggingInterface = nullptr) {
        if (!loggingInterface) {
            return;
        }
        if (owner && BML_CORE_LOGGING_HAS_MEMBER(loggingInterface, SetLogFilterOwned)) {
            loggingInterface->SetLogFilterOwned(owner, static_cast<BML_LogSeverity>(level));
        } else if (loggingInterface->SetLogFilter) {
            loggingInterface->SetLogFilter(static_cast<BML_LogSeverity>(level));
        }
    }

    // ============================================================================
    // Logger Wrapper
    // ============================================================================

    /**
     * @brief Convenient logger with tag support
     *
     * Example:
     *   bml::Logger log(ctx, "MyMod");
     *   log.Info("Loaded %d items", count);
     *   log.Error("Failed to load: %s", error_msg);
     */
    class Logger {
    public:
        /**
         * @brief Construct a logger
         * @param ctx The BML context
         * @param tag Optional tag for log messages
         */
        explicit Logger(Context ctx, std::string_view tag = "")
            : m_ctx(ctx.Handle()), m_tag(tag) {}

        Logger(Context ctx,
               std::string_view tag,
               const BML_CoreLoggingInterface *loggingInterface)
            : m_ctx(ctx.Handle()), m_tag(tag), m_LoggingInterface(loggingInterface) {}
        Logger(Context ctx,
               std::string_view tag,
               const BML_CoreLoggingInterface *loggingInterface,
               BML_Mod owner)
            : m_ctx(ctx.Handle()),
              m_tag(tag),
              m_LoggingInterface(loggingInterface),
              m_Owner(owner) {}

        /**
         * @brief Construct a logger with raw context handle
         * @param ctx The BML context handle
         * @param tag Optional tag for log messages
         */
        explicit Logger(BML_Context ctx, std::string_view tag = "") : m_ctx(ctx), m_tag(tag) {}

        Logger(BML_Context ctx,
               std::string_view tag,
               const BML_CoreLoggingInterface *loggingInterface)
            : m_ctx(ctx), m_tag(tag), m_LoggingInterface(loggingInterface) {}
        Logger(BML_Context ctx,
               std::string_view tag,
               const BML_CoreLoggingInterface *loggingInterface,
               BML_Mod owner)
            : m_ctx(ctx), m_tag(tag), m_LoggingInterface(loggingInterface), m_Owner(owner) {}

        // ========================================================================
        // Generic Log
        // ========================================================================

        /**
         * @brief Log a message with specified level
         * @param level Log severity level
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Log(LogLevel level, const char *fmt, ...) const {
            if (!m_LoggingInterface) return;
            va_list args;
            va_start(args, fmt);
            Dispatch(static_cast<BML_LogSeverity>(level), fmt, args);
            va_end(args);
        }

        // ========================================================================
        // Level-Specific Methods
        // ========================================================================

        /**
         * @brief Log a trace message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Trace(const char *fmt, ...) const {
            if (!m_LoggingInterface) return;
            va_list args;
            va_start(args, fmt);
            Dispatch(BML_LOG_TRACE, fmt, args);
            va_end(args);
        }

        /**
         * @brief Log a debug message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Debug(const char *fmt, ...) const {
            if (!m_LoggingInterface) return;
            va_list args;
            va_start(args, fmt);
            Dispatch(BML_LOG_DEBUG, fmt, args);
            va_end(args);
        }

        /**
         * @brief Log an info message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Info(const char *fmt, ...) const {
            if (!m_LoggingInterface) return;
            va_list args;
            va_start(args, fmt);
            Dispatch(BML_LOG_INFO, fmt, args);
            va_end(args);
        }

        /**
         * @brief Log a warning message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Warn(const char *fmt, ...) const {
            if (!m_LoggingInterface) return;
            va_list args;
            va_start(args, fmt);
            Dispatch(BML_LOG_WARN, fmt, args);
            va_end(args);
        }

        /**
         * @brief Log an error message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Error(const char *fmt, ...) const {
            if (!m_LoggingInterface) return;
            va_list args;
            va_start(args, fmt);
            Dispatch(BML_LOG_ERROR, fmt, args);
            va_end(args);
        }

        /**
         * @brief Log a fatal message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Fatal(const char *fmt, ...) const {
            if (!m_LoggingInterface) return;
            va_list args;
            va_start(args, fmt);
            Dispatch(BML_LOG_FATAL, fmt, args);
            va_end(args);
        }

        // ========================================================================
        // Accessors
        // ========================================================================

        /**
         * @brief Get the tag
         */
        const std::string &GetTag() const noexcept { return m_tag; }

        /**
         * @brief Set the tag
         */
        void SetTag(std::string_view tag) { m_tag = tag; }

    private:
        BML_Context m_ctx;
        std::string m_tag;
        const BML_CoreLoggingInterface *m_LoggingInterface = nullptr;
        BML_Mod m_Owner = nullptr;

        void Dispatch(BML_LogSeverity level, const char *fmt, va_list args) const {
            if (!m_LoggingInterface) {
                return;
            }
            if (m_Owner && BML_CORE_LOGGING_HAS_MEMBER(m_LoggingInterface, LogVaOwned)) {
                m_LoggingInterface->LogVaOwned(
                    m_Owner,
                    m_ctx,
                    level,
                    m_tag.empty() ? nullptr : m_tag.c_str(),
                    fmt,
                    args);
            } else if (m_LoggingInterface->LogVa) {
                m_LoggingInterface->LogVa(
                    m_ctx,
                    level,
                    m_tag.empty() ? nullptr : m_tag.c_str(),
                    fmt,
                    args);
            }
        }
    };

#undef BML_CORE_LOGGING_HAS_MEMBER
} // namespace bml

#endif /* BML_LOGGER_HPP */
