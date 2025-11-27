#ifndef BML_LOGGING_H
#define BML_LOGGING_H

#include <stdarg.h>

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

typedef enum BML_LogSeverity {
    BML_LOG_TRACE = 0,
    BML_LOG_DEBUG = 1,
    BML_LOG_INFO  = 2,
    BML_LOG_WARN  = 3,
    BML_LOG_ERROR = 4,
    BML_LOG_FATAL = 5,
    _BML_LOG_SEVERITY_FORCE_32BIT = 0x7FFFFFFF  /**< Force 32-bit enum (Task 1.4) */
} BML_LogSeverity;

/**
 * @brief Log a formatted message
 * @threadsafe Yes
 */
typedef void (*PFN_BML_Log)(BML_Context ctx, BML_LogSeverity level, const char *tag, const char *fmt, ...);

/**
 * @brief Log a formatted message with va_list
 * @threadsafe Yes
 */
typedef void (*PFN_BML_LogVa)(BML_Context ctx, BML_LogSeverity level, const char *tag, const char *fmt, va_list args);

/**
 * @brief Set minimum log severity filter
 * @threadsafe No (call from main thread only)
 */
typedef void (*PFN_BML_SetLogFilter)(BML_LogSeverity minimum_level);

typedef enum BML_LogCapabilityFlags {
    BML_LOG_CAP_STRUCTURED_TAGS   = 1u << 0,
    BML_LOG_CAP_VARIADIC          = 1u << 1,
    BML_LOG_CAP_FILTER_OVERRIDE   = 1u << 2,
    BML_LOG_CAP_CONTEXT_ROUTING   = 1u << 3,
    _BML_LOG_CAP_FORCE_32BIT      = 0x7FFFFFFF  /**< Force 32-bit enum (Task 1.4) */
} BML_LogCapabilityFlags;

typedef enum BML_LogCreateFlags {
    BML_LOG_CREATE_ALLOW_TAGS     = 1u << 0,
    BML_LOG_CREATE_ALLOW_FILTER   = 1u << 1,
    _BML_LOG_CREATE_FORCE_32BIT   = 0x7FFFFFFF  /**< Force 32-bit enum (Task 1.4) */
} BML_LogCreateFlags;

#define BML_LOG_SEVERITY_MASK(level) (1u << (level))

/**
 * @brief Logging sink creation descriptor (Task 1.2)
 */
typedef struct BML_LogCreateDesc {
    size_t struct_size;                  /**< sizeof(BML_LogCreateDesc), must be first field */
    BML_Version api_version;             /**< API version */
    BML_LogSeverity default_min_severity; /**< Default minimum severity */
    uint32_t flags;                      /**< BML_LogCreateFlags */
} BML_LogCreateDesc;

/**
 * @def BML_LOG_CREATE_DESC_INIT
 * @brief Static initializer for BML_LogCreateDesc
 */
#define BML_LOG_CREATE_DESC_INIT { sizeof(BML_LogCreateDesc), BML_VERSION_INIT(0,0,0), BML_LOG_INFO, 0 }

/**
 * @brief Logging subsystem capabilities (Task 1.2)
 */
typedef struct BML_LogCaps {
    size_t struct_size;                  /**< sizeof(BML_LogCaps), must be first field */
    BML_Version api_version;             /**< API version */
    uint32_t capability_flags;           /**< Bitmask of BML_LogCapabilityFlags */
    uint32_t supported_severities_mask;  /**< Bitmask of supported BML_LogSeverity values */
    BML_LogCreateDesc default_sink;      /**< Default sink configuration */
    BML_ThreadingModel threading_model;  /**< Threading model of Logging APIs */
} BML_LogCaps;

/**
 * @def BML_LOG_CAPS_INIT
 * @brief Static initializer for BML_LogCaps
 */
#define BML_LOG_CAPS_INIT { sizeof(BML_LogCaps), BML_VERSION_INIT(0,0,0), 0, 0, BML_LOG_CREATE_DESC_INIT, BML_THREADING_SINGLE }

typedef BML_Result (*PFN_BML_GetLoggingCaps)(BML_LogCaps *out_caps);

typedef struct BML_LoggerApi {
    PFN_BML_Log   Log;
    PFN_BML_LogVa LogVa;
    PFN_BML_SetLogFilter SetFilter;
} BML_LoggerApi;

extern PFN_BML_Log   bmlLog;
extern PFN_BML_LogVa bmlLogVa;
extern PFN_BML_SetLogFilter bmlSetLogFilter;
extern PFN_BML_GetLoggingCaps bmlGetLoggingCaps;

BML_END_CDECLS

/* ========================================================================
 * Compile-Time Assertions for ABI Stability
 * ======================================================================== */

#ifdef __cplusplus
#include <cstddef>
#define BML_LOG_OFFSETOF(type, member) offsetof(type, member)
#else
#include <stddef.h>
#define BML_LOG_OFFSETOF(type, member) offsetof(type, member)
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
    #define BML_LOG_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define BML_LOG_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define BML_LOG_STATIC_ASSERT_CONCAT_(a, b) a##b
    #define BML_LOG_STATIC_ASSERT_CONCAT(a, b) BML_LOG_STATIC_ASSERT_CONCAT_(a, b)
    #define BML_LOG_STATIC_ASSERT(cond, msg) \
        typedef char BML_LOG_STATIC_ASSERT_CONCAT(bml_log_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* Verify struct_size is at offset 0 */
BML_LOG_STATIC_ASSERT(BML_LOG_OFFSETOF(BML_LogCaps, struct_size) == 0,
    "BML_LogCaps.struct_size must be at offset 0");

/* Verify enum sizes are 32-bit */
BML_LOG_STATIC_ASSERT(sizeof(BML_LogSeverity) == sizeof(int32_t),
    "BML_LogSeverity must be 32-bit");

#endif /* BML_LOGGING_H */
