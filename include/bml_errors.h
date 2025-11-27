#ifndef BML_ERRORS_H
#define BML_ERRORS_H

/**
 * @file bml_errors.h
 * @brief Unified error handling, diagnostics, and result codes for BML APIs
 * 
 * This file consolidates all error-related functionality:
 * - Error code definitions (BML_Result values)
 * - Error checking macros (BML_SUCCEEDED, BML_FAILED, BML_CHECK, etc.)
 * - Error retrieval APIs (bmlGetLastError, bmlGetErrorString)
 * - Bootstrap diagnostics structures
 * - C++ exception support
 * 
 * @since 0.4.0
 */

#include "bml_types.h"

BML_BEGIN_CDECLS

/* ========================================================================
 * Result Type
 * ======================================================================== */

typedef int32_t BML_Result;

/**
 * @defgroup ErrorCodes Error Codes
 * @brief Standardized error codes for all BML APIs
 * 
 * All BML APIs return BML_Result to indicate success or failure.
 * Use BML_SUCCEEDED() and BML_FAILED() macros for result checking.
 * Use bmlGetLastError() to retrieve detailed error information.
 * 
 * Error code ranges:
 *   -   0       : Success
 *   -  -1 to  -99: Generic errors
 *   - -100 to -199: Config API errors
 *   - -200 to -299: Extension API errors
 *   - -300 to -399: IMC (Inter-Mod Communication) errors
 *   - -400 to -499: Resource API errors
 *   - -500 to -599: Logging API errors
 *   - -600 to -699: Sync API errors
 * 
 * @{
 */

/* ========================================================================
 * Result Checking Macros
 * ======================================================================== */

/**
 * @def BML_SUCCEEDED
 * @brief Check if a BML_Result indicates success (>= 0)
 */
#define BML_SUCCEEDED(result) ((result) >= 0)

/**
 * @def BML_FAILED
 * @brief Check if a BML_Result indicates failure (< 0)
 */
#define BML_FAILED(result) ((result) < 0)

/**
 * @def BML_CHECK
 * @brief Check BML result and return immediately on failure
 * 
 * @code
 * BML_Result MyFunction() {
 *     BML_CHECK(SomeApiCall());
 *     BML_CHECK(AnotherApiCall());
 *     return BML_RESULT_OK;
 * }
 * @endcode
 */
#define BML_CHECK(expr) do { \
    BML_Result _bml_check_result_ = (expr); \
    if (BML_FAILED(_bml_check_result_)) return _bml_check_result_; \
} while(0)

/**
 * @def BML_CHECK_OR
 * @brief Check BML result and execute custom action on failure
 * 
 * @code
 * BML_CHECK_OR(SomeApiCall(), return nullptr);
 * BML_CHECK_OR(AnotherApiCall(), { cleanup(); return -1; });
 * @endcode
 */
#define BML_CHECK_OR(expr, action) do { \
    BML_Result _bml_check_result_ = (expr); \
    if (BML_FAILED(_bml_check_result_)) { action; } \
} while(0)

/**
 * @def BML_CHECK_PTR
 * @brief Check pointer and return BML_RESULT_INVALID_ARGUMENT if NULL
 */
#define BML_CHECK_PTR(ptr) do { \
    if ((ptr) == NULL) return BML_RESULT_INVALID_ARGUMENT; \
} while(0)

/**
 * @def BML_CHECK_SIZE
 * @brief Check struct_size field for ABI compatibility
 * 
 * @code
 * BML_Result MyApi(const BML_SomeStruct* s) {
 *     BML_CHECK_SIZE(s, BML_SomeStruct);
 *     // ...
 * }
 * @endcode
 */
#define BML_CHECK_SIZE(ptr, type) do { \
    if ((ptr) == NULL || (ptr)->struct_size < sizeof(type)) \
        return BML_RESULT_INVALID_SIZE; \
} while(0)

