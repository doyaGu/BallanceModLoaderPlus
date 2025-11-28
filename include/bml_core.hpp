/**
 * @file bml_core.hpp
 * @brief BML C++ Core API Wrapper
 * 
 * Provides RAII-friendly and type-safe wrappers for BML core functionality
 * including context management, version querying, and shutdown hooks.
 */

#ifndef BML_CORE_HPP
#define BML_CORE_HPP

#include "bml_core.h"
#include "bml_export.h"
#include "bml_errors.h"
#include "bml_context.hpp"

#include <optional>
#include <functional>
#include <memory>

namespace bml {
    // ============================================================================
    // Version Utilities
    // ============================================================================

    /**
     * @brief Get the runtime version
     * @return Version if available, std::nullopt otherwise
     */
    inline std::optional<BML_Version> GetRuntimeVersion() {
        if (!bmlGetRuntimeVersion) return std::nullopt;
        const auto *version = bmlGetRuntimeVersion();
        return version ? std::optional<BML_Version>(*version) : std::nullopt;
    }

    /**
     * @brief Compare two versions
     * @param a First version
     * @param b Second version
     * @return <0 if a < b, 0 if a == b, >0 if a > b
     */
    inline int CompareVersions(const BML_Version &a, const BML_Version &b) noexcept {
        if (a.major != b.major) return static_cast<int>(a.major) - static_cast<int>(b.major);
        if (a.minor != b.minor) return static_cast<int>(a.minor) - static_cast<int>(b.minor);
        return static_cast<int>(a.patch) - static_cast<int>(b.patch);
    }

    /**
     * @brief Create a version struct
     * @param major Major version
     * @param minor Minor version
     * @param patch Patch version
     * @return BML_Version struct
     */
    inline BML_Version MakeVersion(uint16_t major, uint16_t minor, uint16_t patch) noexcept {
        return bmlMakeVersion(major, minor, patch);
    }

    /**
     * @brief Convert version to uint32_t for comparison
     * @param version Version struct
     * @return Encoded version as uint32_t
     */
    inline uint32_t VersionToUint(const BML_Version &version) noexcept {
        return bmlVersionToUint(&version);
    }

    // ============================================================================
    // Core Capabilities Query
    // ============================================================================

    /**
     * @brief Query core subsystem capabilities
     * @return Capabilities if successful
     */
    inline std::optional<BML_CoreCaps> GetCoreCaps() {
        if (!bmlCoreGetCaps) return std::nullopt;
        BML_CoreCaps caps = BML_CORE_CAPS_INIT;
        if (bmlCoreGetCaps(&caps) == BML_RESULT_OK) {
            return caps;
        }
        return std::nullopt;
    }

    /**
     * @brief Check if a core capability is available
     * @param flag Capability flag to check
     * @return true if capability is available
     */
    inline bool HasCoreCap(BML_CoreCapabilityFlags flag) {
        auto caps = GetCoreCaps();
        return caps && (caps->capability_flags & flag);
    }

    // ============================================================================
    // Mod Handle Wrapper
    // ============================================================================

    /**
     * @brief Lightweight wrapper for BML_Mod handle
     *
     * This is a non-owning handle wrapper. Use for type-safe passing of mod handles.
     */
    class Mod {
    public:
        Mod() : m_mod(nullptr) {}

        explicit Mod(BML_Mod mod) : m_mod(mod) {}

        /**
         * @brief Get the underlying handle
         */
        BML_Mod handle() const noexcept { return m_mod; }

        /**
         * @brief Check if handle is valid
         */
        explicit operator bool() const noexcept { return m_mod != nullptr; }

        /**
         * @brief Get the mod ID
         * @return Mod ID if available
         */
        std::optional<const char *> GetId() const {
            if (!bmlGetModId || !m_mod) return std::nullopt;
            const char *id = nullptr;
            if (bmlGetModId(m_mod, &id) == BML_RESULT_OK && id) {
                return id;
            }
            return std::nullopt;
        }

        /**
         * @brief Get the mod version
         * @return Version if available
         */
        std::optional<BML_Version> GetVersion() const {
            if (!bmlGetModVersion || !m_mod) return std::nullopt;
            BML_Version version = BML_VERSION_INIT(0, 0, 0);
            if (bmlGetModVersion(m_mod, &version) == BML_RESULT_OK) {
                return version;
            }
            return std::nullopt;
        }

        /**
         * @brief Request a capability for this mod
         * @param capability_id Capability identifier
         * @return true if request succeeded
         */
        bool RequestCapability(const char *capability_id) const {
            if (!bmlRequestCapability || !m_mod || !capability_id) return false;
            return bmlRequestCapability(m_mod, capability_id) == BML_RESULT_OK;
        }

        /**
         * @brief Check if a capability is supported for this mod
         * @param capability_id Capability identifier
         * @return true if capability is supported
         */
        bool CheckCapability(const char *capability_id) const {
            if (!bmlCheckCapability || !m_mod || !capability_id) return false;
            BML_Bool supported = BML_FALSE;
            if (bmlCheckCapability(m_mod, capability_id, &supported) == BML_RESULT_OK) {
                return supported != BML_FALSE;
            }
            return false;
        }

    private:
        BML_Mod m_mod;
    };

