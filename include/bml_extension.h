#ifndef BML_EXTENSION_H
#define BML_EXTENSION_H

#include "bml_types.h"
#include "bml_errors.h"
#include "bml_version.h"
#include "bml_logging.h"
#include "bml_resource.h"

BML_BEGIN_CDECLS

/**
 * @file bml_extension.h
 * @brief Extension API for BML v2 - Modular API extension system
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
 * - **Discovery**: Enumerate all available extensions
 * 
 * @section extension_workflow Typical Workflow
 * 
 * **Provider Side (e.g.,
        param($match)
        $ws1 = $match.Groups[1].Value
        $typeName = $match.Groups[2].Value
        $ws2 = $match.Groups[3].Value
        if ($typeName -notmatch 'Fnposing ImGui):**
 * @code
 * // Define API structure
 * typedef struct BML_ImGuiApi {
 *     void (*Begin)(const char* name);
 *     void (*End)(void);
 *     void (*Text)(const char* fmt, ...);
 * } BML_ImGuiApi;
 * 
 * // During BML_ModEntrypoint attach:
 * static BML_ImGuiApi s_ImGuiApi = { ImGui_Begin, ImGui_End, ImGui_Text };
 * bmlExtensionRegister("BML_EXT_ImGui", 1, 0, &s_ImGuiApi, sizeof(s_ImGuiApi));
 * @endcode
 * 
 * **Consumer Side (e.g., MyMod using ImGui):**
 * @code
 * // During BML_ModEntrypoint attach:
 * BML_Bool available;
 * if (bmlExtensionQuery("BML_EXT_ImGui", NULL, &available) == BML_RESULT_OK && available) {
 *     BML_ImGuiApi* imgui = NULL;
 *     if (bmlExtensionLoad("BML_EXT_ImGui", (void**)&imgui) == BML_RESULT_OK) {
 *         imgui->Begin("My Window");
 *         imgui->Text("Hello from extension!");
 *         imgui->End();
 *     }
 * }
 * @endcode
 * 
 * @section extension_versioning Version Negotiation Rules
 * 
 * Extensions use semantic versioning (MAJOR.MINOR):
 * - **MAJOR**: Incremented for breaking changes (incompatible API modifications)
 * - **MINOR**: Incremented for backward-compatible additions
 * 
 * Compatibility matrix:
 * | Provider | Consumer Required | Result |
 * |----------|-------------------|--------|
 * | v1.5     | v1.3             | Compatible (same major, provider minor >= required) |
 * | v1.2     | v1.5             | Incompatible (provider minor < required) |
 * | v2.0     | v1.9             | Incompatible (major version mismatch) |
 * 
 * @section extension_naming Naming Conventions
 * 
 * Extension names should follow these conventions:
 * - **Core extensions**: `BML_EXT_*` (e.g., `BML_EXT_ImGui`, `BML_EXT_Input`)
 * - **Third-party extensions**: `<ModID>_EXT_*` (e.g., `MyMod_EXT_CustomPhysics`)
 * 
 * This prevents naming conflicts between different modules.
 */

/* ========== Extension System Types ========== */

#include "bml_types.h"
#include "bml_errors.h"

/**
 * @defgroup ExtensionAPI Extension API
 * @brief Dynamic extension system for module-provided APIs
 * @{
 */

/* ========== Extension Metadata ========== */

/**
 * @brief Metadata describing an extension
 * 
 * This structure is returned by query and enumeration operations to
 * provide information about available extensions.
 */
typedef struct BML_ExtensionInfo {
    size_t struct_size;         ///< sizeof(BML_ExtensionInfo), must be first field (Task 1.2)
    const char* name;           ///< Extension name (e.g., "BML_EXT_ImGui")
    uint32_t version_major;     ///< Major version (breaking changes)
    uint32_t version_minor;     ///< Minor version (backward-compatible additions)
    const char* provider_id;    ///< Module ID that provides this extension
    const char* description;    ///< Human-readable description (may be NULL)
} BML_ExtensionInfo;

/**
 * @def BML_EXTENSION_INFO_INIT
 * @brief Static initializer for BML_ExtensionInfo
 */
#define BML_EXTENSION_INFO_INIT { sizeof(BML_ExtensionInfo), NULL, 0, 0, NULL, NULL }

typedef enum BML_ExtensionCapabilityFlags {
    BML_EXTENSION_CAP_REGISTER       = 1u << 0,
    BML_EXTENSION_CAP_QUERY          = 1u << 1,
    BML_EXTENSION_CAP_LOAD           = 1u << 2,
    BML_EXTENSION_CAP_LOAD_VERSIONED = 1u << 3,
    BML_EXTENSION_CAP_ENUMERATE      = 1u << 4,
    BML_EXTENSION_CAP_UNREGISTER     = 1u << 5,
    _BML_EXTENSION_CAP_FORCE_32BIT   = 0x7FFFFFFF  /**< Force 32-bit enum (Task 1.4) */
} BML_ExtensionCapabilityFlags;

