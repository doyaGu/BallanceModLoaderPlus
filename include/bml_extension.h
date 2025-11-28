/**
 * @file bml_extension.h
 * @brief Extension API for BML - Modular API extension system
 *
 * This header defines a plugin system inspired by OpenGL/Vulkan extensions,
 * allowing modules to expose custom APIs that other modules can dynamically
 * discover and load.
 *
 * @section extension_overview Overview
 *
 * The extension system enables:
 * - **API Providers**: Modules can register custom API tables for others to use
 * - **API Consumers**: Modules can query and load extensions at runtime
 * - **Version Negotiation**: Semantic versioning ensures ABI compatibility
 * - **Discovery**: Enumerate and filter available extensions
 * - **Lifecycle Management**: Track extension registration/unregistration events
 *
 * @section extension_workflow Typical Workflow
 *
 * **Provider Side (e.g., exposing ImGui):**
 * @code
 * // Define API structure
 * typedef struct BML_ImGuiApi {
 *     void (*Begin)(const char* name);
 *     void (*End)(void);
 *     void (*Text)(const char* fmt, ...);
 * } BML_ImGuiApi;
 *
 * // Create and register extension
 * static BML_ImGuiApi s_ImGuiApi = { ImGui_Begin, ImGui_End, ImGui_Text };
 *
 * BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
 * desc.name = "BML_EXT_ImGui";
 * desc.version = bmlMakeVersion(1, 0, 0);
 * desc.api_table = &s_ImGuiApi;
 * desc.api_size = sizeof(s_ImGuiApi);
 * desc.description = "ImGui integration for BML";
 * bmlExtensionRegister(&desc);
 * @endcode
 *
 * **Consumer Side (e.g., MyMod using ImGui):**
 * @code
 * BML_ImGuiApi* imgui = NULL;
 * BML_Version req = bmlMakeVersion(1, 0, 0);
 * if (bmlExtensionLoad("BML_EXT_ImGui", &req, (void**)&imgui, NULL) == BML_RESULT_OK) {
 *     imgui->Begin("My Window");
 *     imgui->Text("Hello from extension!");
 *     imgui->End();
 * }
 * @endcode
 *
 * @section extension_versioning Version Negotiation Rules
 *
 * Extensions use semantic versioning (MAJOR.MINOR.PATCH) via BML_Version:
 * - **MAJOR**: Incremented for breaking changes (incompatible API modifications)
 * - **MINOR**: Incremented for backward-compatible additions
 * - **PATCH**: Incremented for bug fixes
 *
 * Compatibility matrix:
 * | Provider | Consumer Required | Result |
 * |----------|-------------------|--------|
 * | v1.5.0   | v1.3.0           | Compatible (same major, provider minor >= required) |
 * | v1.2.0   | v1.5.0           | Incompatible (provider minor < required) |
 * | v2.0.0   | v1.9.0           | Incompatible (major version mismatch) |
 *
 * @section extension_naming Naming Conventions
 *
 * Extension names should follow these conventions:
 * - **Core extensions**: `BML_EXT_*` (e.g., `BML_EXT_ImGui`, `BML_EXT_Input`)
 * - **Third-party extensions**: `<ModID>_EXT_*` (e.g., `MyMod_EXT_CustomPhysics`)
 *
 * @section extension_dependencies Dependency Management
 *
 * Extension dependencies are managed at the mod level via mod manifest.
 * If your extension requires another extension, declare the providing mod
 * as a dependency in your mod's manifest.json. This ensures proper load
 * order and availability.
 */

#ifndef BML_EXTENSION_H
#define BML_EXTENSION_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"

BML_BEGIN_CDECLS

/**
 * @defgroup ExtensionAPI Extension API
 * @brief Dynamic extension system for module-provided APIs
 * @{
 */

/* ========================================================================
 * Extension State and Capability Enums
 * ======================================================================== */

/**
 * @brief Extension state enumeration
 */