/* ========================================================================
 * Error Code Definitions
 * ======================================================================== */

/* ========== Generic Error Codes (-1 to -99) ========== */
enum {
    BML_RESULT_OK = 0,                      /**< Success */
    BML_RESULT_FAIL = -1,                   /**< Generic failure */
    BML_RESULT_INVALID_ARGUMENT = -2,       /**< Invalid function argument */
    BML_RESULT_INVALID_STATE = -3,          /**< Operation invalid in current state */
    BML_RESULT_INVALID_CONTEXT = -4,        /**< Invalid context handle */
    BML_RESULT_NOT_FOUND = -5,              /**< Requested item not found */
    BML_RESULT_OUT_OF_MEMORY = -6,          /**< Memory allocation failed */
    BML_RESULT_NOT_SUPPORTED = -7,          /**< Operation not supported */
    BML_RESULT_TIMEOUT = -8,                /**< Operation timed out */
    BML_RESULT_WOULD_BLOCK = -9,            /**< Operation would block */
    BML_RESULT_ALREADY_EXISTS = -10,        /**< Item already exists */
    BML_RESULT_VERSION_MISMATCH = -11,      /**< Version mismatch */
    BML_RESULT_PERMISSION_DENIED = -12,     /**< Permission denied */
    BML_RESULT_IO_ERROR = -13,              /**< I/O operation failed */
    BML_RESULT_UNSUPPORTED = -14,           /**< Feature not supported */
    BML_RESULT_UNKNOWN_ERROR = -15,         /**< Unknown error occurred */
    BML_RESULT_INVALID_SIZE = -16,          /**< Invalid struct_size field */
    BML_RESULT_BUFFER_TOO_SMALL = -17,      /**< Buffer too small for result */
    BML_RESULT_INVALID_HANDLE = -18,        /**< Invalid handle (NULL or already released) */
    BML_RESULT_NOT_INITIALIZED = -19,       /**< Subsystem not initialized */
    BML_RESULT_ALREADY_INITIALIZED = -20,   /**< Subsystem already initialized */
    BML_RESULT_INTERNAL_ERROR = -21,        /**< Unexpected internal failure */
    
    /* ========== Config API Errors (-100 to -199) ========== */
    BML_RESULT_CONFIG_KEY_NOT_FOUND = -100,     /**< Config key does not exist */
    BML_RESULT_CONFIG_TYPE_MISMATCH = -101,     /**< Config value type mismatch */
    BML_RESULT_CONFIG_READ_ONLY = -102,         /**< Config key is read-only */
    BML_RESULT_CONFIG_INVALID_CATEGORY = -103,  /**< Invalid config category */
    BML_RESULT_CONFIG_INVALID_NAME = -104,      /**< Invalid config key name */
    BML_RESULT_CONFIG_VALUE_OUT_OF_RANGE = -105, /**< Config value out of valid range */
    
    /* ========== Extension API Errors (-200 to -299) ========== */
    BML_RESULT_EXTENSION_NOT_FOUND = -200,          /**< Extension not registered */
    BML_RESULT_EXTENSION_VERSION_TOO_OLD = -201,    /**< Extension version too old */
    BML_RESULT_EXTENSION_VERSION_TOO_NEW = -202,    /**< Extension version too new */
    BML_RESULT_EXTENSION_INCOMPATIBLE = -203,       /**< Extension incompatible (major version mismatch) */
    BML_RESULT_EXTENSION_ALREADY_REGISTERED = -204, /**< Extension name already in use */
    BML_RESULT_EXTENSION_INVALID_NAME = -205,       /**< Invalid extension name format */
    
