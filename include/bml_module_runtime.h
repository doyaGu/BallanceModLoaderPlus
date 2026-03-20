#ifndef BML_MODULE_RUNTIME_H
#define BML_MODULE_RUNTIME_H

/**
 * @file bml_module_runtime.h
 * @brief Module Runtime Provider interface
 *
 * Allows native modules to register themselves as loaders for non-DLL
 * module entry files (e.g. ".as" scripts). Core's ModuleLoader consults
 * registered providers before attempting LoadLibrary.
 *
 * Providers are registered via bmlRegisterRuntimeProvider() (resolved
 * through bmlGetProcAddress). They are NOT registered through
 * InterfaceRegistry --that system enforces unique IDs per interface_id
 * and cannot support multiple simultaneous providers.
 *
 * @section lifecycle Lifecycle Contract
 *   1. CanHandle()    --called during load, before LoadLibrary
 *   2. AttachModule() --called instead of LoadLibrary + entrypoint(ATTACH)
 *   3. PrepareDetach()--called instead of entrypoint(PREPARE_DETACH)
 *   4. DetachModule() --called instead of entrypoint(DETACH) + FreeLibrary
 *   5. ReloadModule() --called by HotReloadCoordinator for non-DLL reloads
 */

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_export.h"

BML_BEGIN_CDECLS

/**
 * @brief Vtable for a module runtime provider.
 *
 * A native module (e.g. BML_Scripting) populates a static instance of
 * this struct and registers it with bmlRegisterRuntimeProvider().
 *
 * The struct uses BML_InterfaceHeader-compatible layout (struct_size as
 * first field) for forward compatibility. New methods may be appended;
 * Core checks struct_size before accessing them.
 */
typedef struct BML_ModuleRuntimeProvider {
    /** @brief sizeof(BML_ModuleRuntimeProvider). Must be first field. */
    size_t struct_size;

    /**
     * @brief Check if this provider can load the given entry file.
     * @param entry_path Resolved absolute path to the entry file (UTF-8)
     * @return BML_TRUE if this provider handles the file type
     */
    BML_Bool (*CanHandle)(const char *entry_path);

    /**
     * @brief Attach (compile + initialize) a non-native module.
     *
     * Called by ModuleLoader after CreateModHandle() and setting
     * g_CurrentModule. The provider must compile/prepare the module
     * and invoke any initialization callbacks.
     *
     * @param mod        Module handle (already created by Core)
     * @param get_proc   BML API resolver (same as native modules get)
     * @param entry_path Resolved absolute path to entry file (UTF-8)
     * @param module_dir Module directory (UTF-8, from manifest)
     * @return BML_RESULT_OK on success
     */
    BML_Result (*AttachModule)(BML_Mod mod,
                               PFN_BML_GetProcAddress get_proc,
                               const char *entry_path,
                               const char *module_dir);

    /**
     * @brief Prepare for detach (gating check).
     * @param mod Module handle
     * @return BML_RESULT_OK to proceed, other value to block detach
     */
    BML_Result (*PrepareDetach)(BML_Mod mod);

    /**
     * @brief Detach and clean up a non-native module.
     *
     * Called instead of entrypoint(DETACH) + FreeLibrary. The provider
     * must invoke any cleanup callbacks and release compiled module state.
     * Core calls CleanupModuleKernelState() afterwards to handle IMC
     * subscriptions, timers, hooks, and interface leases.
     *
     * @param mod Module handle
     * @return BML_RESULT_OK on success
     */
    BML_Result (*DetachModule)(BML_Mod mod);

    /**
     * @brief Hot-reload a non-native module in place.
     *
     * Called by HotReloadCoordinator when source files change. The
     * provider manages the full reload cycle internally (compile new,
     * detach old, attach new) with rollback on failure.
     *
     * @param mod Module handle
     * @return BML_RESULT_OK on success, BML_RESULT_FAIL on compile error
     */
    BML_Result (*ReloadModule)(BML_Mod mod);

} BML_ModuleRuntimeProvider;

/**
 * @brief Initializer for BML_ModuleRuntimeProvider.
 *
 * Usage:
 * @code
 * static const BML_ModuleRuntimeProvider provider = {
 *     BML_MODULE_RUNTIME_PROVIDER_INIT,
 *     MyCanHandle, MyAttach, MyPrepareDetach, MyDetach, MyReload
 * };
 * @endcode
 */
#define BML_MODULE_RUNTIME_PROVIDER_INIT sizeof(BML_ModuleRuntimeProvider)

/** @brief Function type for bmlRegisterRuntimeProvider */
typedef BML_Result (*PFN_BML_RegisterRuntimeProvider)(
    const BML_ModuleRuntimeProvider *provider,
    const char *owner_id);

/** @brief Function type for bmlUnregisterRuntimeProvider */
typedef BML_Result (*PFN_BML_UnregisterRuntimeProvider)(
    const BML_ModuleRuntimeProvider *provider);

/**
 * @brief Reset all kernel-managed state for a module handle.
 *
 * Cleans up IMC subscriptions, timers, hooks, locale data, interface
 * registrations, and lease tracking attributed to the given module.
 * Intended for use by runtime providers during hot-reload: call after
 * DetachModule and before re-AttachModule so the new script version
 * starts with a clean slate.
 *
 * This is the public equivalent of Core's internal
 * CleanupModuleKernelState(). Safe to call multiple times.
 *
 * @param mod Module handle whose kernel state should be reset
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_HANDLE if mod is NULL or unrecognized
 */
typedef BML_Result (*PFN_BML_CleanupModuleState)(BML_Mod mod);

BML_END_CDECLS

#endif /* BML_MODULE_RUNTIME_H */
