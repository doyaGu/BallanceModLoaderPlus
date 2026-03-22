#ifndef BML_HOOK_H
#define BML_HOOK_H

/**
 * @file bml_hook.h
 * @brief Hook Registry API for centralized hook visibility and conflict detection.
 *
 * Provides an advisory registry so modules can declare which addresses they
 * hook and the system can detect conflicts. The registry does NOT control
 * MinHook -- it provides visibility, not enforcement.
 */

#include "bml_types.h"
#include "bml_errors.h"

BML_BEGIN_CDECLS

/* ========================================================================
 * Hook Flags
 * ======================================================================== */

#define BML_HOOK_FLAG_NONE     0x00000000u
/** Set by the registry on output when a conflict is detected. */
#define BML_HOOK_FLAG_CONFLICT 0x00000001u

/* ========================================================================
 * Hook Descriptor
 * ======================================================================== */

/**
 * @brief Describes a single hook registration.
 */
typedef struct BML_HookDesc {
    size_t struct_size;         /**< sizeof(BML_HookDesc), must be first */
    const char *target_name;    /**< Human-readable (e.g. "CKRenderContext::Render") */
    void *target_address;       /**< Actual hooked address */
    int32_t priority;           /**< Lower = runs first in chain */
    uint32_t flags;             /**< BML_HOOK_FLAG_* (conflict flag set on output) */
} BML_HookDesc;

/** @brief Static initializer for BML_HookDesc */
#define BML_HOOK_DESC_INIT { sizeof(BML_HookDesc), NULL, NULL, 0, 0 }

/* ========================================================================
 * Callback Types
 * ======================================================================== */

/**
 * @brief Enumeration callback for iterating registered hooks.
 *
 * @param[in] desc            Hook descriptor
 * @param[in] owner_module_id Module ID that registered this hook
 * @param[in] user_data       User-provided context
 */
typedef void (*BML_HookEnumCallback)(const BML_HookDesc *desc,
                                      const char *owner_module_id,
                                      void *user_data);

/* ========================================================================
 * Function Pointer Typedefs
 * ======================================================================== */

/**
 * @brief Register a hook in the registry.
 *
 * If the target_address is already registered by another module, the call
 * still succeeds but BML_HOOK_FLAG_CONFLICT is set in the registry entry
 * and a warning is logged.
 *
 * @param[in] desc Hook descriptor (target_name and target_address required)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if desc or target_address is NULL
 */
typedef BML_Result (*PFN_BML_HookRegister)(const BML_HookDesc *desc);
typedef BML_Result (*PFN_BML_HookRegisterOwned)(BML_Mod owner, const BML_HookDesc *desc);

/**
 * @brief Unregister a hook for the calling module at target_address.
 *
 * @param[in] target_address The hooked address to unregister
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if no hook registered by this module at that address
 */
typedef BML_Result (*PFN_BML_HookUnregister)(void *target_address);
typedef BML_Result (*PFN_BML_HookUnregisterOwned)(BML_Mod owner, void *target_address);

/**
 * @brief Enumerate all registered hooks.
 *
 * @param[in] callback Called once per registered hook entry
 * @param[in] user_data Opaque pointer passed to callback
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if callback is NULL
 */
typedef BML_Result (*PFN_BML_HookEnumerate)(BML_HookEnumCallback callback,
                                             void *user_data);

BML_END_CDECLS

#endif /* BML_HOOK_H */
