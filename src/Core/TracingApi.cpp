/**
 * @file TracingApi.cpp
 * @brief Implementation of API tracing and statistics collection
 */

#include "bml_types.h"
#include "ApiRegistrationMacros.h"
#include "ApiRegistry.h"
#include "Context.h"
#include "Logging.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

// Types formerly in bml_api_tracing.h -- now internal to the tracing subsystem.

typedef void (*PFN_BML_TraceCallback)(
    BML_Context ctx, const char *api_name, const char *args_summary,
    int result_code, uint64_t duration_ns, void *user_data);

typedef struct BML_ApiStats {
    size_t struct_size;
    const char *api_name;
    uint64_t call_count;
    uint64_t total_time_ns;
    uint64_t min_time_ns;
    uint64_t max_time_ns;
    uint64_t error_count;
} BML_ApiStats;

typedef BML_Bool (*PFN_BML_StatsEnumerator)(
    BML_Context ctx, const BML_ApiStats *stats, void *user_data);

namespace {
    constexpr char kTracingLogCategory[] = "api.tracing";

    // Global state
    std::atomic<bool> g_TracingEnabled{false};
    PFN_BML_TraceCallback g_TraceCallback = nullptr;
    void *g_TraceUserData = nullptr;
    std::mutex g_TraceMutex;

    // Statistics storage
    struct InternalApiStats {
        std::atomic<uint64_t> call_count{0};
        std::atomic<uint64_t> total_time_ns{0};
        std::atomic<uint64_t> min_time_ns{UINT64_MAX};
        std::atomic<uint64_t> max_time_ns{0};
        std::atomic<uint64_t> error_count{0};
    };

    std::unordered_map<std::string, InternalApiStats> g_Stats;
    std::mutex g_StatsMutex;

    void TraceOutput(const char *api_name, const char *args, int result, uint64_t duration_ns);
    void UpdateStats(const char *name, uint64_t duration_ns, bool is_error) noexcept;

    class ApiTraceScope {
    public:
        explicit ApiTraceScope(const char *api_name)
            : m_ApiName(api_name),
              m_Start(std::chrono::steady_clock::now()) {
        }

        void Complete(int result_code = 0) {
            if (!m_ApiName || m_Completed) {
                return;
            }

            m_Completed = true;
            auto end = std::chrono::steady_clock::now();
            uint64_t duration_ns = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_Start).count());
            if (duration_ns == 0) {
                duration_ns = 1;
            }
            if (g_TracingEnabled.load(std::memory_order_acquire)) {
                TraceOutput(m_ApiName, "", result_code, duration_ns);
            }
            UpdateStats(m_ApiName, duration_ns, result_code < 0);
        }

        ~ApiTraceScope() {
            Complete();
        }

    private:
        const char *m_ApiName{nullptr};
        std::chrono::steady_clock::time_point m_Start;
        bool m_Completed{false};
    };

    void TraceOutput(const char *api_name, const char *args, int result, uint64_t duration_ns) {
        if (g_TraceCallback) {
            BML_Context ctx = BML::Core::Kernel().context->GetHandle();
            g_TraceCallback(ctx, api_name, args, result, duration_ns, g_TraceUserData);
        } else {
            BML::Core::CoreLog(BML_LOG_DEBUG, kTracingLogCategory,
                    "%s(%s) -> %d (%.2f us)",
                    api_name ? api_name : "<null>", args ? args : "",
                    result, duration_ns / 1000.0);
        }
    }

    void UpdateStats(const char *name, uint64_t duration_ns, bool is_error) noexcept {
        if (!name) return;

        // Lock-free path: try to find existing entry first
        InternalApiStats *stats_ptr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_StatsMutex);
            // operator[] may throw on allocation, but stats are non-critical
            try {
                stats_ptr = &g_Stats[name];
            } catch (...) {
                return; // Silently ignore allocation failure for stats
            }
        }

        if (!stats_ptr) return;

        // All atomic operations are lock-free and noexcept
        stats_ptr->call_count.fetch_add(1, std::memory_order_relaxed);
        stats_ptr->total_time_ns.fetch_add(duration_ns, std::memory_order_relaxed);

        if (is_error) {
            stats_ptr->error_count.fetch_add(1, std::memory_order_relaxed);
        }

        // Update min/max using compare-exchange (lock-free, noexcept)
        uint64_t old_min = stats_ptr->min_time_ns.load(std::memory_order_relaxed);
        while (duration_ns < old_min &&
            !stats_ptr->min_time_ns.compare_exchange_weak(old_min, duration_ns, std::memory_order_relaxed, std::memory_order_relaxed)) {
        }

        uint64_t old_max = stats_ptr->max_time_ns.load(std::memory_order_relaxed);
        while (duration_ns > old_max &&
            !stats_ptr->max_time_ns.compare_exchange_weak(old_max, duration_ns, std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
    }
} // anonymous namespace

namespace BML::Core {
    // ============================================================================
    // API Tracing Control
    // ============================================================================

    void BML_EnableApiTracing(BML_Bool enable) {
        ApiTraceScope trace("bmlEnableApiTracing");
        g_TracingEnabled.store(enable != BML_FALSE, std::memory_order_release);
        trace.Complete();
    }

