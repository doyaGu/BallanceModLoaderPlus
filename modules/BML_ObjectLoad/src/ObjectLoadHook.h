/**
 * @file ObjectLoadHook.h
 * @brief Object loading hook for Virtools ObjectLoad behavior
 * 
 * Provides hooks into the ObjectLoad BB to intercept and
 * notify mods when objects/scripts are loaded.
 */

#ifndef BML_OBJECTLOAD_HOOK_H
#define BML_OBJECTLOAD_HOOK_H

#include "CKContext.h"
#include "bml_hook_context.h"

namespace BML_ObjectLoad {
    /**
     * @brief Initialize object load hooks
     *
     * Hooks into the ObjectLoad behavior to provide callbacks
     * when objects and scripts are loaded.
     * Uses ModuleServices pattern to access core services.
     *
     * @param context CKContext for the current session
     * @param services Module services for logging
     * @return true if hooks were installed successfully
     */
    bool InitializeObjectLoadHook(CKContext *context, const BML_HookContext *ctx);

    /**
     * @brief Publish the initial base-game object/script snapshot.
     *
     * Restores the legacy OnLoadGame behavior by emitting one synthetic
     * OnLoadObject event followed by OnLoadScript events for already-loaded
     * scripts once the base runtime is ready.
     *
     * @param context CKContext for the current session
     * @return true if the snapshot was published, or had already been published
     */
    bool PublishInitialLoadSnapshot(CKContext *context);

    /**
     * @brief Shutdown object load hooks
     *
     * Removes all hooks and restores original behavior.
     */
    void ShutdownObjectLoadHook();
} // namespace BML_ObjectLoad

#endif // BML_OBJECTLOAD_HOOK_H
