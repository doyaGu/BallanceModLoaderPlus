/**
 * @file bml_core.h
 * @brief BML Core API - Context, Version, and Lifecycle Management
 * 
 * The Core API provides fundamental functionality for the BML runtime:
 * - Context lifecycle management with reference counting
 * - Runtime and API version querying
 * - Mod metadata access (ID, version)
 * - Capability request and checking
 * - Thread-local module binding
 * - Shutdown hook registration
 * 
 * @section core_threading Threading Model
 * All Core APIs are thread-safe unless otherwise noted. Context reference
 * counting uses atomic operations, and thread-local module binding uses TLS.
 * 
 * @section core_lifecycle Lifecycle
 * The global context is created during BML initialization and remains valid
 * until shutdown. Mods can retain/release references for safe access.
 * 
 * @section core_usage Usage Example
 * @code
 * // Get global context
 * BML_Context ctx = bmlGetGlobalContext();
 * if (ctx) {
 *     // Retain reference for long-term use
 *     bmlContextRetain(ctx);
 *     
 *     // Get runtime version
 *     const BML_Version* ver = bmlGetRuntimeVersion();
 *     printf("BML v%u.%u.%u\n", ver->major, ver->minor, ver->patch);
 *     
 *     // Release when done
 *     bmlContextRelease(ctx);
 * }
 * @endcode
 * 
 * @see bml_context.hpp for C++ RAII wrappers
 * @see bml_core.hpp for C++ convenience functions
 */

#ifndef BML_CORE_H
#define BML_CORE_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

/* ========================================================================
 * Context Lifecycle Function Types
 * ======================================================================== */

/**
 * @defgroup CoreContext Context Management
 * @brief Reference-counted context lifecycle management
 * @{
 */

/**
 * @brief Function pointer type for incrementing context reference count
 * 
 * Increments the reference count of the specified context. The context
 * remains valid as long as the reference count is greater than zero.
 * 
 * @param[in] ctx Context handle to retain
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_HANDLE if ctx is NULL or invalid
 * 
 * @threadsafe Yes - uses atomic reference counting
 * 
 * @see PFN_BML_ContextRelease for releasing references
 * @see PFN_BML_GetGlobalContext for obtaining the global context
 */
typedef BML_Result (*PFN_BML_ContextRetain)(BML_Context ctx);

/**
 * @brief Function pointer type for decrementing context reference count
 * 
 * Decrements the reference count of the specified context. When the count
 * reaches zero, the context may be destroyed. After calling this function,
 * the caller should no longer use the context handle.
 * 
 * @param[in] ctx Context handle to release
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_HANDLE if ctx is NULL or already released
 * 
 * @threadsafe Yes - uses atomic reference counting
 * 
 * @warning Do not use the context after releasing the last reference
 * 
 * @see PFN_BML_ContextRetain for retaining references
 */
typedef BML_Result (*PFN_BML_ContextRelease)(BML_Context ctx);

/**
 * @brief Function pointer type for obtaining the global BML context
 * 
 * Returns the singleton global context that is shared across all mods.
 * The global context is created during BML initialization and destroyed
 * during shutdown.
 * 
 * @return Global context handle, or NULL if BML is not initialized
 * 
 * @threadsafe Yes - returns immutable singleton reference
 * 
 * @note The returned context does not need to be released unless you
 *       explicitly called bmlContextRetain() on it
 * 
 * @code
 * BML_Context ctx = bmlGetGlobalContext();
 * if (ctx) {
 *     // Use context for API calls
 * }
 * @endcode
 */
typedef BML_Context (*PFN_BML_GetGlobalContext)(void);

/**
 * @brief Function pointer type for setting user data on context
 * 
 * Associates an opaque user data pointer with the context using a key.
 * This allows mods and subsystems to store arbitrary data that can be
 * retrieved later. Each key can have one associated value.
 * 
 * @param[in] ctx Context handle
 * @param[in] key Unique key string identifying this data slot
 * @param[in] data Opaque pointer to user data (may be NULL to clear)
 * @param[in] destructor Optional destructor called when data is replaced or context is destroyed (may be NULL)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_HANDLE if ctx is NULL
 * @return BML_RESULT_INVALID_ARGUMENT if key is NULL
 * 
 * @threadsafe Yes - uses internal synchronization
 * 
 * @note The destructor is called with the old data when:
 *       - New data is set for the same key
 *       - Data is cleared (set to NULL)
 *       - Context is destroyed
 * 
 * @code
 * // Store CKContext pointer (registered by ModLoader)
 * bmlContextSetUserData(ctx, "virtools.ckcontext", ckContext, NULL);
 * 
 * // Store custom data with destructor
 * bmlContextSetUserData(ctx, "mymod.state", state, MyStateDestructor);
 * @endcode
 * 
 * @see PFN_BML_ContextGetUserData for retrieving data
 */
