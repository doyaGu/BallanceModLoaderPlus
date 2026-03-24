/**
 * @file bml_services.hpp
 * @brief Service Hub and Module Services for BML v0.4
 *
 * Provides RuntimeServiceHub (runtime-wide typed service access) and
 * ModuleServices (per-module bound service view). These types eliminate
 * repeated runtime interface binding boilerplate across modules.
 *
 * @section service_layers Service Access Layers
 *
 * Three layers exist, each for a different context:
 *   - **ModuleServices** -- Use in all module code. Provides typed accessors
 *     (Log(), Config(), Timer(), etc.) and automatic owner injection.
 *   - **BML_Services*** -- Raw C struct. Use only in C callbacks or when
 *     passing interface pointers across DLL boundaries.
 *   - **RuntimeServiceHub** -- Internal to BML.dll (Core). Modules should
 *     never access this directly.
 */

#ifndef BML_SERVICES_HPP
#define BML_SERVICES_HPP

#include "bml_export.h"
#include "bml_core.h"
#include "bml_logging.h"
#include "bml_config.h"
#include "bml_memory.h"
#include "bml_resource.h"
#include "bml_interface.h"
#include "bml_imc.h"
#include "bml_timer.h"
#include "bml_locale.h"
#include "bml_hook.h"
#include "bml_sync.h"
#include "bml_profiling.h"
#include "bml_module_runtime.h"
#include "bml_interface.hpp"
#include "bml_locale.hpp"
#include "bml_logger.hpp"
#include "bml_config.hpp"
#include "bml_timer.hpp"
#include "bml_hook.hpp"
#include "bml_imc_subscription.hpp"
#include "bml_imc_rpc.hpp"

#include <optional>

namespace bml {

// ============================================================================
// RuntimeServiceHub
// ============================================================================

/**
 * @brief Runtime-wide service access layer.
 *
 * One instance per runtime, owned by BML::Core::Context.
 * Modules access it through a borrowed const pointer injected during attach.
 */
class RuntimeServiceHub {
public:
    RuntimeServiceHub() noexcept
        : m_Services{
              &m_ContextInterface,
              &m_LoggingInterface,
              &m_ModuleInterface,
              &m_ConfigInterface,
              &m_MemoryInterface,
              &m_ResourceInterface,
              &m_DiagnosticInterface,
              &m_InterfaceControlInterface,
              &m_ImcBusInterface,
              &m_ImcRpcInterface,
              &m_TimerInterface,
              &m_LocaleInterface,
              &m_HookRegistryInterface,
              &m_SyncInterface,
              &m_ProfilingInterface,
              &m_HostRuntimeInterface} {}

    const BML_Services &Interfaces() const noexcept { return m_Services; }

    template <typename T>
    InterfaceLease<T> Acquire(BML_Mod owner,
                              const char *id,
                              uint16_t major,
                              uint16_t minor = 0,
                              uint16_t patch = 0) const {
        return bml::AcquireInterface<T>(owner, id, major, minor, patch);
    }

    BML_Services m_Services{};
    BML_CoreContextInterface m_ContextInterface{};
    BML_CoreLoggingInterface m_LoggingInterface{};
    BML_CoreModuleInterface m_ModuleInterface{};
    BML_CoreConfigInterface m_ConfigInterface{};
    BML_CoreMemoryInterface m_MemoryInterface{};
    BML_CoreResourceInterface m_ResourceInterface{};
    BML_CoreDiagnosticInterface m_DiagnosticInterface{};
    BML_CoreInterfaceControlInterface m_InterfaceControlInterface{};
    BML_ImcBusInterface m_ImcBusInterface{};
    BML_ImcRpcInterface m_ImcRpcInterface{};
    BML_CoreTimerInterface m_TimerInterface{};
    BML_CoreLocaleInterface m_LocaleInterface{};
    BML_CoreHookRegistryInterface m_HookRegistryInterface{};
    BML_CoreSyncInterface m_SyncInterface{};
    BML_CoreProfilingInterface m_ProfilingInterface{};
    BML_HostRuntimeInterface m_HostRuntimeInterface{};
};

// ============================================================================
// ModuleServices
// ============================================================================

/**
 * @brief Per-module bound service view.
 *
 * Holds module identity plus a reference to the injected runtime service bundle.
 * Provides bound convenience wrappers for Log, Config, and
 * SubscriptionManager. All caller-sensitive operations use explicit
 * module-bound runtime entry points.
 *
 * Move-only. Held by the module instance (bml::Module::m_Services).
 */
class ModuleServices {
public:
    ModuleServices() = default;

    ModuleServices(BML_Mod mod, const BML_Services *services)
        : m_Handle(mod), m_Interfaces(services) {
        if (!services) return;

        const auto &b = *services;
        m_GlobalContext = b.Context ? b.Context->Context : nullptr;

        const char *mod_id = nullptr;
        if (b.Module && b.Module->GetModId) {
            b.Module->GetModId(mod, &mod_id);
        }

        m_Logger = bml::Logger(mod_id ? mod_id : "", b.Logging, m_Handle);
        m_Locale = bml::Locale(m_Handle, b.Locale, b.Module);
    }

    ModuleServices(const ModuleServices &) = delete;
    ModuleServices &operator=(const ModuleServices &) = delete;

    ModuleServices(ModuleServices &&other) noexcept
        : m_Handle(other.m_Handle),
          m_Interfaces(other.m_Interfaces),
          m_Logger(std::move(other.m_Logger)),
          m_GlobalContext(other.m_GlobalContext),
          m_Locale(std::move(other.m_Locale)) {
        other.m_Handle = nullptr;
        other.m_Interfaces = nullptr;
        other.m_GlobalContext = nullptr;
        other.m_Locale = bml::Locale();
    }

