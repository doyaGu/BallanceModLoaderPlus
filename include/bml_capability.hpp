/**
 * @file bml_capability.hpp
 * @brief BML C++ Capability and API Discovery Wrappers
 * 
 * Provides runtime capability querying and API enumeration.
 */

#ifndef BML_CAPABILITY_HPP
#define BML_CAPABILITY_HPP

#include "bml_loader.h"
#include "bml_capabilities.h"
#include "bml_errors.h"

#include <string_view>
#include <optional>
#include <vector>

namespace bml {

// ============================================================================
// Capability Query Functions
// ============================================================================

/**
 * @brief Query all capabilities at runtime
 * @return Bitmask of available capabilities
 * 
 * Example:
 *   uint64_t caps = bml::QueryCapabilities();
 *   if (caps & BML_CAP_IMC_RPC) {
 *       // Use RPC features
 *   }
 */
inline uint64_t QueryCapabilities() {
    return bmlQueryCapabilities ? bmlQueryCapabilities() : 0;
}

/**
 * @brief Check if a specific capability is available
 * @param cap Capability flag to check
 * @return true if capability is available
 */
inline bool HasCapability(uint64_t cap) {
    return bmlHasCapability && bmlHasCapability(cap) != BML_FALSE;
}

// ============================================================================
// Version Compatibility
// ============================================================================

/**
 * @brief Check version and capability compatibility
 * 
 * @param major Minimum major version
 * @param minor Minimum minor version
 * @param patch Minimum patch version
 * @param required_caps Required capability flags
 * @return true if compatible
 * 
 * Example:
 *   if (!bml::CheckCompatibility(0, 5, 0, BML_CAP_IMC_RPC)) {
 *       // Handle incompatibility
 *   }
 */
inline bool CheckCompatibility(uint16_t major, uint16_t minor, uint16_t patch, uint64_t required_caps = 0) {
    if (!bmlCheckCompatibility) return false;
    BML_VersionRequirement req = BML_VERSION_REQUIREMENT_INIT(major, minor, patch);
    req.required_caps = required_caps;
    return bmlCheckCompatibility(&req) == 0;
}

// Note: GetRuntimeVersion() is defined in bml_core.hpp to avoid ODR violations

// ============================================================================
// API Descriptor Wrapper
// ============================================================================

/**
 * @brief RAII wrapper for API descriptor
 * 
 * Example:
 *   if (auto api = bml::ApiDescriptor::ByName("bmlLog")) {
 *       std::cout << api->name() << " v" << api->versionMajor() << std::endl;
 *   }
 */
class ApiDescriptor {
public:
    ApiDescriptor() = default;
    
    /**
     * @brief Query API by ID
     * @param id API ID
     * @return Descriptor if found
     */
    static std::optional<ApiDescriptor> ById(uint32_t id) {
        if (!bmlGetApiDescriptor) return std::nullopt;
        BML_ApiDescriptor desc = BML_API_DESCRIPTOR_INIT;
        if (bmlGetApiDescriptor(id, &desc)) {
            return ApiDescriptor(desc);
        }
        return std::nullopt;
    }
    
    /**
     * @brief Query API by name
     * @param name API name
     * @return Descriptor if found
     */
    static std::optional<ApiDescriptor> ByName(std::string_view name) {
        if (!bmlGetApiDescriptorByName) return std::nullopt;
        BML_ApiDescriptor desc = BML_API_DESCRIPTOR_INIT;
        if (bmlGetApiDescriptorByName(name.data(), &desc)) {
            return ApiDescriptor(desc);
        }
        return std::nullopt;
    }
    
    // ========================================================================
    // Accessors
    // ========================================================================
    
    uint32_t id() const noexcept { return m_desc.id; }
    const char* name() const noexcept { return m_desc.name; }
    BML_ApiType type() const noexcept { return m_desc.type; }
    uint16_t versionMajor() const noexcept { return m_desc.version_major; }
    uint16_t versionMinor() const noexcept { return m_desc.version_minor; }
    uint16_t versionPatch() const noexcept { return m_desc.version_patch; }
    uint64_t capabilities() const noexcept { return m_desc.capabilities; }
    BML_ThreadingModel threading() const noexcept { return m_desc.threading; }
    const char* provider() const noexcept { return m_desc.provider_mod; }
    const char* description() const noexcept { return m_desc.description; }
    uint64_t callCount() const noexcept { return m_desc.call_count; }
    
