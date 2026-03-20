/**
 * @file PhysicsHook.h
 * @brief Physics system hook for CKIpionManager
 * 
 * Provides hooks into the Virtools physics engine (IpionManager)
 * to intercept and extend physics-related functionality.
 */

#ifndef BML_PHYSICS_HOOK_H
#define BML_PHYSICS_HOOK_H

#include "CKBaseManager.h"
#include "bml_builtin_interfaces.h"
#include "bml_services.hpp"

namespace BML_Physics {

/**
 * @brief Initialize physics hooks
 *
 * Hooks into CKIpionManager and Physicalize behavior to provide
 * physics-related callbacks to mods.
 * Uses ModuleServices pattern to access core services.
 *
 * @param context CKContext for the current session
 * @param services Module services for logging
 * @return true if hooks were installed successfully
 */
bool InitializePhysicsHook(CKContext *context, const bml::ModuleServices &services);

/**
 * @brief Shutdown physics hooks
 * 
 * Removes all physics hooks and restores original behavior.
 */
void ShutdownPhysicsHook();

/**
 * @brief Called after physics post-processing
 * 
 * Invokes the original PostProcess on the physics manager.
 */
void PhysicsPostProcess();

} // namespace BML_Physics

#endif // BML_PHYSICS_HOOK_H
