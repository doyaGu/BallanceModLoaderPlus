/**
 * @file bml_core.hpp
 * @brief BML C++ Core API Wrapper
 * 
 * Provides RAII-friendly and type-safe wrappers for BML core functionality
 * including context management, version querying, and shutdown hooks.
 */

#ifndef BML_CORE_HPP
#define BML_CORE_HPP

#include "bml_builtin_interfaces.h"
#include "bml_core.h"
#include "bml_export.h"
#include "bml_errors.h"
#include "bml_context.hpp"

#include <optional>
#include <functional>
#include <memory>
#include <vector>

namespace bml {
    // ============================================================================
    // Version Utilities
    // ============================================================================

    /**
     * @brief Get the runtime version
     * @return Version if available, std::nullopt otherwise
     */
    inline std::optional<BML_Version> GetRuntimeVersion(
        const BML_CoreContextInterface *contextInterface = nullptr) {
        if (!contextInterface || !contextInterface->GetRuntimeVersion) return std::nullopt;
        const auto *version = contextInterface->GetRuntimeVersion();
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
    // Mod Handle Wrapper
    // ============================================================================

    /**
     * @brief Lightweight wrapper for BML_Mod handle
     *
     * This is a non-owning handle wrapper. Use for type-safe passing of mod handles.
     */
    class Mod {
    public:
        Mod() : m_Mod(nullptr) {}

        explicit Mod(BML_Mod mod, const BML_CoreModuleInterface *moduleInterface = nullptr)
            : m_Mod(mod), m_ModuleInterface(moduleInterface) {}

        /**
         * @brief Get the underlying handle
         */
        BML_Mod Handle() const noexcept { return m_Mod; }

        /**
         * @brief Get the module interface used by this wrapper.
         */
        const BML_CoreModuleInterface *ModuleInterface() const noexcept { return m_ModuleInterface; }

        /**
         * @brief Check if handle is valid
         */
        explicit operator bool() const noexcept { return m_Mod != nullptr; }

        /**
         * @brief Get the mod ID
         * @return Mod ID if available
         */
        std::optional<const char *> GetId() const {
            if (!m_ModuleInterface || !m_ModuleInterface->GetModId || !m_Mod) return std::nullopt;
            const char *id = nullptr;
            if (m_ModuleInterface->GetModId(m_Mod, &id) == BML_RESULT_OK && id) {
                return id;
            }
            return std::nullopt;
        }

        /**
         * @brief Get the mod version
         * @return Version if available
         */
        std::optional<BML_Version> GetVersion() const {
            if (!m_ModuleInterface || !m_ModuleInterface->GetModVersion || !m_Mod) return std::nullopt;
            BML_Version version = BML_VERSION_INIT(0, 0, 0);
            if (m_ModuleInterface->GetModVersion(m_Mod, &version) == BML_RESULT_OK) {
                return version;
            }
            return std::nullopt;
        }

        /**
         * @brief Request a capability for this mod
         * @param capabilityId Capability identifier
         * @return true if request succeeded
         */
        bool RequestCapability(const char *capabilityId) const {
            if (!m_ModuleInterface || !m_ModuleInterface->RequestCapability || !m_Mod || !capabilityId) {
                return false;
            }
            return m_ModuleInterface->RequestCapability(m_Mod, capabilityId) == BML_RESULT_OK;
        }

        /**
         * @brief Check if a capability is supported for this mod
         * @param capabilityId Capability identifier
         * @return true if capability is supported
         */
        bool CheckCapability(const char *capabilityId) const {
            if (!m_ModuleInterface || !m_ModuleInterface->CheckCapability || !m_Mod || !capabilityId) {
                return false;
            }
            BML_Bool supported = BML_FALSE;
            if (m_ModuleInterface->CheckCapability(m_Mod, capabilityId, &supported) == BML_RESULT_OK) {
                return supported != BML_FALSE;
            }
            return false;
        }

    private:
        BML_Mod m_Mod;
        const BML_CoreModuleInterface *m_ModuleInterface = nullptr;
    };

    /**
     * @brief Enumerate loaded modules as lightweight Mod wrappers.
     * @return Snapshot vector of loaded modules. Empty if APIs are unavailable.
     */
    inline std::vector<Mod> GetLoadedModules(const BML_CoreModuleInterface *moduleInterface) {
        std::vector<Mod> modules;
        if (!moduleInterface ||
            !moduleInterface->GetLoadedModuleCount ||
            !moduleInterface->GetLoadedModuleAt) {
            return modules;
        }
        const uint32_t count = moduleInterface->GetLoadedModuleCount();
        modules.reserve(count);
        for (uint32_t index = 0; index < count; ++index) {
            BML_Mod mod = moduleInterface->GetLoadedModuleAt(index);
            if (mod) {
                modules.emplace_back(mod, moduleInterface);
            }
        }
        return modules;
    }

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
            bool self_owned = false; // If true, Invoke deletes this

            static void Invoke(BML_Context ctx, void *user_data) {
                (void) ctx;
                auto *storage = static_cast<ShutdownHookStorage *>(user_data);
                if (storage) {
                    if (storage->callback) {
                        storage->callback();
                    }
                    if (storage->self_owned) {
                        delete storage;
                    }
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
        ShutdownHook(BML_Mod mod,
                     std::function<void()> callback,
                     const BML_CoreModuleInterface *moduleInterface = nullptr)
            : m_Storage(new detail::ShutdownHookStorage()) {
            if (!moduleInterface || !mod) {
                throw Exception(BML_RESULT_NOT_FOUND, "Shutdown hook API unavailable");
            }

            m_Storage->callback = std::move(callback);
            m_Storage->self_owned = true;

            BML_Result result = BML_RESULT_NOT_SUPPORTED;
            if (moduleInterface->RegisterShutdownHook) {
                result = moduleInterface->RegisterShutdownHook(
                    mod, detail::ShutdownHookStorage::Invoke, m_Storage);
            } else if (moduleInterface->RegisterShutdownHook) {
                result = moduleInterface->RegisterShutdownHook(
                    mod, detail::ShutdownHookStorage::Invoke, m_Storage);
            }
            if (result != BML_RESULT_OK) {
                delete m_Storage;
                m_Storage = nullptr;
                throw Exception(result, "Failed to register shutdown hook");
            }
        }

        ShutdownHook(Mod mod, std::function<void()> callback)
            : ShutdownHook(mod.Handle(), std::move(callback), mod.ModuleInterface()) {}

        // Non-copyable
        ShutdownHook(const ShutdownHook &) = delete;
        ShutdownHook &operator=(const ShutdownHook &) = delete;

        // Movable
        ShutdownHook(ShutdownHook &&other) noexcept : m_Storage(other.m_Storage) {
            other.m_Storage = nullptr;
        }

        ShutdownHook &operator=(ShutdownHook &&other) noexcept {
            if (this != &other) {
                m_Storage = other.m_Storage;
                other.m_Storage = nullptr;
            }
            return *this;
        }

        /**
         * @brief Check if hook is registered
         */
        explicit operator bool() const noexcept { return m_Storage != nullptr; }

    private:
        detail::ShutdownHookStorage *m_Storage{nullptr};
    };

    /**
     * @brief Register a shutdown hook (convenience function)
     * @param mod The mod handle
     * @param callback Callback to invoke on shutdown
     * @return true if registration succeeded
     */
    inline bool RegisterShutdownHook(Mod mod,
                                     std::function<void()> callback,
                                     const BML_CoreModuleInterface *moduleInterface = nullptr) {
        const auto *resolvedInterface = moduleInterface ? moduleInterface : mod.ModuleInterface();
        if (!resolvedInterface || !mod) return false;

        auto *storage = new detail::ShutdownHookStorage();
        storage->callback = std::move(callback);
        storage->self_owned = true; // Invoke will delete this

        BML_Result result = BML_RESULT_NOT_SUPPORTED;
        if (resolvedInterface->RegisterShutdownHook) {
            result = resolvedInterface->RegisterShutdownHook(
                mod.Handle(), detail::ShutdownHookStorage::Invoke, storage);
        } else if (resolvedInterface->RegisterShutdownHook) {
            result = resolvedInterface->RegisterShutdownHook(
                mod.Handle(), detail::ShutdownHookStorage::Invoke, storage);
        }
        if (result != BML_RESULT_OK) {
            delete storage;
            return false;
        }
        return true;
    }
} // namespace bml

#endif /* BML_CORE_HPP */