typedef struct BML_ExtensionCaps {
    size_t struct_size;            /**< sizeof(BML_ExtensionCaps), must be first field */
    BML_Version api_version;
    uint32_t capability_flags;
    uint32_t max_extensions_hint;
} BML_ExtensionCaps;

/**
 * @def BML_EXTENSION_CAPS_INIT
 * @brief Static initializer for BML_ExtensionCaps
 */
#define BML_EXTENSION_CAPS_INIT { sizeof(BML_ExtensionCaps), BML_VERSION_INIT(0,0,0), 0, 0 }

/* ========== Extension Management ========== */

/**
 * @brief Registers an extension API
 * 
 * Makes an API table available for other modules to load. The API table
 * must remain valid for the lifetime of the provider module.
 * 
 * @param[in] name Extension name (must follow naming conventions)
 * @param[in] version_major Major version number
 * @param[in] version_minor Minor version number
 * @param[in] api_table Pointer to API function table (must remain valid)
 * @param[in] api_size Size of API struct in bytes (for version checking)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_PARAM if name or api_table is NULL
 * @return BML_RESULT_ALREADY_EXISTS if extension name is already registered
 * 
 * @note Call this during BML_ModEntrypoint attach after setting up your API table
 * @note The api_table pointer must remain valid until module shutdown
 * @note Use static or module-lifetime heap allocation for the API table
 * 
 * @warning Extension names must be globally unique
 * @warning Unregistration happens automatically during module shutdown
 * 
 * @code
 * typedef struct MyApi {
 *     int (*GetValue)(void);
 *     void (*SetValue)(int value);
 * } MyApi;
 * 
 * static MyApi s_MyApi = { GetValue, SetValue };
 * 
 * // During BML_ModEntrypoint attach:
 * bmlExtensionRegister("MyMod_EXT_Core", 1, 0, &s_MyApi, sizeof(MyApi));
 * @endcode
 */
typedef BML_Result (*PFN_BML_ExtensionRegister)(
    const char* name,
    uint32_t version_major,
    uint32_t version_minor,
    const void* api_table,
    size_t api_size
);

/**
 * @brief Queries if an extension is available
 * 
 * Checks whether an extension is registered without loading it.
 * Useful for feature detection and version checking.
 * 
 * @param[in] name Extension name to query
 * @param[out] out_info Receives extension metadata (may be NULL if not needed)
 * @param[out] out_supported Receives BML_TRUE if available, BML_FALSE otherwise (may be NULL)
 * @return BML_RESULT_OK if the query succeeded (extension may or may not exist)
 * @return BML_RESULT_NOT_FOUND if extension doesn't exist
 * @return BML_RESULT_INVALID_PARAM if name is NULL
 * 
 * @note Both out_info and out_supported are optional (can be NULL)
 * @note Use this before attempting to load to avoid unnecessary errors
 * 
 * @code
 * BML_ExtensionInfo info;
 * BML_Bool available;
 * if (bmlExtensionQuery("BML_EXT_ImGui", &info, &available) == BML_RESULT_OK) {
 *     if (available) {
 *         printf("ImGui v%u.%u available from %s\n",
 *                info.version_major, info.version_minor, info.provider_id);
 *     }
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_ExtensionQuery)(
    const char* name,
    BML_ExtensionInfo* out_info,
    BML_Bool* out_supported
);

/**
 * @brief Loads an extension's API table
 * 
 * Retrieves the API function table for an extension. No version checking
 * is performed - use ExtensionLoadVersioned for version negotiation.
 * 
 * @param[in] name Extension name to load
 * @param[out] out_api_table Receives pointer to the API table (must not be NULL)
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if extension doesn't exist
 * @return BML_RESULT_INVALID_PARAM if name or out_api_table is NULL
 * 
 * @note The returned pointer remains valid until the provider module is unloaded
 * @note No reference counting - do not cache the pointer across module lifetimes
 * @note Prefer ExtensionLoadVersioned for production code
 * 
 * @code
 * BML_ImGuiApi* imgui = NULL;
 * if (bmlExtensionLoad("BML_EXT_ImGui", (void**)&imgui) == BML_RESULT_OK) {
 *     imgui->Begin("Window");
 *     imgui->Text("Hello!");
 *     imgui->End();
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_ExtensionLoad)(
    const char* name,
    void** out_api_table
);

/**
 * @brief Loads an extension with version negotiation
 * 
 * Like ExtensionLoad, but performs version compatibility checking before
 * returning the API table. This is the recommended way to load extensions
 * in production code.
 * 
 * @param[in] name Extension name to load
 * @param[in] required_major Required major version (must match exactly)
 * @param[in] required_minor Minimum required minor version
 * @param[out] out_api_table Receives pointer to the API table (must not be NULL)
 * @param[out] out_actual_major Receives actual major version (may be NULL)
 * @param[out] out_actual_minor Receives actual minor version (may be NULL)
 * @return BML_RESULT_OK if compatible and loaded successfully
 * @return BML_RESULT_VERSION_MISMATCH if version is incompatible
 * @return BML_RESULT_NOT_FOUND if extension doesn't exist
 * @return BML_RESULT_INVALID_PARAM if name or out_api_table is NULL
 * 
 * @note Major version must match exactly (breaking changes)
 * @note Provider's minor version must be >= required_minor
 * @note out_actual_major and out_actual_minor are optional
 * 
 * @section version_negotiation Version Negotiation Examples
 * 
 * @code
 * // Require ImGui v1.3 or higher (same major version)
 * BML_ImGuiApi* imgui = NULL;
 * uint32_t actual_major, actual_minor;
 * BML_Result result = bmlExtensionLoadVersioned(
 *     "BML_EXT_ImGui",
 *     1,     // Major must be 1
 *     3,     // Minor must be >= 3
 *     (void**)&imgui,
 *     &actual_major,
 *     &actual_minor
 * );
 * 
 * if (result == BML_RESULT_OK) {
 *     printf("Loaded ImGui v%u.%u (compatible)\n", actual_major, actual_minor);
 * } else if (result == BML_RESULT_VERSION_MISMATCH) {
 *     printf("ImGui version incompatible (need v1.3+)\n");
 * }
 * @endcode
 */
