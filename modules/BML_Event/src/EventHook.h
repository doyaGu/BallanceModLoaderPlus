/**
 * @file EventHook.h
 * @brief Game Event Hook for BML_Event Module
 * 
 * Hooks game scripts to capture and publish game events via IMC.
 * Depends on BML_ObjectLoad for script load notifications.
 */

#ifndef BML_EVENT_EVENTHOOK_H
#define BML_EVENT_EVENTHOOK_H

#include "CKAll.h"

namespace BML_Event {

/**
 * @brief Initialize event hooks
 * 
 * Registers topic IDs and sets up for script hooking.
 * 
 * @param ctx CKContext
 * @return true on success
 */
bool InitEventHooks(CKContext *ctx);

/**
 * @brief Shutdown event hooks
 */
void ShutdownEventHooks();

/**
 * @brief Register hooks for base event handler script
 * 
 * Called when "base.nmo" script is loaded.
 * 
 * @param script The base event handler behavior
 * @return true on success
 */
bool RegisterBaseEventHandler(CKBehavior *script);

/**
 * @brief Register hooks for gameplay ingame script
 * 
 * Called when "Gameplay_Ingame" script is loaded.
 * 
 * @param script The gameplay ingame behavior
 * @return true on success
 */
bool RegisterGameplayIngame(CKBehavior *script);

/**
 * @brief Register hooks for gameplay energy script
 * 
 * Called when "Gameplay_Energy" script is loaded.
 * 
 * @param script The gameplay energy behavior
 * @return true on success
 */
bool RegisterGameplayEnergy(CKBehavior *script);

/**
 * @brief Register hooks for gameplay events script
 * 
 * Called when "Gameplay_Events" script is loaded.
 * 
 * @param script The gameplay events behavior
 * @return true on success
 */
bool RegisterGameplayEvents(CKBehavior *script);

/**
 * @brief Handle script load notification
 * 
 * Called when a script is loaded (from BML_ObjectLoad subscription).
 * Determines script type and applies appropriate hooks.
 * 
 * @param filename Loaded filename
 * @param script Loaded script behavior
 */
void OnScriptLoaded(const char *filename, CKBehavior *script);

} // namespace BML_Event

#endif // BML_EVENT_EVENTHOOK_H
