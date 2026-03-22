/**
 * @file bml_services.hpp
 * @brief Service Hub and Module Services for BML v0.4
 *
 * Provides RuntimeServiceHub (runtime-wide typed service access) and
 * ModuleServices (per-module bound service view). These types eliminate
 * repeated interface acquisition boilerplate across modules.
 *
 * Type hierarchy:
 *   RuntimeServiceHub  - type public, single instance owned by Context
 *   BuiltinServices    - raw pointers to immutable static vtables
 *   ModuleServices     - per-module, owns Logger/Config/SubscriptionManager
 *   ScopedModuleContext - compatibility guard for legacy TLS-only paths
 */

#ifndef BML_SERVICES_HPP
#define BML_SERVICES_HPP

#include "bml_builtin_interfaces.h"
#include "bml_interface.hpp"
#include "bml_locale.hpp"
#include "bml_logger.hpp"
#include "bml_config.hpp"
#include "bml_timer.hpp"
#include "bml_hook.hpp"
#include "bml_imc_subscription.hpp"
#include "bml_imc_rpc.hpp"

#include <optional>

// Forward declarations for opt-in headers
namespace bml {
    class SyncService;
    class ProfilingService;
}

namespace bml {

// ============================================================================
// ScopedModuleContext
// ============================================================================

/**
 * @brief Compatibility guard that sets g_CurrentModule for the current scope.
 *
 * Uses the BML_CoreModuleInterface vtable to set/restore the current
 * module context. New core paths should prefer explicit owner-aware APIs;
 * this guard exists only for legacy paths that still require TLS rebinding.
 */
class ScopedModuleContext {
public:
    explicit ScopedModuleContext(BML_Mod mod,
                                const BML_CoreModuleInterface *iface) noexcept
        : m_Interface(iface) {
        if (iface && iface->GetCurrentModule) {
            m_Previous = iface->GetCurrentModule();
        }
        if (iface && iface->SetCurrentModule) {
            iface->SetCurrentModule(mod);
        }
    }

    ~ScopedModuleContext() {
        if (m_Interface && m_Interface->SetCurrentModule) {
            m_Interface->SetCurrentModule(m_Previous);
        }
    }

    ScopedModuleContext(const ScopedModuleContext &) = delete;
    ScopedModuleContext &operator=(const ScopedModuleContext &) = delete;

private:
    const BML_CoreModuleInterface *m_Interface = nullptr;
    BML_Mod m_Previous = nullptr;
};

// ============================================================================
// BuiltinServices
// ============================================================================

/**
 * @brief Fixed aggregate of stable builtin interface vtable pointers.
 *
 * These are raw pointers to static globals in BuiltinInterfaces.cpp,
 * registered with BML_INTERFACE_FLAG_IMMUTABLE. They are never
 * unregistered or relocated and outlive all modules.
 *
 * MUST NOT be passed by value across DLL boundaries.
 */
struct BuiltinServices {
    const BML_CoreContextInterface *Context = nullptr;
    const BML_CoreLoggingInterface *Logging = nullptr;
    const BML_CoreModuleInterface *Module = nullptr;
    const BML_CoreConfigInterface *Config = nullptr;
    const BML_CoreMemoryInterface *Memory = nullptr;
    const BML_CoreResourceInterface *Resource = nullptr;
    const BML_CoreDiagnosticInterface *Diagnostic = nullptr;
    const BML_ImcBusInterface *ImcBus = nullptr;
    const BML_ImcRpcInterface *ImcRpc = nullptr;
    const BML_CoreTimerInterface *Timer = nullptr;
    const BML_CoreLocaleInterface *Locale = nullptr;
    const BML_CoreHookRegistryInterface *HookRegistry = nullptr;
    const BML_CoreSyncInterface *Sync = nullptr;
    const BML_CoreProfilingInterface *Profiling = nullptr;
    const BML_HostRuntimeInterface *HostRuntime = nullptr;
};

// ============================================================================
// RuntimeServiceHub
// ============================================================================

/**
 * @brief Runtime-wide service access layer.
 *
 * One instance per runtime, owned by BML::Core::Context.
 * Modules access it through a borrowed const pointer delivered via
 * BML_ModAttachArgs::service_hub.
 */
class RuntimeServiceHub {
public:
    const BuiltinServices &Builtins() const noexcept { return m_Builtins; }

    template <typename T>
    InterfaceLease<T> Acquire(const char *id,
                              uint16_t major,
                              uint16_t minor = 0,
                              uint16_t patch = 0) const {
        return bml::AcquireInterface<T>(id, major, minor, patch);
    }

    BuiltinServices m_Builtins{};
};

// ============================================================================
// ModuleServices
// ============================================================================

/**
 * @brief Per-module bound service view.
 *
 * Holds module identity plus a reference to the RuntimeServiceHub.
 * Provides bound convenience wrappers for Log, Config, and
 * SubscriptionManager. Owner-aware interfaces are preferred first;
 * TLS rebinding is only used as a legacy fallback when the runtime
 * does not expose an explicit owner path yet.
 *
 * Move-only. Owned by the module instance (bml::Module::m_Services).
 */
class ModuleServices {
public:
    ModuleServices() = default;