typedef BML_Result (*PFN_BML_ExtensionLoadVersioned)(
    const char* name,
    uint32_t required_major,
    uint32_t required_minor,
    void** out_api_table,
    uint32_t* out_actual_major,
    uint32_t* out_actual_minor
);

/**
 * @brief Enumerates all registered extensions
 * 
 * Calls the provided callback for each registered extension, passing
 * its metadata. Useful for debugging, tooling, and discovery.
 * 
 * @param[in] callback Function to call for each extension (must not be NULL)
 * @param[in] user_data User-defined pointer passed to each callback invocation
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_INVALID_PARAM if callback is NULL
 * 
 * @note Do not register/unregister extensions from within the callback
 * @note The ExtensionInfo pointer is valid only during the callback
 * 
 * @code
 * void list_extension(const BML_ExtensionInfo* info, void* user_data) {
 *     printf("Extension: %s v%u.%u (from %s)\n",
 *            info->name, info->version_major, info->version_minor,
 *            info->provider_id);
 *     if (info->description) {
 *         printf("  Description: %s\n", info->description);
 *     }
 * }
 * 
 * bmlExtensionEnumerate(list_extension, NULL);
 * @endcode
 */
/**
 * @brief Extension enumeration callback (Task 2.3 - unified signature)
 * @param[in] ctx BML context (first parameter for consistency)
 * @param[in] info Extension metadata
 * @param[in] user_data User-provided context (always last parameter)
 */
typedef void (*BML_ExtensionEnumCallback)(BML_Context ctx,
                                          const BML_ExtensionInfo* info,
                                          void* user_data);

typedef BML_Result (*PFN_BML_ExtensionEnumerate)(
    BML_ExtensionEnumCallback callback,
    void* user_data
);

/**
 * @brief Unregisters an extension
 * 
 * Removes an extension from the registry. This is automatically called
 * during module shutdown, but can be called earlier if needed.
 * 
 * @param[in] name Extension name to unregister
 * @return BML_RESULT_OK on success
 * @return BML_RESULT_NOT_FOUND if extension doesn't exist
 * @return BML_RESULT_INVALID_PARAM if name is NULL
 * 
 * @warning Unsafe if other modules hold references to the API
 * @warning Ensure proper coordination via dependency ordering or shutdown hooks
 * @note Automatic unregistration happens during BML_ModEntrypoint detach
 * 
 * @code
 * // Explicit unregistration (usually not needed):
 * bmlExtensionUnregister("MyMod_EXT_Core");
 * @endcode
 */
typedef BML_Result (*PFN_BML_ExtensionUnregister)(const char* name);

typedef BML_Result (*PFN_BML_GetExtensionCaps)(BML_ExtensionCaps* out_caps);

/** @} */ // end of ExtensionAPI group

/* ========== Global Function Pointers ========== */