    /* ========== IMC (Inter-Mod Communication) Errors (-300 to -399) ========== */
    BML_RESULT_IMC_QUEUE_FULL = -300,           /**< IMC message queue is full */
    BML_RESULT_IMC_NO_SUBSCRIBERS = -301,       /**< No subscribers for topic */
    BML_RESULT_IMC_INVALID_TOPIC = -302,        /**< Invalid topic name format */
    BML_RESULT_IMC_RPC_NOT_REGISTERED = -303,   /**< RPC handler not registered */
    BML_RESULT_IMC_RPC_ALREADY_REGISTERED = -304, /**< RPC handler already exists */
    BML_RESULT_IMC_FUTURE_CANCELLED = -305,     /**< Future was cancelled */
    BML_RESULT_IMC_FUTURE_FAILED = -306,        /**< Future failed to complete */
    BML_RESULT_IMC_SUBSCRIPTION_CLOSED = -307,  /**< Subscription already closed */
    
    /* ========== Resource API Errors (-400 to -499) ========== */
    BML_RESULT_RESOURCE_INVALID_HANDLE = -400,      /**< Invalid resource handle */
    BML_RESULT_RESOURCE_HANDLE_EXPIRED = -401,      /**< Resource handle expired (generation mismatch) */
    BML_RESULT_RESOURCE_TYPE_NOT_REGISTERED = -402, /**< Resource type not registered */
    BML_RESULT_RESOURCE_SLOT_EXHAUSTED = -403,      /**< No more resource slots available */
    
    /* ========== Logging API Errors (-500 to -599) ========== */
    BML_RESULT_LOG_INVALID_SEVERITY = -500,     /**< Invalid log severity level */
    BML_RESULT_LOG_SINK_UNAVAILABLE = -501,     /**< Log sink not available */
    BML_RESULT_LOG_FILTER_REJECTED = -502,      /**< Message rejected by filter */

    /* ========== Sync API Errors (-600 to -699) ========== */
    BML_RESULT_SYNC_DEADLOCK = -600,            /**< Deadlock detected */
    BML_RESULT_SYNC_INVALID_HANDLE = -601,      /**< Invalid synchronization handle */
    BML_RESULT_SYNC_NOT_OWNER = -602            /**< Current thread doesn't own the lock */
};

/** @} */ /* end of ErrorCodes group */

/* ========================================================================
 * Compile-Time Assertions
 * ======================================================================== */

/**
 * @def BML_STATIC_ASSERT
 * @brief Compile-time assertion (C11/C++11 compatible)
 */
