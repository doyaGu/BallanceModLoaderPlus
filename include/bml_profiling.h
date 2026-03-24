#ifndef BML_PROFILING_H
#define BML_PROFILING_H

/**
 * @file bml_profiling.h
 * @brief Performance profiling and tracing API
 * 
 * Provides tools for performance analysis compatible with external profilers
 * like Tracy, Chrome Tracing, and RenderDoc.
 *
 * Profiling APIs are runtime services. Module code should use the injected
 * `bml.core.profiling` runtime interface. Host bootstrap does not publish
 * profiling globals.
 */

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_interface.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

#define BML_CORE_PROFILING_INTERFACE_ID "bml.core.profiling"

/* ========== Trace Events ========== */

/**
 * @brief Mark the beginning of a timed scope
 * 
 * @param[in] name Scope name (must be string literal or stable pointer)
 * @param[in] category Category for filtering (may be NULL)
 * 
 * @threadsafe Yes
 * 
 * @note Must be paired with bmlTraceEnd()
 * @note Name pointer must remain valid until bmlTraceEnd()
 * 
 * @code
 * void MyFunction() {
 *     traceBegin("MyFunction", "gameplay");
 *     // ... work ...
 *     traceEnd();
 * }
 * @endcode
 */
typedef void (*PFN_BML_TraceBegin)(BML_Context ctx, const char *name, const char *category);

/**
 * @brief Mark the end of a timed scope
 * 
 * @threadsafe Yes
 * 
 * @note Must be paired with bmlTraceBegin()
 */
typedef void (*PFN_BML_TraceEnd)(BML_Context ctx);

/**
 * @brief Mark an instantaneous event
 * 
 * @param[in] name Event name
 * @param[in] category Category for filtering (may be NULL)
 * 
 * @threadsafe Yes
 * 
 * @note Used for events with no duration (e.g., "player spawned")
 */
typedef void (*PFN_BML_TraceInstant)(BML_Context ctx, const char *name, const char *category);

/**
 * @brief Set the name of the current thread
 * 
 * @param[in] name Thread name (copied internally)
 * 
 * @threadsafe Yes
 * 
 * @note Helps identify threads in profiler UI
 */
typedef void (*PFN_BML_TraceSetThreadName)(BML_Context ctx, const char *name);

/**
 * @brief Emit a counter value
 * 
 * @param[in] name Counter name
 * @param[in] value Counter value
 * 
 * @threadsafe Yes
 * 
 * @note Useful for tracking memory usage, FPS, entity count, etc.
 * 
 * @code
 * traceCounter("EntityCount", entity_mgr.count());
 * traceCounter("MemoryMB", allocated_bytes / (1024*1024));
 * @endcode
 */
typedef void (*PFN_BML_TraceCounter)(BML_Context ctx, const char *name, int64_t value);

/**
 * @brief Mark a frame boundary
 * 
 * @threadsafe Yes
 * 
 * @note Call once per frame for frame-based profiling
 */
typedef void (*PFN_BML_TraceFrameMark)(BML_Context ctx);

/* ========== Performance Counters ========== */

/**
 * @brief Get total number of API calls
 * 
 * @param[in] api_name API function name (e.g., "bmlImcPublish", "bmlConfigGet")
 * @return Call count since startup, or 0 if tracking disabled
 * 
 * @threadsafe Yes
 */
typedef uint64_t (*PFN_BML_GetApiCallCount)(BML_Context ctx, const char *api_name);

/**
 * @brief Get total bytes allocated
 * 
 * @return Total allocated bytes across all allocators
 * 
 * @threadsafe Yes
 */
typedef uint64_t (*PFN_BML_GetTotalAllocBytes)(BML_Context ctx);

/**
 * @brief Get high-resolution timestamp
 * 
 * @return Nanoseconds since unspecified epoch
 * 
 * @threadsafe Yes
 * 
 * @note Use for manual timing measurements
 * 
 * @code
 * uint64_t start = getTimestampNs();
 * DoWork();
 * uint64_t end = getTimestampNs();
 * printf("Elapsed: %llu ns\n", end - start);
 * @endcode
 */
typedef uint64_t (*PFN_BML_GetTimestampNs)(BML_Context ctx);

/**
 * @brief Get CPU frequency estimate
 * 
 * @return CPU frequency in Hz, or 0 if unavailable
 * 
 * @threadsafe Yes
 */
