#ifndef BML_INTERFACE_H
#define BML_INTERFACE_H

#include "bml_errors.h"
#include "bml_types.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

BML_DECLARE_HANDLE(BML_InterfaceLease);
BML_DECLARE_HANDLE(BML_InterfaceRegistration);

#define BML_CORE_DIAGNOSTIC_INTERFACE_ID       "bml.core.diagnostic"
#define BML_CORE_INTERFACE_CONTROL_INTERFACE_ID "bml.core.interface_control"

/* ========================================================================
 * Interface Vtable Header
 * ======================================================================== */

/**
 * @brief Standard header embedded at the start of every interface vtable.
 *
 * Makes every vtable self-describing: version, identity, and forward-
 * compatible sizing.  Cast any vtable pointer to @c BML_InterfaceHeader*
 * to introspect it without knowing the concrete type.
 *
 * @code
 * typedef struct BML_FooInterface {
 *     BML_InterfaceHeader header;
 *     BML_Result (*DoSomething)(int x);
 * } BML_FooInterface;
 *
 * const BML_FooInterface vtable = {
 *     BML_IFACE_HEADER(BML_FooInterface, "bml.foo", 1, 0),
 *     FooDoSomething,
 * };
 * @endcode
 */
typedef struct BML_InterfaceHeader {
    size_t struct_size;         /**< sizeof the containing vtable struct */
    uint16_t major;             /**< ABI major version (breaking changes) */
    uint16_t minor;             /**< ABI minor version (backward-compatible additions) */
    const char *interface_id;   /**< Dot-separated identity string, e.g. "bml.core.context" */
} BML_InterfaceHeader;

/**
 * @brief Designated initializer for BML_InterfaceHeader.
 *
 * @param type  The containing vtable struct type
 * @param id    Interface ID string literal
 * @param maj   Major version
 * @param min   Minor version
 */
#define BML_IFACE_HEADER(type, id, maj, min) \
    { sizeof(type), (maj), (min), (id) }

/**
 * @brief Check if a vtable has a specific member (forward-compatible field check).
 *
 * Returns true if the vtable is non-null, large enough to contain the member,
 * and the member's function pointer is non-null.
 *
 * @code
 * if (BML_IFACE_HAS(rpcIface, BML_ImcRpcInterface, CallRpcEx)) {
 *     rpcIface->CallRpcEx(...);
 * }
 * @endcode
 */
#ifdef __cplusplus
#define BML_IFACE_HAS(iface, type, member) \
    ((iface) != nullptr && \
     (iface)->header.struct_size >= offsetof(type, member) + sizeof(((type *)0)->member) && \
     (iface)->member != nullptr)
#endif

/* ========================================================================
 * Vtable Design Convention
 * ========================================================================
 *
 * Every interface vtable struct follows a uniform layout:
 *
 *   typedef struct BML_Core*Interface {
 *       BML_InterfaceHeader header;      // [Mandatory] first field
 *       BML_Context Context;             // [Mandatory] second field
 *       PFN_... FunctionPointers;        // subsystem-specific
 *   } BML_Core*Interface;
 *
 * STRUCTURAL INVARIANT:
 *   All vtables include `BML_Context Context` as the second field for
 *   consistency, future-proofing, and lifetime validation. The caller
 *   passes `interface->Context` to functions that require it.
 *
 * FIRST PARAMETER CONVENTION for PFN function pointers:
 *   Per-mod operation  -> BML_Mod owner    (Config.Get, Timer.Schedule)
 *   Global operation   -> BML_Context ctx  (MutexLock, TraceBegin)
 *   Handle operation   -> the handle       (Unsubscribe, FutureCancel)
 *   Stateless          -> no identity      (AtomicIncrement32)
 *
 *   A function MUST NOT take both BML_Mod and BML_Context.
 *   Global resource creation with ownership tracking uses:
 *       (BML_Context ctx, BML_Mod owner, ...)
 *
 * CALLBACK CONVENTION:
 *   Framework-delivered callbacks receive BML_Context ctx as first param
 *   and void *user_data as last param. Sync enumerators and destructors
 *   may omit ctx.
 *
 * EXTENSION RULES:
 *   - Append new PFN fields at the end only; never insert or delete.
 *   - Consumers detect new fields via header.struct_size.
 *   - New PFN fields may be NULL; callers must check before calling.
 *
 * ======================================================================== */

/* ========================================================================
 * Key-Value Metadata
 * ======================================================================== */