#if defined(__cplusplus) && __cplusplus >= 201103L
    #define BML_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define BML_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    /* Fallback for older compilers */
    #define BML_STATIC_ASSERT_CONCAT_(a, b) a##b
    #define BML_STATIC_ASSERT_CONCAT(a, b) BML_STATIC_ASSERT_CONCAT_(a, b)
    #define BML_STATIC_ASSERT(cond, msg) \
        typedef char BML_STATIC_ASSERT_CONCAT(bml_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/**
 * @def BML_ASSERT_STRUCT_SIZE_FIRST
 * @brief Compile-time check that struct_size is the first field (offset 0)
 * 
 * @code
 * BML_ASSERT_STRUCT_SIZE_FIRST(BML_Version);
 * BML_ASSERT_STRUCT_SIZE_FIRST(BML_ErrorInfo);
 * @endcode
 */
#ifdef __cplusplus
    #include <cstddef>
    #define BML_ASSERT_STRUCT_SIZE_FIRST(type) \
        BML_STATIC_ASSERT(offsetof(type, struct_size) == 0, \
                          #type ".struct_size must be at offset 0")
#else
    #include <stddef.h>
    #define BML_ASSERT_STRUCT_SIZE_FIRST(type) \
        BML_STATIC_ASSERT(offsetof(type, struct_size) == 0, \
                          #type ".struct_size must be at offset 0")
#endif

/**
 * @def BML_ASSERT_ENUM_32BIT
 * @brief Compile-time check that enum is exactly 32 bits
 */
#define BML_ASSERT_ENUM_32BIT(type) \
    BML_STATIC_ASSERT(sizeof(type) == sizeof(int32_t), \
                      #type " must be 32-bit")

/**
 * @def BML_API_ID_CHECK
 * @brief Compile-time check for valid API ID range
 * 
 * @code
 * BML_API_ID_CHECK(BML_API_ID_bmlImcPublish);
 * @endcode
 */
#define BML_API_ID_CHECK(id) \
    BML_STATIC_ASSERT((id) > 0 && (id) < 10000, \
                      "Invalid API ID: must be in range [1, 9999]")

/* ========================================================================
 * Bootstrap Diagnostics (for mod loading errors)
 * ======================================================================== */

/**
 * @brief Manifest parsing error details
 */
typedef struct BML_BootstrapManifestError {
    const char *message;        /**< Error message */
    const char *file;           /**< File path (if available) */
    int32_t line;               /**< Line number (if available) */
    int32_t column;             /**< Column number (if available) */
    uint8_t has_file;           /**< Non-zero if file is valid */
    uint8_t has_line;           /**< Non-zero if line is valid */
    uint8_t has_column;         /**< Non-zero if column is valid */
    uint8_t reserved_;          /**< Padding for alignment */
} BML_BootstrapManifestError;

/**
 * @brief Dependency resolution error details
 */
typedef struct BML_BootstrapDependencyError {
    const char *message;        /**< Error message */
    const char **chain;         /**< Dependency chain leading to error */
    uint32_t chain_count;       /**< Number of items in chain */
} BML_BootstrapDependencyError;

/**
 * @brief Module loading error details
 */
typedef struct BML_BootstrapLoadError {
    const char *module_id;      /**< Module ID that failed to load */
    const char *path_utf8;      /**< Path to the module (UTF-8) */
    const char *message;        /**< Error message */
    int32_t system_code;        /**< OS error code */
    uint8_t has_error;          /**< Non-zero if error occurred */
    uint8_t reserved_[3];       /**< Padding for alignment */
} BML_BootstrapLoadError;

/**
 * @brief Complete bootstrap diagnostics
 * 
 * Returned by bmlAttach() to provide detailed error information
 * when mod loading fails.
 */
typedef struct BML_BootstrapDiagnostics {
    const BML_BootstrapManifestError *manifest_errors;  /**< Array of manifest errors */
    uint32_t manifest_error_count;                      /**< Number of manifest errors */
    BML_BootstrapDependencyError dependency_error;      /**< Dependency resolution error */
    BML_BootstrapLoadError load_error;                  /**< Module load error */
    const char **load_order;                            /**< Resolved load order (for debugging) */
    uint32_t load_order_count;                          /**< Number of modules in load order */
} BML_BootstrapDiagnostics;

/* ========================================================================
 * Error Handling API
 * ======================================================================== */

/**
 * @brief Get detailed error information from the last failed API call
 * 
 * Error information is stored in thread-local storage and remains valid
 * until the next BML API call on the same thread.
 * 
 * @param[out] out_info Pointer to receive error information
 *                      Must have struct_size initialized to sizeof(BML_ErrorInfo)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if out_info is NULL
 * @return BML_RESULT_INVALID_SIZE if out_info->struct_size is invalid
 * @return BML_RESULT_NOT_FOUND if no error information is available
 * 
 * @threadsafe Yes (uses thread-local storage)
 * @since 0.4.0
 * 
 * @code
 * BML_Result result = bmlSomeApi(...);
 * if (BML_FAILED(result)) {
 *     BML_ErrorInfo info = BML_ERROR_INFO_INIT;
 *     if (bmlGetLastError(&info) == BML_RESULT_OK) {
 *         printf("Error: %s (code %d)\n", info.message, info.result_code);
 *     }
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_GetLastError)(BML_ErrorInfo* out_info);

/**
 * @brief Clear the last error information for the current thread
 * 
 * @threadsafe Yes (uses thread-local storage)
 * @since 0.4.0
 */
typedef void (*PFN_BML_ClearLastError)(void);

/**
 * @brief Convert a BML_Result code to a human-readable string
 * 
 * @param[in] result The result code to convert
 * @return Static string describing the error (never NULL)
 * 
 * @threadsafe Yes (returns static strings)
 * @since 0.4.0
 * 
 * @code
 * BML_Result result = bmlSomeApi(...);
 * printf("Result: %s\n", bmlGetErrorString(result));
 * @endcode
 */
typedef const char* (*PFN_BML_GetErrorString)(BML_Result result);

/* Global function pointers */
extern PFN_BML_GetLastError     bmlGetLastError;
extern PFN_BML_ClearLastError   bmlClearLastError;
extern PFN_BML_GetErrorString   bmlGetErrorString;

BML_END_CDECLS

/* ========================================================================
 * C++ Exception Support
 * ======================================================================== */

#ifdef __cplusplus

#include <stdexcept>
#include <string>
#include <optional>

namespace bml {

/**
 * @brief Exception class for BML errors
 * 
 * Wraps a BML_Result code with optional context message.
 * 
 * @code
 * try {
 *     bml::checked([&]() { return bmlSomeApiCall(); });
 * } catch (const bml::Exception& e) {
 *     std::cerr << "Error: " << e.what() << " (code: " << e.code() << ")\n";
 * }
 * @endcode
 */
class Exception : public std::runtime_error {
public:
    explicit Exception(BML_Result code)
        : std::runtime_error(FormatMessage(code, nullptr))
        , m_code(code) {}
    
    Exception(BML_Result code, const char* context)
        : std::runtime_error(FormatMessage(code, context))
        , m_code(code) {}
    
    /** @brief Get the BML_Result code */
    BML_Result code() const noexcept { return m_code; }
    
private:
    BML_Result m_code;
    
    static std::string FormatMessage(BML_Result code, const char* context) {
        std::string msg = "BML error " + std::to_string(code);
        if (bmlGetErrorString) {
            msg += " (";
            msg += bmlGetErrorString(code);
            msg += ")";
        }
        if (context) {
            msg += ": ";
            msg += context;
        }
        return msg;
    }
};

/**
 * @brief Execute a function and throw on error
 * 
 * @code
 * bml::checked([&]() { return bmlSomeApiCall(); });
 * @endcode
 */
template<typename Fn>
inline void checked(Fn&& fn) {
    BML_Result r = fn();
    if (BML_FAILED(r)) {
        throw Exception(r);
    }
}

/**
 * @brief Execute a function and throw with context on error
 * 
 * @code
 * bml::checked([&]() { return bmlSomeApiCall(); }, "Failed to call SomeApi");
 * @endcode
 */
template<typename Fn>
inline void checked(Fn&& fn, const char* context) {
    BML_Result r = fn();
    if (BML_FAILED(r)) {
        throw Exception(r, context);
    }
}

/**
 * @brief Get last error info as optional
 * 
 * @return std::optional<BML_ErrorInfo> containing error info, or nullopt if unavailable
 */
inline std::optional<BML_ErrorInfo> GetLastErrorInfo() {
    if (!bmlGetLastError) return std::nullopt;
    BML_ErrorInfo info = BML_ERROR_INFO_INIT;
    if (bmlGetLastError(&info) == BML_RESULT_OK) {
        return info;
    }
    return std::nullopt;
}

/**
 * @brief Check macro for BML API calls that throws on failure
 * 
 * @code
 * void MyFunction() {
 *     BML_CPP_CHECK(bmlSomeApi());
 *     BML_CPP_CHECK(bmlOtherApi());
 * }
 * @endcode
 */
#define BML_CPP_CHECK(expr) do { \
    BML_Result _r = (expr); \
    if (BML_FAILED(_r)) { \
        throw bml::Exception(_r, #expr " failed"); \
    } \
} while(0)

} // namespace bml

#endif /* __cplusplus */

#endif /* BML_ERRORS_H */
