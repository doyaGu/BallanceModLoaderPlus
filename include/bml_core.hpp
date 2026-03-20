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
    namespace detail {
        inline const BML_CoreModuleInterface *ResolveModuleInterface(
            const BML_CoreModuleInterface *moduleInterface) noexcept {
            if (moduleInterface) {
                return moduleInterface;
            }
            if (!bmlInterfaceAcquire) {
                return nullptr;
            }

            const void *implementation = nullptr;
            BML_InterfaceLease lease = nullptr;
            const BML_Version required = bmlMakeVersion(1, 0, 0);
            if (bmlInterfaceAcquire(BML_CORE_MODULE_INTERFACE_ID,
                                    &required,
                                    &implementation,
                                    &lease) != BML_RESULT_OK) {
                return nullptr;
            }

            if (lease && bmlInterfaceRelease) {
                bmlInterfaceRelease(lease);
            }
            return static_cast<const BML_CoreModuleInterface *>(implementation);
        }

        inline PFN_BML_SetCurrentModule ResolveSetCurrentModuleProc(
            const BML_CoreModuleInterface *moduleInterface) noexcept {
            if (moduleInterface && moduleInterface->SetCurrentModule) {
                return moduleInterface->SetCurrentModule;
            }
            return bmlSetCurrentModule;
        }

        inline PFN_BML_GetCurrentModule ResolveGetCurrentModuleProc(
            const BML_CoreModuleInterface *moduleInterface) noexcept {
            const auto *resolvedInterface = ResolveModuleInterface(moduleInterface);
            if (resolvedInterface && resolvedInterface->GetCurrentModule) {
                return resolvedInterface->GetCurrentModule;
            }
            return nullptr;
        }
    } // namespace detail

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

    // ============================================================================
    // Thread-Local Module Helpers
    // ============================================================================

    /**
     * @brief Bind the current thread to a module handle so implicit APIs resolve correctly.
     * @param mod Module handle to bind. Pass nullptr to clear.
     * @return true if the runtime supports the API and the assignment succeeded.
     */
    inline bool SetCurrentModule(BML_Mod mod, const BML_CoreModuleInterface *moduleInterface = nullptr) {
        const auto setCurrentModule = detail::ResolveSetCurrentModuleProc(moduleInterface);
        if (!setCurrentModule) return false;
        return setCurrentModule(mod) == BML_RESULT_OK;
    }

    /**
     * @brief Get the module currently bound to the calling thread.
     * @return Optional mod wrapper (empty if API unavailable or no module bound).
     */
    inline std::optional<Mod> GetCurrentModule(const BML_CoreModuleInterface *moduleInterface = nullptr) {
        const auto *resolvedInterface = detail::ResolveModuleInterface(moduleInterface);
        const auto getCurrentModule = detail::ResolveGetCurrentModuleProc(moduleInterface);
        if (!getCurrentModule) return std::nullopt;
        return Mod(getCurrentModule(), resolvedInterface);
    }

    /**
     * @brief Enumerate loaded modules as lightweight Mod wrappers.
     * @return Snapshot vector of loaded modules. Empty if APIs are unavailable.
     */
    inline std::vector<Mod> GetLoadedModules(const BML_CoreModuleInterface *moduleInterface = nullptr) {
        std::vector<Mod> modules;
        const auto *resolvedInterface = detail::ResolveModuleInterface(moduleInterface);
        if (!resolvedInterface ||
            !resolvedInterface->GetLoadedModuleCount ||
            !resolvedInterface->GetLoadedModuleAt) {
            return modules;
        }
        const uint32_t count = resolvedInterface->GetLoadedModuleCount();
        modules.reserve(count);
        for (uint32_t index = 0; index < count; ++index) {
            BML_Mod mod = resolvedInterface->GetLoadedModuleAt(index);
            if (mod) {
                modules.emplace_back(mod, resolvedInterface);
            }
        }
        return modules;
    }

    /**
     * @brief RAII helper that temporarily overrides the current module binding.
     */
    class CurrentModuleScope {
    public:
        explicit CurrentModuleScope(BML_Mod mod,
                                    const BML_CoreModuleInterface *moduleInterface = nullptr)
            : m_ModuleInterface(moduleInterface) {
            const auto getCurrentModule = detail::ResolveGetCurrentModuleProc(m_ModuleInterface);
            if (getCurrentModule) {
                m_Previous = getCurrentModule();
                m_HasPrevious = true;
            }
            const auto setCurrentModule = detail::ResolveSetCurrentModuleProc(m_ModuleInterface);
            if (setCurrentModule) {
                setCurrentModule(mod);
                m_Active = true;
            }
        }

        ~CurrentModuleScope() {
            const auto setCurrentModule = detail::ResolveSetCurrentModuleProc(m_ModuleInterface);
            if (m_Active && setCurrentModule) {
                setCurrentModule(m_HasPrevious ? m_Previous : nullptr);
            }
        }

        CurrentModuleScope(const CurrentModuleScope &) = delete;
        CurrentModuleScope &operator=(const CurrentModuleScope &) = delete;

        CurrentModuleScope(CurrentModuleScope &&other) noexcept
            : m_Previous(other.m_Previous),
              m_HasPrevious(other.m_HasPrevious),
              m_Active(other.m_Active),
              m_ModuleInterface(other.m_ModuleInterface) {
            other.m_Active = false;
            other.m_HasPrevious = false;
            other.m_Previous = nullptr;
            other.m_ModuleInterface = nullptr;
        }

        CurrentModuleScope &operator=(CurrentModuleScope &&other) noexcept {
            if (this != &other) {
                const auto setCurrentModule = detail::ResolveSetCurrentModuleProc(m_ModuleInterface);
                if (m_Active && setCurrentModule) {
                    setCurrentModule(m_HasPrevious ? m_Previous : nullptr);
                }
                m_Previous = other.m_Previous;
                m_HasPrevious = other.m_HasPrevious;
                m_Active = other.m_Active;
                m_ModuleInterface = other.m_ModuleInterface;
                other.m_Active = false;
                other.m_HasPrevious = false;
                other.m_Previous = nullptr;
                other.m_ModuleInterface = nullptr;
            }
            return *this;
        }

    private:
        BML_Mod m_Previous{nullptr};
        bool m_HasPrevious{false};
        bool m_Active{false};
        const BML_CoreModuleInterface *m_ModuleInterface{nullptr};
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
            if (!moduleInterface || !moduleInterface->RegisterShutdownHook || !mod) {
                throw Exception(BML_RESULT_NOT_FOUND, "Shutdown hook API unavailable");
            }

            m_Storage->callback = std::move(callback);
            m_Storage->self_owned = true;

            auto result = moduleInterface->RegisterShutdownHook(
                mod,
                detail::ShutdownHookStorage::Invoke,
                m_Storage);
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
        if (!resolvedInterface || !resolvedInterface->RegisterShutdownHook || !mod) return false;

        auto *storage = new detail::ShutdownHookStorage();
        storage->callback = std::move(callback);
        storage->self_owned = true; // Invoke will delete this

        return resolvedInterface->RegisterShutdownHook(
            mod.Handle(),
            detail::ShutdownHookStorage::Invoke,
            storage) == BML_RESULT_OK;
    }
} // namespace bml

#endif /* BML_CORE_HPP */
