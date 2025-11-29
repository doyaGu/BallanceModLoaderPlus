/**
 * @file bml_profiling.hpp
 * @brief BML C++ Profiling and Tracing Wrapper
 * 
 * Provides RAII-friendly wrappers for BML profiling and tracing APIs.
 */

#ifndef BML_PROFILING_HPP
#define BML_PROFILING_HPP

#include "bml_profiling.h"
#include "bml_errors.h"

#include <optional>
#include <string>

namespace bml {
    // ============================================================================
    // Profiling Capabilities Query
    // ============================================================================

    /**
     * @brief Query profiling subsystem capabilities
     * @return Capabilities if successful
     */
    inline std::optional<BML_ProfilingCaps> GetProfilingCaps() {
        if (!bmlProfilingGetCaps) return std::nullopt;
        BML_ProfilingCaps caps = BML_PROFILING_CAPS_INIT;
        if (bmlProfilingGetCaps(&caps) == BML_RESULT_OK) {
            return caps;
        }
        return std::nullopt;
    }

    /**
     * @brief Check if a profiling capability is available
     * @param flag Capability flag to check
     * @return true if capability is available
     */
    inline bool HasProfilingCap(BML_ProfilingCapabilityFlags flag) {
        auto caps = GetProfilingCaps();
        return caps && (caps->capability_flags & flag);
    }

    /**
     * @brief Get profiling statistics
     * @return Statistics if available
     */
    inline std::optional<BML_ProfilingStats> GetProfilingStats() {
        if (!bmlGetProfilingStats) return std::nullopt;
        BML_ProfilingStats stats = BML_PROFILING_STATS_INIT;
        if (bmlGetProfilingStats(&stats) == BML_RESULT_OK) {
            return stats;
        }
        return std::nullopt;
    }

    // ============================================================================
    // Profiling Control
    // ============================================================================

    /**
     * @brief Enable or disable profiling
     * @param enable true to enable, false to disable
     * @return true if successful
     */
    inline bool SetProfilingEnabled(bool enable) {
        if (!bmlSetProfilingEnabled) return false;
        return bmlSetProfilingEnabled(enable ? BML_TRUE : BML_FALSE) == BML_RESULT_OK;
    }

    /**
     * @brief Check if profiling is enabled
     * @return true if profiling is enabled
     */
    inline bool IsProfilingEnabled() {
        return bmlIsProfilingEnabled && bmlIsProfilingEnabled() != BML_FALSE;
    }

    /**
     * @brief Get the active profiler backend
     * @return Profiler backend type
     */
    inline BML_ProfilerBackend GetProfilerBackend() {
        return bmlGetProfilerBackend ? bmlGetProfilerBackend() : BML_PROFILER_NONE;
    }

    /**
     * @brief Flush profiling data to disk
     * @param filename Output filename (nullptr for default)
     * @return true if successful
     */
    inline bool FlushProfilingData(const char *filename = nullptr) {
        if (!bmlFlushProfilingData) return false;
        return bmlFlushProfilingData(filename) == BML_RESULT_OK;
    }

    // ============================================================================
    // Trace Functions
    // ============================================================================

    /**
     * @brief Mark the beginning of a timed scope
     * @param name Scope name
     * @param category Category for filtering (optional)
     */
    inline void TraceBegin(const char *name, const char *category = nullptr) {
        if (bmlTraceBegin) bmlTraceBegin(name, category);
    }

    /**
     * @brief Mark the end of a timed scope
     */
    inline void TraceEnd() {
        if (bmlTraceEnd) bmlTraceEnd();
    }

    /**
     * @brief Mark an instantaneous event
     * @param name Event name
     * @param category Category for filtering (optional)
     */
    inline void TraceInstant(const char *name, const char *category = nullptr) {
        if (bmlTraceInstant) bmlTraceInstant(name, category);
    }

    /**
     * @brief Set the name of the current thread
     * @param name Thread name
     */
    inline void TraceSetThreadName(const char *name) {
        if (bmlTraceSetThreadName) bmlTraceSetThreadName(name);
    }

    /**
     * @brief Emit a counter value
     * @param name Counter name
     * @param value Counter value
     */
    inline void TraceCounter(const char *name, int64_t value) {
        if (bmlTraceCounter) bmlTraceCounter(name, value);
    }

    /**
     * @brief Mark a frame boundary
     */
    inline void TraceFrameMark() {
        if (bmlTraceFrameMark) bmlTraceFrameMark();
    }

    // ============================================================================
    // Performance Counters
    // ============================================================================

    /**
     * @brief Get total number of API calls
     * @param api_name API function name
     * @return Call count since startup
     */
    inline uint64_t GetApiCallCount(const char *api_name) {
        return bmlGetApiCallCount ? bmlGetApiCallCount(api_name) : 0;
    }