typedef void (*BML_UserDataDestructor)(void *data);
typedef BML_Result (*PFN_BML_ContextSetUserData)(BML_Context ctx, const char *key, void *data, BML_UserDataDestructor destructor);

/**
 * @brief Function pointer type for getting user data from context
 * 
 * Retrieves the user data pointer previously associated with the given key.
 * 
 * @param[in] ctx Context handle
 * @param[in] key Key string identifying the data slot
 * @param[out] out_data Receives the user data pointer (NULL if not found)
 * @return BML_RESULT_OK on success (even if key not found, out_data will be NULL)
 * @return BML_RESULT_INVALID_HANDLE if ctx is NULL
 * @return BML_RESULT_INVALID_ARGUMENT if key or out_data is NULL
 * 
 * @threadsafe Yes - uses internal synchronization
 * 
 * @code
 * // Retrieve CKContext (set by ModLoader)
 * void* ckContext = NULL;
 * if (bmlContextGetUserData(ctx, "virtools.ckcontext", &ckContext) == BML_RESULT_OK && ckContext) {
 *     CKContext* ck = (CKContext*)ckContext;
 *     // Use ckContext...
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_ContextGetUserData)(BML_Context ctx, const char *key, void **out_data);

/** @} */ /* end CoreContext group */

/* ========================================================================
 * Version Query Function Types
 * ======================================================================== */

/**
 * @defgroup CoreVersion Version Information
 * @brief Runtime and API version querying
 * @{
 */

/**
 * @brief Function pointer type for getting runtime version
 * 
 * Returns the version of the currently running BML runtime. This may
 * differ from the API version if the runtime has been updated.
 * 
 * @return Pointer to static version structure, or NULL on error
 * 
 * @threadsafe Yes - returns pointer to static data
 * 
 * @code
 * const BML_Version* ver = bmlGetRuntimeVersion();
 * if (ver) {
 *     printf("Runtime: v%u.%u.%u\n", ver->major, ver->minor, ver->patch);
 * }
 * @endcode
 * 
 * @see bmlGetApiVersion() for compile-time API version
 */
typedef const BML_Version *(*PFN_BML_GetRuntimeVersion)(void);

/** @} */ /* end CoreVersion group */

/* ========================================================================
 * Capability Management Function Types
 * ======================================================================== */

/**
 * @defgroup CoreCapability Capability Management
 * @brief Request and check mod capabilities
 * @{
 */

/**
 * @brief Function pointer type for requesting a capability
 * 
 * Requests that a specific capability be enabled for the mod. Capabilities
 * provide opt-in access to advanced features or permissions.
 * 
 * @param[in] mod Mod handle requesting the capability
 * @param[in] capability_id Unique identifier for the capability (e.g., "bml.unsafe_memory")
 * @return BML_RESULT_OK if capability was granted
 * @return BML_RESULT_PERMISSION_DENIED if capability was denied
 * @return BML_RESULT_NOT_FOUND if capability_id is unknown
 * @return BML_RESULT_INVALID_ARGUMENT if mod or capability_id is NULL
 * 
 * @threadsafe Yes
 * 
 * @code
 * BML_Result result = bmlRequestCapability(my_mod, "bml.profiling");
 * if (BML_SUCCEEDED(result)) {
 *     // Profiling features are now available
 * }
 * @endcode
 * 
 * @see PFN_BML_CheckCapability for querying capability status
 */
typedef BML_Result (*PFN_BML_RequestCapability)(BML_Mod mod, const char *capability_id);

/**
 * @brief Function pointer type for checking capability support
 * 
 * Checks whether a specific capability is supported and enabled for the mod.
 * 
 * @param[in] mod Mod handle to check
 * @param[in] capability_id Capability identifier to check
 * @param[out] out_supported Receives BML_TRUE if supported, BML_FALSE otherwise
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if any parameter is NULL
 * 
 * @threadsafe Yes
 * 
 * @code
 * BML_Bool supported = BML_FALSE;
 * if (bmlCheckCapability(my_mod, "bml.imc", &supported) == BML_RESULT_OK) {
 *     if (supported) {
 *         // IMC features available
 *     }
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_CheckCapability)(BML_Mod mod, const char *capability_id, BML_Bool *out_supported);

/** @} */ /* end CoreCapability group */