typedef enum BML_ExtensionState {
    BML_EXTENSION_STATE_ACTIVE       = 0, /**< Extension is active and usable */
    BML_EXTENSION_STATE_DEPRECATED   = 1, /**< Extension is deprecated but still usable */
    BML_EXTENSION_STATE_DISABLED     = 2, /**< Extension is disabled */
    _BML_EXTENSION_STATE_FORCE_32BIT = 0x7FFFFFFF
} BML_ExtensionState;

/**
 * @brief Extension lifecycle event types
 */
typedef enum BML_ExtensionEvent {
    BML_EXTENSION_EVENT_REGISTERED   = 0, /**< Extension was registered */
    BML_EXTENSION_EVENT_UNREGISTERED = 1, /**< Extension was unregistered */
    BML_EXTENSION_EVENT_DEPRECATED   = 2, /**< Extension was marked deprecated */
    BML_EXTENSION_EVENT_UPDATED      = 3, /**< Extension metadata was updated */
    _BML_EXTENSION_EVENT_FORCE_32BIT = 0x7FFFFFFF
} BML_ExtensionEvent;

/**
 * @brief Capability flags for the extension subsystem
 */
typedef enum BML_ExtensionCapFlags {
    BML_EXTENSION_CAP_REGISTER     = 1u << 0, /**< Can register extensions */
    BML_EXTENSION_CAP_QUERY        = 1u << 1, /**< Can query extensions */
    BML_EXTENSION_CAP_LOAD         = 1u << 2, /**< Can load extensions */
    BML_EXTENSION_CAP_ENUMERATE    = 1u << 3, /**< Can enumerate extensions */
    BML_EXTENSION_CAP_UNREGISTER   = 1u << 4, /**< Can unregister extensions */
    BML_EXTENSION_CAP_UPDATE       = 1u << 5, /**< Can update extension metadata */
    BML_EXTENSION_CAP_LIFECYCLE    = 1u << 6, /**< Lifecycle hooks available */
    BML_EXTENSION_CAP_FILTER       = 1u << 7, /**< Advanced filtering available */
    _BML_EXTENSION_CAP_FORCE_32BIT = 0x7FFFFFFF
} BML_ExtensionCapFlags;

/* ========================================================================
 * Extension Metadata Structures
 * ======================================================================== */

/**
 * @brief Metadata describing a registered extension
 *
 * Returned by query and enumeration operations. All string pointers remain
 * valid only during the callback invocation or until the extension is unregistered.
 */
typedef struct BML_ExtensionInfo {
    size_t struct_size; /**< sizeof(BML_ExtensionInfo), must be first */

    /* Identity */
    const char *name;        /**< Extension name (e.g., "BML_EXT_ImGui") */
    const char *provider_id; /**< Module ID that provides this extension */

    /* Version - use BML_Version for consistency */
    BML_Version version; /**< Extension version (semantic versioning) */

    /* State and metadata */
    BML_ExtensionState state; /**< Current extension state */
    const char *description;  /**< Human-readable description (may be NULL) */
    size_t api_size;          /**< Size of the API table in bytes */
    uint64_t capabilities;    /**< Capability flags provided by this extension */

    /* Tags for categorization */
    const char *const*tags; /**< Array of tags (may be NULL) */
    uint32_t tag_count;     /**< Number of tags */

    /* Deprecation info */
    const char *replacement_name;    /**< Replacement extension name if deprecated (may be NULL) */
    const char *deprecation_message; /**< Deprecation message (may be NULL) */
} BML_ExtensionInfo;

#define BML_EXTENSION_INFO_INIT { sizeof(BML_ExtensionInfo), \
    NULL, NULL, BML_VERSION_INIT(0, 0, 0), BML_EXTENSION_STATE_ACTIVE, NULL, 0, 0, \
    NULL, 0, NULL, NULL }

/**
 * @brief Extension subsystem capabilities
 */
