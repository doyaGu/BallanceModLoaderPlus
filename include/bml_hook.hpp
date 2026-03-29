/**
 * @file bml_hook.hpp
 * @brief C++ wrapper for BML Hook Registry
 *
 * Provides RAII hook registration and convenient enumeration.
 *
 * @code
 * // Register a hook
 * HookRegistration reg("CKRenderContext::Render", renderAddr, 0, bus);
 *
 * // Enumerate all hooks
 * HookRegistry::Enumerate([](const HookInfo &info) {
 *     printf("[%s] %s @ %p\n", info.owner.c_str(), info.name.c_str(), info.address);
 * }, bus);
 * @endcode
 */

#ifndef BML_HOOK_HPP
#define BML_HOOK_HPP

#include "bml_hook.h"
#include "bml_errors.h"
#include "bml_assert.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace bml {

    // ========================================================================
    // Hook Info (read-only snapshot from enumeration)
    // ========================================================================

    struct HookInfo {
        std::string name;
        void *address = nullptr;
        int32_t priority = 0;
        uint32_t flags = 0;
        uint32_t hook_type = 0;
        std::string owner;

        bool HasConflict() const noexcept {
            return (flags & BML_HOOK_FLAG_CONFLICT) != 0;
        }
    };

    // ========================================================================
    // RAII Hook Registration
    // ========================================================================

    /**
     * @brief RAII hook registration handle
     *
     * Registers a hook on construction, unregisters on destruction.
     *
     * @code
     * {
     *     HookRegistration reg("CKInputManager::IsKeyDown", addr, 0, hookIface);
     *     // Hook is registered
     * } // Automatically unregistered
     * @endcode
     */
    class HookRegistration {
    public:
        HookRegistration() = default;

        HookRegistration(const char *targetName,
                         void *targetAddress,
                         int32_t priority = 0,
                         const BML_CoreHookRegistryInterface *iface = nullptr,
                         BML_Mod owner = nullptr,
                         uint32_t hookType = 0)
            : m_Address(targetAddress), m_Interface(iface), m_Owner(owner) {
            BML_ASSERT(iface);
            BML_ASSERT(targetAddress);
            BML_HookDesc desc = BML_HOOK_DESC_INIT;
            desc.target_name = targetName;
            desc.target_address = targetAddress;
            desc.priority = priority;
            desc.hook_type = hookType;
            BML_Result result = BML_RESULT_NOT_SUPPORTED;
            if (owner && iface->Register) {
                result = iface->Register(owner, &desc);
            }
            if (result != BML_RESULT_OK) {
                m_Address = nullptr;
            }
        }

        ~HookRegistration() { Unregister(); }

        HookRegistration(HookRegistration &&other) noexcept
            : m_Address(other.m_Address), m_Interface(other.m_Interface), m_Owner(other.m_Owner) {
            other.m_Address = nullptr;
            other.m_Interface = nullptr;
            other.m_Owner = nullptr;
        }

        HookRegistration &operator=(HookRegistration &&other) noexcept {
            if (this != &other) {
                Unregister();
                m_Address = other.m_Address;
                m_Interface = other.m_Interface;
                m_Owner = other.m_Owner;
                other.m_Address = nullptr;
                other.m_Interface = nullptr;
                other.m_Owner = nullptr;
            }
            return *this;
        }

        HookRegistration(const HookRegistration &) = delete;
        HookRegistration &operator=(const HookRegistration &) = delete;

        void Unregister() {
            if (!m_Address || !m_Interface) {
                return;
            }
            if (m_Owner && m_Interface->Unregister) {
                m_Interface->Unregister(m_Owner, m_Address);
            }
            m_Address = nullptr;
        }

        bool Valid() const noexcept { return m_Address != nullptr; }
        explicit operator bool() const noexcept { return Valid(); }
        void *Address() const noexcept { return m_Address; }

    private:
        void *m_Address = nullptr;
        const BML_CoreHookRegistryInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

    // ========================================================================
    // Hook Registry Query
    // ========================================================================

    /**
     * @brief Static helper for querying the hook registry
     */
    struct HookRegistry {
        /**
         * @brief Enumerate all hooks via callback
         */
        static void Enumerate(std::function<void(const HookInfo &)> callback,
                              const BML_CoreHookRegistryInterface *iface) {
            if (!iface || !iface->Context || !iface->Enumerate || !callback) return;

            struct Ctx {
                std::function<void(const HookInfo &)> *fn;
            } ctx{&callback};

            iface->Enumerate(
                iface->Context,
                [](const BML_HookDesc *desc, const char *owner, void *ud) {
                    auto *c = static_cast<Ctx *>(ud);
                    HookInfo info;
                    info.name = desc->target_name ? desc->target_name : "";
                    info.address = desc->target_address;
                    info.priority = desc->priority;
                    info.flags = desc->flags;
                    info.hook_type = desc->hook_type;
                    info.owner = owner ? owner : "";
                    (*c->fn)(info);
                },
                &ctx);
        }

        /**
         * @brief Collect all hooks into a vector
         */
        static std::vector<HookInfo> GetAll(const BML_CoreHookRegistryInterface *iface) {
            std::vector<HookInfo> result;
            Enumerate([&result](const HookInfo &info) {
                result.push_back(info);
            }, iface);
            return result;
        }
    };

    // ========================================================================
    // Hook Service (bound wrapper for ModuleServices)
    // ========================================================================

    class HookService {
    public:
        HookService() = default;
        explicit HookService(const BML_CoreHookRegistryInterface *iface,
                             BML_Mod owner = nullptr) noexcept
            : m_Interface(iface), m_Owner(owner) {}

        HookRegistration Register(const char *name, void *address, int32_t priority = 0, uint32_t hookType = 0) const {
            return HookRegistration(name, address, priority, m_Interface, m_Owner, hookType);
        }

        void Enumerate(std::function<void(const HookInfo &)> callback) const {
            HookRegistry::Enumerate(std::move(callback), m_Interface);
        }

        std::vector<HookInfo> GetAll() const {
            return HookRegistry::GetAll(m_Interface);
        }

        explicit operator bool() const noexcept { return m_Interface != nullptr; }

    private:
        const BML_CoreHookRegistryInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

} // namespace bml

#endif /* BML_HOOK_HPP */