/**
 * @brief Single key-value metadata entry.
 *
 * Providers attach an array of these to @c BML_InterfaceDesc to publish
 * arbitrary metadata that tooling and other modules can query.  Keys are
 * conventionally dot-separated (e.g. @c "vendor.url").
 */
typedef struct BML_InterfaceMetadata {
    const char *key;
    const char *value;
} BML_InterfaceMetadata;

/* ========================================================================
 * Interface Descriptors
 * ======================================================================== */

/**
 * @brief Provider-side interface descriptor.
 *
 * Passed to @c bmlInterfaceRegister.  All pointer fields must remain
 * valid for the lifetime of the registration (the core copies strings
 * but references the implementation pointer directly).
 *
 * Forward-compatible: consumers check @c struct_size before accessing
 * fields added after the initial layout.
 */
typedef struct BML_InterfaceDesc {
    size_t struct_size;
    const char *interface_id;
    BML_Version abi_version;
    const void *implementation;
    size_t implementation_size;
    uint64_t flags;
    uint64_t capabilities;
    const char *description;            /**< Human-readable summary (optional, NULL ok) */
    const char *category;               /**< Grouping tag, e.g. "core", "input" (optional) */
    const char *superseded_by;          /**< Interface ID that replaces this one (NULL = not deprecated) */
    const BML_InterfaceMetadata *metadata;  /**< Extensible key-value array (optional) */
    uint32_t metadata_count;            /**< Number of entries in @c metadata */
} BML_InterfaceDesc;

#define BML_INTERFACE_DESC_INIT \
    { sizeof(BML_InterfaceDesc), NULL, BML_VERSION_INIT(0, 0, 0), NULL, 0, \
      0, 0, NULL, NULL, NULL, NULL, 0 }

/**
 * @brief Runtime view of a registered interface.
 *
 * Returned by reflection queries.  All strings point into core-owned
 * storage and are valid until the interface is unregistered.
 */
typedef struct BML_InterfaceRuntimeDesc {
    size_t struct_size;
    const char *interface_id;
    const char *provider_id;
    BML_Version abi_version;
    size_t implementation_size;
    uint64_t flags;
    uint64_t capabilities;
    const char *description;
    const char *category;
    const char *superseded_by;
    const BML_InterfaceMetadata *metadata;
    uint32_t metadata_count;
    uint32_t lease_count;               /**< Current number of active leases */
} BML_InterfaceRuntimeDesc;

#define BML_INTERFACE_RUNTIME_DESC_INIT \
    { sizeof(BML_InterfaceRuntimeDesc), NULL, NULL, BML_VERSION_INIT(0, 0, 0), \
      0u, 0u, 0u, NULL, NULL, NULL, NULL, 0u, 0u }

/* ========================================================================
 * Interface Flags
 * ======================================================================== */

#define BML_INTERFACE_FLAG_NONE             0ULL
#define BML_INTERFACE_FLAG_MAIN_THREAD_ONLY (1ULL << 0)
#define BML_INTERFACE_FLAG_INTERNAL         (1ULL << 1)
#define BML_INTERFACE_FLAG_HOST_OWNED       (1ULL << 2)
#define BML_INTERFACE_FLAG_TOKENIZED        (1ULL << 3)
#define BML_INTERFACE_FLAG_IMMUTABLE        (1ULL << 4)

/* ========================================================================
 * Function Pointer Types
 * ======================================================================== */

typedef void (*PFN_BML_InterfaceRuntimeEnumerator)(const BML_InterfaceRuntimeDesc *desc,
                                                   void *user_data);

/* Diagnostic reflection function pointer types */
typedef BML_Result (*PFN_BML_GetLastError)(BML_Context ctx, BML_ErrorInfo *out_error);
typedef void (*PFN_BML_ClearLastError)(BML_Context ctx);
typedef BML_Bool (*PFN_BML_DiagGetInterfaceDescriptor)(BML_Context ctx,
                                                       const char *interface_id,
                                                       BML_InterfaceRuntimeDesc *out_desc);
typedef void (*PFN_BML_DiagEnumerateInterfaces)(BML_Context ctx,
                                                PFN_BML_InterfaceRuntimeEnumerator callback,
                                                void *user_data,
                                                uint64_t required_flags_mask);
typedef BML_Bool (*PFN_BML_DiagInterfaceExists)(BML_Context ctx, const char *interface_id);
typedef BML_Bool (*PFN_BML_DiagIsInterfaceCompatible)(BML_Context ctx,
                                                      const char *interface_id,
                                                      const BML_Version *required_abi);
