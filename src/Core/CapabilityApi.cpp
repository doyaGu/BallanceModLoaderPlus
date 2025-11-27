/**
 * @file CapabilityApi.cpp
 * @brief Implementation of capability query and API discovery C APIs
 * 
 * This file implements the C API for:
 * - Capability querying (bmlQueryCapabilities, bmlHasCapability)
 * - API discovery (bmlGetApiDescriptor, bmlEnumerateApis)
 * - Version compatibility checking (bmlCheckCompatibility)
 * - Extension registration (bmlRegisterExtensionApi)
 * 
 * @see include/bml_capabilities.h for API declarations
 * @see docs/api-improvements/03-feature-extensions.md for design
 */

// Include BML/Version.h first before other headers that may conflict
#include "BML/Version.h"  // For BML_MAJOR_VERSION, BML_MINOR_VERSION, BML_PATCH_VERSION

#include "bml_capabilities.h"
#include "ApiRegistrationMacros.h"
#include "ApiRegistry.h"
#include "Context.h"

#include <cstring>

using BML::Core::ApiRegistry;

// ============================================================================
// Capability Query APIs (Internal implementations)
// ============================================================================

static uint64_t BML_QueryCapabilities(void) {
    return ApiRegistry::Instance().GetTotalCapabilities();
}

static BML_Bool BML_HasCapability(uint64_t cap) {
    uint64_t available = ApiRegistry::Instance().GetTotalCapabilities();
    return ((available & cap) == cap) ? BML_TRUE : BML_FALSE;
}

static int BML_CheckCompatibility(const BML_VersionRequirement* requirement) {
    if (!requirement) {
        return -1; // Invalid argument
    }
    
    // Check BML version
    uint16_t current_major = BML_MAJOR_VERSION;
    uint16_t current_minor = BML_MINOR_VERSION;
    uint16_t current_patch = BML_PATCH_VERSION;
    
    // Major version must match exactly (breaking changes)
    if (current_major != requirement->min_major) {
        return -2; // Major version mismatch
    }
    
    // Minor version must be >= required
    if (current_minor < requirement->min_minor) {
        return -3; // Minor version too low
    }
    
    // If minor matches, check patch
    if (current_minor == requirement->min_minor && 
        current_patch < requirement->min_patch) {
        return -4; // Patch version too low
    }
    
    // Check required capabilities
    if (requirement->required_caps != 0) {
        uint64_t available = ApiRegistry::Instance().GetTotalCapabilities();
        if ((available & requirement->required_caps) != requirement->required_caps) {
            return -5; // Missing required capabilities
        }
    }
    
    return 0; // BML_OK - compatible
}

// ============================================================================
// API Discovery APIs (Internal implementations)
// ============================================================================

static BML_Bool BML_GetApiDescriptor(uint32_t id, BML_ApiDescriptor* out_desc) {
    if (!out_desc) {
        return BML_FALSE;
    }
    
    return ApiRegistry::Instance().GetDescriptor(id, out_desc) ? BML_TRUE : BML_FALSE;
}

static BML_Bool BML_GetApiDescriptorByName(const char* name, BML_ApiDescriptor* out_desc) {
    if (!name || !out_desc) {
        return BML_FALSE;
    }
    
    ApiRegistry::ApiMetadata meta;
    if (!ApiRegistry::Instance().TryGetMetadata(std::string(name), meta)) {
        return BML_FALSE;
    }
    
    out_desc->id = meta.id;
    out_desc->name = meta.name;
    out_desc->type = meta.type;
    out_desc->version_major = meta.version_major;
    out_desc->version_minor = meta.version_minor;
    out_desc->version_patch = meta.version_patch;
    out_desc->reserved = 0;
    out_desc->capabilities = meta.capabilities;
    out_desc->threading = meta.threading;
    out_desc->provider_mod = meta.provider_mod;
    out_desc->description = meta.description;
    out_desc->call_count = meta.call_count.load(std::memory_order_relaxed);
    
    return BML_TRUE;
}

static void BML_EnumerateApis(
    PFN_BML_ApiEnumerator callback, 
    void* user_data,
    BML_ApiType type_filter
) {
    if (!callback) {
        return;
    }
    
    // Callback signature matches exactly - no cast needed
    ApiRegistry::Instance().Enumerate(
        callback,
        user_data, 
        static_cast<int>(type_filter));
}

static uint32_t BML_GetApiIntroducedVersion(uint32_t id) {
    BML_ApiDescriptor desc;
    if (!BML_GetApiDescriptor(id, &desc)) {
        return 0; // Not found
    }
    
    // Encode version as (major << 16 | minor << 8 | patch)
    return (static_cast<uint32_t>(desc.version_major) << 16) |
           (static_cast<uint32_t>(desc.version_minor) << 8) |
           static_cast<uint32_t>(desc.version_patch);
}

// ============================================================================
// Extension Registration APIs (Internal implementations)
// ============================================================================

// Thread-local storage for current mod context (used by RegisterExtension)
static thread_local const char* g_CurrentModId = "BML";

static uint32_t BML_RegisterExtensionApi(
    const char* name,
    uint32_t version_major,
    uint32_t version_minor,
    const void* api_table,
    size_t api_size
) {
    if (!name || !api_table || api_size == 0) {
        return 0; // Invalid arguments
    }
    
    const char* provider = g_CurrentModId ? g_CurrentModId : "BML";
    if (auto* mod = BML::Core::Context::GetCurrentModule()) {
        provider = mod->id.c_str();
    }
    
    return ApiRegistry::Instance().RegisterExtension(
        name,
        version_major,
        version_minor,
        api_table,
        api_size,
        provider ? provider : "Unknown"
    );
}

// Internal function to set current mod context (called during mod loading)
extern "C" void bml_internal_set_current_mod(const char* mod_id) {
    g_CurrentModId = mod_id;
}

// ============================================================================
// API Registration
// ============================================================================

namespace BML::Core {

void RegisterCapabilityApis() {
    BML_BEGIN_API_REGISTRATION();
    
    /* Capability queries - simple functions */
    BML_REGISTER_API(bmlQueryCapabilities, BML_QueryCapabilities);
    BML_REGISTER_API(bmlHasCapability, BML_HasCapability);
    BML_REGISTER_API(bmlCheckCompatibility, BML_CheckCompatibility);
    
    /* API discovery */
    BML_REGISTER_API(bmlGetApiDescriptor, BML_GetApiDescriptor);
    BML_REGISTER_API(bmlGetApiDescriptorByName, BML_GetApiDescriptorByName);
    BML_REGISTER_API(bmlEnumerateApis, BML_EnumerateApis);
    BML_REGISTER_API(bmlGetApiIntroducedVersion, BML_GetApiIntroducedVersion);
    
    /* Extension registration */
    BML_REGISTER_API(bmlRegisterExtensionApi, BML_RegisterExtensionApi);
}

} // namespace BML::Core