/* ========================================================================
 * Mod Metadata Function Types
 * ======================================================================== */

/**
 * @defgroup CoreModInfo Mod Information
 * @brief Access mod metadata (ID, version)
 * @{
 */

/**
 * @brief Function pointer type for getting mod ID
 * 
 * Retrieves the unique identifier string for a mod as declared in its manifest.
 * 
 * @param[in] mod Mod handle to query
 * @param[out] out_id Receives pointer to null-terminated ID string (do not free)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_HANDLE if mod is NULL or invalid
 * @return BML_RESULT_INVALID_ARGUMENT if out_id is NULL
 * 
 * @threadsafe Yes - returns pointer to immutable data
 * 
 * @note The returned string is owned by the mod and remains valid for its lifetime
 * 
 * @code
 * const char* id = NULL;
 * if (bmlGetModId(my_mod, &id) == BML_RESULT_OK) {
 *     printf("Mod ID: %s\n", id);
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_GetModId)(BML_Mod mod, const char **out_id);

/**
 * @brief Function pointer type for getting mod version
 * 
 * Retrieves the semantic version of a mod as declared in its manifest.
 * 
 * @param[in] mod Mod handle to query
 * @param[out] out_version Receives mod version (must have struct_size set)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_HANDLE if mod is NULL or invalid
 * @return BML_RESULT_INVALID_ARGUMENT if out_version is NULL
 * @return BML_RESULT_INVALID_SIZE if out_version->struct_size is incorrect
 * 
 * @threadsafe Yes
 * 
 * @code
 * BML_Version version = BML_VERSION_INIT(0, 0, 0);
 * if (bmlGetModVersion(my_mod, &version) == BML_RESULT_OK) {
 *     printf("Mod v%u.%u.%u\n", version.major, version.minor, version.patch);
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_GetModVersion)(BML_Mod mod, BML_Version *out_version);

/** @} */ /* end CoreModInfo group */

/* ========================================================================
 * Thread-Local Module Binding Function Types
 * ======================================================================== */

/**
 * @defgroup CoreTLS Thread-Local Module Binding
 * @brief Bind modules to threads for implicit context resolution
 * @{
 */

/**
 * @brief Function pointer type for setting thread-local module binding
 * 
 * Binds a module to the calling thread. This allows APIs that require
 * a module context to implicitly resolve it from TLS instead of requiring
 * explicit parameters.
 * 
 * @param[in] mod Module handle to bind, or NULL to clear the binding
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_SUPPORTED if TLS is not available
 * 
 * @threadsafe Yes - uses per-thread storage
 * 
 * @note Previous binding is overwritten; use CurrentModuleScope for RAII
 * 
 * @code
 * // Bind module to current thread
 * bmlSetCurrentModule(my_mod);
 * 
 * // APIs can now resolve module from TLS
 * bmlLog(NULL, "Message"); // Uses TLS-bound module
 * 
 * // Clear binding
 * bmlSetCurrentModule(NULL);
 * @endcode
 * 
 * @see PFN_BML_GetCurrentModule for querying current binding
 * @see bml::CurrentModuleScope for C++ RAII wrapper
 */
typedef BML_Result (*PFN_BML_SetCurrentModule)(BML_Mod mod);

/**
 * @brief Function pointer type for getting thread-local module binding
 * 
 * Returns the module currently bound to the calling thread via
 * bmlSetCurrentModule().
 * 
 * @return Module handle bound to current thread, or NULL if none
 * 
 * @threadsafe Yes - uses per-thread storage
 * 
 * @code
 * BML_Mod current = bmlGetCurrentModule();
 * if (current) {
 *     const char* id = NULL;
 *     bmlGetModId(current, &id);
 *     printf("Current module: %s\n", id);
 * }
 * @endcode
 */
typedef BML_Mod (*PFN_BML_GetCurrentModule)(void);

/** @} */ /* end CoreTLS group */

/* ========================================================================
 * Shutdown Hook Types
 * ======================================================================== */

/**
 * @defgroup CoreShutdown Shutdown Hooks
 * @brief Register callbacks for cleanup during BML shutdown
 * @{
 */