typedef uint64_t (*PFN_BML_GetCpuFrequency)(BML_Context ctx);

/* ========== External Profiler Integration ========== */

/**
 * @brief Profiler backend type
 */
typedef enum BML_ProfilerBackend {
    BML_PROFILER_NONE = 0,          /**< No profiler */
    BML_PROFILER_CHROME_TRACING,    /**< Chrome Tracing JSON */
    BML_PROFILER_TRACY,             /**< Tracy profiler */
    BML_PROFILER_RENDERDOC,         /**< RenderDoc markers */
    BML_PROFILER_CUSTOM,            /**< Custom backend */
    _BML_PROFILER_BACKEND_FORCE_32BIT = 0x7FFFFFFF  /**< Force 32-bit enum */
} BML_ProfilerBackend;

/**
 * @brief Get active profiler backend
 * 
 * @return Current profiler backend
 * 
 * @threadsafe Yes
 */
typedef BML_ProfilerBackend (*PFN_BML_GetProfilerBackend)(BML_Context ctx);

/**
 * @brief Enable/disable profiling
 * 
 * @param[in] enable BML_TRUE to enable, BML_FALSE to disable
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe No (call during initialization)
 * 
 * @note When disabled, trace calls become no-ops
 */
typedef BML_Result (*PFN_BML_SetProfilingEnabled)(BML_Context ctx, BML_Bool enable);

/**
 * @brief Check if profiling is enabled
 * 
 * @return BML_TRUE if enabled, BML_FALSE otherwise
 * 
 * @threadsafe Yes
 */
typedef BML_Bool (*PFN_BML_IsProfilingEnabled)(BML_Context ctx);

/**
 * @brief Flush profiling data to disk
 * 
 * @param[in] filename Output filename (may be NULL for default)
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 * 
 * @note For Chrome Tracing backend, writes JSON file
 */
typedef BML_Result (*PFN_BML_FlushProfilingData)(BML_Context ctx, const char *filename);

/* ========== Profiling Statistics ========== */

/**
 * @brief Profiling statistics
 */
typedef struct BML_ProfilingStats {
    size_t struct_size;             /**< sizeof(BML_ProfilingStats), must be first field */
    uint64_t total_events;          /**< Total trace events emitted */
    uint64_t total_scopes;          /**< Total begin/end scope pairs */
    uint64_t active_scopes;         /**< Currently active scopes */
    uint64_t dropped_events;        /**< Events dropped (buffer full) */
    uint64_t memory_used_bytes;     /**< Memory used by profiler */
} BML_ProfilingStats;

/**
 * @def BML_PROFILING_STATS_INIT
 * @brief Static initializer for BML_ProfilingStats
 */
#define BML_PROFILING_STATS_INIT { sizeof(BML_ProfilingStats), 0, 0, 0, 0, 0 }

/**
 * @brief Get profiling statistics
 * 
 * @param[out] out_stats Receives statistics
 * @return BML_RESULT_OK on success
 * 
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_GetProfilingStats)(BML_Context ctx, BML_ProfilingStats *out_stats);

typedef struct BML_CoreProfilingInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_TraceBegin TraceBegin;
    PFN_BML_TraceEnd TraceEnd;
    PFN_BML_TraceInstant TraceInstant;
    PFN_BML_TraceSetThreadName TraceSetThreadName;
    PFN_BML_TraceCounter TraceCounter;
    PFN_BML_TraceFrameMark TraceFrameMark;
    PFN_BML_GetApiCallCount GetApiCallCount;
    PFN_BML_GetTotalAllocBytes GetTotalAllocBytes;
    PFN_BML_GetTimestampNs GetTimestampNs;
    PFN_BML_GetCpuFrequency GetCpuFrequency;
    PFN_BML_GetProfilerBackend GetProfilerBackend;
    PFN_BML_SetProfilingEnabled SetProfilingEnabled;
    PFN_BML_IsProfilingEnabled IsProfilingEnabled;
    PFN_BML_FlushProfilingData FlushProfilingData;
    PFN_BML_GetProfilingStats GetProfilingStats;
} BML_CoreProfilingInterface;


BML_END_CDECLS

/* Compile-time assertions */
#ifdef __cplusplus
#include <cstddef>
static_assert(offsetof(BML_ProfilingStats, struct_size) == 0, "BML_ProfilingStats.struct_size must be at offset 0");
#endif

#endif /* BML_PROFILING_H */
