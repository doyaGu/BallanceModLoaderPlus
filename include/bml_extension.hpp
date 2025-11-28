/**
 * @file bml_extension.hpp
 * @brief BML C++ Extension Wrapper
 *
 * Provides type-safe C++ access to BML extension registration and loading.
 */

#ifndef BML_EXTENSION_HPP
#define BML_EXTENSION_HPP

#include "bml_extension.h"

#include <string_view>
#include <string>
#include <optional>
#include <vector>
#include <functional>

namespace bml {
    // ============================================================================
    // Extension Info Wrapper
    // ============================================================================

    /**
     * @brief C++ wrapper for extension metadata
     */
    struct ExtensionInfo {
        std::string name;
        std::string provider_id;
        BML_Version version{};
        BML_ExtensionState state{BML_EXTENSION_STATE_ACTIVE};
        std::string description;
        size_t api_size{0};
        uint64_t capabilities{0};
        std::vector<std::string> tags;
        std::string replacement_name;
        std::string deprecation_message;

        ExtensionInfo() = default;

        explicit ExtensionInfo(const BML_ExtensionInfo &info)
            : name(info.name ? info.name : ""),
              provider_id(info.provider_id ? info.provider_id : ""),
              version(info.version),
              state(info.state),
              description(info.description ? info.description : ""),
              api_size(info.api_size),
              capabilities(info.capabilities),
              replacement_name(info.replacement_name ? info.replacement_name : ""),
              deprecation_message(info.deprecation_message ? info.deprecation_message : "") {
            if (info.tags && info.tag_count > 0) {
                tags.reserve(info.tag_count);
                for (uint32_t i = 0; i < info.tag_count; ++i) {
                    if (info.tags[i]) tags.emplace_back(info.tags[i]);
                }
            }
        }

        bool IsDeprecated() const { return state == BML_EXTENSION_STATE_DEPRECATED; }
        bool IsActive() const { return state == BML_EXTENSION_STATE_ACTIVE; }
    };

    // ============================================================================
    // Extension Builder
    // ============================================================================

    /**
     * @brief Fluent builder for extension registration
     *
     * Example:
     *   bml::ExtensionBuilder("MyMod_EXT_Core")
     *       .Version(1, 2, 0)
     *       .Description("Core API for MyMod")
     *       .Tags({"utility", "core"})
     *       .Api(&s_MyApi)
     *       .Register();
     */
    class ExtensionBuilder {
    public:
        explicit ExtensionBuilder(std::string_view name) {
            m_desc = BML_EXTENSION_DESC_INIT;
            m_name = std::string(name);
            m_desc.name = m_name.c_str();
        }

        ExtensionBuilder &Version(uint16_t major, uint16_t minor = 0, uint16_t patch = 0) {
            m_desc.version = bmlMakeVersion(major, minor, patch);
            return *this;
        }

        ExtensionBuilder &Version(const BML_Version &ver) {
            m_desc.version = ver;
            return *this;
        }

        template <typename T>
        ExtensionBuilder &Api(const T *table) {
            m_desc.api_table = table;
            m_desc.api_size = sizeof(T);
            return *this;
        }

        ExtensionBuilder &Api(const void *table, size_t size) {
            m_desc.api_table = table;
            m_desc.api_size = size;
            return *this;
        }

        ExtensionBuilder &Description(std::string_view desc) {
            m_description = std::string(desc);
            m_desc.description = m_description.c_str();
            return *this;
        }

        ExtensionBuilder &Capabilities(uint64_t caps) {
            m_desc.capabilities = caps;
            return *this;
        }

        ExtensionBuilder &Tags(std::initializer_list<const char *> tags) {
            m_tags.clear();
            m_tagPtrs.clear();
            for (const auto &tag : tags) {
                m_tags.emplace_back(tag);
            }
            for (const auto &tag : m_tags) {
                m_tagPtrs.push_back(tag.c_str());
            }
            m_desc.tags = m_tagPtrs.data();
            m_desc.tag_count = static_cast<uint32_t>(m_tags.size());
            return *this;
        }

