#include "ProfilingManager.h"
#include "ApiRegistry.h"
#include "CoreErrors.h"
#include "DiagnosticManager.h"
#include "MemoryManager.h"

#include <cstdio>
#include <unordered_map>

namespace BML::Core {
    ProfilingManager &ProfilingManager::Instance() {
        static ProfilingManager instance;
        return instance;
    }

    ProfilingManager::ProfilingManager()
        : backend_(BML_PROFILER_CHROME_TRACING),
          enabled_(false),
          total_events_(0),
          total_scopes_(0),
          dropped_events_(0),
          buffer_lock_(),
          qpc_frequency_(0),
          startup_time_ns_(0) {
        InitializeCriticalSection(&buffer_lock_);
        event_buffer_.reserve(MAX_EVENTS);

        // Get QPC frequency
        LARGE_INTEGER freq;
        if (QueryPerformanceFrequency(&freq)) {
            qpc_frequency_ = freq.QuadPart;
        }

        // Record startup time
        startup_time_ns_ = GetTimestampNs();
    }

    ProfilingManager::~ProfilingManager() {
        DeleteCriticalSection(&buffer_lock_);
    }

    uint64_t ProfilingManager::GetTimestampNs() {
        LARGE_INTEGER counter;
        if (!QueryPerformanceCounter(&counter)) {
            return 0;
        }

        // Convert to nanoseconds
        return (counter.QuadPart * 1000000000ULL) / qpc_frequency_;
    }

    uint64_t ProfilingManager::GetCpuFrequency() {
        return qpc_frequency_;
    }

    ProfilingManager::ThreadContext &ProfilingManager::GetThreadContext() {
        thread_local ThreadContext ctx;
        return ctx;
    }

    void ProfilingManager::TraceBegin(const char *name, const char *category) {
        if (!enabled_) return;
        if (!name) return;

        auto &ctx = GetThreadContext();
        if (ctx.scope_stack.size() >= MAX_SCOPE_DEPTH) {
            dropped_events_++;
            return;
        }

        ctx.scope_stack.push_back(name);

        TraceEvent evt;
        evt.type = TraceEvent::BEGIN;
        evt.name = name;
        if (category) evt.category = category;
        evt.timestamp_ns = GetTimestampNs() - startup_time_ns_;
        evt.thread_id = GetCurrentThreadId();

        EnterCriticalSection(&buffer_lock_);
        try {
            if (event_buffer_.size() < MAX_EVENTS) {
                event_buffer_.push_back(std::move(evt));
                ++total_events_;
                ++total_scopes_;
            } else {
                ++dropped_events_;
            }
        } catch (...) {
            ++dropped_events_;
        }
        LeaveCriticalSection(&buffer_lock_);
    }

    void ProfilingManager::TraceEnd() {
        if (!enabled_) return;

        auto &ctx = GetThreadContext();
        if (ctx.scope_stack.empty()) {
            return; // Unmatched end
        }

        const char *name = ctx.scope_stack.back();
        ctx.scope_stack.pop_back();

        TraceEvent evt;
        evt.type = TraceEvent::END;
        evt.name = name;
        evt.timestamp_ns = GetTimestampNs() - startup_time_ns_;
        evt.thread_id = GetCurrentThreadId();

        EnterCriticalSection(&buffer_lock_);
        try {
            if (event_buffer_.size() < MAX_EVENTS) {
                event_buffer_.push_back(std::move(evt));
                ++total_events_;
            } else {
                ++dropped_events_;
            }
        } catch (...) {
            ++dropped_events_;
        }
        LeaveCriticalSection(&buffer_lock_);
    }

    void ProfilingManager::TraceInstant(const char *name, const char *category) {
        if (!enabled_) return;
        if (!name) return;

        TraceEvent evt;
        evt.type = TraceEvent::INSTANT;
        evt.name = name;
        if (category) evt.category = category;
        evt.timestamp_ns = GetTimestampNs() - startup_time_ns_;
        evt.thread_id = GetCurrentThreadId();

        EnterCriticalSection(&buffer_lock_);
        try {
            if (event_buffer_.size() < MAX_EVENTS) {
                event_buffer_.push_back(std::move(evt));
                ++total_events_;
            } else {
                ++dropped_events_;
            }
        } catch (...) {
            ++dropped_events_;
        }
        LeaveCriticalSection(&buffer_lock_);
    }

    void ProfilingManager::TraceSetThreadName(const char *name) {
        if (!name) return;
        auto &ctx = GetThreadContext();
        ctx.name = name;
    }

    void ProfilingManager::TraceCounter(const char *name, int64_t value) {
        if (!enabled_) return;
        if (!name) return;

        TraceEvent evt;
        evt.type = TraceEvent::COUNTER;
        evt.name = name;
        evt.timestamp_ns = GetTimestampNs() - startup_time_ns_;
        evt.thread_id = GetCurrentThreadId();
        evt.counter_value = value;

        EnterCriticalSection(&buffer_lock_);
        try {
            if (event_buffer_.size() < MAX_EVENTS) {
                event_buffer_.push_back(std::move(evt));
                ++total_events_;
            } else {
                ++dropped_events_;
            }
        } catch (...) {
            ++dropped_events_;
        }
        LeaveCriticalSection(&buffer_lock_);
    }

    void ProfilingManager::TraceFrameMark() {
        if (!enabled_) return;

        TraceEvent evt;
        evt.type = TraceEvent::FRAME;
        evt.name = "Frame";
        evt.timestamp_ns = GetTimestampNs() - startup_time_ns_;
        evt.thread_id = GetCurrentThreadId();

        EnterCriticalSection(&buffer_lock_);
        try {
            if (event_buffer_.size() < MAX_EVENTS) {
                event_buffer_.push_back(std::move(evt));
                ++total_events_;
            } else {
                ++dropped_events_;
            }
        } catch (...) {
            ++dropped_events_;
        }
        LeaveCriticalSection(&buffer_lock_);
    }