    // ============================================================================
    // Thread-Local Module Helpers
    // ============================================================================

    /**
     * @brief Bind the current thread to a module handle so implicit APIs resolve correctly.
     * @param mod Module handle to bind. Pass nullptr to clear.
     * @return true if the runtime supports the API and the assignment succeeded.
     */
    inline bool SetCurrentModule(BML_Mod mod) {
        if (!bmlSetCurrentModule) return false;
        return bmlSetCurrentModule(mod) == BML_RESULT_OK;
    }

    /**
     * @brief Get the module currently bound to the calling thread.
     * @return Optional mod wrapper (empty if API unavailable or no module bound).
     */
    inline std::optional<Mod> GetCurrentModule() {
        if (!bmlGetCurrentModule) return std::nullopt;
        return Mod(bmlGetCurrentModule());
    }

    /**
     * @brief RAII helper that temporarily overrides the current module binding.
     */
    class CurrentModuleScope {
    public:
        explicit CurrentModuleScope(BML_Mod mod) {
            if (bmlGetCurrentModule) {
                previous_ = bmlGetCurrentModule();
                has_previous_ = true;
            }
            if (bmlSetCurrentModule) {
                bmlSetCurrentModule(mod);
                active_ = true;
            }
        }

        ~CurrentModuleScope() {
            if (active_ && bmlSetCurrentModule) {
                bmlSetCurrentModule(has_previous_ ? previous_ : nullptr);
            }
        }

        CurrentModuleScope(const CurrentModuleScope &) = delete;
        CurrentModuleScope &operator=(const CurrentModuleScope &) = delete;

        CurrentModuleScope(CurrentModuleScope &&other) noexcept
            : previous_(other.previous_), has_previous_(other.has_previous_), active_(other.active_) {
            other.active_ = false;
            other.has_previous_ = false;
            other.previous_ = nullptr;
        }

        CurrentModuleScope &operator=(CurrentModuleScope &&other) noexcept {
            if (this != &other) {
                if (active_ && bmlSetCurrentModule) {
                    bmlSetCurrentModule(has_previous_ ? previous_ : nullptr);
                }
                previous_ = other.previous_;
                has_previous_ = other.has_previous_;
                active_ = other.active_;
                other.active_ = false;
                other.has_previous_ = false;
                other.previous_ = nullptr;
            }
            return *this;
        }

    private:
        BML_Mod previous_{nullptr};
        bool has_previous_{false};
        bool active_{false};
    };

    // ============================================================================
    // Shutdown Hook Management
    // ============================================================================

    namespace detail {
        /**
         * @internal
         * @brief Storage for shutdown hook callbacks
         */
        struct ShutdownHookStorage {
            std::function<void()> callback;

            static void Invoke(BML_Context ctx, void *user_data) {
                (void) ctx;
                auto *storage = static_cast<ShutdownHookStorage *>(user_data);
                if (storage && storage->callback) {
                    storage->callback();
                }
            }
        };
    } // namespace detail

    /**
     * @brief RAII wrapper for shutdown hook registration
     *
     * Example:
     *   auto hook = bml::ShutdownHook(mod, []() {
     *       // Cleanup code
     *   });
     */
    class ShutdownHook {
    public:
        ShutdownHook() = default;

        /**
         * @brief Register a shutdown hook
         * @param mod The mod handle
         * @param callback Callback to invoke on shutdown
         */
        ShutdownHook(BML_Mod mod, std::function<void()> callback)
            : m_storage(std::make_unique<detail::ShutdownHookStorage>()) {
            if (!bmlRegisterShutdownHook || !mod) {
                throw Exception(BML_RESULT_NOT_FOUND, "Shutdown hook API unavailable");
            }

            m_storage->callback = std::move(callback);

            auto result = bmlRegisterShutdownHook(mod, detail::ShutdownHookStorage::Invoke, m_storage.get());
            if (result != BML_RESULT_OK) {
                m_storage.reset();
                throw Exception(result, "Failed to register shutdown hook");
            }
        }

        // Non-copyable
        ShutdownHook(const ShutdownHook &) = delete;
        ShutdownHook &operator=(const ShutdownHook &) = delete;

        // Movable
        ShutdownHook(ShutdownHook &&other) noexcept = default;
        ShutdownHook &operator=(ShutdownHook &&other) noexcept = default;

        /**
         * @brief Check if hook is registered
         */
        explicit operator bool() const noexcept { return m_storage != nullptr; }

    private:
        std::unique_ptr<detail::ShutdownHookStorage> m_storage;
    };

    /**
     * @brief Register a shutdown hook (convenience function)
     * @param mod The mod handle
     * @param callback Callback to invoke on shutdown
     * @return true if registration succeeded
     */
    inline bool RegisterShutdownHook(Mod mod, std::function<void()> callback) {
        if (!bmlRegisterShutdownHook || !mod) return false;

        // Leak the storage intentionally - it will be cleaned up by the callback
        auto *storage = new detail::ShutdownHookStorage();
        storage->callback = std::move(callback);

        return bmlRegisterShutdownHook(mod.handle(), detail::ShutdownHookStorage::Invoke, storage) == BML_RESULT_OK;
    }
} // namespace bml

#endif /* BML_CORE_HPP */