        template <typename OnLoad, typename OnUnload>
        ExtensionBuilder &Lifecycle(OnLoad onLoad, OnUnload onUnload, void *userData = nullptr) {
            m_desc.on_load = onLoad;
            m_desc.on_unload = onUnload;
            m_desc.user_data = userData;
            return *this;
        }

        /**
         * @brief Registers the extension
         * @return true if registration succeeded
         */
        bool Register() {
            if (!bmlExtensionRegister) return false;
            return bmlExtensionRegister(&m_desc) == BML_RESULT_OK;
        }

        const BML_ExtensionDesc &Descriptor() const { return m_desc; }

    private:
        BML_ExtensionDesc m_desc{};
        std::string m_name;
        std::string m_description;
        std::vector<std::string> m_tags;
        std::vector<const char *> m_tagPtrs;
    };

    // ============================================================================
    // Extension Manager
    // ============================================================================

    /**
     * @brief Static utility class for extension management
     */
    class Extension {
    public:
        Extension() = delete;

        // ========================================================================
        // Registration
        // ========================================================================

        /**
         * @brief Register an extension using descriptor
         */
        static bool Register(const BML_ExtensionDesc &desc) {
            if (!bmlExtensionRegister) return false;
            return bmlExtensionRegister(&desc) == BML_RESULT_OK;
        }

        /**
         * @brief Unregister an extension
         */
        static bool Unregister(std::string_view name) {
            if (!bmlExtensionUnregister) return false;
            return bmlExtensionUnregister(name.data()) == BML_RESULT_OK;
        }

        // ========================================================================
        // Query
        // ========================================================================

        /**
         * @brief Query extension information
         */
        static std::optional<ExtensionInfo> Query(std::string_view name) {
            if (!bmlExtensionQuery) return std::nullopt;
            BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
            if (bmlExtensionQuery(name.data(), &info) == BML_RESULT_OK) {
                return ExtensionInfo(info);
            }
            return std::nullopt;
        }

        /**
         * @brief Check if an extension is available
         */
        static bool IsAvailable(std::string_view name) {
            return Query(name).has_value();
        }

        // ========================================================================
        // Loading
        // ========================================================================

        /**
         * @brief Load an extension API (typed)
         */
        template <typename T>
        static T *Load(std::string_view name, uint16_t req_major = 0, uint16_t req_minor = 0) {
            if (!bmlExtensionLoad) return nullptr;
            void *api = nullptr;
            BML_Version req = bmlMakeVersion(req_major, req_minor, 0);
            if (bmlExtensionLoad(name.data(), &req, &api, nullptr) == BML_RESULT_OK) {
                return static_cast<T *>(api);
            }
            return nullptr;
        }

        /**
         * @brief Load an extension API with version info
         */
        template <typename T>
        static T *Load(std::string_view name, const BML_Version &required,
                       ExtensionInfo *out_info) {
            if (!bmlExtensionLoad) return nullptr;
            void *api = nullptr;
            BML_ExtensionInfo info = BML_EXTENSION_INFO_INIT;
            if (bmlExtensionLoad(name.data(), &required, &api,
                                 out_info ? &info : nullptr) == BML_RESULT_OK) {
                if (out_info) *out_info = ExtensionInfo(info);
                return static_cast<T *>(api);
            }
            return nullptr;
        }

        // ========================================================================
        // Enumeration
        // ========================================================================

        /**
         * @brief Enumerate all extensions
         */
        static void Enumerate(std::function<bool(const ExtensionInfo &)> callback) {
            if (!bmlExtensionEnumerate || !callback) return;

            struct Context {
                std::function<bool(const ExtensionInfo &)> *callback;
            };
            Context ctx{&callback};

            bmlExtensionEnumerate(nullptr, [](BML_Context, const BML_ExtensionInfo *info, void *ud) -> BML_Bool {
                auto *c = static_cast<Context *>(ud);
                if (info && c->callback) {
                    return (*c->callback)(ExtensionInfo(*info)) ? BML_TRUE : BML_FALSE;
                }
                return BML_TRUE;
            }, &ctx);
        }

