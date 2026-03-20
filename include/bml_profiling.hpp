/**
 * @file bml_profiling.hpp
 * @brief C++ wrappers for BML profiling API
 */

#ifndef BML_PROFILING_HPP
#define BML_PROFILING_HPP

#include "bml_builtin_interfaces.h"
#include <optional>

namespace bml {

    // ========================================================================
    // ScopedTrace -- RAII trace scope
    // ========================================================================

    class ScopedTrace {
    public:
        ScopedTrace(const char *name, const char *category, const BML_CoreProfilingInterface *iface)
            : m_Interface(iface) {
            if (m_Interface && m_Interface->TraceBegin) {
                m_Interface->TraceBegin(name, category);
            }
        }

        ~ScopedTrace() {
            if (m_Interface && m_Interface->TraceEnd) {
                m_Interface->TraceEnd();
            }
        }

        ScopedTrace(const ScopedTrace &) = delete;
        ScopedTrace &operator=(const ScopedTrace &) = delete;

    private:
        const BML_CoreProfilingInterface *m_Interface = nullptr;
    };

    // ========================================================================
    // ProfilingService -- convenience wrapper
    // ========================================================================

    class ProfilingService {
    public:
        ProfilingService() = default;
        explicit ProfilingService(const BML_CoreProfilingInterface *iface) noexcept
            : m_Interface(iface) {}

        void TraceBegin(const char *name, const char *category = nullptr) const {
            if (m_Interface && m_Interface->TraceBegin) {
                m_Interface->TraceBegin(name, category);
            }
        }

        void TraceEnd() const {
            if (m_Interface && m_Interface->TraceEnd) {
                m_Interface->TraceEnd();
            }
        }

        void Instant(const char *name, const char *category = nullptr) const {
            if (m_Interface && m_Interface->TraceInstant) {
                m_Interface->TraceInstant(name, category);
            }
        }

        void SetThreadName(const char *name) const {
            if (m_Interface && m_Interface->TraceSetThreadName) {
                m_Interface->TraceSetThreadName(name);
            }
        }

        void Counter(const char *name, int64_t value) const {
            if (m_Interface && m_Interface->TraceCounter) {
                m_Interface->TraceCounter(name, value);
            }
        }

        void FrameMark() const {
            if (m_Interface && m_Interface->TraceFrameMark) {
                m_Interface->TraceFrameMark();
            }
        }

        uint64_t TimestampNs() const {
            if (m_Interface && m_Interface->GetTimestampNs) {
                return m_Interface->GetTimestampNs();
            }
            return 0;
        }

        uint64_t ApiCallCount(const char *name) const {
            if (m_Interface && m_Interface->GetApiCallCount) {
                return m_Interface->GetApiCallCount(name);
            }
            return 0;
        }

        uint64_t TotalAllocBytes() const {
            if (m_Interface && m_Interface->GetTotalAllocBytes) {
                return m_Interface->GetTotalAllocBytes();
            }
            return 0;
        }

        uint64_t CpuFrequency() const {
            if (m_Interface && m_Interface->GetCpuFrequency) {
                return m_Interface->GetCpuFrequency();
            }
            return 0;
        }

        BML_ProfilerBackend Backend() const {
            if (m_Interface && m_Interface->GetProfilerBackend) {
                return m_Interface->GetProfilerBackend();
            }
            return BML_PROFILER_NONE;
        }

        bool IsEnabled() const {
            if (m_Interface && m_Interface->IsProfilingEnabled) {
                return m_Interface->IsProfilingEnabled() == BML_TRUE;
            }
            return false;
        }

        BML_Result SetEnabled(bool enabled) const {
            if (m_Interface && m_Interface->SetProfilingEnabled) {
                return m_Interface->SetProfilingEnabled(enabled ? BML_TRUE : BML_FALSE);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        BML_Result Flush(const char *filename = nullptr) const {
            if (m_Interface && m_Interface->FlushProfilingData) {
                return m_Interface->FlushProfilingData(filename);
            }
            return BML_RESULT_NOT_INITIALIZED;
        }

        std::optional<BML_ProfilingStats> Stats() const {
            if (m_Interface && m_Interface->GetProfilingStats) {
                BML_ProfilingStats stats = BML_PROFILING_STATS_INIT;
                if (m_Interface->GetProfilingStats(&stats) == BML_RESULT_OK) {
                    return stats;
                }
            }
            return std::nullopt;
        }

        explicit operator bool() const noexcept { return m_Interface != nullptr; }

    private:
        const BML_CoreProfilingInterface *m_Interface = nullptr;
    };

} // namespace bml

#endif /* BML_PROFILING_HPP */
