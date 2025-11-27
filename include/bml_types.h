#ifndef BML_TYPES_H
#define BML_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifndef BML_BEGIN_CDECLS
#ifdef __cplusplus
#define BML_BEGIN_CDECLS extern "C" {
#else
#define BML_BEGIN_CDECLS
#endif
#endif // !BML_BEGIN_CDECLS

#ifndef BML_END_CDECLS
#ifdef __cplusplus
#define BML_END_CDECLS }
#else
#define BML_END_CDECLS
#endif
#endif // !BML_END_CDECLS

BML_BEGIN_CDECLS

/* ========================================================================
 * Type-Safe Handle Declaration Macro (Task 1.3)
 * ======================================================================== */

/**
 * @def BML_DECLARE_HANDLE
 * @brief Declares a type-safe opaque handle type
 * 
 * In C++: Uses forward-declared struct pointers for compile-time type checking.
 * In C: Uses incomplete struct types for type safety.
 * 
 * This provides compile-time detection of handle type mismatches, preventing
 * bugs like passing a BML_Mutex to a function expecting BML_RwLock.
 * 
 * @param name The handle type name (without trailing _T)
 * 
 * @example
 * BML_DECLARE_HANDLE(BML_MyHandle)
 * // Creates: typedef struct BML_MyHandle_T* BML_MyHandle;
 */
#ifdef __cplusplus
    #define BML_DECLARE_HANDLE(name) \
        struct name##_T; \
        typedef struct name##_T* name
#else
    #define BML_DECLARE_HANDLE(name) \
        typedef struct name##_T { int _unused; } *name
#endif

/* ========================================================================
 * Thread Safety Annotation Macros (Task 1.5)
 * ======================================================================== */

/**
 * @def BML_THREADSAFE
 * @brief Indicates the function is safe to call from multiple threads concurrently.
 * 
 * Functions marked with this attribute guarantee thread-safety through internal
 * synchronization or lock-free algorithms.
 */
#define BML_THREADSAFE

/**
 * @def BML_NOT_THREADSAFE
 * @brief Indicates the function is NOT safe to call from multiple threads.
 * 
 * Callers must provide external synchronization when using these functions
 * from multiple threads.
 */
#define BML_NOT_THREADSAFE

/**
 * @def BML_MAIN_THREAD_ONLY
 * @brief Indicates the function must only be called from the main thread.
 * 
 * Typically used for UI, rendering, or Virtools SDK operations.
 */
#define BML_MAIN_THREAD_ONLY

/* ========================================================================
 * Opaque Handle Type Declarations (Task 1.3)
 * ======================================================================== */

/**
 * @brief Core BML context handle
 * @threadsafe Reference counting is thread-safe
 */
BML_DECLARE_HANDLE(BML_Context);

/**
 * @brief Mod instance handle
 * @threadsafe Read operations are thread-safe
 */
BML_DECLARE_HANDLE(BML_Mod);

/**
 * @brief IMC subscription handle
 * @threadsafe All operations are thread-safe
 */
BML_DECLARE_HANDLE(BML_Subscription);

/**
 * @brief Async future handle
 * @threadsafe All operations are thread-safe
 */
BML_DECLARE_HANDLE(BML_Future);

/**
 * @brief Zero-copy buffer handle
 * @threadsafe Reference counting is thread-safe
 */
BML_DECLARE_HANDLE(BML_Buffer);

/* ========================================================================
 * Basic Types
 * ======================================================================== */

typedef uint32_t BML_Bool;
#define BML_FALSE 0u
#define BML_TRUE  1u

/**
 * @brief Threading model for API functions
 */
typedef enum BML_ThreadingModel {
    BML_THREADING_SINGLE = 0,      /**< Single-threaded only */
    BML_THREADING_APARTMENT = 1,   /**< Thread-affinity required */
    BML_THREADING_FREE = 2,        /**< Fully thread-safe */
    _BML_THREADING_MODEL_FORCE_32BIT = 0x7FFFFFFF  /**< Force 32-bit enum (Task 1.4) */
} BML_ThreadingModel;

/* ========================================================================
 * Version Structure (Task 1.2)
 * ======================================================================== */

/**
 * @brief Version information structure
 * 
 * Uses semantic versioning (MAJOR.MINOR.PATCH).
 * struct_size field enables ABI-safe extension in future versions.
 */
