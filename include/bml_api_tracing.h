#ifndef BML_API_TRACING_H
#define BML_API_TRACING_H

/**
 * @file bml_api_tracing.h
 * @brief API call tracing and debugging utilities
 * 
 * This file provides debugging tools for tracing API calls,
 * measuring performance, and diagnosing issues during development.
 * 
 * Usage:
 *   bmlEnableApiTracing(BML_TRUE);
 *   // ... your code ...
 *   bmlDumpApiStats("api_stats.json");
 * 
 * Output example:
 *   [BML Trace] bmlImcPublish("player.score", 4 bytes) -> OK (23μs)
 * 
 * @see docs/api-improvements/02-usability-enhancements.md
 */

#include "bml_types.h"

BML_BEGIN_CDECLS

/* ========================================================================
 * Callback Types (Task 2.3 - unified signature)
 * ======================================================================== */

/**
 * @brief Trace callback for API call tracing (Task 2.3 - unified signature)
 * 
 * @param[in] ctx BML context (first parameter for consistency)
 * @param[in] api_name Name of the API being called
 * @param[in] args_summary Brief summary of arguments
 * @param[in] result_code Result code returned by the API
 * @param[in] duration_ns Duration in nanoseconds
 * @param[in] user_data User-provided context (always last parameter)
 * 
 * @threadsafe Callbacks may be invoked from any thread
 */
typedef void (*PFN_BML_TraceCallback)(
    BML_Context ctx,
    const char* api_name,
    const char* args_summary,
    int result_code,
    uint64_t duration_ns,
    void* user_data
);

/* ========================================================================
 * API Statistics Structure (Task 1.2)
 * ======================================================================== */

/**
 * @brief API call statistics
 */
typedef struct BML_ApiStats {
    size_t struct_size;           /**< sizeof(BML_ApiStats), must be first field */
    uint32_t api_id;              /**< API identifier */
    const char* api_name;         /**< API name */
    uint64_t call_count;          /**< Total number of calls */
    uint64_t total_time_ns;       /**< Total execution time in nanoseconds */
    uint64_t min_time_ns;         /**< Minimum call duration */
    uint64_t max_time_ns;         /**< Maximum call duration */
    uint64_t error_count;         /**< Number of calls that returned errors */
} BML_ApiStats;

/**
 * @def BML_API_STATS_INIT
 * @brief Static initializer for BML_ApiStats
 */
#define BML_API_STATS_INIT { sizeof(BML_ApiStats), 0, NULL, 0, 0, 0, 0, 0 }

/**
 * @brief Callback for enumerating API statistics (Task 2.3 - unified signature)
 * 
 * @param[in] ctx BML context (first parameter for consistency)
 * @param[in] stats Pointer to statistics for one API
 * @param[in] user_data User-provided context (always last parameter)
 * @return BML_TRUE to continue enumeration, BML_FALSE to stop
 */
typedef BML_Bool (*PFN_BML_StatsEnumerator)(
    BML_Context ctx,
    const BML_ApiStats* stats,
    void* user_data
);

/* ========================================================================
 * Function Pointer Types
 * ======================================================================== */

/**
 * @brief Enable or disable API call tracing
 * @param enable BML_TRUE to enable, BML_FALSE to disable
 * @note This has significant performance overhead; use only for debugging.
 */
typedef void (*PFN_BML_EnableApiTracing)(BML_Bool enable);

/**
 * @brief Check if API tracing is enabled
 * @return BML_TRUE if tracing is enabled
 */
typedef BML_Bool (*PFN_BML_IsApiTracingEnabled)(void);

/**
 * @brief Set tracing output callback
 * @param callback Function to call for each traced API call
 * @param user_data User context passed to callback
 */
typedef void (*PFN_BML_SetTraceCallback)(PFN_BML_TraceCallback callback, void* user_data);

/**
 * @brief Get statistics for a specific API
 * @param api_id API ID to query
 * @param out_stats Pointer to receive statistics
 * @return BML_TRUE if found, BML_FALSE otherwise
 */
typedef BML_Bool (*PFN_BML_GetApiStats)(uint32_t api_id, BML_ApiStats* out_stats);

/**
 * @brief Enumerate all API statistics
 * @param callback Function called for each API
 * @param user_data User context
 */
typedef void (*PFN_BML_EnumerateApiStats)(PFN_BML_StatsEnumerator callback, void* user_data);

/**
 * @brief Dump API statistics to a JSON file
 * @param output_file Path to output file
 * @return BML_TRUE on success
 */
typedef BML_Bool (*PFN_BML_DumpApiStats)(const char* output_file);

/**
 * @brief Reset all API statistics
 */
typedef void (*PFN_BML_ResetApiStats)(void);

/**
 * @brief Validate an API ID at runtime
 * @param api_id API ID to validate
 * @param context Description of the context (for error messages)
 * @return BML_TRUE if valid
 */
typedef BML_Bool (*PFN_BML_ValidateApiId)(uint32_t api_id, const char* context);

/* ========================================================================
 * Global Function Pointers
 * ======================================================================== */

extern PFN_BML_EnableApiTracing    bmlEnableApiTracing;
extern PFN_BML_IsApiTracingEnabled bmlIsApiTracingEnabled;
extern PFN_BML_SetTraceCallback    bmlSetTraceCallback;
extern PFN_BML_GetApiStats         bmlGetApiStats;
extern PFN_BML_EnumerateApiStats   bmlEnumerateApiStats;
extern PFN_BML_DumpApiStats        bmlDumpApiStats;
extern PFN_BML_ResetApiStats       bmlResetApiStats;
extern PFN_BML_ValidateApiId       bmlValidateApiId;

/* ========================================================================
 * Debug Macros
 * ======================================================================== */

/**
 * @brief Debug mode check macro
 * In debug builds, validates API ID and logs warning if invalid.
 */
#ifdef BML_DEBUG
#define BML_API_ID_VALIDATE(id, ctx) \
    do { if (bmlValidateApiId) bmlValidateApiId(id, ctx); } while(0)
#else
#define BML_API_ID_VALIDATE(id, ctx) ((void)0)
#endif

BML_END_CDECLS

#endif /* BML_API_TRACING_H */
