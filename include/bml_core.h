/**
 * @file bml_core.h
 * @brief BML Core API - Context, Version, and Lifecycle Management
 * 
 * The Core API provides fundamental functionality for the BML runtime:
 * - Context lifecycle management with reference counting
 * - Runtime and API version querying
 * - Mod metadata access (ID, version)
 * - Capability request and checking
 * - Shutdown hook registration
 * 
 * @section core_threading Threading Model
 * Core APIs support concurrent access for thread-local state, retained context
 * handles, and internally synchronized stores. Global lifecycle transitions
 * such as attach, discover, load, and detach should still be serialized by
 * the host.
 * 
 * @section core_lifecycle Lifecycle
 * The runtime context is created during BML initialization and remains valid
 * until shutdown. Mods receive it through attach arguments or a bound runtime
 * context interface and may retain/release references for safe access.
 * 
 * Host code may resolve raw Core exports explicitly when bootstrapping.
 * Module code should consume the injected runtime services such as
 * `args->services->Context` and `args->services->Module`.
 *
 * @section core_usage Usage Example
 * @code
 * const BML_CoreContextInterface *ctxApi = args->services->Context;
 * BML_Context ctx = args->context;
 * if (ctxApi && ctxApi->Retain && ctxApi->Release && ctxApi->GetRuntimeVersion) {
 *     ctxApi->Retain(ctx);
 *
 *     const BML_Version *ver = ctxApi->GetRuntimeVersion(ctxApi->Context);
 *     printf("BML v%u.%u.%u\n", ver->major, ver->minor, ver->patch);
 *
 *     ctxApi->Release(ctx);
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
#include "bml_interface.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

#define BML_CORE_CONTEXT_INTERFACE_ID "bml.core.context"
#define BML_CORE_MODULE_INTERFACE_ID  "bml.core.module"

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
 * @see BML_CoreContextInterface for runtime-bound context access
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
 * const BML_CoreContextInterface *ctxApi = args->services->Context;
 * const BML_Version *ver =
 *     (ctxApi && ctxApi->GetRuntimeVersion) ? ctxApi->GetRuntimeVersion(ctxApi->Context) : NULL;
 * if (ver) {
 *     printf("Runtime: v%u.%u.%u\n", ver->major, ver->minor, ver->patch);
 * }
 * @endcode
 * 
 * @see bmlGetApiVersion() for compile-time API version
 */
typedef const BML_Version *(*PFN_BML_GetRuntimeVersion)(BML_Context ctx);

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
 * @brief Function pointer type for getting a mod's asset mount directory.
 *
 * Retrieves the manifest-defined asset mount relative to the mod directory.
 * Returns NULL when the mod does not declare an asset mount.
 *
 * @param[in] mod Mod handle to query
 * @param[out] out_mount Receives pointer to null-terminated UTF-8 path fragment, or NULL
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if mod is NULL/invalid or out_mount is NULL
 *
 * @threadsafe Yes - returns pointer to immutable cached data
 * @lifetime mod - returned pointer valid for mod's lifetime
 */
typedef BML_Result (*PFN_BML_GetModAssetMount)(BML_Mod mod, const char **out_mount);

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

typedef uint32_t (*PFN_BML_GetLoadedModuleCount)(BML_Context ctx);
typedef BML_Mod (*PFN_BML_GetLoadedModuleAt)(BML_Context ctx, uint32_t index);
typedef BML_Mod (*PFN_BML_FindModuleById)(BML_Context ctx, const char *id);

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
typedef BML_Result (*PFN_BML_RegisterShutdownHook)(BML_Mod owner,
                                                   BML_ShutdownCallback callback,
                                                   void *user_data);

typedef struct BML_CoreContextInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_GetRuntimeVersion GetRuntimeVersion;
    PFN_BML_ContextRetain Retain;
    PFN_BML_ContextRelease Release;
    PFN_BML_ContextSetUserData SetUserData;
    PFN_BML_ContextGetUserData GetUserData;
} BML_CoreContextInterface;

typedef struct BML_CoreModuleInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_GetModId GetModId;
    PFN_BML_GetModVersion GetModVersion;
    PFN_BML_RequestCapability RequestCapability;
    PFN_BML_CheckCapability CheckCapability;
    PFN_BML_RegisterShutdownHook RegisterShutdownHook;
    PFN_BML_GetLoadedModuleCount GetLoadedModuleCount;
    PFN_BML_GetLoadedModuleAt GetLoadedModuleAt;
    PFN_BML_GetManifestString GetManifestString;
    PFN_BML_GetManifestInt GetManifestInt;
    PFN_BML_GetManifestFloat GetManifestFloat;
    PFN_BML_GetManifestBool GetManifestBool;
    PFN_BML_GetModDirectory GetModDirectory;
    PFN_BML_GetModAssetMount GetModAssetMount;
    PFN_BML_GetModName GetModName;
    PFN_BML_GetModDescription GetModDescription;
    PFN_BML_GetModAuthorCount GetModAuthorCount;
    PFN_BML_GetModAuthorAt GetModAuthorAt;
    PFN_BML_FindModuleById FindModuleById;
} BML_CoreModuleInterface;

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
 * The bootstrap-only loader contract exposes no Core lifecycle globals.
 * Hosts may resolve raw Core exports explicitly; modules should consume the
 * injected runtime interfaces instead of proc lookup.
 * @{
 */
/** @} */ /* end CoreBootstrapGlobals group */

BML_END_CDECLS

#endif /* BML_CORE_H */