typedef struct BML_ExtensionCaps {
    size_t struct_size;        /**< sizeof(BML_ExtensionCaps), must be first */
    BML_Version api_version;   /**< Extension API version */
    uint32_t capability_flags; /**< Bitmask of BML_ExtensionCapFlags */
    uint32_t registered_count; /**< Currently registered extension count */
    uint32_t max_extensions;   /**< Maximum extensions (0 = unlimited) */
} BML_ExtensionCaps;

#define BML_EXTENSION_CAPS_INIT { sizeof(BML_ExtensionCaps), BML_VERSION_INIT(0,0,0), 0, 0, 0 }

/* ========================================================================
 * Extension Registration Descriptor
 * ======================================================================== */

/**
 * @brief Descriptor for registering an extension
 *
 * Provides full control over extension registration including metadata
 * and lifecycle callbacks.
 *
 * @note Extension dependencies are managed at the mod level via mod manifest.
 *       Extensions inherit the dependency graph of their providing mod.
 */
typedef struct BML_ExtensionDesc {
    size_t struct_size; /**< sizeof(BML_ExtensionDesc), must be first */

    /* Required fields */
    const char *name;      /**< Extension name (required) */
    BML_Version version;   /**< Extension version (semantic versioning) */
    const void *api_table; /**< Pointer to API function table (required) */
    size_t api_size;       /**< Size of API table in bytes (required) */

    /* Optional metadata */
    const char *description; /**< Human-readable description */
    uint64_t capabilities;   /**< Capability flags this extension provides */

    /* Tags for categorization */
    const char *const*tags; /**< Array of tags */
    uint32_t tag_count;     /**< Number of tags */

    /* Lifecycle callbacks */
    void (*on_load)(BML_Context ctx, const char *consumer_id, void *user_data);
    void (*on_unload)(BML_Context ctx, const char *consumer_id, void *user_data);
    void *user_data; /**< User data for callbacks */
} BML_ExtensionDesc;

#define BML_EXTENSION_DESC_INIT { sizeof(BML_ExtensionDesc), \
    NULL, BML_VERSION_INIT(0, 0, 0), NULL, 0, NULL, 0, NULL, 0, NULL, NULL, NULL }

/* ========================================================================
 * Extension Filter for Queries
 * ======================================================================== */

/**
 * @brief Filter criteria for extension enumeration
 */
typedef struct BML_ExtensionFilter {
    size_t struct_size; /**< sizeof(BML_ExtensionFilter), must be first */

    /* Name/provider pattern (glob-style, NULL = match all) */
    const char *name_pattern;     /**< Glob pattern for extension name */
    const char *provider_pattern; /**< Glob pattern for provider ID */

    /* Version requirements - use BML_Version for consistency */
    BML_Version min_version; /**< Minimum version (0.0.0 = any) */
    BML_Version max_version; /**< Maximum version (0.0.0 = any) */

    /* Capability filtering */
    uint64_t required_caps; /**< Required capability flags (0 = any) */

    /* State filtering */
    uint32_t include_states; /**< Bitmask of states to include (0 = all) */

    /* Tag filtering */
    const char *const*required_tags; /**< All must be present */
    uint32_t required_tag_count;
} BML_ExtensionFilter;

#define BML_EXTENSION_FILTER_INIT { sizeof(BML_ExtensionFilter), \
    NULL, NULL, BML_VERSION_INIT(0, 0, 0), BML_VERSION_INIT(0, 0, 0), 0, 0, NULL, 0 }

/* ========================================================================
 * Callback Types
 * ======================================================================== */

/**
 * @brief Extension enumeration callback
 * @param ctx BML context
 * @param info Extension metadata
 * @param user_data User-provided context
 * @return BML_TRUE to continue, BML_FALSE to stop
 */
typedef BML_Bool (*BML_ExtensionEnumCallback)(
    BML_Context ctx,
    const BML_ExtensionInfo *info,
    void *user_data
);