    /**
     * @brief Get total bytes allocated
     * @return Total allocated bytes
     */
    inline uint64_t GetTotalAllocBytes() {
        return bmlGetTotalAllocBytes ? bmlGetTotalAllocBytes() : 0;
    }

    /**
     * @brief Get high-resolution timestamp
     * @return Nanoseconds since unspecified epoch
     */
    inline uint64_t GetTimestampNs() {
        return bmlGetTimestampNs ? bmlGetTimestampNs() : 0;
    }

    /**
     * @brief Get CPU frequency estimate
     * @return CPU frequency in Hz
     */
    inline uint64_t GetCpuFrequency() {
        return bmlGetCpuFrequency ? bmlGetCpuFrequency() : 0;
    }

    // ============================================================================
    // Scoped Trace (RAII)
    // ============================================================================

    /**
     * @brief RAII wrapper for scoped tracing
     *
     * Example:
     *   void MyFunction() {
     *       bml::ScopedTrace trace("MyFunction", "gameplay");
     *       // ... work ...
     *   } // Automatically ends trace on scope exit
     */
    class ScopedTrace {
    public:
        /**
         * @brief Begin a trace scope
         * @param name Scope name
         * @param category Category for filtering (optional)
         */
        ScopedTrace(const char *name, const char *category = nullptr) {
            if (bmlTraceBegin) bmlTraceBegin(name, category);
        }

        ~ScopedTrace() {
            if (bmlTraceEnd) bmlTraceEnd();
        }

        // Non-copyable
        ScopedTrace(const ScopedTrace &) = delete;
        ScopedTrace &operator=(const ScopedTrace &) = delete;
    };

    /**
     * @brief RAII wrapper for conditional scoped tracing
     *
     * Only traces if profiling is enabled. Zero overhead when disabled.
     */
    class ConditionalScopedTrace {
    public:
        ConditionalScopedTrace(const char *name, const char *category = nullptr)
            : m_Active(IsProfilingEnabled()) {
            if (m_Active && bmlTraceBegin) {
                bmlTraceBegin(name, category);
            }
        }

        ~ConditionalScopedTrace() {
            if (m_Active && bmlTraceEnd) {
                bmlTraceEnd();
            }
        }

        ConditionalScopedTrace(const ConditionalScopedTrace &) = delete;
        ConditionalScopedTrace &operator=(const ConditionalScopedTrace &) = delete;

    private:
        bool m_Active;
    };

    // ============================================================================
    // Timer Utility
    // ============================================================================

    /**
     * @brief High-resolution timer for manual measurements
     *
     * Example:
     *   bml::Timer timer;
     *   DoWork();
     *   auto elapsed_ns = timer.ElapsedNs();
     *   auto elapsed_ms = timer.ElapsedMs();
     */
    class Timer {
    public:
        Timer() : m_Start(GetTimestampNs()) {}

        /**
         * @brief Reset the timer
         */
        void reset() {
            m_Start = GetTimestampNs();
        }

        /**
         * @brief Get elapsed time in nanoseconds
         */
        uint64_t ElapsedNs() const {
            return GetTimestampNs() - m_Start;
        }

        /**
         * @brief Get elapsed time in microseconds
         */
        double ElapsedUs() const {
            return static_cast<double>(ElapsedNs()) / 1000.0;
        }

        /**
         * @brief Get elapsed time in milliseconds
         */
        double ElapsedMs() const {
            return static_cast<double>(ElapsedNs()) / 1000000.0;
        }

        /**
         * @brief Get elapsed time in seconds
         */
        double ElapsedSec() const {
            return static_cast<double>(ElapsedNs()) / 1000000000.0;
        }

    private:
        uint64_t m_Start;
    };

    // ============================================================================
    // Convenience Macros
    // ============================================================================

    /**
     * @brief Scoped trace macro with automatic naming
     *
     * Usage:
     *   void MyFunction() {
     *       BML_TRACE_SCOPE_CPP("gameplay");
     *       // ...
     *   }
     *
     * Note: The C API defines BML_TRACE_SCOPE(name, category) with two arguments.
     * In C++ code, prefer using BML_TRACE_SCOPE_CPP which uses __FUNCTION__ automatically,
     * or use BML_TRACE_SCOPE_NAMED for explicit naming.
     */
#define BML_TRACE_SCOPE_CPP(category) \
    bml::ScopedTrace _bml_trace_##__LINE__(__FUNCTION__, category)

    /**
     * @brief Conditional scoped trace macro
     */
#define BML_TRACE_SCOPE_IF_ENABLED(category) \
    bml::ConditionalScopedTrace _bml_trace_##__LINE__(__FUNCTION__, category)

    /**
     * @brief Named scoped trace macro (compatible with C version)
     */
#ifndef BML_TRACE_SCOPE_NAMED
#define BML_TRACE_SCOPE_NAMED(name, category) \
    bml::ScopedTrace _bml_trace_##__LINE__(name, category)
#endif
} // namespace bml

#endif /* BML_PROFILING_HPP */