    /**
     * @brief Get the raw descriptor
     */
    const BML_ApiDescriptor& raw() const noexcept { return m_desc; }
    
    // ========================================================================
    // Utility Methods
    // ========================================================================
    
    /**
     * @brief Check if API provides specific capability
     * @param cap Capability flag
     * @return true if API has the capability
     */
    bool hasCapability(uint64_t cap) const noexcept {
        return (m_desc.capabilities & cap) == cap;
    }
    
    /**
     * @brief Check if API is thread-safe
     * @return true if API can be called from any thread
     */
    bool isThreadSafe() const noexcept {
        return m_desc.threading == BML_THREADING_FREE;
    }
    
    /**
     * @brief Get encoded version as single uint32_t
     * @return Version encoded as (major << 16) | (minor << 8) | patch
     */
    uint32_t encodedVersion() const noexcept {
        return (static_cast<uint32_t>(m_desc.version_major) << 16) |
               (static_cast<uint32_t>(m_desc.version_minor) << 8) |
               static_cast<uint32_t>(m_desc.version_patch);
    }
    
private:
    explicit ApiDescriptor(const BML_ApiDescriptor& desc) : m_desc(desc) {}
    BML_ApiDescriptor m_desc{};
};

// ============================================================================
// API Enumeration
// ============================================================================

/**
 * @brief Enumerate all APIs with optional type filter
 * 
 * @tparam Callback Callable type: bool(const BML_ApiDescriptor&)
 * @param callback Callback function, return false to stop
 * @param type_filter Filter by API type, -1 for all
 * 
 * Example:
 *   bml::EnumerateApis([](const BML_ApiDescriptor& desc) {
 *       std::cout << desc.name << " v" << desc.version_major << std::endl;
 *       return true; // continue
 *   }, BML_API_TYPE_EXTENSION);
 */
template<typename Callback>
inline void EnumerateApis(Callback&& callback, int type_filter = -1) {
    if (!bmlEnumerateApis) return;
    
    struct Context {
        Callback* cb;
    } ctx{&callback};
    
    auto wrapper = [](BML_Context, const BML_ApiDescriptor* desc, void* user_data) -> BML_Bool {
        auto* c = static_cast<Context*>(user_data);
        return (*c->cb)(*desc) ? BML_TRUE : BML_FALSE;
    };
    
    bmlEnumerateApis(wrapper, &ctx, static_cast<BML_ApiType>(type_filter));
}

/**
 * @brief Get all APIs as a vector
 * @param type_filter Filter by API type, -1 for all
 * @return Vector of API descriptors
 */
inline std::vector<ApiDescriptor> GetAllApis(int type_filter = -1) {
    std::vector<ApiDescriptor> result;
    EnumerateApis([&result](const BML_ApiDescriptor& desc) {
        auto api = ApiDescriptor::ById(desc.id);
        if (api) {
            result.push_back(std::move(*api));
        }
        return true;
    }, type_filter);
    return result;
}

// ============================================================================
// Macros
// ============================================================================

/**
 * @brief Require a specific capability or throw
 */
#define BML_REQUIRE_CAPABILITY(cap) do { \
    if (!bml::HasCapability(cap)) { \
        throw bml::Exception(BML_RESULT_NOT_SUPPORTED, \
            "Required capability not available: " #cap); \
    } \
} while(0)

// ============================================================================
// Feature Test Macros
// ============================================================================

// Compile-time version encoding
#define BML_VERSION_ENCODE(major, minor, patch) \
    (((major) << 16) | ((minor) << 8) | (patch))

// Current BML version (compile-time)
#define BML_COMPILED_VERSION BML_VERSION_ENCODE(BML_MAJOR_VERSION, BML_MINOR_VERSION, BML_PATCH_VERSION)

// Feature availability checks (compile-time, based on header version)
#if BML_COMPILED_VERSION >= BML_VERSION_ENCODE(0, 5, 0)
#define BML_HAS_CAPABILITY_API 1
#define BML_HAS_API_DISCOVERY 1
#define BML_HAS_UNIFIED_EXTENSION 1
#else
#define BML_HAS_CAPABILITY_API 0
#define BML_HAS_API_DISCOVERY 0
#define BML_HAS_UNIFIED_EXTENSION 0
#endif

} // namespace bml

#endif /* BML_CAPABILITY_HPP */
