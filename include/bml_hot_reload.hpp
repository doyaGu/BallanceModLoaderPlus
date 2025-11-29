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
     *       std::cout << "Mod: " << event->ModId() << std::endl;
     *       std::cout << "Version: " << event->VersionString() << std::endl;
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
        static std::optional<ModLifecycleEvent> Parse(const void *payload, size_t payload_len) {
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
        std::string_view ModId() const noexcept {
            return std::string_view(m_Event.mod_id, m_Event.mod_id_length);
        }

        /**
         * @brief Get the mod ID as a string
         * @return Mod ID string
         */
        std::string ModIdString() const {
            return std::string(m_Event.mod_id, m_Event.mod_id_length);
        }

        /**
         * @brief Get the mod version
         * @return Version value (use BML_VERSION_* macros to extract parts)
         */
        BML_Version Version() const noexcept {
            return m_Event.version;
        }

        /**
         * @brief Get the major version number
         */
        uint16_t VersionMajor() const noexcept {
            return m_Event.version.major;
        }

        /**
         * @brief Get the minor version number
         */
        uint16_t VersionMinor() const noexcept {
            return m_Event.version.minor;
        }

        /**
         * @brief Get the patch version number
         */
        uint16_t VersionPatch() const noexcept {
            return m_Event.version.patch;
        }

        /**
         * @brief Get the version as a string "major.minor.patch"
         * @return Version string
         */
        std::string VersionString() const {
            return std::to_string(VersionMajor()) + "." +
                std::to_string(VersionMinor()) + "." +
                std::to_string(VersionPatch());
        }

        /**
         * @brief Access the underlying C struct
         */
        const BML_ModLifecycleEvent &Handle() const noexcept {
            return m_Event;
        }

    private:
        explicit ModLifecycleEvent(const BML_ModLifecycleEvent &event)
            : m_Event(event) {}

        BML_ModLifecycleEvent m_Event;
    };

    // ============================================================================
    // Lifecycle Event Builder (for testing/publishing)
    // ============================================================================

    /**
     * @brief Builder for creating mod lifecycle event payloads
     *
     * Example:
     *   auto payload = bml::ModLifecycleEventBuilder("MyMod", 1, 0, 0).Build();
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
            : m_ModId(mod_id), m_Version(bmlMakeVersion(major, minor, patch)) {}

        /**
         * @brief Construct a builder with a version struct
         * @param mod_id Mod identifier
         * @param version Version structure
         */
        ModLifecycleEventBuilder(std::string_view mod_id, BML_Version version)
            : m_ModId(mod_id), m_Version(version) {}

        /**
         * @brief Build the wire-format payload
         * @return Payload data as a vector
         */
        std::vector<uint8_t> Build() const {
            std::vector<uint8_t> result;
            result.resize(sizeof(BML_ModLifecycleWireHeader) + m_ModId.size());

            BML_ModLifecycleWireHeader header{};
            header.version = m_Version;
            header.id_length = static_cast<uint32_t>(m_ModId.size());

            std::memcpy(result.data(), &header, sizeof(header));
            std::memcpy(result.data() + sizeof(header), m_ModId.data(), m_ModId.size());

            return result;
        }

    private:
        std::string m_ModId;
        BML_Version m_Version;
    };
} // namespace bml

#endif // BML_HOT_RELOAD_HPP

