/**
 * @file bml_profiling.hpp
 * @brief C++ wrappers for BML profiling API
 */

#ifndef BML_PROFILING_HPP
#define BML_PROFILING_HPP

#include "bml_profiling.h"
#include "bml_assert.hpp"
#include <optional>

namespace bml {

    // ========================================================================
    // ScopedTrace -- RAII trace scope
    // ========================================================================

    class ScopedTrace {
    public:
        ScopedTrace(const char *name, const char *category, const BML_CoreProfilingInterface *iface)
            : m_Interface(iface) {
            BML_ASSERT(iface);
            m_Interface->TraceBegin(m_Interface->Context, name, category);
        }

        ~ScopedTrace() {
            m_Interface->TraceEnd(m_Interface->Context);
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
        explicit ProfilingService(const BML_CoreProfilingInterface *iface,
                                  BML_Mod owner = nullptr) noexcept
            : m_Interface(iface), m_Owner(owner) {}

        void TraceBegin(const char *name, const char *category = nullptr) const {
            m_Interface->TraceBegin(m_Interface->Context, name, category);
        }

        void TraceEnd() const {
            m_Interface->TraceEnd(m_Interface->Context);
        }

        void Instant(const char *name, const char *category = nullptr) const {
            m_Interface->TraceInstant(m_Interface->Context, name, category);
        }

        void SetThreadName(const char *name) const {
            m_Interface->TraceSetThreadName(m_Interface->Context, name);
        }

        void Counter(const char *name, int64_t value) const {
            m_Interface->TraceCounter(m_Interface->Context, name, value);
        }

        void FrameMark() const {
            m_Interface->TraceFrameMark(m_Interface->Context);
        }

        uint64_t TimestampNs() const {
            return m_Interface->GetTimestampNs(m_Interface->Context);
        }

        uint64_t ApiCallCount(const char *name) const {
            return m_Interface->GetApiCallCount(m_Interface->Context, name);
        }

        uint64_t TotalAllocBytes() const {
            return m_Interface->GetTotalAllocBytes(m_Interface->Context);
        }

        uint64_t CpuFrequency() const {
            return m_Interface->GetCpuFrequency(m_Interface->Context);
        }

        BML_ProfilerBackend Backend() const {
            return m_Interface->GetProfilerBackend(m_Interface->Context);
        }

        bool IsEnabled() const {
            return m_Interface->IsProfilingEnabled(m_Interface->Context) == BML_TRUE;
        }

        BML_Result SetEnabled(bool enabled) const {
            return m_Interface->SetProfilingEnabled(m_Interface->Context, enabled ? BML_TRUE : BML_FALSE);
        }

        BML_Result Flush(const char *filename = nullptr) const {
            return m_Interface->FlushProfilingData(m_Interface->Context, filename);
        }

        std::optional<BML_ProfilingStats> Stats() const {
            BML_ProfilingStats stats = BML_PROFILING_STATS_INIT;
            if (m_Interface->GetProfilingStats(m_Interface->Context, &stats) == BML_RESULT_OK) {
                return stats;
            }
            return std::nullopt;
        }

        explicit operator bool() const noexcept { return m_Interface != nullptr; }

    private:
        const BML_CoreProfilingInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

} // namespace bml

#endif /* BML_PROFILING_HPP */