typedef void (*PFN_BML_DiagEnumerateByProvider)(BML_Context ctx,
                                                const char *provider_id,
                                                PFN_BML_InterfaceRuntimeEnumerator callback,
                                                void *user_data);
typedef void (*PFN_BML_DiagEnumerateByCapability)(BML_Context ctx,
                                                  uint64_t required_caps,
                                                  PFN_BML_InterfaceRuntimeEnumerator callback,
                                                  void *user_data);
typedef uint32_t (*PFN_BML_DiagGetInterfaceCount)(BML_Context ctx);
typedef uint32_t (*PFN_BML_DiagGetLeaseCount)(BML_Context ctx, const char *interface_id);

/* Host runtime contribution function pointer types */
typedef BML_Result (*PFN_BML_HostRegisterContribution)(BML_Mod provider_mod,
                                                        const char *host_interface_id,
                                                        BML_InterfaceRegistration *out_registration);
typedef BML_Result (*PFN_BML_HostUnregisterContribution)(BML_InterfaceRegistration registration);

/*
 * Interface runtime contract:
 *
 * - Consumers use acquire/release to obtain typed synchronous capabilities.
 * - Providers use register/unregister for ordinary interface publication.
 * - HOST_OWNED contribution lifetimes are not published through register/unregister;
 *   they must be mediated by the internal host runtime interface.
 *
 * These entry points are part of the runtime interface surface after bootstrap
 * has been established. They are distinct from the bootstrap minimum itself.
 */
typedef BML_Result (*PFN_BML_InterfaceRegister)(BML_Mod owner,
                                                const BML_InterfaceDesc *desc);
typedef BML_Result (*PFN_BML_InterfaceAcquire)(BML_Mod owner,
                                               const char *interface_id,
                                               const BML_Version *required_abi,
                                               const void **out_implementation,
                                               BML_InterfaceLease *out_lease);
typedef BML_Result (*PFN_BML_InterfaceRelease)(BML_InterfaceLease lease);
typedef BML_Result (*PFN_BML_InterfaceUnregister)(BML_Mod owner,
                                                  const char *interface_id);

typedef struct BML_CoreDiagnosticInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_GetLastError GetLastError;
    PFN_BML_ClearLastError ClearLastError;
    PFN_BML_GetErrorString GetErrorString;
    PFN_BML_DiagInterfaceExists InterfaceExists;
    PFN_BML_DiagGetInterfaceDescriptor GetInterfaceDescriptor;
    PFN_BML_DiagIsInterfaceCompatible IsInterfaceCompatible;
    PFN_BML_DiagGetInterfaceCount GetInterfaceCount;
    PFN_BML_DiagGetLeaseCount GetLeaseCount;
    PFN_BML_DiagEnumerateInterfaces EnumerateInterfaces;
    PFN_BML_DiagEnumerateByProvider EnumerateByProvider;
    PFN_BML_DiagEnumerateByCapability EnumerateByCapability;
} BML_CoreDiagnosticInterface;

typedef struct BML_CoreInterfaceControlInterface {
    BML_InterfaceHeader header;
    BML_Context Context;
    PFN_BML_InterfaceRegister Register;
    PFN_BML_InterfaceAcquire Acquire;
    PFN_BML_InterfaceRelease Release;
    PFN_BML_InterfaceUnregister Unregister;
} BML_CoreInterfaceControlInterface;

extern PFN_BML_InterfaceRegister bmlInterfaceRegister;
extern PFN_BML_InterfaceAcquire bmlInterfaceAcquire;
extern PFN_BML_InterfaceRelease bmlInterfaceRelease;
extern PFN_BML_InterfaceUnregister bmlInterfaceUnregister;

BML_END_CDECLS

/* Compile-time assertions */
#ifdef __cplusplus
#include <cstddef>
static_assert(offsetof(BML_InterfaceHeader, struct_size) == 0, "BML_InterfaceHeader.struct_size must be at offset 0");
static_assert(offsetof(BML_InterfaceDesc, struct_size) == 0, "BML_InterfaceDesc.struct_size must be at offset 0");
static_assert(offsetof(BML_InterfaceRuntimeDesc, struct_size) == 0, "BML_InterfaceRuntimeDesc.struct_size must be at offset 0");
#endif

#endif /* BML_INTERFACE_H */