    uint64_t ProfilingManager::GetApiCallCount(const char *api_name) {
        if (!api_name) {
            return 0;
        }
        return ApiRegistry::Instance().GetCallCount(api_name);
    }

    uint64_t ProfilingManager::GetTotalAllocBytes() {
        BML_MemoryStats stats;
        if (MemoryManager::Instance().GetStats(&stats) == BML_RESULT_OK) {
            return stats.total_allocated;
        }
        return 0;
    }

    BML_Result ProfilingManager::SetProfilingEnabled(BML_Bool enable) {
        enabled_ = (enable == BML_TRUE);
        return BML_RESULT_OK;
    }

    void ProfilingManager::WriteJsonEvent(const TraceEvent &evt, FILE *fp) {
        const char *phase = "";
        switch (evt.type) {
        case TraceEvent::BEGIN: phase = "B";
            break;
        case TraceEvent::END: phase = "E";
            break;
        case TraceEvent::INSTANT: phase = "i";
            break;
        case TraceEvent::COUNTER: phase = "C";
            break;
        case TraceEvent::FRAME: phase = "i";
            break;
        }

        // Convert ns to microseconds (Chrome Tracing uses μs)
        double ts_us = evt.timestamp_ns / 1000.0;

        fprintf(fp, "    {\"name\":\"%s\",\"ph\":\"%s\",\"ts\":%.3f,\"pid\":1,\"tid\":%llu",
                evt.name.c_str(), phase, ts_us, evt.thread_id);

        if (!evt.category.empty()) {
            fprintf(fp, ",\"cat\":\"%s\"", evt.category.c_str());
        }

        if (evt.type == TraceEvent::COUNTER) {
            fprintf(fp, ",\"args\":{\"%s\":%lld}", evt.name.c_str(), evt.counter_value);
        }

        if (evt.type == TraceEvent::INSTANT) {
            fprintf(fp, ",\"s\":\"t\""); // thread scope
        }

        fprintf(fp, "}");
    }

    BML_Result ProfilingManager::FlushProfilingData(const char *filename) {
        if (!filename) {
            filename = "bml_trace.json";
        }

        // Extract events under lock to minimize lock contention during file I/O
        std::vector<TraceEvent> events_snapshot;
        EnterCriticalSection(&buffer_lock_);
        try {
            events_snapshot = event_buffer_; // Copy while holding lock
        } catch (...) {
            LeaveCriticalSection(&buffer_lock_);
            SetLastError(BML_RESULT_OUT_OF_MEMORY, "Failed to copy event buffer", "bmlFlushProfilingData");
            return BML_RESULT_OUT_OF_MEMORY;
        }
        LeaveCriticalSection(&buffer_lock_);

        // RAII wrapper for FILE* handle
        struct FileGuard {
            FILE *fp;
            ~FileGuard() { if (fp) fclose(fp); }
        };

        FILE *fp = fopen(filename, "w");
        if (!fp) {
            SetLastError(BML_RESULT_IO_ERROR, "Failed to open trace file for writing", "bmlFlushProfilingData");
            return BML_RESULT_IO_ERROR;
        }
        FileGuard guard{fp};

        fprintf(fp, "{\n");
        fprintf(fp, "  \"displayTimeUnit\": \"ns\",\n");
        fprintf(fp, "  \"traceEvents\": [\n");

        size_t count = events_snapshot.size();
        for (size_t i = 0; i < count; ++i) {
            WriteJsonEvent(events_snapshot[i], fp);
            if (i + 1 < count) {
                fprintf(fp, ",\n");
            } else {
                fprintf(fp, "\n");
            }
        }

        fprintf(fp, "  ]\n");
        fprintf(fp, "}\n");

        return BML_RESULT_OK;
    }

    BML_Result ProfilingManager::GetProfilingStats(BML_ProfilingStats *out_stats) {
        if (!out_stats) {
            SetLastError(BML_RESULT_INVALID_ARGUMENT, "out_stats is NULL", "bmlGetProfilingStats");
            return BML_RESULT_INVALID_ARGUMENT;
        }

        out_stats->total_events = total_events_;
        out_stats->total_scopes = total_scopes_;
        out_stats->dropped_events = dropped_events_;
        out_stats->memory_used_bytes = event_buffer_.capacity() * sizeof(TraceEvent);

        EnterCriticalSection(&buffer_lock_);
        size_t active = 0;
        // Count active scopes across all threads (simplified)
        LeaveCriticalSection(&buffer_lock_);
        out_stats->active_scopes = active;

        return BML_RESULT_OK;
    }

    BML_Result ProfilingManager::GetProfilingCaps(BML_ProfilingCaps *out_caps) {
        if (!out_caps) {
            SetLastError(BML_RESULT_INVALID_ARGUMENT, "out_caps is NULL", "bmlProfilingGetCaps");
            return BML_RESULT_INVALID_ARGUMENT;
        }

        out_caps->struct_size = sizeof(BML_ProfilingCaps);
        out_caps->api_version = bmlGetApiVersion();
        out_caps->capability_flags =
            BML_PROFILING_CAP_TRACE_EVENTS |
            BML_PROFILING_CAP_COUNTERS |
            BML_PROFILING_CAP_MEMORY_TRACKING;
        out_caps->active_backend = backend_;
        out_caps->max_scope_depth = MAX_SCOPE_DEPTH;
        out_caps->event_buffer_size = MAX_EVENTS;

        return BML_RESULT_OK;
    }
} // namespace BML::Core
