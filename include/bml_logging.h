#ifndef BML_LOGGING_H
#define BML_LOGGING_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

typedef enum BML_LogSeverity {
    BML_LOG_TRACE                 = 0,
    BML_LOG_DEBUG                 = 1,
    BML_LOG_INFO                  = 2,
    BML_LOG_WARN                  = 3,
    BML_LOG_ERROR                 = 4,
    BML_LOG_FATAL                 = 5,
    _BML_LOG_SEVERITY_FORCE_32BIT = 0x7FFFFFFF /**< Force 32-bit enum */
} BML_LogSeverity;

/* ========================================================================
 * Log Message Info (for sink override callbacks)
 * ======================================================================== */

/**
 * @brief Log message information passed to sink override callbacks
 */
typedef struct BML_LogMessageInfo {
    size_t struct_size;         /**< sizeof(BML_LogMessageInfo), must be first */
    BML_Version api_version;    /**< API version */
    BML_Mod mod;                /**< Originating mod handle (may be NULL) */
    const char *mod_id;         /**< Originating mod ID (may be NULL) */
    BML_LogSeverity severity;   /**< Log severity level */
    const char *tag;            /**< Log tag (may be NULL) */
    const char *message;        /**< Log message body */
    const char *formatted_line; /**< Fully formatted log line */
} BML_LogMessageInfo;

#define BML_LOG_MESSAGE_INFO_INIT { sizeof(BML_LogMessageInfo), BML_VERSION_INIT(0,0,0), \
    NULL, NULL, BML_LOG_INFO, NULL, NULL, NULL }

/**
 * @brief Log sink dispatch callback type
 * @param ctx BML context
 * @param info Log message information
 * @param user_data User-provided context from registration
 */
typedef void (*BML_LogSinkDispatchFn)(BML_Context ctx, const BML_LogMessageInfo *info, void *user_data);

/**
 * @brief Log sink shutdown callback type
 * @param user_data User-provided context from registration
 */
typedef void (*BML_LogSinkShutdownFn)(void *user_data);

/**
 * @brief Flags for log sink override behavior
 */
typedef enum BML_LogSinkOverrideFlags {
    BML_LOG_SINK_OVERRIDE_SUPPRESS_DEFAULT   = 1u << 0, /**< Don't write to default log after dispatch */
    _BML_LOG_SINK_OVERRIDE_FLAGS_FORCE_32BIT = 0x7FFFFFFF
} BML_LogSinkOverrideFlags;

/**
 * @brief Descriptor for registering a custom log sink override
 */
typedef struct BML_LogSinkOverrideDesc {
    size_t struct_size;                /**< sizeof(BML_LogSinkOverrideDesc), must be first */
    BML_LogSinkDispatchFn dispatch;    /**< Required: called for each log message */
    BML_LogSinkShutdownFn on_shutdown; /**< Optional: called when sink is cleared */
    void *user_data;                   /**< User context passed to callbacks */
    uint32_t flags;                    /**< Bitmask of BML_LogSinkOverrideFlags */
} BML_LogSinkOverrideDesc;

#define BML_LOG_SINK_OVERRIDE_DESC_INIT { sizeof(BML_LogSinkOverrideDesc), NULL, NULL, NULL, 0 }

/**
 * @brief Register a log sink override to capture log messages
 * @param desc Sink override descriptor
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_ALREADY_EXISTS if a sink is already registered
 * @return BML_RESULT_INVALID_ARGUMENT if desc is invalid
 */
typedef BML_Result (*PFN_BML_RegisterLogSinkOverride)(const BML_LogSinkOverrideDesc *desc);

/**
 * @brief Clear the current log sink override
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if no sink is registered
 */
typedef BML_Result (*PFN_BML_ClearLogSinkOverride)(void);

/* ========================================================================
 * Core Logging APIs
 * ======================================================================== */

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
    BML_LOG_CAP_STRUCTURED_TAGS = 1u << 0,
    BML_LOG_CAP_VARIADIC        = 1u << 1,
    BML_LOG_CAP_FILTER_OVERRIDE = 1u << 2,
    BML_LOG_CAP_CONTEXT_ROUTING = 1u << 3,
    _BML_LOG_CAP_FORCE_32BIT    = 0x7FFFFFFF /**< Force 32-bit enum */
} BML_LogCapabilityFlags;

typedef enum BML_LogCreateFlags {
    BML_LOG_CREATE_ALLOW_TAGS   = 1u << 0,
    BML_LOG_CREATE_ALLOW_FILTER = 1u << 1,
    _BML_LOG_CREATE_FORCE_32BIT = 0x7FFFFFFF /**< Force 32-bit enum */
} BML_LogCreateFlags;

#define BML_LOG_SEVERITY_MASK(level) (1u << (level))

/**
 * @brief Logging sink creation descriptor
 */
typedef struct BML_LogCreateDesc {
    size_t struct_size;                   /**< sizeof(BML_LogCreateDesc), must be first field */
    BML_Version api_version;              /**< API version */
    BML_LogSeverity default_min_severity; /**< Default minimum severity */
    uint32_t flags;                       /**< BML_LogCreateFlags */
} BML_LogCreateDesc;

/**
 * @def BML_LOG_CREATE_DESC_INIT
 * @brief Static initializer for BML_LogCreateDesc
 */
#define BML_LOG_CREATE_DESC_INIT { sizeof(BML_LogCreateDesc), BML_VERSION_INIT(0,0,0), BML_LOG_INFO, 0 }

/**
 * @brief Logging subsystem capabilities
 */
typedef struct BML_LogCaps {
    size_t struct_size;                 /**< sizeof(BML_LogCaps), must be first field */
    BML_Version api_version;            /**< API version */
    uint32_t capability_flags;          /**< Bitmask of BML_LogCapabilityFlags */
    uint32_t supported_severities_mask; /**< Bitmask of supported BML_LogSeverity values */
    BML_LogCreateDesc default_sink;     /**< Default sink configuration */
    BML_ThreadingModel threading_model; /**< Threading model of Logging APIs */
} BML_LogCaps;

/**
 * @def BML_LOG_CAPS_INIT
 * @brief Static initializer for BML_LogCaps
 */
#define BML_LOG_CAPS_INIT { sizeof(BML_LogCaps), BML_VERSION_INIT(0,0,0), 0, 0, BML_LOG_CREATE_DESC_INIT, BML_THREADING_SINGLE }

typedef BML_Result (*PFN_BML_GetLoggingCaps)(BML_LogCaps *out_caps);

typedef struct BML_LoggerApi {
    PFN_BML_Log Log;
    PFN_BML_LogVa LogVa;
    PFN_BML_SetLogFilter SetFilter;
} BML_LoggerApi;

extern PFN_BML_Log bmlLog;
extern PFN_BML_LogVa bmlLogVa;
extern PFN_BML_SetLogFilter bmlSetLogFilter;
extern PFN_BML_GetLoggingCaps bmlGetLoggingCaps;
extern PFN_BML_RegisterLogSinkOverride bmlRegisterLogSinkOverride;
extern PFN_BML_ClearLogSinkOverride bmlClearLogSinkOverride;

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
