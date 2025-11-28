/**
 * @file bml_logger.hpp
 * @brief BML C++ Logging Wrapper
 * 
 * Provides convenient logging interface with severity levels.
 */

#ifndef BML_LOGGER_HPP
#define BML_LOGGER_HPP

#include "bml_logging.h"
#include "bml_context.hpp"

#include <string>
#include <string_view>
#include <cstdarg>
#include <optional>

namespace bml {
    // ============================================================================
    // Logging Capabilities Query
    // ============================================================================

    /**
     * @brief Query logging subsystem capabilities
     * @return Capabilities if successful
     */
    inline std::optional<BML_LogCaps> GetLoggingCaps() {
        if (!bmlLoggingGetCaps) return std::nullopt;
        BML_LogCaps caps = BML_LOG_CAPS_INIT;
        if (bmlLoggingGetCaps(&caps) == BML_RESULT_OK) {
            return caps;
        }
        return std::nullopt;
    }

    /**
     * @brief Check if a logging capability is available
     * @param flag Capability flag to check
     * @return true if capability is available
     */
    inline bool HasLoggingCap(BML_LogCapabilityFlags flag) {
        auto caps = GetLoggingCaps();
        return caps && (caps->capability_flags & flag);
    }

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
    inline void SetLogFilter(LogLevel level) {
        if (bmlSetLogFilter) bmlSetLogFilter(static_cast<BML_LogSeverity>(level));
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
            : m_ctx(ctx.handle()), m_tag(tag) {}

        /**
         * @brief Construct a logger with raw context handle
         * @param ctx The BML context handle
         * @param tag Optional tag for log messages
         */
        explicit Logger(BML_Context ctx, std::string_view tag = "") : m_ctx(ctx), m_tag(tag) {}

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
            if (!bmlLogVa) return;
            va_list args;
            va_start(args, fmt);
            bmlLogVa(m_ctx, static_cast<BML_LogSeverity>(level), m_tag.empty() ? nullptr : m_tag.c_str(), fmt, args);
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
            if (!bmlLogVa) return;
            va_list args;
            va_start(args, fmt);
            bmlLogVa(m_ctx, BML_LOG_TRACE, m_tag.empty() ? nullptr : m_tag.c_str(), fmt, args);
            va_end(args);
        }

        /**
         * @brief Log a debug message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Debug(const char *fmt, ...) const {
            if (!bmlLogVa) return;
            va_list args;
            va_start(args, fmt);
            bmlLogVa(m_ctx, BML_LOG_DEBUG, m_tag.empty() ? nullptr : m_tag.c_str(), fmt, args);
            va_end(args);
        }

        /**
         * @brief Log an info message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Info(const char *fmt, ...) const {
            if (!bmlLogVa) return;
            va_list args;
            va_start(args, fmt);
            bmlLogVa(m_ctx, BML_LOG_INFO, m_tag.empty() ? nullptr : m_tag.c_str(), fmt, args);
            va_end(args);
        }

        /**
         * @brief Log a warning message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Warn(const char *fmt, ...) const {
            if (!bmlLogVa) return;
            va_list args;
            va_start(args, fmt);
            bmlLogVa(m_ctx, BML_LOG_WARN, m_tag.empty() ? nullptr : m_tag.c_str(), fmt, args);
            va_end(args);
        }

        /**
         * @brief Log an error message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Error(const char *fmt, ...) const {
            if (!bmlLogVa) return;
            va_list args;
            va_start(args, fmt);
            bmlLogVa(m_ctx, BML_LOG_ERROR, m_tag.empty() ? nullptr : m_tag.c_str(), fmt, args);
            va_end(args);
        }

        /**
         * @brief Log a fatal message
         * @param fmt Printf-style format string
         * @param ... Format arguments
         */
        void Fatal(const char *fmt, ...) const {
            if (!bmlLogVa) return;
            va_list args;
            va_start(args, fmt);
            bmlLogVa(m_ctx, BML_LOG_FATAL, m_tag.empty() ? nullptr : m_tag.c_str(), fmt, args);
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
    };
} // namespace bml

#endif /* BML_LOGGER_HPP */
