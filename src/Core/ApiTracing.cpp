/**
 * @file ApiTracing.cpp
 * @brief Implementation of API tracing and statistics collection
 */

#include "bml_api_tracing.h"
#include "ApiRegistrationMacros.h"
#include "ApiRegistry.h"
#include "Context.h"
#include "bml_capabilities.h"

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace {

// Global state
std::atomic<bool> g_TracingEnabled{false};
PFN_BML_TraceCallback g_TraceCallback = nullptr;
void* g_TraceUserData = nullptr;
std::mutex g_TraceMutex;

// Statistics storage
struct InternalApiStats {
    std::atomic<uint64_t> call_count{0};
    std::atomic<uint64_t> total_time_ns{0};
    std::atomic<uint64_t> min_time_ns{UINT64_MAX};
    std::atomic<uint64_t> max_time_ns{0};
    std::atomic<uint64_t> error_count{0};
};

std::unordered_map<uint32_t, InternalApiStats> g_Stats;
std::mutex g_StatsMutex;

void TraceOutput(const char* api_name, const char* args, int result, uint64_t duration_ns) {
    if (g_TraceCallback) {
        BML_Context ctx = BML::Core::Context::Instance().GetHandle();
        g_TraceCallback(ctx, api_name, args, result, duration_ns, g_TraceUserData);
    } else {
        // Default: OutputDebugString
        char buf[512];
        snprintf(buf, sizeof(buf), "[BML Trace] %s(%s) -> %d (%.2f μs)\n",
                 api_name, args ? args : "", result, duration_ns / 1000.0);
        OutputDebugStringA(buf);
    }
}

void UpdateStats(uint32_t api_id, uint64_t duration_ns, bool is_error) noexcept {
    // Lock-free path: try to find existing entry first
    InternalApiStats* stats_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_StatsMutex);
        // operator[] may throw on allocation, but stats are non-critical
        try {
            stats_ptr = &g_Stats[api_id];
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
           !stats_ptr->min_time_ns.compare_exchange_weak(old_min, duration_ns,
               std::memory_order_relaxed, std::memory_order_relaxed)) {}
    
    uint64_t old_max = stats_ptr->max_time_ns.load(std::memory_order_relaxed);
    while (duration_ns > old_max && 
           !stats_ptr->max_time_ns.compare_exchange_weak(old_max, duration_ns,
               std::memory_order_relaxed, std::memory_order_relaxed)) {}
}

} // anonymous namespace

// ============================================================================
// API Tracing Control (Internal implementations)
// ============================================================================

static void BML_EnableApiTracing(BML_Bool enable) {
    g_TracingEnabled.store(enable != BML_FALSE, std::memory_order_release);
}

static BML_Bool BML_IsApiTracingEnabled(void) {
    return g_TracingEnabled.load(std::memory_order_acquire) ? BML_TRUE : BML_FALSE;
}

static void BML_SetTraceCallback(PFN_BML_TraceCallback callback, void* user_data) {
    std::lock_guard<std::mutex> lock(g_TraceMutex);
    g_TraceCallback = callback;
    g_TraceUserData = user_data;
}

// ============================================================================
// API Statistics (Internal implementations)
// ============================================================================

static BML_Bool BML_GetApiStats(uint32_t api_id, BML_ApiStats* out_stats) {
    if (!out_stats) return BML_FALSE;
    
    std::lock_guard<std::mutex> lock(g_StatsMutex);
    
    auto it = g_Stats.find(api_id);
    if (it == g_Stats.end()) {
        return BML_FALSE;
    }
    
    const auto& stats = it->second;
    out_stats->api_id = api_id;
    out_stats->api_name = nullptr; // Would need ApiRegistry lookup
    out_stats->call_count = stats.call_count.load(std::memory_order_relaxed);
    out_stats->total_time_ns = stats.total_time_ns.load(std::memory_order_relaxed);
    out_stats->min_time_ns = stats.min_time_ns.load(std::memory_order_relaxed);
    out_stats->max_time_ns = stats.max_time_ns.load(std::memory_order_relaxed);
    out_stats->error_count = stats.error_count.load(std::memory_order_relaxed);
    
    // Get name from registry
    BML_ApiDescriptor desc;
    if (BML::Core::ApiRegistry::Instance().GetDescriptor(api_id, &desc)) {
        out_stats->api_name = desc.name;
    }
    
    return BML_TRUE;
}

static void BML_EnumerateApiStats(PFN_BML_StatsEnumerator callback, void* user_data) {
    if (!callback) return;
    
    std::lock_guard<std::mutex> lock(g_StatsMutex);
    BML_Context ctx = BML::Core::Context::Instance().GetHandle();
    
    for (const auto& [api_id, internal_stats] : g_Stats) {
        BML_ApiStats stats;
        stats.api_id = api_id;
        stats.api_name = nullptr;
        stats.call_count = internal_stats.call_count.load(std::memory_order_relaxed);
        stats.total_time_ns = internal_stats.total_time_ns.load(std::memory_order_relaxed);
        stats.min_time_ns = internal_stats.min_time_ns.load(std::memory_order_relaxed);
        stats.max_time_ns = internal_stats.max_time_ns.load(std::memory_order_relaxed);
        stats.error_count = internal_stats.error_count.load(std::memory_order_relaxed);
        
        // Get name from registry
        BML_ApiDescriptor desc;
        if (BML::Core::ApiRegistry::Instance().GetDescriptor(api_id, &desc)) {
            stats.api_name = desc.name;
        }
        
        if (!callback(ctx, &stats, user_data)) {
            break;
        }
    }
}

