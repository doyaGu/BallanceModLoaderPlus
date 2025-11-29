#ifndef BML_CORE_PROFILING_MANAGER_H
#define BML_CORE_PROFILING_MANAGER_H

#include "bml_profiling.h"

#include <vector>
#include <string>
#include <atomic>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace BML::Core {
    /**
     * @brief Manages performance profiling and tracing
     *
     * Provides Chrome Tracing JSON export and high-resolution timing.
     * Thread-safe for all trace operations.
     */
    class ProfilingManager {
    public:
        static ProfilingManager &Instance();

        ProfilingManager(const ProfilingManager &) = delete;
        ProfilingManager &operator=(const ProfilingManager &) = delete;

        /* Trace Events */
        void TraceBegin(const char *name, const char *category);
        void TraceEnd();
        void TraceInstant(const char *name, const char *category);
        void TraceSetThreadName(const char *name);
        void TraceCounter(const char *name, int64_t value);
        void TraceFrameMark();

        /* Performance Counters */
        uint64_t GetApiCallCount(const char *api_name);
        uint64_t GetTotalAllocBytes();
        uint64_t GetTimestampNs();
        uint64_t GetCpuFrequency();

        /* Backend Control */
        BML_ProfilerBackend GetProfilerBackend() const { return m_Backend; }
        BML_Result SetProfilingEnabled(BML_Bool enable);
        BML_Bool IsProfilingEnabled() const { return m_Enabled ? BML_TRUE : BML_FALSE; }
        BML_Result FlushProfilingData(const char *filename);

        /* Statistics */
        BML_Result GetProfilingStats(BML_ProfilingStats *out_stats);
        BML_Result GetProfilingCaps(BML_ProfilingCaps *out_caps);

    private:
        ProfilingManager();
        ~ProfilingManager();

        struct TraceEvent {
            enum Type { BEGIN, END, INSTANT, COUNTER, FRAME };

            Type type;
            std::string name;
            std::string category;
            uint64_t timestamp_ns;
            uint64_t thread_id;
            int64_t counter_value;
        };

        struct ThreadContext {
            std::string name;
            std::vector<const char *> scope_stack; // Track nested scopes
        };

        ThreadContext &GetThreadContext();
        void WriteJsonEvent(const TraceEvent &evt, FILE *fp);

        BML_ProfilerBackend m_Backend;
        std::atomic<bool> m_Enabled;

        std::atomic<uint64_t> m_TotalEvents;
        std::atomic<uint64_t> m_TotalScopes;
        std::atomic<uint64_t> m_DroppedEvents;

        std::vector<TraceEvent> m_EventBuffer;
        CRITICAL_SECTION m_BufferLock;

        uint64_t m_QpcFrequency;
        uint64_t m_StartupTimeNs;

        static constexpr size_t MAX_EVENTS = 100000;
        static constexpr size_t MAX_SCOPE_DEPTH = 64;
    };
} // namespace BML::Core

#endif /* BML_CORE_PROFILING_MANAGER_H */