/**
 * @brief Extension lifecycle event callback
 * @param ctx BML context
 * @param event Event type
 * @param info Extension metadata
 * @param user_data User-provided context
 */
typedef void (*BML_ExtensionEventCallback)(
    BML_Context ctx,
    BML_ExtensionEvent event,
    const BML_ExtensionInfo *info,
    void *user_data
);

/* ========================================================================
 * Core Extension API Function Types
 * ======================================================================== */

/**
 * @brief Registers an extension
 * @param desc Extension descriptor
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_ARGUMENT if desc is invalid
 * @return BML_RESULT_ALREADY_EXISTS if name is already registered
 * @return BML_RESULT_NOT_FOUND if a dependency is not available
 */
typedef BML_Result (*PFN_BML_ExtensionRegister)(const BML_ExtensionDesc *desc);

/**
 * @brief Unregisters an extension
 * @param name Extension name to unregister
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if extension doesn't exist
 * @return BML_RESULT_PERMISSION_DENIED if caller is not the owner
 */
typedef BML_Result (*PFN_BML_ExtensionUnregister)(const char *name);

/**
 * @brief Queries extension information
 * @param name Extension name
 * @param out_info Receives extension metadata (may be NULL)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if extension doesn't exist
 */
typedef BML_Result (*PFN_BML_ExtensionQuery)(
    const char *name,
    BML_ExtensionInfo *out_info
);

/**
 * @brief Loads an extension's API table with version checking
 * @param name Extension name
 * @param required_version Minimum required version (major must match exactly, minor/patch are minimum)
 * @param out_api Receives pointer to the API table
 * @param out_info Receives extension info (may be NULL)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if extension doesn't exist
 * @return BML_RESULT_VERSION_MISMATCH if version is incompatible
 */
typedef BML_Result (*PFN_BML_ExtensionLoad)(
    const char *name,
    const BML_Version *required_version,
    void **out_api,
    BML_ExtensionInfo *out_info
);

/**
 * @brief Enumerates extensions matching filter criteria
 * @param filter Filter criteria (NULL = enumerate all)
 * @param callback Callback for each matching extension
 * @param user_data User context
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_ExtensionEnumerate)(
    const BML_ExtensionFilter *filter,
    BML_ExtensionEnumCallback callback,
    void *user_data
);

/**
 * @brief Counts extensions matching filter criteria
 * @param filter Filter criteria (NULL = count all)
 * @param out_count Receives the count
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_ExtensionCount)(
    const BML_ExtensionFilter *filter,
    uint32_t *out_count
);

/* ========================================================================
 * Extension Update API
 * ======================================================================== */

/**
 * @brief Updates extension API table (hot-reload support)
 * @param name Extension name
 * @param api_table New API table
 * @param api_size Size of API table
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if extension doesn't exist
 * @return BML_RESULT_PERMISSION_DENIED if caller is not the owner
 */
typedef BML_Result (*PFN_BML_ExtensionUpdateApi)(
    const char *name,
    const void *api_table,
    size_t api_size
);

/**
 * @brief Marks an extension as deprecated
 * @param name Extension name
 * @param replacement Optional replacement extension name
 * @param message Optional deprecation message
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_ExtensionDeprecate)(
    const char *name,
    const char *replacement,
    const char *message
);

/* ========================================================================
 * Lifecycle Listener API
 * ======================================================================== */

/**
 * @brief Adds a lifecycle event listener
 * @param callback Event callback
 * @param event_mask Bitmask of events to listen for (0 = all)
 * @param user_data User context
 * @param out_id Receives listener ID for removal
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_ExtensionAddListener)(
    BML_ExtensionEventCallback callback,
    uint32_t event_mask,
    void *user_data,
    uint64_t *out_id
);

/**
 * @brief Removes a lifecycle event listener
 * @param id Listener ID from AddListener
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_ExtensionRemoveListener)(uint64_t id);

/* ========================================================================
 * Capability Query
 * ======================================================================== */