    ModuleServices(BML_Mod mod, const RuntimeServiceHub *hub)
        : m_Handle(mod), m_Hub(hub) {
        if (!hub) return;

        const auto &b = hub->Builtins();
        m_GlobalContext = (b.Context && b.Context->GetGlobalContext)
            ? b.Context->GetGlobalContext()
            : nullptr;

        const char *mod_id = nullptr;
        if (b.Module && b.Module->GetModId) {
            b.Module->GetModId(mod, &mod_id);
        }

        m_Logger.emplace(m_GlobalContext, mod_id ? mod_id : "", b.Logging, m_Handle);
        m_Locale = bml::Locale(m_Handle, b.Locale, b.Module);
    }

    ModuleServices(const ModuleServices &) = delete;
    ModuleServices &operator=(const ModuleServices &) = delete;

    ModuleServices(ModuleServices &&other) noexcept
        : m_Handle(other.m_Handle),
          m_Hub(other.m_Hub),
          m_Logger(std::move(other.m_Logger)),
          m_GlobalContext(other.m_GlobalContext),
          m_Locale(std::move(other.m_Locale)) {
        other.m_Handle = nullptr;
        other.m_Hub = nullptr;
        other.m_GlobalContext = nullptr;
        other.m_Locale = bml::Locale();
    }

    ModuleServices &operator=(ModuleServices &&other) noexcept {
        if (this != &other) {
            m_Handle = other.m_Handle;
            m_Hub = other.m_Hub;
            m_Logger = std::move(other.m_Logger);
            m_GlobalContext = other.m_GlobalContext;
            m_Locale = std::move(other.m_Locale);
            other.m_Handle = nullptr;
            other.m_Hub = nullptr;
            other.m_GlobalContext = nullptr;
            other.m_Locale = bml::Locale();
        }
        return *this;
    }

    explicit operator bool() const noexcept {
        return m_Handle != nullptr && m_Hub != nullptr;
    }

    BML_Mod Handle() const noexcept { return m_Handle; }
    BML_Context GlobalContext() const noexcept { return m_GlobalContext; }

    const RuntimeServiceHub &Hub() const noexcept { return *m_Hub; }
    const BuiltinServices &Builtins() const noexcept { return m_Hub->Builtins(); }

    const bml::Logger &Log() const noexcept { return *m_Logger; }

    bml::Config Config() const {
        return bml::Config(m_Handle, m_Hub ? m_Hub->Builtins().Config : nullptr);
    }

    const bml::Locale &Locale() const noexcept {
        return m_Locale;
    }

    bml::TimerService Timer() const {
        return bml::TimerService(m_Hub ? m_Hub->Builtins().Timer : nullptr, m_Handle);
    }

    bml::HookService Hooks() const {
        return bml::HookService(m_Hub ? m_Hub->Builtins().HookRegistry : nullptr);
    }

    // Opt-in wrappers - require explicit #include of bml_sync.hpp / bml_profiling.hpp
    bml::SyncService Sync() const;
    bml::ProfilingService Profiling() const;

    bml::imc::SubscriptionManager CreateSubscriptions() const {
        return bml::imc::SubscriptionManager(m_Hub ? m_Hub->Builtins().ImcBus : nullptr, m_Handle);
    }

    bml::imc::RpcServiceManager CreateRpcServices() const {
        return bml::imc::RpcServiceManager(m_Hub ? m_Hub->Builtins().ImcRpc : nullptr, m_Handle);
    }

    bml::imc::RpcClient CreateRpcClient(std::string_view name) const {
        return bml::imc::RpcClient(name, m_Hub ? m_Hub->Builtins().ImcRpc : nullptr, m_Handle);
    }

    /**
     * @brief Get this mod's installation directory as UTF-8
     * @return The directory path, or std::nullopt if unavailable
     */
    std::optional<std::string_view> Directory() const {
        const auto *mod = m_Hub ? m_Hub->Builtins().Module : nullptr;
        if (!mod || !mod->GetModDirectory) return std::nullopt;
        const char *dir = nullptr;
        if (mod->GetModDirectory(m_Handle, &dir) == BML_RESULT_OK && dir)
            return std::string_view(dir);
        return std::nullopt;
    }

    /**
     * @brief Get this mod's display name
     * @return The name, or std::nullopt if unavailable
     */
    std::optional<std::string_view> Name() const {
        const auto *mod = m_Hub ? m_Hub->Builtins().Module : nullptr;
        if (!mod || !mod->GetModName) return std::nullopt;
        const char *name = nullptr;
        if (mod->GetModName(m_Handle, &name) == BML_RESULT_OK && name)
            return std::string_view(name);
        return std::nullopt;
    }