typedef struct BML_Version {
    size_t struct_size;    /**< sizeof(BML_Version), must be first field */
    uint16_t major;        /**< Major version (breaking changes) */
    uint16_t minor;        /**< Minor version (backward-compatible additions) */
    uint16_t patch;        /**< Patch version (bug fixes) */
    uint16_t reserved;     /**< Reserved for future use, set to 0 */
} BML_Version;

/**
 * @def BML_VERSION_INIT
 * @brief Static initializer for BML_Version
 * @param maj Major version
 * @param min Minor version
 * @param pat Patch version
 */
#define BML_VERSION_INIT(maj, min, pat) { sizeof(BML_Version), (maj), (min), (pat), 0 }

/**
 * @brief Create a version structure at runtime
 * @param major Major version number
 * @param minor Minor version number
 * @param patch Patch version number
 * @return Initialized BML_Version structure
 */
static inline BML_Version bmlMakeVersion(uint16_t major, uint16_t minor, uint16_t patch) {
    BML_Version v;
    v.struct_size = sizeof(BML_Version);
    v.major = major;
    v.minor = minor;
    v.patch = patch;
    v.reserved = 0;
    return v;
}

/**
 * @brief Convert version to packed 32-bit integer for comparison
 * @param version Pointer to version structure
 * @return Packed version as uint32_t (MAJOR << 16 | MINOR << 8 | PATCH)
 */
static inline uint32_t bmlVersionToUint(const BML_Version *version) {
    return ((uint32_t)version->major << 16) | ((uint32_t)version->minor << 8) | ((uint32_t)version->patch);
}

/* ========================================================================
 * Error Information Structure (Task 1.1)
 * ======================================================================== */

/**
 * @brief Extended error information
 * 
 * Provides detailed error context beyond the basic BML_Result code.
 * Retrieved via bmlGetLastError() after an API call returns an error.
 */
typedef struct BML_ErrorInfo {
    size_t struct_size;        /**< sizeof(BML_ErrorInfo), must be first field */
    int32_t result_code;       /**< The BML_Result error code */
    const char* message;       /**< Human-readable error message (may be NULL) */
    const char* source_file;   /**< Source file where error occurred (may be NULL) */
    int32_t source_line;       /**< Source line number (0 if unknown) */
    const char* api_name;      /**< Name of the API that failed (may be NULL) */
} BML_ErrorInfo;

/**
 * @def BML_ERROR_INFO_INIT
 * @brief Static initializer for BML_ErrorInfo
 */
#define BML_ERROR_INFO_INIT { sizeof(BML_ErrorInfo), 0, NULL, NULL, 0, NULL }

BML_END_CDECLS

/* ========================================================================
 * Compile-Time Assertions for ABI Stability
 * ======================================================================== */

/*
 * These assertions verify critical ABI invariants:
 * 1. struct_size field is always at offset 0 (enables version extension)
 * 2. Enums are always 32-bit (prevents ABI issues across compilers)
 * 
 * Any violation will cause a compile-time error.
 */

#ifdef __cplusplus
#include <cstddef>
#define BML_TYPES_OFFSETOF(type, member) offsetof(type, member)
#else
#include <stddef.h>
#define BML_TYPES_OFFSETOF(type, member) offsetof(type, member)
#endif

/* C11 / C++11 static_assert */
#if defined(__cplusplus) && __cplusplus >= 201103L
    #define BML_TYPES_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define BML_TYPES_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    /* Fallback: typedef trick */
    #define BML_TYPES_STATIC_ASSERT_CONCAT_(a, b) a##b
    #define BML_TYPES_STATIC_ASSERT_CONCAT(a, b) BML_TYPES_STATIC_ASSERT_CONCAT_(a, b)
    #define BML_TYPES_STATIC_ASSERT(cond, msg) \
        typedef char BML_TYPES_STATIC_ASSERT_CONCAT(bml_types_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* Verify struct_size is at offset 0 for all extensible structures */
BML_TYPES_STATIC_ASSERT(BML_TYPES_OFFSETOF(BML_Version, struct_size) == 0,
    "BML_Version.struct_size must be at offset 0");
BML_TYPES_STATIC_ASSERT(BML_TYPES_OFFSETOF(BML_ErrorInfo, struct_size) == 0,
    "BML_ErrorInfo.struct_size must be at offset 0");

/* Verify enum sizes are 32-bit for ABI stability */
BML_TYPES_STATIC_ASSERT(sizeof(BML_ThreadingModel) == sizeof(int32_t),
    "BML_ThreadingModel must be 32-bit");

#endif /* BML_TYPES_H */