    BML_Bool BML_IsApiTracingEnabled() {
        ApiTraceScope trace("bmlIsApiTracingEnabled");
        BML_Bool enabled = g_TracingEnabled.load(std::memory_order_acquire) ? BML_TRUE : BML_FALSE;
        trace.Complete();
        return enabled;
    }

    void BML_SetTraceCallback(PFN_BML_TraceCallback callback, void *user_data) {
        ApiTraceScope trace("bmlSetTraceCallback");
        std::lock_guard<std::mutex> lock(g_TraceMutex);
        g_TraceCallback = callback;
        g_TraceUserData = user_data;
        trace.Complete();
    }

    // ============================================================================
    // API Statistics
    // ============================================================================

    BML_Bool BML_GetApiStats(const char *api_name, BML_ApiStats *out_stats) {
        ApiTraceScope trace("bmlGetApiStats");
        if (!api_name || !out_stats) return BML_FALSE;

        std::lock_guard<std::mutex> lock(g_StatsMutex);

        auto it = g_Stats.find(api_name);
        if (it == g_Stats.end()) {
            return BML_FALSE;
        }

        const auto &stats = it->second;
        out_stats->struct_size = sizeof(BML_ApiStats);
        out_stats->api_name = it->first.c_str();
        out_stats->call_count = stats.call_count.load(std::memory_order_relaxed);
        out_stats->total_time_ns = stats.total_time_ns.load(std::memory_order_relaxed);
        out_stats->min_time_ns = stats.min_time_ns.load(std::memory_order_relaxed);
        out_stats->max_time_ns = stats.max_time_ns.load(std::memory_order_relaxed);
        out_stats->error_count = stats.error_count.load(std::memory_order_relaxed);

        return BML_TRUE;
    }

    void BML_EnumerateApiStats(PFN_BML_StatsEnumerator callback, void *user_data) {
        ApiTraceScope trace("bmlEnumerateApiStats");
        if (!callback) return;

        std::lock_guard<std::mutex> lock(g_StatsMutex);
        BML_Context ctx = BML::Core::Kernel().context->GetHandle();

        for (const auto &[name, internal_stats] : g_Stats) {
            BML_ApiStats stats{};
            stats.struct_size = sizeof(BML_ApiStats);
            stats.api_name = name.c_str();
            stats.call_count = internal_stats.call_count.load(std::memory_order_relaxed);
            stats.total_time_ns = internal_stats.total_time_ns.load(std::memory_order_relaxed);
            stats.min_time_ns = internal_stats.min_time_ns.load(std::memory_order_relaxed);
            stats.max_time_ns = internal_stats.max_time_ns.load(std::memory_order_relaxed);
            stats.error_count = internal_stats.error_count.load(std::memory_order_relaxed);

            if (!callback(ctx, &stats, user_data)) {
                break;
            }
        }
    }

    BML_Bool BML_DumpApiStats(const char *output_file) {
        ApiTraceScope trace("bmlDumpApiStats");
        if (!output_file) return BML_FALSE;

        std::ofstream out(output_file);
        if (!out) return BML_FALSE;

        out << "{\n";

        std::lock_guard<std::mutex> lock(g_StatsMutex);

        bool first = true;
        for (const auto &[name, internal_stats] : g_Stats) {
            if (!first) out << ",\n";
            first = false;

            uint64_t calls = internal_stats.call_count.load(std::memory_order_relaxed);
            uint64_t total_ns = internal_stats.total_time_ns.load(std::memory_order_relaxed);
            uint64_t min_ns = internal_stats.min_time_ns.load(std::memory_order_relaxed);
            uint64_t max_ns = internal_stats.max_time_ns.load(std::memory_order_relaxed);
            uint64_t errors = internal_stats.error_count.load(std::memory_order_relaxed);

            uint64_t avg_us = calls > 0 ? (total_ns / calls / 1000) : 0;

            out << "  \"" << name << "\": {"
                << "\"calls\": " << calls << ", "
                << "\"total_time_us\": " << (total_ns / 1000) << ", "
                << "\"avg_time_us\": " << avg_us << ", "
                << "\"min_time_us\": " << (min_ns == UINT64_MAX ? 0 : min_ns / 1000) << ", "
                << "\"max_time_us\": " << (max_ns / 1000) << ", "
                << "\"errors\": " << errors
                << "}";
        }

        out << "\n}\n";
        return BML_TRUE;
    }

    void BML_ResetApiStats() {
        std::lock_guard<std::mutex> lock(g_StatsMutex);
        g_Stats.clear();
    }

    // ============================================================================
    // Registration
    // ============================================================================

    void RegisterTracingApis() {
        BML_BEGIN_API_REGISTRATION();

        /* Tracing control */
        BML_REGISTER_API(bmlEnableApiTracing, BML_EnableApiTracing);
        BML_REGISTER_API(bmlIsApiTracingEnabled, BML_IsApiTracingEnabled);
        BML_REGISTER_API(bmlSetTraceCallback, BML_SetTraceCallback);

        /* Statistics */
        BML_REGISTER_API(bmlGetApiStats, BML_GetApiStats);
        BML_REGISTER_API(bmlEnumerateApiStats, BML_EnumerateApiStats);
        BML_REGISTER_API(bmlDumpApiStats, BML_DumpApiStats);
        BML_REGISTER_API(bmlResetApiStats, BML_ResetApiStats);
    }
} // namespace BML::Core
