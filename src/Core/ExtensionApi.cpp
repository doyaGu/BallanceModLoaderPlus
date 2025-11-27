#include "bml_extension.h"

#include "ApiRegistry.h"
#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "ModHandle.h"
#include "CoreErrors.h"
#include "bml_api_ids.h"
#include "bml_capabilities.h"

#include <algorithm>
#include <limits>

using namespace BML::Core;

/* ============================================================================
 * Extension Management C API Implementation
 * 
 * All extension APIs use the unified ApiRegistry for both core and extension
 * API management.
 * ============================================================================ */

static BML_Result BML_API_ExtensionRegister(
    const char* name,
    uint32_t version_major,
    uint32_t version_minor,
    const void* api_table,
    size_t api_size
) {
    if (!name || !api_table || api_size == 0) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    // Get current module to determine provider ID
    BML_Mod current_mod = Context::GetCurrentModule();
    if (!current_mod) {
        return BML_RESULT_INVALID_CONTEXT;
    }

    auto* mod_handle = Context::Instance().ResolveModHandle(current_mod);
    if (!mod_handle) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    // Register through unified ApiRegistry
    BML_ApiId id = ApiRegistry::Instance().RegisterExtension(
        name,
        version_major,
        version_minor,
        api_table,
        api_size,
        mod_handle->id
    );

    return (id != BML_API_INVALID_ID) ? BML_RESULT_OK : BML_RESULT_ALREADY_EXISTS;
}

static BML_Result BML_API_ExtensionQuery(
    const char* name,
    BML_ExtensionInfo* out_info,
    BML_Bool* out_supported
) {
    if (!name) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    ApiRegistry::ApiMetadata meta;
    bool found = ApiRegistry::Instance().TryGetMetadata(std::string(name), meta);

    if (out_supported) {
        *out_supported = found ? BML_TRUE : BML_FALSE;
    }

    if (!found) {
        return BML_RESULT_NOT_FOUND;
    }

    if (out_info && found) {
        out_info->name = meta.name;
        out_info->version_major = meta.version_major;
        out_info->version_minor = meta.version_minor;
        out_info->provider_id = meta.provider_mod;
        out_info->description = meta.description;
    }

    return BML_RESULT_OK;
}

static BML_Result BML_API_ExtensionLoad(
    const char* name,
    void** out_api_table
) {
    if (!name || !out_api_table) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    ApiRegistry::ApiMetadata meta;
    if (!ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
        return BML_RESULT_NOT_FOUND;
    }

    *out_api_table = meta.pointer;
    return BML_RESULT_OK;
}

static BML_Result BML_API_ExtensionLoadVersioned(
    const char* name,
    uint32_t required_major,
    uint32_t required_minor,
    void** out_api_table,
    uint32_t* out_actual_major,
    uint32_t* out_actual_minor
) {
    if (!name || !out_api_table) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    const void* api = nullptr;
    bool compatible = ApiRegistry::Instance().LoadVersioned(
        name,
        required_major,
        required_minor,
        &api,
        out_actual_major,
        out_actual_minor
    );

    if (!compatible) {
        // Check if extension exists but version doesn't match
        ApiRegistry::ApiMetadata meta;
        if (ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
            return BML_RESULT_VERSION_MISMATCH;
        }
        return BML_RESULT_NOT_FOUND;
    }

    *out_api_table = const_cast<void*>(api);
    return BML_RESULT_OK;
}

