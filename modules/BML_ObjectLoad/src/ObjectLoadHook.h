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

namespace BML_ObjectLoad {
    /**
     * @brief Initialize object load hooks
     *
     * Hooks into the ObjectLoad behavior to provide callbacks
     * when objects and scripts are loaded.
     *
     * @param context CKContext for the current session
     * @return true if hooks were installed successfully
     */
    bool InitializeObjectLoadHook(CKContext *context);

    /**
     * @brief Shutdown object load hooks
     *
     * Removes all hooks and restores original behavior.
     */
    void ShutdownObjectLoadHook();
} // namespace BML_ObjectLoad

#endif // BML_OBJECTLOAD_HOOK_H