static BML_Bool BML_DumpApiStats(const char* output_file) {
    if (!output_file) return BML_FALSE;
    
    std::ofstream out(output_file);
    if (!out) return BML_FALSE;
    
    out << "{\n";
    
    std::lock_guard<std::mutex> lock(g_StatsMutex);
    
    bool first = true;
    for (const auto& [api_id, internal_stats] : g_Stats) {
        if (!first) out << ",\n";
        first = false;
        
        // Get API name
        const char* name = "unknown";
        BML_ApiDescriptor desc;
        if (BML::Core::ApiRegistry::Instance().GetDescriptor(api_id, &desc) && desc.name) {
            name = desc.name;
        }
        
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

static void BML_ResetApiStats(void) {
    std::lock_guard<std::mutex> lock(g_StatsMutex);
    g_Stats.clear();
}

// ============================================================================
// Debug Helpers (Internal implementations)
// ============================================================================

static BML_Bool BML_ValidateApiId(uint32_t api_id, const char* context) {
    if (api_id == 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[BML] WARNING: Invalid API ID (0) in context: %s",
                 context ? context : "unknown");
        OutputDebugStringA(buf);
        return BML_FALSE;
    }
    
    // Check if ID is registered
    BML::Core::ApiRegistry::ApiMetadata meta;
    if (!BML::Core::ApiRegistry::Instance().TryGetMetadata(api_id, meta)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[BML] WARNING: Unregistered API ID (%u) in context: %s",
                 api_id, context ? context : "unknown");
        OutputDebugStringA(buf);
        return BML_FALSE;
    }
    
    return BML_TRUE;
}

// ============================================================================
// Internal: Trace Helper for API Implementations
// ============================================================================

namespace BML::Core {

class ScopedApiTrace {
public:
    ScopedApiTrace(uint32_t api_id, const char* api_name, const char* args = nullptr) noexcept
        : m_api_id(api_id)
        , m_api_name(api_name)
        , m_args(args)
        , m_enabled(g_TracingEnabled.load(std::memory_order_acquire)) {
        if (m_enabled) {
            m_start = std::chrono::high_resolution_clock::now();
        }
    }
    
    // Destructor must be noexcept to avoid issues during stack unwinding
    ~ScopedApiTrace() noexcept {
        if (m_enabled) {
            try {
                auto end = std::chrono::high_resolution_clock::now();
                uint64_t duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - m_start).count();
                
                TraceOutput(m_api_name, m_args, m_result, duration_ns);
                UpdateStats(m_api_id, duration_ns, m_result != 0);
            } catch (...) {
                // Silently ignore any exceptions during tracing cleanup
            }
        }
    }
    
    // Non-copyable, non-movable
    ScopedApiTrace(const ScopedApiTrace&) = delete;
    ScopedApiTrace& operator=(const ScopedApiTrace&) = delete;
    ScopedApiTrace(ScopedApiTrace&&) = delete;
    ScopedApiTrace& operator=(ScopedApiTrace&&) = delete;
    
    void SetResult(int result) noexcept { m_result = result; }
    
private:
    uint32_t m_api_id;
    const char* m_api_name;
    const char* m_args;
    bool m_enabled;
    int m_result = 0;
    std::chrono::high_resolution_clock::time_point m_start;
};

// ============================================================================
// API Registration
// ============================================================================

void RegisterTracingApis() {
    BML_BEGIN_API_REGISTRATION();
    
    /* Tracing control */
    BML_REGISTER_API_WITH_CAPS(bmlEnableApiTracing, BML_EnableApiTracing, BML_CAP_API_TRACING);
    BML_REGISTER_API_WITH_CAPS(bmlIsApiTracingEnabled, BML_IsApiTracingEnabled, BML_CAP_API_TRACING);
    BML_REGISTER_API_WITH_CAPS(bmlSetTraceCallback, BML_SetTraceCallback, BML_CAP_API_TRACING);
    
    /* Statistics */
    BML_REGISTER_API_WITH_CAPS(bmlGetApiStats, BML_GetApiStats, BML_CAP_API_TRACING);
    BML_REGISTER_API_WITH_CAPS(bmlEnumerateApiStats, BML_EnumerateApiStats, BML_CAP_API_TRACING);
    BML_REGISTER_API_WITH_CAPS(bmlDumpApiStats, BML_DumpApiStats, BML_CAP_API_TRACING);
    BML_REGISTER_API_WITH_CAPS(bmlResetApiStats, BML_ResetApiStats, BML_CAP_API_TRACING);
    BML_REGISTER_API_WITH_CAPS(bmlValidateApiId, BML_ValidateApiId, BML_CAP_API_TRACING);
}

} // namespace BML::Core