/**
 * @name Global Extension Function Pointers
 * @brief Convenient global pointers populated by bmlLoadAPI()
 * @{
 */
extern PFN_BML_ExtensionRegister      bmlExtensionRegister;
extern PFN_BML_ExtensionQuery         bmlExtensionQuery;
extern PFN_BML_ExtensionLoad          bmlExtensionLoad;
extern PFN_BML_ExtensionLoadVersioned bmlExtensionLoadVersioned;
extern PFN_BML_ExtensionEnumerate     bmlExtensionEnumerate;
extern PFN_BML_ExtensionUnregister    bmlExtensionUnregister;
extern PFN_BML_GetExtensionCaps       bmlGetExtensionCaps;

/* ========== Optional Extensibility Hooks ========== */

typedef struct BML_ConfigLoadContext {
    uint32_t struct_size;
    BML_Version api_version;
    BML_Mod mod;
    const char *mod_id;
    const char *config_path;
} BML_ConfigLoadContext;

/**
 * @brief Config load callback (Task 2.3 - unified signature)
 * 
 * @param[in] ctx BML context (first parameter for consistency)
 * @param[in] load_ctx Config load context with mod information
 * @param[in] user_data User-provided context (always last parameter)
 * 
 * @threadsafe Callbacks may be invoked from any thread
 */
typedef void (*BML_ConfigLoadCallback)(BML_Context ctx, const BML_ConfigLoadContext *load_ctx, void *user_data);

typedef struct BML_ConfigLoadHooks {
    uint32_t struct_size;
    BML_ConfigLoadCallback on_pre_load;
    BML_ConfigLoadCallback on_post_load;
    void *user_data;
} BML_ConfigLoadHooks;

typedef struct BML_LogMessageInfo {
    uint32_t struct_size;
    BML_Version api_version;
    BML_Mod mod;
    const char *mod_id;
    BML_LogSeverity severity;
    const char *tag;
    const char *message;
    const char *formatted_line;
} BML_LogMessageInfo;

/**
 * @brief Log dispatch callback (Task 2.3 - unified signature)
 * 
 * @param[in] ctx BML context (first parameter for consistency)
 * @param[in] info Log message information
 * @param[in] user_data User-provided context (always last parameter)
 * 
 * @threadsafe Callbacks may be invoked from any thread
 */
typedef void (*BML_LogDispatchCallback)(BML_Context ctx, const BML_LogMessageInfo *info, void *user_data);

typedef enum BML_LogSinkOverrideFlags {
    BML_LOG_SINK_OVERRIDE_SUPPRESS_DEFAULT = 1u << 0,
    _BML_LOG_SINK_OVERRIDE_FORCE_32BIT     = 0x7FFFFFFF  /**< Force 32-bit enum (Task 1.4) */
} BML_LogSinkOverrideFlags;

typedef struct BML_LogSinkOverrideDesc {
    uint32_t struct_size;
    uint32_t flags;
    BML_LogDispatchCallback dispatch;
    void (*on_shutdown)(void *user_data);
    void *user_data;
} BML_LogSinkOverrideDesc;

/**
 * @brief Resource handle finalize callback (Task 2.3 - unified signature)
 * 
 * @param[in] ctx BML context (first parameter for consistency)
 * @param[in] desc Handle descriptor being finalized
 * @param[in] user_data User-provided context (always last parameter)
 * 
 * @threadsafe Callbacks may be invoked from any thread
 */
typedef void (*BML_ResourceHandleFinalize)(BML_Context ctx, const BML_HandleDesc *desc, void *user_data);

typedef struct BML_ResourceTypeDesc {
    uint32_t struct_size;
    const char *name;
    BML_ResourceHandleFinalize on_finalize;
    void *user_data;
} BML_ResourceTypeDesc;

typedef BML_Result (*PFN_BML_RegisterConfigLoadHooks)(const BML_ConfigLoadHooks *hooks);
typedef BML_Result (*PFN_BML_RegisterLogSinkOverride)(const BML_LogSinkOverrideDesc *desc);
typedef BML_Result (*PFN_BML_ClearLogSinkOverride)(void);
typedef BML_Result (*PFN_BML_RegisterResourceType)(const BML_ResourceTypeDesc *desc,
                                                   BML_HandleType *out_type);

extern PFN_BML_RegisterConfigLoadHooks   bmlRegisterConfigLoadHooks;
extern PFN_BML_RegisterLogSinkOverride   bmlRegisterLogSinkOverride;
extern PFN_BML_ClearLogSinkOverride      bmlClearLogSinkOverride;
extern PFN_BML_RegisterResourceType      bmlRegisterResourceType;
/** @} */

BML_END_CDECLS

#endif /* BML_EXTENSION_H */