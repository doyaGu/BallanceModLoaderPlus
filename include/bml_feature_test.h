#ifndef BML_FEATURE_TEST_H
#define BML_FEATURE_TEST_H

/**
 * @file bml_feature_test.h
 * @brief Compile-time and runtime feature detection macros
 * 
 * This file provides macros for detecting BML features at both
 * compile-time (based on header version) and runtime (based on
 * loaded BML version).
 * 
 * Usage:
 *   // Compile-time check
 *   #if BML_HAS_FEATURE_IMC_RPC
 *       // Use RPC in code
 *   #endif
 *   
 *   // Runtime check
 *   if (BML_RUNTIME_HAS(BML_CAP_IMC_RPC)) {
 *       // Use RPC
 *   }
 * 
 * @see docs/api-improvements/03-feature-extensions.md
 */

#include "bml_version.h"
#include "bml_capabilities.h"

/* ========================================================================
 * Version Encoding
 * ======================================================================== */

/**
 * @brief Encode version components into single uint32_t
 */
#define BML_VERSION_ENCODE(major, minor, patch) \
    (((uint32_t)(major) << 16) | ((uint32_t)(minor) << 8) | (uint32_t)(patch))

/**
 * @brief Current BML version (compile-time)
 */
#define BML_COMPILED_VERSION \
    BML_VERSION_ENCODE(BML_MAJOR_VERSION, BML_MINOR_VERSION, BML_PATCH_VERSION)

/* ========================================================================
 * Compile-Time Feature Detection
 * ======================================================================== */

/* v0.4.0 features */
#if BML_COMPILED_VERSION >= BML_VERSION_ENCODE(0, 4, 0)
#define BML_HAS_FEATURE_IMC_BASIC      1
#define BML_HAS_FEATURE_CONFIG         1
#define BML_HAS_FEATURE_LOGGING        1
#define BML_HAS_FEATURE_EXTENSION      1
#define BML_HAS_FEATURE_SYNC           1
#else
#define BML_HAS_FEATURE_IMC_BASIC      0
#define BML_HAS_FEATURE_CONFIG         0
#define BML_HAS_FEATURE_LOGGING        0
#define BML_HAS_FEATURE_EXTENSION      0
#define BML_HAS_FEATURE_SYNC           0
#endif

/* v0.5.0 features (capability system) */
#if BML_COMPILED_VERSION >= BML_VERSION_ENCODE(0, 5, 0)
#define BML_HAS_FEATURE_CAPABILITY_API 1
#define BML_HAS_FEATURE_API_DISCOVERY  1
#define BML_HAS_FEATURE_UNIFIED_EXT    1
#define BML_HAS_FEATURE_API_TRACING    1
#define BML_HAS_FEATURE_DIRECT_INDEX   1
#else
#define BML_HAS_FEATURE_CAPABILITY_API 0
#define BML_HAS_FEATURE_API_DISCOVERY  0
#define BML_HAS_FEATURE_UNIFIED_EXT    0
#define BML_HAS_FEATURE_API_TRACING    0
#define BML_HAS_FEATURE_DIRECT_INDEX   0
#endif

/* Future features (placeholder) */
#if BML_COMPILED_VERSION >= BML_VERSION_ENCODE(0, 6, 0)
#define BML_HAS_FEATURE_SECURITY       1
#define BML_HAS_FEATURE_SANDBOX        1
#else
#define BML_HAS_FEATURE_SECURITY       0
#define BML_HAS_FEATURE_SANDBOX        0
#endif

/* ========================================================================
 * Runtime Feature Detection Macros
 * ======================================================================== */

/**
 * @brief Check runtime capability (requires capability API)
 */
#if BML_HAS_FEATURE_CAPABILITY_API
#define BML_RUNTIME_HAS(cap) (bmlHasCapability(cap) != BML_FALSE)
#else
#define BML_RUNTIME_HAS(cap) 0
#endif

/**
 * @brief Require a feature or return error
 * 
 * Usage in function:
 *   BML_REQUIRE_FEATURE(BML_CAP_IMC_RPC);
 */
#define BML_REQUIRE_FEATURE(cap) do { \
    if (!BML_RUNTIME_HAS(cap)) { \
        return BML_RESULT_NOT_SUPPORTED; \
    } \
} while(0)

/**
 * @brief Require a feature or execute fallback code
 * 
 * Usage:
 *   BML_REQUIRE_FEATURE_OR(BML_CAP_IMC_RPC, {
 *       // Fallback code when RPC not available
 *       UseBasicImc();
 *   });
 */
#define BML_REQUIRE_FEATURE_OR(cap, fallback) do { \
    if (!BML_RUNTIME_HAS(cap)) { \
        fallback; \
    } \
} while(0)

/* ========================================================================
 * Deprecation Helpers
 * ======================================================================== */

/**
 * @brief Mark APIs as deprecated from a specific version
 */
#if defined(__GNUC__) || defined(__clang__)
#define BML_DEPRECATED_SINCE(ver, msg) \
    __attribute__((deprecated("Deprecated since v" #ver ": " msg)))
#elif defined(_MSC_VER)
#define BML_DEPRECATED_SINCE(ver, msg) \
    __declspec(deprecated("Deprecated since v" #ver ": " msg))
#else
#define BML_DEPRECATED_SINCE(ver, msg)
#endif

/**
 * @brief Mark API as removed in a future version
 */
#define BML_WILL_REMOVE_IN(ver, replacement) \
    BML_DEPRECATED_SINCE(ver, "Will be removed. Use " replacement " instead")

/* ========================================================================
 * Version Comparison Helpers
 * ======================================================================== */

/**
 * @brief Check if compiled against at least version X.Y.Z
 */
#define BML_VERSION_AT_LEAST(major, minor, patch) \
    (BML_COMPILED_VERSION >= BML_VERSION_ENCODE(major, minor, patch))

/**
 * @brief Check if compiled against exactly version X.Y.Z
 */
#define BML_VERSION_EXACT(major, minor, patch) \
    (BML_COMPILED_VERSION == BML_VERSION_ENCODE(major, minor, patch))

/**
 * @brief Check if compiled against version before X.Y.Z
 */
#define BML_VERSION_BEFORE(major, minor, patch) \
    (BML_COMPILED_VERSION < BML_VERSION_ENCODE(major, minor, patch))

#endif /* BML_FEATURE_TEST_H */
