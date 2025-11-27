#ifndef CORE_PROFILING_MANAGER_H
#define CORE_PROFILING_MANAGER_H

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
        BML_ProfilerBackend GetProfilerBackend() const { return backend_; }
        BML_Result SetProfilingEnabled(BML_Bool enable);
        BML_Bool IsProfilingEnabled() const { return enabled_ ? BML_TRUE : BML_FALSE; }
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

        BML_ProfilerBackend backend_;
        std::atomic<bool> enabled_;

        std::atomic<uint64_t> total_events_;
        std::atomic<uint64_t> total_scopes_;
        std::atomic<uint64_t> dropped_events_;

        std::vector<TraceEvent> event_buffer_;
        CRITICAL_SECTION buffer_lock_;

        uint64_t qpc_frequency_;
        uint64_t startup_time_ns_;

        static constexpr size_t MAX_EVENTS = 100000;
        static constexpr size_t MAX_SCOPE_DEPTH = 64;
    };
} // namespace BML::Core

#endif /* CORE_PROFILING_MANAGER_H */
