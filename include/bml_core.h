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
 * Core APIs support concurrent access for thread-local state, retained context
 * handles, and internally synchronized stores. Global lifecycle transitions
 * such as attach, discover, load, and detach should still be serialized by
 * the host.
 * 
 * @section core_lifecycle Lifecycle
 * The global context is created during BML initialization and remains valid
 * until shutdown. Mods can retain/release references for safe access.
 * 
 * After bootstrap, only `bmlSetCurrentModule` remains available as a global
 * proc pointer. Resolve all other Core APIs explicitly through
 * `bmlGetProcAddress(...)` or consume them through an acquired builtin
 * interface such as `bml.core.context` or `bml.core.module`.
 *
 * @section core_usage Usage Example
 * @code
 * PFN_BML_GetGlobalContext getGlobalContext =
 *     (PFN_BML_GetGlobalContext)bmlGetProcAddress("bmlGetGlobalContext");
 * PFN_BML_ContextRetain contextRetain =
 *     (PFN_BML_ContextRetain)bmlGetProcAddress("bmlContextRetain");
 * PFN_BML_ContextRelease contextRelease =
 *     (PFN_BML_ContextRelease)bmlGetProcAddress("bmlContextRelease");
 * PFN_BML_GetRuntimeVersion getRuntimeVersion =
 *     (PFN_BML_GetRuntimeVersion)bmlGetProcAddress("bmlGetRuntimeVersion");
 *
 * BML_Context ctx = getGlobalContext ? getGlobalContext() : NULL;
 * if (ctx) {
 *     // Retain reference for long-term use
 *     contextRetain(ctx);
 *     
 *     // Get runtime version
 *     const BML_Version* ver = getRuntimeVersion ? getRuntimeVersion() : NULL;
 *     printf("BML v%u.%u.%u\n", ver->major, ver->minor, ver->patch);
 *     
 *     // Release when done
 *     contextRelease(ctx);
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
 *       explicitly retained it through PFN_BML_ContextRetain
 * 
 * @code
 * PFN_BML_GetGlobalContext getGlobalContext =
 *     (PFN_BML_GetGlobalContext)bmlGetProcAddress("bmlGetGlobalContext");
 * BML_Context ctx = getGlobalContext ? getGlobalContext() : NULL;
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
 * contextSetUserData(ctx, "virtools.ckcontext", ckContext, NULL);
 * 
 * // Store custom data with destructor
 * contextSetUserData(ctx, "mymod.state", state, MyStateDestructor);
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
 * if (contextGetUserData(ctx, "virtools.ckcontext", &ckContext) == BML_RESULT_OK && ckContext) {
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
 * @return Pointer to a thread-local version snapshot, or NULL on error
 *
 * @threadsafe Yes - each calling thread receives its own stable snapshot
 * 
 * @code
 * PFN_BML_GetRuntimeVersion getRuntimeVersion =
 *     (PFN_BML_GetRuntimeVersion)bmlGetProcAddress("bmlGetRuntimeVersion");
 * const BML_Version* ver = getRuntimeVersion ? getRuntimeVersion() : NULL;
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
 * BML_Result result = requestCapability(my_mod, "bml.profiling");
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
 * if (checkCapability(my_mod, "bml.imc", &supported) == BML_RESULT_OK) {
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
 * @lifetime mod - returned pointer valid for mod's lifetime
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

/**
 * @brief Function pointer type for getting mod installation directory
 *
 * Retrieves the UTF-8 encoded directory path where the mod is installed.
 *
 * @param[in] mod Mod handle to query
 * @param[out] out_dir Receives pointer to null-terminated UTF-8 path (do not free)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if mod is NULL/invalid or out_dir is NULL
 *
 * @threadsafe Yes - returns pointer to immutable cached data
 * @lifetime mod - returned pointer valid for mod's lifetime
 */
typedef BML_Result (*PFN_BML_GetModDirectory)(BML_Mod mod, const char **out_dir);

/**
 * @brief Function pointer type for getting mod display name
 *
 * Retrieves the human-readable name of a mod as declared in its manifest.
 *
 * @param[in] mod Mod handle to query
 * @param[out] out_name Receives pointer to null-terminated name string (do not free)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if mod is NULL/invalid or out_name is NULL
 *
 * @threadsafe Yes - returns pointer to immutable data
 * @lifetime mod - returned pointer valid for mod's lifetime
 */
typedef BML_Result (*PFN_BML_GetModName)(BML_Mod mod, const char **out_name);

/**
 * @brief Function pointer type for getting mod description
 *
 * Retrieves the description of a mod as declared in its manifest.
 *
 * @param[in] mod Mod handle to query
 * @param[out] out_desc Receives pointer to null-terminated description string (do not free)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if mod is NULL/invalid or out_desc is NULL
 *
 * @threadsafe Yes - returns pointer to immutable data
 * @lifetime mod - returned pointer valid for mod's lifetime
 */
typedef BML_Result (*PFN_BML_GetModDescription)(BML_Mod mod, const char **out_desc);

/**
 * @brief Function pointer type for getting mod author count
 *
 * @param[in] mod Mod handle to query
 * @param[out] out_count Receives the number of authors
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if mod is NULL/invalid or out_count is NULL
 *
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_GetModAuthorCount)(BML_Mod mod, uint32_t *out_count);

/**
 * @brief Function pointer type for getting mod author at index
 *
 * @param[in] mod Mod handle to query
 * @param[in] index Zero-based author index
 * @param[out] out_author Receives pointer to null-terminated author string (do not free)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if mod is NULL/invalid, out_author is NULL, or index out of range
 *
 * @threadsafe Yes - returns pointer to immutable data
 * @lifetime mod - returned pointer valid for mod's lifetime
 */
typedef BML_Result (*PFN_BML_GetModAuthorAt)(BML_Mod mod, uint32_t index, const char **out_author);

/** @} */ /* end CoreModInfo group */

/* ========================================================================
 * Manifest Custom Field Access
 * ======================================================================== */

/**
 * @defgroup CoreManifest Manifest Custom Fields
 * @brief Read custom fields from a mod's mod.toml manifest
 *
 * Modules can define custom top-level sections in mod.toml. These are
 * accessible at runtime via dot-separated paths:
 *
 * @code
 * # mod.toml
 * [mymod]
 * greeting = "Hello"
 * max_speed = 42
 * debug = true
 * scale = 1.5
 * @endcode
 *
 * Access: `bmlGetManifestString(mod, "mymod.greeting", &value)`
 * @{
 */

typedef BML_Result (*PFN_BML_GetManifestString)(BML_Mod mod, const char *path, const char **out_value);
typedef BML_Result (*PFN_BML_GetManifestInt)(BML_Mod mod, const char *path, int64_t *out_value);
typedef BML_Result (*PFN_BML_GetManifestFloat)(BML_Mod mod, const char *path, double *out_value);
typedef BML_Result (*PFN_BML_GetManifestBool)(BML_Mod mod, const char *path, BML_Bool *out_value);

/** @} */ /* end CoreManifest group */

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
 * // APIs that support implicit TLS lookup can now resolve the module context
 * // without taking an explicit BML_Mod parameter.
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
 * PFN_BML_GetCurrentModule getCurrentModule =
 *     (PFN_BML_GetCurrentModule)bmlGetProcAddress("bmlGetCurrentModule");
 * BML_Mod current = getCurrentModule ? getCurrentModule() : NULL;
 * if (current) {
 *     const char* id = NULL;
 *     getModId(current, &id);
 *     printf("Current module: %s\n", id);
 * }
 * @endcode
 */
typedef BML_Mod (*PFN_BML_GetCurrentModule)(void);

/**
 * @brief Function pointer type for getting the number of loaded modules.
 *
 * Returns the number of module handles currently loaded into the runtime.
 * The result can be paired with PFN_BML_GetLoadedModuleAt to iterate all
 * loaded modules.
 *
 * @return Number of currently loaded modules.
 *
 * @threadsafe Yes
 * @note Count and At are not atomic as a pair. If modules can be loaded or
 *       unloaded concurrently (e.g. hot-reload), the count may change between
 *       calls. GetLoadedModuleAt gracefully returns NULL for out-of-range
 *       indices so a stale count is safe -- just check for NULL.
 */
typedef uint32_t (*PFN_BML_GetLoadedModuleCount)(void);

/**
 * @brief Function pointer type for getting a loaded module handle by index.
 *
 * @param[in] index Zero-based module index.
 * @return Module handle for the given index, or NULL if the index is out of range.
 *
 * @threadsafe Yes
 */
typedef BML_Mod (*PFN_BML_GetLoadedModuleAt)(uint32_t index);

/**
 * @brief Function pointer type for finding a loaded module by its manifest ID.
 *
 * @param[in] id Null-terminated module ID string (e.g. "com.example.mymod")
 * @return Module handle if found, or NULL if no loaded module matches.
 *
 * @threadsafe Yes
 */
typedef BML_Mod (*PFN_BML_FindModuleById)(const char *id);

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
 * registerShutdownHook(my_mod, OnShutdown, &g_state);
 * @endcode
 * 
 * @see bml::ShutdownHook for C++ RAII wrapper
 */
typedef BML_Result (*PFN_BML_RegisterShutdownHook)(BML_Mod mod, BML_ShutdownCallback callback, void *user_data);

/** @} */ /* end CoreShutdown group */

/* ========================================================================
 * Core Capability Flags
 * ======================================================================== */

/* ========================================================================
 * Global Function Pointers
 * ======================================================================== */

/**
 * @defgroup CoreBootstrapGlobals Bootstrap Global Function Pointers
 * @brief Bootstrap-loaded Core proc pointers
 *
 * The bootstrap-only loader contract exposes only the thread-local module
 * binding helper as a global proc pointer. All other Core APIs must be
 * resolved explicitly through `bmlGetProcAddress(...)` or consumed through an
 * acquired builtin interface.
 * @{
 */

/** @brief Set thread-local module @see PFN_BML_SetCurrentModule */
extern PFN_BML_SetCurrentModule bmlSetCurrentModule;

/** @} */ /* end CoreBootstrapGlobals group */

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

#endif /* BML_CORE_H */