    /**
     * @brief Get this mod's description
     * @return The description, or std::nullopt if unavailable
     */
    std::optional<std::string_view> Description() const {
        const auto *mod = m_Hub ? m_Hub->Builtins().Module : nullptr;
        if (!mod || !mod->GetModDescription) return std::nullopt;
        const char *desc = nullptr;
        if (mod->GetModDescription(m_Handle, &desc) == BML_RESULT_OK && desc)
            return std::string_view(desc);
        return std::nullopt;
    }

    /**
     * @brief Get the number of authors for this mod
     */
    uint32_t AuthorCount() const {
        const auto *mod = m_Hub ? m_Hub->Builtins().Module : nullptr;
        if (!mod || !mod->GetModAuthorCount) return 0;
        uint32_t count = 0;
        mod->GetModAuthorCount(m_Handle, &count);
        return count;
    }

    /**
     * @brief Get an author string by index
     * @param index Zero-based author index
     * @return The author string, or std::nullopt if out of range
     */
    std::optional<std::string_view> AuthorAt(uint32_t index) const {
        const auto *mod = m_Hub ? m_Hub->Builtins().Module : nullptr;
        if (!mod || !mod->GetModAuthorAt) return std::nullopt;
        const char *author = nullptr;
        if (mod->GetModAuthorAt(m_Handle, index, &author) == BML_RESULT_OK && author)
            return std::string_view(author);
        return std::nullopt;
    }

    /**
     * @brief Read a custom string field from this mod's manifest.
     * @param path Dot-separated path, e.g. "mymod.greeting"
     * @return The string value, or std::nullopt if not found or wrong type
     */
    std::optional<std::string_view> ManifestString(const char *path) const {
        const auto *mod = m_Hub ? m_Hub->Builtins().Module : nullptr;
        if (!mod || !mod->GetManifestString) return std::nullopt;
        const char *value = nullptr;
        if (mod->GetManifestString(m_Handle, path, &value) == BML_RESULT_OK && value)
            return std::string_view(value);
        return std::nullopt;
    }

    std::optional<int64_t> ManifestInt(const char *path) const {
        const auto *mod = m_Hub ? m_Hub->Builtins().Module : nullptr;
        if (!mod || !mod->GetManifestInt) return std::nullopt;
        int64_t value = 0;
        if (mod->GetManifestInt(m_Handle, path, &value) == BML_RESULT_OK)
            return value;
        return std::nullopt;
    }

    std::optional<double> ManifestFloat(const char *path) const {
        const auto *mod = m_Hub ? m_Hub->Builtins().Module : nullptr;
        if (!mod || !mod->GetManifestFloat) return std::nullopt;
        double value = 0.0;
        if (mod->GetManifestFloat(m_Handle, path, &value) == BML_RESULT_OK)
            return value;
        return std::nullopt;
    }

    std::optional<bool> ManifestBool(const char *path) const {
        const auto *mod = m_Hub ? m_Hub->Builtins().Module : nullptr;
        if (!mod || !mod->GetManifestBool) return std::nullopt;
        BML_Bool value = BML_FALSE;
        if (mod->GetManifestBool(m_Handle, path, &value) == BML_RESULT_OK)
            return value == BML_TRUE;
        return std::nullopt;
    }

    template <typename T>
    InterfaceLease<T> Acquire() const {
        using Trait = bml::InterfaceTrait<T>;
        auto lease = bml::AcquireInterfaceOwned<T>(
            m_Handle, Trait::Id, Trait::AbiMajor, Trait::AbiMinor);
        if (lease) {
            return lease;
        }

        ScopedModuleContext guard(m_Handle, m_Hub ? m_Hub->Builtins().Module : nullptr);
        return bml::Acquire<T>();
    }

    template <typename T>
    InterfaceLease<T> Acquire(const char *id,
                              uint16_t major,
                              uint16_t minor = 0,
                              uint16_t patch = 0) const {
        auto lease = bml::AcquireInterfaceOwned<T>(m_Handle, id, major, minor, patch);
        if (lease) {
            return lease;
        }

        ScopedModuleContext guard(m_Handle, m_Hub ? m_Hub->Builtins().Module : nullptr);
        return bml::AcquireInterface<T>(id, major, minor, patch);
    }

private:
    BML_Mod m_Handle = nullptr;
    const RuntimeServiceHub *m_Hub = nullptr;
    std::optional<bml::Logger> m_Logger;
    BML_Context m_GlobalContext = nullptr;
    bml::Locale m_Locale;
};

// ============================================================================
// Opt-in method implementations
// ============================================================================

#ifdef BML_SYNC_HPP
inline bml::SyncService ModuleServices::Sync() const {
    return bml::SyncService(m_Hub ? m_Hub->Builtins().Sync : nullptr);
}
#endif

#ifdef BML_PROFILING_HPP
inline bml::ProfilingService ModuleServices::Profiling() const {
    return bml::ProfilingService(m_Hub ? m_Hub->Builtins().Profiling : nullptr);
}
#endif

} // namespace bml

#endif /* BML_SERVICES_HPP */
