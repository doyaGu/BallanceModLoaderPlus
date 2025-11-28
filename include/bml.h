/**
 * @file bml.h
 * @brief BML v2 Unified C Header
 * 
 * This is the single-include header for the complete BML v2 C API.
 * It provides access to all BML functionality through a pure C interface
 * that can be used by any language with C FFI support.
 * 
 * The API is organized into the following subsystems:
 * 
 * - **Core** (bml_core.h) - Context management, version queries, lifecycle hooks
 * - **Types** (bml_types.h) - Fundamental types, handles, version structures
 * - **Errors** (bml_errors.h) - Error codes and diagnostics
 * - **Config** (bml_config.h) - Configuration read/write
 * - **Logging** (bml_logging.h) - Structured logging with severity levels
 * - **IMC** (bml_imc.h) - Inter-Mod Communication (pub/sub, RPC, futures)
 * - **Extension** (bml_extension.h) - Dynamic extension registration and loading
 * - **Sync** (bml_sync.h) - Synchronization primitives (mutex, rwlock, etc.)
 * - **Memory** (bml_memory.h) - Cross-DLL safe memory allocation
 * - **Resource** (bml_resource.h) - Generic handle/resource management
 * - **Profiling** (bml_profiling.h) - Performance tracing and statistics
 * - **Capabilities** (bml_capabilities.h) - Runtime feature detection
 * - **Hot Reload** (bml_hot_reload.h) - Mod lifecycle events for hot reload
 * 
 * @section c_usage Usage (C)
 * 
 * Include this header to access all BML C APIs:
 * @code
 * #include <bml.h>
 * 
 * // All BML C functions and types are now available
 * BML_Context ctx = bmlGetGlobalContext();
 * @endcode
 * 
 * For dynamic loading (header-only, GLAD-style), use bml_loader.h:
 * @code
 * // In ONE .c file:
 * #define BML_LOADER_IMPLEMENTATION
 * #include <bml_loader.h>
 * 
 * // In other files:
 * #include <bml_loader.h>
 * 
 * // Initialize:
 * bmlLoadAPI(get_proc_fn);
 * @endcode
 * 
 * @section cpp_usage Usage (C++)
 * 
 * For C++, prefer the unified C++ header for RAII wrappers:
 * @code
 * #include <bml.hpp>
 * 
 * // Use C++ wrappers
 * bml::Context ctx = bml::GetGlobalContext();
 * bml::Config config(mod);
 * auto value = config.GetString("category", "key").value_or("default");
 * @endcode
 * 
 * @section abi_stability ABI Stability
 * 
 * All BML structures use the "struct_size pattern" for forward/backward
 * ABI compatibility:
 * - First field is always `size_t struct_size`
 * - Initialize with provided macros (e.g., BML_VERSION_INIT)
 * - Newer runtime can read older structs; older code ignores new fields
 * 
 * All enums include a _FORCE_32BIT sentinel to ensure consistent size
 * across different compilers and platforms.
 * 
 * @section api_ids API IDs
 * 
 * BML uses stable 32-bit API IDs for binary compatibility (like syscalls):
 * - IDs are defined in bml_api_ids.h
 * - Once assigned, IDs never change
 * - Use bmlGetProcAddressById() for 3-5x faster lookup
 * 
 * @section threading Threading Model
 * 
 * Thread safety annotations:
 * - BML_THREADSAFE: Safe to call from any thread
 * - BML_NOT_THREADSAFE: Requires external synchronization
 * - BML_MAIN_THREAD_ONLY: Must be called from main/render thread
 * 
 * Most APIs are thread-safe. Check individual function documentation.
 * 
 * @see bml.hpp for C++ wrappers
 * @see bml_loader.h for dynamic loading
 */

#ifndef BML_H
#define BML_H

/* ========================================================================
 * Foundation Headers (order matters)
 * ======================================================================== */

/* Basic type definitions - must come first */
#include "bml_types.h"

/* Error codes and result checking - needed by all other headers */
#include "bml_errors.h"

/* Version utilities */
#include "bml_version.h"

/* Export/import macros and mod entrypoint */
#include "bml_export.h"

/* Stable API identifiers */
#include "bml_api_ids.h"

/* ========================================================================
 * Core Subsystem Headers
 * ======================================================================== */

/* Core context and lifecycle management */
#include "bml_core.h"

/* Configuration read/write */
#include "bml_config.h"

/* Logging with severity levels */
#include "bml_logging.h"

/* ========================================================================
 * Communication and Extension Headers
 * ======================================================================== */

/* Inter-Mod Communication (pub/sub, RPC) */
#include "bml_imc.h"

/* Extension registration and loading */
#include "bml_extension.h"

/* ========================================================================
 * Resource and Memory Management
 * ======================================================================== */

/* Synchronization primitives */
#include "bml_sync.h"

/* Memory allocation */
#include "bml_memory.h"

/* Resource handle management */
#include "bml_resource.h"

/* ========================================================================
 * Diagnostics and Profiling
 * ======================================================================== */

/* Runtime capability detection */
#include "bml_capabilities.h"

/* Profiling and tracing */
#include "bml_profiling.h"

/* API call tracing */
#include "bml_api_tracing.h"

/* ========================================================================
 * Lifecycle and Hot Reload
 * ======================================================================== */

/* Hot reload lifecycle events */
#include "bml_hot_reload.h"

/* ========================================================================
 * Feature Test Macros
 * ======================================================================== */

/**
 * @def BML_HAS_FEATURE_IMC
 * @brief Compile-time check for IMC support
 */
#define BML_HAS_FEATURE_IMC 1

/**
 * @def BML_HAS_FEATURE_EXTENSION
 * @brief Compile-time check for extension system support
 */
#define BML_HAS_FEATURE_EXTENSION 1

/**
 * @def BML_HAS_FEATURE_SYNC
 * @brief Compile-time check for sync primitives support
 */
#define BML_HAS_FEATURE_SYNC 1

/**
 * @def BML_HAS_FEATURE_MEMORY
 * @brief Compile-time check for memory management support
 */
#define BML_HAS_FEATURE_MEMORY 1

/**
 * @def BML_HAS_FEATURE_PROFILING
 * @brief Compile-time check for profiling support
 */
#define BML_HAS_FEATURE_PROFILING 1

/**
 * @def BML_HAS_FEATURE_RESOURCE
 * @brief Compile-time check for resource handle support
 */
#define BML_HAS_FEATURE_RESOURCE 1

/**
 * @def BML_HAS_FEATURE_CAPABILITIES
 * @brief Compile-time check for capability query support
 */
#define BML_HAS_FEATURE_CAPABILITIES 1

#endif /* BML_H */