/**
 * @brief Callback function type for shutdown notifications
 * 
 * This callback is invoked during BML shutdown to allow mods to perform
 * cleanup operations. Callbacks are called in reverse registration order.
 * 
 * @param[in] ctx BML context (first parameter for consistency with other callbacks)
 * @param[in] user_data User-provided context from registration (always last parameter)
 * 
 * @note Called from main thread during shutdown sequence
 * @note Keep implementation fast; avoid blocking operations
 * 
 * @code
 * void MyShutdownHandler(BML_Context ctx, void* user_data) {
 *     MyModState* state = (MyModState*)user_data;
 *     state->SaveConfig();
 *     state->ReleaseResources();
 * }
 * @endcode
 */
typedef void (*BML_ShutdownCallback)(BML_Context ctx, void *user_data);

/**
 * @brief Function pointer type for registering shutdown hooks
 * 
 * Registers a callback to be invoked during BML shutdown. Multiple
 * callbacks can be registered; they are called in reverse order
 * (LIFO - last registered, first called).
 * 
 * @param[in] mod Mod handle registering the hook
 * @param[in] callback Callback function to invoke
 * @param[in] user_data Opaque pointer passed to callback
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if mod or callback is NULL
 * @return BML_RESULT_OUT_OF_MEMORY if hook storage is exhausted
 * 
 * @threadsafe Yes - hook list is synchronized
 * 
 * @warning Hooks cannot be unregistered; ensure user_data remains valid
 * 
 * @code
 * static MyState g_state;
 * 
 * void OnShutdown(BML_Context ctx, void* ud) {
 *     MyState* s = (MyState*)ud;
 *     s->Cleanup();
 * }
 * 
 * // During mod initialization
 * bmlRegisterShutdownHook(my_mod, OnShutdown, &g_state);
 * @endcode
 * 
 * @see bml::ShutdownHook for C++ RAII wrapper
 */
typedef BML_Result (*PFN_BML_RegisterShutdownHook)(BML_Mod mod, BML_ShutdownCallback callback, void *user_data);

/** @} */ /* end CoreShutdown group */

/* ========================================================================
 * Core API Structure
 * ======================================================================== */

/**
 * @defgroup CoreApiStruct Core API Table
 * @brief Function pointer table for Core API dispatch
 * @{
 */

/**
 * @brief Core API function pointer table
 * 
 * Contains all function pointers for the Core API subsystem. This structure
 * is populated by the loader during initialization and provides access to
 * all core functionality.
 * 
 * @note Members may be NULL if the corresponding feature is not supported
 *       by the runtime. Always check before calling.
 * 
 * @code
 * // Example: Direct use of API table (advanced usage)
 * BML_CoreApi* core = GetCoreApiTable();
 * if (core && core->GetRuntimeVersion) {
 *     const BML_Version* ver = core->GetRuntimeVersion();
 * }
 * @endcode
 * 
 */
typedef struct BML_CoreApi {
    PFN_BML_ContextRetain           ContextRetain;           /**< @see PFN_BML_ContextRetain */
    PFN_BML_ContextRelease          ContextRelease;          /**< @see PFN_BML_ContextRelease */
    PFN_BML_GetGlobalContext        GetGlobalContext;        /**< @see PFN_BML_GetGlobalContext */
    PFN_BML_GetRuntimeVersion       GetRuntimeVersion;       /**< @see PFN_BML_GetRuntimeVersion */
    PFN_BML_ContextSetUserData      ContextSetUserData;      /**< @see PFN_BML_ContextSetUserData */
    PFN_BML_ContextGetUserData      ContextGetUserData;      /**< @see PFN_BML_ContextGetUserData */
    PFN_BML_RequestCapability       RequestCapability;       /**< @see PFN_BML_RequestCapability */
    PFN_BML_CheckCapability         CheckCapability;         /**< @see PFN_BML_CheckCapability */
    PFN_BML_GetModId                GetModId;                /**< @see PFN_BML_GetModId */
    PFN_BML_GetModVersion           GetModVersion;           /**< @see PFN_BML_GetModVersion */
    PFN_BML_RegisterShutdownHook    RegisterShutdownHook;    /**< @see PFN_BML_RegisterShutdownHook */
    PFN_BML_SetCurrentModule        SetCurrentModule;        /**< @see PFN_BML_SetCurrentModule */
    PFN_BML_GetCurrentModule        GetCurrentModule;        /**< @see PFN_BML_GetCurrentModule */
} BML_CoreApi;