    ModuleServices &operator=(ModuleServices &&other) noexcept {
        if (this != &other) {
            m_Handle = other.m_Handle;
            m_Interfaces = other.m_Interfaces;
            m_Logger = std::move(other.m_Logger);
            m_GlobalContext = other.m_GlobalContext;
            m_Locale = std::move(other.m_Locale);
            other.m_Handle = nullptr;
            other.m_Interfaces = nullptr;
            other.m_GlobalContext = nullptr;
            other.m_Locale = bml::Locale();
        }
        return *this;
    }

    explicit operator bool() const noexcept {
        return m_Handle != nullptr && m_Interfaces != nullptr;
    }

    BML_Mod Handle() const noexcept { return m_Handle; }
    BML_Context GlobalContext() const noexcept { return m_GlobalContext; }

    const BML_Services &Interfaces() const noexcept { return *m_Interfaces; }

    const bml::Logger &Log() const noexcept { return m_Logger; }

    bml::Config Config() const {
        return bml::Config(m_Handle, m_Interfaces ? m_Interfaces->Config : nullptr);
    }

    const bml::Locale &Locale() const noexcept {
        return m_Locale;
    }

    bml::TimerService Timer() const {
        return bml::TimerService(m_Interfaces ? m_Interfaces->Timer : nullptr, m_Handle);
    }

    bml::HookService Hooks() const {
        return bml::HookService(m_Interfaces ? m_Interfaces->HookRegistry : nullptr, m_Handle);
    }

    // Opt-in wrappers: include bml_sync.hpp / bml_profiling.hpp BEFORE this header
#ifdef BML_SYNC_HPP
    bml::SyncService Sync() const {
        return bml::SyncService(m_Interfaces ? m_Interfaces->Sync : nullptr, m_Handle);
    }
#endif

#ifdef BML_PROFILING_HPP
    bml::ProfilingService Profiling() const {
        return bml::ProfilingService(m_Interfaces ? m_Interfaces->Profiling : nullptr, m_Handle);
    }
#endif

    bml::imc::SubscriptionManager CreateSubscriptions() const {
        return bml::imc::SubscriptionManager(m_Interfaces ? m_Interfaces->ImcBus : nullptr, m_Handle);
    }

    bml::imc::RpcServiceManager CreateRpcServices() const {
        return bml::imc::RpcServiceManager(m_Interfaces ? m_Interfaces->ImcRpc : nullptr, m_Handle);
    }

    bml::imc::RpcClient CreateRpcClient(std::string_view name) const {
        return bml::imc::RpcClient(name, m_Interfaces ? m_Interfaces->ImcRpc : nullptr, m_Handle);
    }

    /**
     * @brief Get this mod's installation directory as UTF-8
     * @return The directory path, or std::nullopt if unavailable
     */
    std::optional<std::string_view> Directory() const {
        const auto *mod = m_Interfaces ? m_Interfaces->Module : nullptr;
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
        const auto *mod = m_Interfaces ? m_Interfaces->Module : nullptr;
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
        const auto *mod = m_Interfaces ? m_Interfaces->Module : nullptr;
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
        const auto *mod = m_Interfaces ? m_Interfaces->Module : nullptr;
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
        const auto *mod = m_Interfaces ? m_Interfaces->Module : nullptr;
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
        const auto *mod = m_Interfaces ? m_Interfaces->Module : nullptr;
        if (!mod || !mod->GetManifestString) return std::nullopt;
        const char *value = nullptr;
        if (mod->GetManifestString(m_Handle, path, &value) == BML_RESULT_OK && value)
            return std::string_view(value);
        return std::nullopt;
    }

    std::optional<int64_t> ManifestInt(const char *path) const {
        const auto *mod = m_Interfaces ? m_Interfaces->Module : nullptr;
        if (!mod || !mod->GetManifestInt) return std::nullopt;
        int64_t value = 0;
        if (mod->GetManifestInt(m_Handle, path, &value) == BML_RESULT_OK)
            return value;
        return std::nullopt;
    }

    std::optional<double> ManifestFloat(const char *path) const {
        const auto *mod = m_Interfaces ? m_Interfaces->Module : nullptr;
        if (!mod || !mod->GetManifestFloat) return std::nullopt;
        double value = 0.0;
        if (mod->GetManifestFloat(m_Handle, path, &value) == BML_RESULT_OK)
            return value;
        return std::nullopt;
    }

    std::optional<bool> ManifestBool(const char *path) const {
        const auto *mod = m_Interfaces ? m_Interfaces->Module : nullptr;
        if (!mod || !mod->GetManifestBool) return std::nullopt;
        BML_Bool value = BML_FALSE;
        if (mod->GetManifestBool(m_Handle, path, &value) == BML_RESULT_OK)
            return value == BML_TRUE;
        return std::nullopt;
    }

    template <typename T>
    InterfaceLease<T> Acquire() const {
        using Trait = bml::InterfaceTrait<T>;
        return bml::AcquireInterface<T>(m_Handle, Trait::Id, Trait::AbiMajor, Trait::AbiMinor);
    }

    template <typename T>
    InterfaceLease<T> Acquire(const char *id,
                              uint16_t major,
                              uint16_t minor = 0,
                              uint16_t patch = 0) const {
        return bml::AcquireInterface<T>(m_Handle, id, major, minor, patch);
    }

private:
    BML_Mod m_Handle = nullptr;
    const BML_Services *m_Interfaces = nullptr;
    bml::Logger m_Logger;
    BML_Context m_GlobalContext = nullptr;
    bml::Locale m_Locale;
};

} // namespace bml

#endif /* BML_SERVICES_HPP */