        /**
         * @brief Get all extension names
         */
        static std::vector<std::string> GetAllNames() {
            std::vector<std::string> names;
            Enumerate([&names](const ExtensionInfo &info) {
                names.push_back(info.name);
                return true;
            });
            return names;
        }

        /**
         * @brief Count registered extensions
         */
        static uint32_t Count() {
            if (!bmlExtensionCount) return 0;
            uint32_t count = 0;
            bmlExtensionCount(nullptr, &count);
            return count;
        }

        // ========================================================================
        // Update
        // ========================================================================

        /**
         * @brief Update extension API table
         */
        template <typename T>
        static bool UpdateApi(std::string_view name, const T *api_table) {
            if (!bmlExtensionUpdateApi) return false;
            return bmlExtensionUpdateApi(name.data(), api_table, sizeof(T)) == BML_RESULT_OK;
        }

        /**
         * @brief Mark extension as deprecated
         */
        static bool Deprecate(std::string_view name,
                              std::string_view replacement = {},
                              std::string_view message = {}) {
            if (!bmlExtensionDeprecate) return false;
            return bmlExtensionDeprecate(
                name.data(),
                replacement.empty() ? nullptr : replacement.data(),
                message.empty() ? nullptr : message.data()
            ) == BML_RESULT_OK;
        }

        // ========================================================================
        // Capabilities
        // ========================================================================

        /**
         * @brief Get extension system capabilities
         */
        static std::optional<BML_ExtensionCaps> GetCapabilities() {
            if (!bmlGetExtensionCaps) return std::nullopt;
            BML_ExtensionCaps caps = BML_EXTENSION_CAPS_INIT;
            if (bmlGetExtensionCaps(&caps) == BML_RESULT_OK) {
                return caps;
            }
            return std::nullopt;
        }

        /**
         * @brief Check if a capability is available
         */
        static bool HasCapability(BML_ExtensionCapFlags cap) {
            auto caps = GetCapabilities();
            return caps && (caps->capability_flags & static_cast<uint32_t>(cap)) != 0;
        }
    };

    // ============================================================================
    // Lifecycle Listener RAII Wrapper
    // ============================================================================

    /**
     * @brief RAII wrapper for extension lifecycle listener
     */
    class ExtensionListener {
    public:
        using Callback = std::function<void(BML_ExtensionEvent, const ExtensionInfo &)>;

        ExtensionListener() = default;

        ExtensionListener(Callback callback, uint32_t event_mask = 0)
            : m_callback(std::move(callback)) {
            if (bmlExtensionAddListener && m_callback) {
                bmlExtensionAddListener(
                    [](BML_Context, BML_ExtensionEvent event, const BML_ExtensionInfo *info, void *ud) {
                        auto *self = static_cast<ExtensionListener *>(ud);
                        if (self->m_callback && info) {
                            self->m_callback(event, ExtensionInfo(*info));
                        }
                    },
                    event_mask,
                    this,
                    &m_id
                );
            }
        }

        ~ExtensionListener() {
            if (m_id != 0 && bmlExtensionRemoveListener) {
                bmlExtensionRemoveListener(m_id);
            }
        }

        // Non-copyable
        ExtensionListener(const ExtensionListener &) = delete;
        ExtensionListener &operator=(const ExtensionListener &) = delete;

        // Movable
        ExtensionListener(ExtensionListener &&other) noexcept
            : m_id(other.m_id), m_callback(std::move(other.m_callback)) {
            other.m_id = 0;
        }

        ExtensionListener &operator=(ExtensionListener &&other) noexcept {
            if (this != &other) {
                if (m_id != 0 && bmlExtensionRemoveListener) {
                    bmlExtensionRemoveListener(m_id);
                }
                m_id = other.m_id;
                m_callback = std::move(other.m_callback);
                other.m_id = 0;
            }
            return *this;
        }

        bool IsValid() const { return m_id != 0; }
        uint64_t Id() const { return m_id; }

    private:
        uint64_t m_id{0};
        Callback m_callback;
    };
} // namespace bml

#endif /* BML_EXTENSION_HPP */