/** @} */ /* end CoreApiStruct group */

/* ========================================================================
 * Core Capability Flags
 * ======================================================================== */

/**
 * @defgroup CoreCaps Core Capabilities
 * @brief Query available Core API features
 * @{
 */

/**
 * @brief Bitmask flags indicating available Core API capabilities
 * 
 * Use with BML_CoreCaps::capability_flags to check feature availability.
 * 
 * @code
 * BML_CoreCaps caps = BML_CORE_CAPS_INIT;
 * if (bmlGetCoreCaps(&caps) == BML_RESULT_OK) {
 *     if (caps.capability_flags & BML_CORE_CAP_SHUTDOWN_HOOKS) {
 *         // Shutdown hooks are supported
 *     }
 * }
 * @endcode
 */
typedef enum BML_CoreCapabilityFlags {
    /** @brief Context retain/release APIs available */
    BML_CORE_CAP_CONTEXT_RETAIN     = 1u << 0,
    
    /** @brief Runtime version query API available */
    BML_CORE_CAP_RUNTIME_QUERY      = 1u << 1,
    
    /** @brief Mod metadata APIs (GetModId, GetModVersion) available */
    BML_CORE_CAP_MOD_METADATA       = 1u << 2,
    
    /** @brief Shutdown hook registration available */
    BML_CORE_CAP_SHUTDOWN_HOOKS     = 1u << 3,
    
    /** @brief Capability request/check APIs available */
    BML_CORE_CAP_CAPABILITY_CHECKS  = 1u << 4,
    
    /** @brief Thread-local module binding APIs available */
    BML_CORE_CAP_CURRENT_MODULE_TLS = 1u << 5,
    
    /** @internal Force enum to 32-bit for ABI stability */
    _BML_CORE_CAP_FORCE_32BIT       = 0x7FFFFFFF
} BML_CoreCapabilityFlags;

/**
 * @brief Core subsystem capabilities structure
 * 
 * Provides comprehensive information about the Core API capabilities
 * of the current runtime. Use bmlGetCoreCaps() to populate.
 * 
 * @note Always initialize with BML_CORE_CAPS_INIT before calling bmlGetCoreCaps()
 * 
 * @code
 * BML_CoreCaps caps = BML_CORE_CAPS_INIT;
 * if (bmlGetCoreCaps(&caps) == BML_RESULT_OK) {
 *     printf("Runtime: v%u.%u.%u\n", 
 *            caps.runtime_version.major,
 *            caps.runtime_version.minor,
 *            caps.runtime_version.patch);
 *     printf("Threading: %s\n", 
 *            caps.threading_model == BML_THREADING_FREE ? "Free" : "Other");
 * }
 * @endcode
 */
typedef struct BML_CoreCaps {
    /** @brief Size of this structure, must be first field for ABI extension */
    size_t struct_size;
    
    /** @brief Runtime version (may differ from API version) */
    BML_Version runtime_version;
    
    /** @brief Bitmask of available capabilities (BML_CoreCapabilityFlags) */
    uint32_t capability_flags;
    
    /** @brief API version this runtime implements */
    BML_Version api_version;
    
    /** @brief Threading model of Core APIs */
    BML_ThreadingModel threading_model;
} BML_CoreCaps;

/**
 * @def BML_CORE_CAPS_INIT
 * @brief Static initializer for BML_CoreCaps
 * 
 * Always use this initializer before calling bmlGetCoreCaps() to ensure
 * struct_size is correctly set for ABI compatibility.
 * 
 * @code
 * BML_CoreCaps caps = BML_CORE_CAPS_INIT;
 * bmlGetCoreCaps(&caps);
 * @endcode
 */
#define BML_CORE_CAPS_INIT { sizeof(BML_CoreCaps), BML_VERSION_INIT(0,0,0), 0, BML_VERSION_INIT(0,0,0), BML_THREADING_SINGLE }

/**
 * @brief Function pointer type for querying Core capabilities
 * 
 * Fills the provided structure with information about available Core
 * API capabilities and version information.
 * 
 * @param[out] out_caps Pointer to receive capabilities (must have struct_size set)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if out_caps is NULL
 * @return BML_RESULT_INVALID_SIZE if out_caps->struct_size is incorrect
 * 
 * @threadsafe Yes
 * 
 * @code
 * BML_CoreCaps caps = BML_CORE_CAPS_INIT;
 * if (bmlCoreGetCaps(&caps) == BML_RESULT_OK) {
 *     if (caps.capability_flags & BML_CORE_CAP_SHUTDOWN_HOOKS) {
 *         // Can use shutdown hooks
 *     }
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_CoreGetCaps)(BML_CoreCaps *out_caps);

/** @} */ /* end CoreCaps group */

