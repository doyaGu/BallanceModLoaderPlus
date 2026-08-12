// The pieces every other public header starts from, and the error codes the loader
// answers with. Nothing here does anything on its own, so a Mod includes it for
// MOD_EXPORT and for the error codes rather than for its own sake.
//
// BML_EXPORT marks what the loader exports and turns into an import when a Mod compiles,
// which is why a Mod must not define BML_EXPORTS. MOD_EXPORT is what the Mod's own
// BMLEntry and BMLExit are marked with, since those two are what the loader looks for in
// the .bmodp file. BML_BEGIN_CDECLS and BML_END_CDECLS wrap the plain C parts so the same
// header compiles as C and as C++.
//
// The error codes are one numbering shared by everything that answers with an int: 0 is
// BML_OK and every failure is negative, grouped by what it is about, and
// BML_GetErrorString turns any of them into a short English line, "Unknown error" for a
// number it does not know. The IMC functions and the C++ facades built on them report
// these. The BML_ functions of BML.h do not: those answer 1 for success and 0 for
// failure, or a null pointer, and say nothing about why.
#ifndef BML_DEFINES_H
#define BML_DEFINES_H

#include "BML/Version.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

#ifndef BML_EXPORT
#ifdef BML_EXPORTS
#define BML_EXPORT __declspec(dllexport)
#else
#define BML_EXPORT __declspec(dllimport)
#endif
#endif

#define MOD_EXPORT extern "C" __declspec(dllexport)

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

// Memory allocation macros. These forward to the CRT of whichever binary
// expands them, so they allocate on the caller's own heap. They are unrelated to
// the exported BML_Malloc, BML_Realloc, and BML_Free functions in BML.h, which
// allocate on the loader's heap and must be paired with each other. Never
// release a BML_Malloc pointer with BML_FREE, or the reverse.
#ifndef BML_MALLOC
#define BML_MALLOC(size) malloc(size)
#endif

#ifndef BML_FREE
#define BML_FREE(ptr) free(ptr)
#endif

#ifndef BML_REALLOC
#define BML_REALLOC(ptr, size) realloc(ptr, size)
#endif

// String utilities
#ifndef BML_STRDUP
#ifdef _WIN32
#define BML_STRDUP(str) _strdup(str)
#else
#define BML_STRDUP(str) strdup(str)
#endif
#endif

// Alignment macros
#define BML_ALIGN(x) __declspec(align(x))
#define BML_CACHE_LINE_SIZE 64
#define BML_CACHE_ALIGNED BML_ALIGN(BML_CACHE_LINE_SIZE)

// Compiler hints. MSVC has no expression-level branch hint, so these expand to
// the plain condition there and carry a real hint under clang-cl, which
// bml_add_mod also accepts.
#ifndef BML_HAS_BUILTIN
#ifdef __has_builtin
#define BML_HAS_BUILTIN(x) __has_builtin(x)
#else
#define BML_HAS_BUILTIN(x) 0
#endif
#endif

#if BML_HAS_BUILTIN(__builtin_expect)
#define BML_LIKELY(x) (__builtin_expect(!!(x), 1))
#define BML_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define BML_LIKELY(x) (!!(x))
#define BML_UNLIKELY(x) (!!(x))
#endif

#ifndef BML_HAS_ATTRIBUTE
#ifdef __has_attribute
#define BML_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define BML_HAS_ATTRIBUTE(x) 0
#endif
#endif

#if BML_HAS_ATTRIBUTE(pure)
#define BML_PURE __attribute__((pure))
#else
#define BML_PURE
#endif

#if BML_HAS_ATTRIBUTE(const)
#define BML_CONST __attribute__((const))
#else
#define BML_CONST
#endif

// Debug assertions
#ifdef NDEBUG
#define BML_ASSERT(expr) ((void)0)
#else
#include <assert.h>
#define BML_ASSERT(expr) assert(expr)
#endif

/**
 * @defgroup ErrorCodes Error codes returned by BML functions
 * @{
 */

