#ifndef BML_HOOK_H
#define BML_HOOK_H

/**
 * @file bml_hook.h
 * @brief Hook Registry API for centralized hook visibility and conflict detection.
 *
 * Provides an advisory registry so modules can declare which addresses they
 * hook and the system can detect conflicts. The registry does NOT control
 * MinHook -- it provides visibility, not enforcement.
 *
 * Usage:
 * @code
 *   BML_HookDesc desc = BML_HOOK_DESC_INIT;
 *   desc.target_name    = "CKRenderContext::Render";
 *   desc.target_address = (void *)original_render_addr;
 *   desc.priority       = 0;
 *   services->HookRegistry->Register(mod, &desc);
 *
 *   // Conflicts are logged as warnings and visible via Enumerate().
 *   // The CONFLICT flag is set on the internal registry entry, not on
 *   // the input descriptor (which is const).
 * @endcode
 */

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_interface.h"

BML_BEGIN_CDECLS

#define BML_CORE_HOOK_REGISTRY_INTERFACE_ID "bml.core.hook_registry"

/* ========================================================================
 * Hook Flags
 * ======================================================================== */

#define BML_HOOK_FLAG_NONE     0x00000000u
/** Set by the registry on output when a conflict is detected. */
#define BML_HOOK_FLAG_CONFLICT 0x00000001u

/* ========================================================================
 * Hook Types
 * ======================================================================== */

#define BML_HOOK_TYPE_UNKNOWN    0  /**< Unspecified hook mechanism */
#define BML_HOOK_TYPE_VTABLE     1  /**< VTable slot replacement */
#define BML_HOOK_TYPE_INLINE     2  /**< MinHook inline patch */
#define BML_HOOK_TYPE_WIN32_API  3  /**< Win32 API hook (MH_CreateHookApi) */
#define BML_HOOK_TYPE_BEHAVIOR   4  /**< CK Behavior function replacement */

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
    uint32_t hook_type;         /**< BML_HOOK_TYPE_* mechanism identifier */
} BML_HookDesc;

/** @brief Static initializer for BML_HookDesc */
#define BML_HOOK_DESC_INIT { sizeof(BML_HookDesc), NULL, NULL, 0, 0, 0 }

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
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_HookRegister)(BML_Mod owner, const BML_HookDesc *desc);

/**
 * @brief Unregister a hook for the calling module at target_address.
 *
 * @param[in] target_address The hooked address to unregister
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if no hook registered by this module at that address
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_HookUnregister)(BML_Mod owner, void *target_address);

/**
 * @brief Enumerate all registered hooks.
 *
 * @param[in] ctx       Runtime context
 * @param[in] callback  Called once per registered hook
 * @param[in] user_data Forwarded to callback
 * @return BML_RESULT_OK on success
 * @threadsafe Yes
 */
typedef BML_Result (*PFN_BML_HookEnumerate)(BML_Context ctx,
                                            BML_HookEnumCallback callback,
                                            void *user_data);

typedef struct BML_CoreHookRegistryInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_HookRegister Register;
    PFN_BML_HookUnregister Unregister;
    PFN_BML_HookEnumerate Enumerate;
} BML_CoreHookRegistryInterface;

BML_END_CDECLS

/* Compile-time assertions */
#ifdef __cplusplus
#include <cstddef>
static_assert(offsetof(BML_HookDesc, struct_size) == 0, "BML_HookDesc.struct_size must be at offset 0");
#endif

#endif /* BML_HOOK_H */