/* ========================================================================
 * Global Function Pointers
 * ======================================================================== */

/**
 * @defgroup CoreGlobals Global Function Pointers
 * @brief Pre-loaded function pointers for Core API access
 * 
 * These global function pointers are populated by the BML loader during
 * initialization. They provide direct access to Core API functions without
 * requiring manual lookup.
 * 
 * @note Always check for NULL before calling, as optional APIs may not be loaded
 * @note For thread-safety, treat these as read-only after initialization
 * @{
 */

/** @brief Increment context reference count @see PFN_BML_ContextRetain */
extern PFN_BML_ContextRetain           bmlContextRetain;

/** @brief Decrement context reference count @see PFN_BML_ContextRelease */
extern PFN_BML_ContextRelease          bmlContextRelease;

/** @brief Get global BML context @see PFN_BML_GetGlobalContext */
extern PFN_BML_GetGlobalContext        bmlGetGlobalContext;

/** @brief Get runtime version @see PFN_BML_GetRuntimeVersion */
extern PFN_BML_GetRuntimeVersion       bmlGetRuntimeVersion;

/** @brief Set user data on context @see PFN_BML_ContextSetUserData */
extern PFN_BML_ContextSetUserData      bmlContextSetUserData;

/** @brief Get user data from context @see PFN_BML_ContextGetUserData */
extern PFN_BML_ContextGetUserData      bmlContextGetUserData;

/** @brief Request a capability for mod @see PFN_BML_RequestCapability */
extern PFN_BML_RequestCapability       bmlRequestCapability;

/** @brief Check if capability is supported @see PFN_BML_CheckCapability */
extern PFN_BML_CheckCapability         bmlCheckCapability;

/** @brief Get mod ID string @see PFN_BML_GetModId */
extern PFN_BML_GetModId                bmlGetModId;

/** @brief Get mod version @see PFN_BML_GetModVersion */
extern PFN_BML_GetModVersion           bmlGetModVersion;

/** @brief Register shutdown callback @see PFN_BML_RegisterShutdownHook */
extern PFN_BML_RegisterShutdownHook    bmlRegisterShutdownHook;

/** @brief Set thread-local module @see PFN_BML_SetCurrentModule */
extern PFN_BML_SetCurrentModule        bmlSetCurrentModule;

/** @brief Get thread-local module @see PFN_BML_GetCurrentModule */
extern PFN_BML_GetCurrentModule        bmlGetCurrentModule;

/** @brief Query Core capabilities @see PFN_BML_CoreGetCaps */
extern PFN_BML_CoreGetCaps             bmlCoreGetCaps;

/** @} */ /* end CoreGlobals group */

BML_END_CDECLS

/* ========================================================================
 * Compile-Time Assertions for ABI Stability
 * 
 * These static assertions verify critical ABI invariants at compile time:
 * 1. struct_size is at offset 0 (required for version extension pattern)
 * 2. Enums are 32-bit (prevents size mismatches across compilers)
 * ======================================================================== */

#ifdef __cplusplus
#include <cstddef>
#define BML_CORE_OFFSETOF(type, member) offsetof(type, member)
#else
#include <stddef.h>
#define BML_CORE_OFFSETOF(type, member) offsetof(type, member)
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
    #define BML_CORE_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define BML_CORE_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define BML_CORE_STATIC_ASSERT_CONCAT_(a, b) a##b
    #define BML_CORE_STATIC_ASSERT_CONCAT(a, b) BML_CORE_STATIC_ASSERT_CONCAT_(a, b)
    #define BML_CORE_STATIC_ASSERT(cond, msg) \
        typedef char BML_CORE_STATIC_ASSERT_CONCAT(bml_core_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/** @brief Verify BML_CoreCaps.struct_size is at offset 0 for ABI extension support */
BML_CORE_STATIC_ASSERT(BML_CORE_OFFSETOF(BML_CoreCaps, struct_size) == 0,
    "BML_CoreCaps.struct_size must be at offset 0");

#endif /* BML_CORE_H */