/* General error codes */
#define BML_OK                              (0)    /**< Operation completed successfully */
#define BML_ERROR_FAIL                      (-1)   /**< General failure */
#define BML_ERROR_FROZEN                    (-2)   /**< Operation cannot be performed in current state */
#define BML_ERROR_NOT_FOUND                 (-3)   /**< Requested item not found */
#define BML_ERROR_NOT_IMPLEMENTED           (-4)   /**< Feature not implemented */
#define BML_ERROR_OUT_OF_MEMORY             (-5)   /**< Memory allocation failed */
#define BML_ERROR_INVALID_PARAMETER         (-6)   /**< Invalid parameter provided */
#define BML_ERROR_ACCESS_DENIED             (-7)   /**< Access to resource denied */
#define BML_ERROR_TIMEOUT                   (-8)   /**< Operation timed out */
#define BML_ERROR_BUSY                      (-9)   /**< Resource busy or locked */
#define BML_ERROR_ALREADY_EXISTS            (-10)  /**< Item already exists */
#define BML_ERROR_INVALID_HANDLE             (-11)  /**< Opaque handle is invalid or stale */
#define BML_ERROR_WOULD_BLOCK                (-12)  /**< Bounded queue cannot accept more work */
#define BML_ERROR_CANCELLED                  (-13)  /**< Asynchronous operation was cancelled */
#define BML_ERROR_WRONG_THREAD               (-14)  /**< Operation is not allowed on this thread */
#define BML_ERROR_MALFORMED_MESSAGE          (-15)  /**< Encoded message is truncated or malformed */
#define BML_ERROR_TYPE_MISMATCH              (-16)  /**< Encoded field has an unexpected type */
#define BML_ERROR_VERSION_MISMATCH           (-17)  /**< Interface exists, but not in the requested major version */

/* Mod-specific error codes */
#define BML_ERROR_MOD_LOAD_FAILED           (-100) /**< Failed to load mod */
#define BML_ERROR_MOD_INVALID               (-101) /**< Invalid mod format or structure */
#define BML_ERROR_MOD_INCOMPATIBLE          (-102) /**< Mod is incompatible with current BML version */
#define BML_ERROR_MOD_INITIALIZATION        (-103) /**< Mod initialization failed */

/* Dependency-specific error codes */
#define BML_ERROR_DEPENDENCY_CIRCULAR       (-200) /**< Circular dependency detected */
#define BML_ERROR_DEPENDENCY_MISSING        (-201) /**< Required dependency not found */
#define BML_ERROR_DEPENDENCY_VERSION        (-202) /**< Dependency version incompatible */
#define BML_ERROR_DEPENDENCY_RESOLUTION     (-203) /**< Failed to resolve dependencies */
#define BML_ERROR_DEPENDENCY_LIMIT          (-204) /**< Too many dependencies */
#define BML_ERROR_DEPENDENCY_INVALID        (-205) /**< Invalid dependency specification */
#define BML_ERROR_DEPENDENCY_CONFLICT       (-206) /**< Conflicting dependencies detected */

/* Resource-specific error codes */
#define BML_ERROR_RESOURCE_NOT_FOUND        (-300) /**< Resource not found */
#define BML_ERROR_RESOURCE_INVALID          (-301) /**< Invalid resource format */
#define BML_ERROR_RESOURCE_BUSY             (-302) /**< Resource is busy or locked */
#define BML_ERROR_RESOURCE_PERMISSION       (-303) /**< Insufficient permission for resource */

/* Script-specific error codes */
#define BML_ERROR_SCRIPT_INVALID            (-400) /**< Invalid script */
#define BML_ERROR_SCRIPT_EXECUTION          (-401) /**< Script execution failed */
#define BML_ERROR_SCRIPT_TIMEOUT            (-402) /**< Script execution timed out */

/* Command-specific error codes */
#define BML_ERROR_COMMAND_INVALID           (-500) /**< Invalid command */
#define BML_ERROR_COMMAND_PERMISSION        (-501) /**< Insufficient permission for command */
#define BML_ERROR_COMMAND_EXECUTION         (-502) /**< Command execution failed */

/* Configuration-specific error codes */
#define BML_ERROR_CONFIG_INVALID            (-600) /**< Invalid configuration */
#define BML_ERROR_CONFIG_READ               (-601) /**< Failed to read configuration */
#define BML_ERROR_CONFIG_WRITE              (-602) /**< Failed to write configuration */
#define BML_ERROR_CONFIG_FORMAT             (-603) /**< Invalid configuration format */

/* IMC-specific error codes. */
#define BML_ERROR_IMC_ENDPOINT_NOT_FOUND       (-702) /**< Requested IMC route was not found */
#define BML_ERROR_IMC_HANDLE_STALE             (-709) /**< IMC-owned handle is stale */
#define BML_ERROR_IMC_UNSUPPORTED              (-710) /**< Operation requires an unavailable IMC feature */
#define BML_ERROR_IMC_API_MISMATCH             (-712) /**< IMC payload type or layout is incompatible */
#define BML_ERROR_IMC_PROVIDER_UNLOADED        (-713) /**< IMC provider was unloaded during an operation */
#define BML_ERROR_IMC_OBJECT_INVALID           (-717) /**< IMC object reference is invalid or expired */
#define BML_ERROR_IMC_SCHEMA_MISMATCH          (-718) /**< IMC payload schema does not match the route */
#define BML_ERROR_IMC_TARGET_EXECUTION_FAILED  (-719) /**< IMC provider callback failed to execute */

BML_BEGIN_CDECLS

BML_EXPORT const char *BML_GetErrorString(int errorCode);

BML_END_CDECLS

#endif // BML_DEFINES_H
