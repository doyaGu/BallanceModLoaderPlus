/**
 * @file bml_hot_reload.hpp
 * @brief BML C++ Hot Reload Wrapper
 * 
 * Provides C++ wrappers for mod lifecycle events used in hot-reloading.
 */

#ifndef BML_HOT_RELOAD_HPP
#define BML_HOT_RELOAD_HPP

#include "bml_hot_reload.h"

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <cstring>

namespace bml {

// ============================================================================
// Mod Lifecycle Event
// ============================================================================

/**
 * @brief C++ wrapper for mod lifecycle events
 * 
 * Example:
 *   auto event = bml::ModLifecycleEvent::Parse(payload, payload_len);
 *   if (event) {
 *       std::cout << "Mod: " << event->mod_id() << std::endl;
 *       std::cout << "Version: " << event->version_string() << std::endl;
 *   }
 */
class ModLifecycleEvent {
public:
    /**
     * @brief Parse a mod lifecycle event from raw payload
     * @param payload Raw payload data
     * @param payload_len Payload size in bytes
     * @return Parsed event or nullopt on error
     */
    static std::optional<ModLifecycleEvent> Parse(const void* payload, size_t payload_len) {
        BML_ModLifecycleEvent raw{};
        if (!bmlParseModLifecycleEvent(payload, payload_len, &raw)) {
            return std::nullopt;
        }
        return ModLifecycleEvent(raw);
    }
    
    /**
     * @brief Get the mod ID
     * @return Mod ID string view
     */
    std::string_view mod_id() const noexcept {
        return std::string_view(m_event.mod_id, m_event.mod_id_length);
    }
    
    /**
     * @brief Get the mod ID as a string
     * @return Mod ID string
     */
    std::string mod_id_string() const {
        return std::string(m_event.mod_id, m_event.mod_id_length);
    }
    
    /**
     * @brief Get the mod version
     * @return Version value (use BML_VERSION_* macros to extract parts)
     */
    BML_Version version() const noexcept {
        return m_event.version;
    }
    
    /**
     * @brief Get the major version number
     */
    uint16_t version_major() const noexcept {
        return m_event.version.major;
    }
    
    /**
     * @brief Get the minor version number
     */
    uint16_t version_minor() const noexcept {
        return m_event.version.minor;
    }
    
    /**
     * @brief Get the patch version number
     */
    uint16_t version_patch() const noexcept {
        return m_event.version.patch;
    }
    
    /**
     * @brief Get the version as a string "major.minor.patch"
     * @return Version string
     */
    std::string version_string() const {
        return std::to_string(version_major()) + "." +
               std::to_string(version_minor()) + "." +
               std::to_string(version_patch());
    }
    
    /**
     * @brief Access the underlying C struct
     */
    const BML_ModLifecycleEvent& handle() const noexcept {
        return m_event;
    }
    
private:
    explicit ModLifecycleEvent(const BML_ModLifecycleEvent& event)
        : m_event(event) {}
    
    BML_ModLifecycleEvent m_event;
};

// ============================================================================
// Lifecycle Event Builder (for testing/publishing)
// ============================================================================

/**
 * @brief Builder for creating mod lifecycle event payloads
 * 
 * Example:
 *   auto payload = bml::ModLifecycleEventBuilder("MyMod", 1, 0, 0).build();
 *   imc.Publish("mod.loaded", payload.data(), payload.size());
 */
class ModLifecycleEventBuilder {
public:
    /**
     * @brief Construct a builder
     * @param mod_id Mod identifier
     * @param major Major version
     * @param minor Minor version
     * @param patch Patch version
     */
    ModLifecycleEventBuilder(std::string_view mod_id,
                              uint16_t major = 0,
                              uint16_t minor = 0,
                              uint16_t patch = 0)
        : m_mod_id(mod_id)
        , m_version(bmlMakeVersion(major, minor, patch)) {}
    
    /**
     * @brief Construct a builder with a version struct
     * @param mod_id Mod identifier
     * @param version Version structure
     */
    ModLifecycleEventBuilder(std::string_view mod_id, BML_Version version)
        : m_mod_id(mod_id)
        , m_version(version) {}
    
    /**
     * @brief Build the wire-format payload
     * @return Payload data as a vector
     */
    std::vector<uint8_t> build() const {
        std::vector<uint8_t> result;
        result.resize(sizeof(BML_ModLifecycleWireHeader) + m_mod_id.size());
        
        BML_ModLifecycleWireHeader header{};
        header.version = m_version;
        header.id_length = static_cast<uint32_t>(m_mod_id.size());
        
        std::memcpy(result.data(), &header, sizeof(header));
        std::memcpy(result.data() + sizeof(header), m_mod_id.data(), m_mod_id.size());
        
        return result;
    }
    
private:
    std::string m_mod_id;
    BML_Version m_version;
};

} // namespace bml

#endif // BML_HOT_RELOAD_HPP