static BML_Result BML_API_ExtensionEnumerate(
    BML_ExtensionEnumCallback callback,
    void* user_data
) {
    if (!callback) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    // Wrapper struct for callback context
    struct EnumContext {
        BML_ExtensionEnumCallback callback;
        void* user_data;
    };
    
    EnumContext ctx{callback, user_data};

    // Use unified enumeration with extension filter
    ApiRegistry::Instance().Enumerate(
        [](BML_Context bmlCtx, const BML_ApiDescriptor* desc, void* raw_ctx) -> BML_Bool {
            auto* ctx = static_cast<EnumContext*>(raw_ctx);
            
            // Convert ApiDescriptor to ExtensionInfo
            BML_ExtensionInfo info{};
            info.struct_size = sizeof(BML_ExtensionInfo);
            info.name = desc->name;
            info.version_major = desc->version_major;
            info.version_minor = desc->version_minor;
            info.provider_id = desc->provider_mod;
            info.description = desc->description;
            
            ctx->callback(bmlCtx, &info, ctx->user_data);
            return BML_TRUE;  // Continue enumeration
        },
        &ctx,
        static_cast<int>(BML_API_TYPE_EXTENSION)  // Only enumerate extensions
    );
    
    return BML_RESULT_OK;
}

static BML_Result BML_API_ExtensionUnregister(const char* name) {
    if (!name) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    // Get current module to verify ownership
    BML_Mod current_mod = Context::GetCurrentModule();
    if (!current_mod) {
        return BML_RESULT_INVALID_CONTEXT;
    }

    auto* mod_handle = Context::Instance().ResolveModHandle(current_mod);
    if (!mod_handle) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    // Verify the extension belongs to this mod before allowing unregister
    ApiRegistry::ApiMetadata meta;
    if (!ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
        return BML_RESULT_NOT_FOUND;
    }

    if (!meta.provider_mod || meta.provider_mod != mod_handle->id) {
        return BML_RESULT_PERMISSION_DENIED;
    }

    bool success = ApiRegistry::Instance().Unregister(name);
    return success ? BML_RESULT_OK : BML_RESULT_NOT_FOUND;
}

static BML_Result BML_API_GetExtensionCaps(BML_ExtensionCaps* out_caps) {
    if (!out_caps) {
        return BML_RESULT_INVALID_ARGUMENT;
    }

    BML_ExtensionCaps caps{};
    caps.struct_size = sizeof(BML_ExtensionCaps);
    caps.api_version = bmlGetApiVersion();
    caps.capability_flags = BML_EXTENSION_CAP_REGISTER |
                            BML_EXTENSION_CAP_QUERY |
                            BML_EXTENSION_CAP_LOAD |
                            BML_EXTENSION_CAP_LOAD_VERSIONED |
                            BML_EXTENSION_CAP_ENUMERATE |
                            BML_EXTENSION_CAP_UNREGISTER;
    size_t count = ApiRegistry::Instance().GetExtensionCount();
    const auto clamped = std::min<size_t>(count, static_cast<size_t>((std::numeric_limits<uint32_t>::max)()));
    caps.max_extensions_hint = static_cast<uint32_t>(clamped);
    *out_caps = caps;
    return BML_RESULT_OK;
}

/* ============================================================================
 * Registration Entry Point
 * ============================================================================ */

namespace BML::Core {

void RegisterExtensionApis() {
    auto& registry = ApiRegistry::Instance();

    // Extension Management APIs
    BML_REGISTER_API_GUARDED(bmlExtensionRegister, "extension", BML_API_ExtensionRegister);
    BML_REGISTER_API_GUARDED(bmlExtensionQuery, "extension", BML_API_ExtensionQuery);
    BML_REGISTER_API_GUARDED(bmlExtensionLoad, "extension", BML_API_ExtensionLoad);
    BML_REGISTER_API_GUARDED(bmlExtensionLoadVersioned, "extension", BML_API_ExtensionLoadVersioned);
    BML_REGISTER_API_GUARDED(bmlExtensionEnumerate, "extension", BML_API_ExtensionEnumerate);
    BML_REGISTER_API_GUARDED(bmlExtensionUnregister, "extension", BML_API_ExtensionUnregister);
    BML_REGISTER_API_GUARDED(bmlGetExtensionCaps, "extension", BML_API_GetExtensionCaps);
}

} // namespace BML::Core