/**
 * @brief Gets extension subsystem capabilities
 * @param out_caps Receives capabilities
 * @return BML_RESULT_OK on success
 */
typedef BML_Result (*PFN_BML_GetExtensionCaps)(BML_ExtensionCaps *out_caps);

/** @} */ /* end of ExtensionAPI group */

/* ========================================================================
 * Global Function Pointers
 * ======================================================================== */

/**
 * @name Extension API Global Pointers
 * @brief Function pointers populated by bmlLoadAPI()
 * @{
 */

/* Core APIs */
extern PFN_BML_ExtensionRegister bmlExtensionRegister;
extern PFN_BML_ExtensionUnregister bmlExtensionUnregister;
extern PFN_BML_ExtensionQuery bmlExtensionQuery;
extern PFN_BML_ExtensionLoad bmlExtensionLoad;
extern PFN_BML_ExtensionEnumerate bmlExtensionEnumerate;
extern PFN_BML_ExtensionCount bmlExtensionCount;

/* Update APIs */
extern PFN_BML_ExtensionUpdateApi bmlExtensionUpdateApi;
extern PFN_BML_ExtensionDeprecate bmlExtensionDeprecate;

/* Lifecycle APIs */
extern PFN_BML_ExtensionAddListener bmlExtensionAddListener;
extern PFN_BML_ExtensionRemoveListener bmlExtensionRemoveListener;

/* Capability query */
extern PFN_BML_GetExtensionCaps bmlGetExtensionCaps;

/** @} */

BML_END_CDECLS

/* ========================================================================
 * Compile-Time Assertions for ABI Stability
 * ======================================================================== */

#ifdef __cplusplus
#include <cstddef>
#define BML_EXT_OFFSETOF(type, member) offsetof(type, member)
#else
#include <stddef.h>
#define BML_EXT_OFFSETOF(type, member) offsetof(type, member)
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
#define BML_EXT_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define BML_EXT_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define BML_EXT_STATIC_ASSERT_CONCAT_(a, b) a##b
#define BML_EXT_STATIC_ASSERT_CONCAT(a, b) BML_EXT_STATIC_ASSERT_CONCAT_(a, b)
#define BML_EXT_STATIC_ASSERT(cond, msg) \
        typedef char BML_EXT_STATIC_ASSERT_CONCAT(bml_ext_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* Verify struct_size is at offset 0 */
BML_EXT_STATIC_ASSERT(BML_EXT_OFFSETOF(BML_ExtensionInfo, struct_size) == 0,
                      "BML_ExtensionInfo.struct_size must be at offset 0");
BML_EXT_STATIC_ASSERT(BML_EXT_OFFSETOF(BML_ExtensionCaps, struct_size) == 0,
                      "BML_ExtensionCaps.struct_size must be at offset 0");
BML_EXT_STATIC_ASSERT(BML_EXT_OFFSETOF(BML_ExtensionDesc, struct_size) == 0,
                      "BML_ExtensionDesc.struct_size must be at offset 0");
BML_EXT_STATIC_ASSERT(BML_EXT_OFFSETOF(BML_ExtensionFilter, struct_size) == 0,
                      "BML_ExtensionFilter.struct_size must be at offset 0");

/* Verify enums are 32-bit */
BML_EXT_STATIC_ASSERT(sizeof(BML_ExtensionState) == sizeof(int32_t),
                      "BML_ExtensionState must be 32-bit");
BML_EXT_STATIC_ASSERT(sizeof(BML_ExtensionEvent) == sizeof(int32_t),
                      "BML_ExtensionEvent must be 32-bit");
BML_EXT_STATIC_ASSERT(sizeof(BML_ExtensionCapFlags) == sizeof(int32_t),
                      "BML_ExtensionCapFlags must be 32-bit");

#endif /* BML_EXTENSION_H */
