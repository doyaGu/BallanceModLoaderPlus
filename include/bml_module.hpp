/**
 * @file bml_module.hpp
 * @brief C++ Module Framework for BML
 *
 * Provides a base class and macro that eliminate all module entry point
 * boilerplate. Module authors derive from bml::Module, override
 * OnAttach(ModuleServices&) and OnDetach(), then use
 * BML_DEFINE_MODULE(ClassName) to generate the entry point.
 *
 * Usage:
 *   #include <bml_module.hpp>
 *
 *   class MyMod : public bml::Module {
 *       bml::imc::SubscriptionManager m_Subs;
 *   public:
 *       BML_Result OnAttach(bml::ModuleServices &services) override {
 *           m_Subs = services.CreateSubscriptions();
 *           m_Subs.Add("BML/Engine/Init", [this](const bml::imc::Message &msg) {
 *               // Handle event
 *           });
 *           return m_Subs.Empty() ? BML_RESULT_FAIL : BML_RESULT_OK;
 *       }
 *       void OnDetach() override {
 *           // cleanup
 *       }
 *   };
 *   BML_DEFINE_MODULE(MyMod)
 *
 * Lifecycle:
 *   Attach: validate args -> bind injected services -> new T() -> set m_Handle
 *           -> construct ModuleServices -> OnAttach(services) ->
 *           on failure: delete + bmlUnloadAPI()
 *   PrepareDetach: validate args -> OnPrepareDetach()
 *   Detach: validate args -> OnDetach() -> delete (RAII dtors run,
 *           function pointers still valid) -> bmlUnloadAPI()
 *
 * Bootstrap note:
 *   Module attach no longer resolves runtime behavior through proc lookup.
 *   The runtime injects a stable service bundle in attach args.
 */

#ifndef BML_MODULE_HPP
#define BML_MODULE_HPP

#include "bml_module.h"
#include "bml.hpp"
#include "bml_services.hpp"

#include <new>
#include <type_traits>

namespace bml {
namespace detail {
    template <typename T> class ModuleEntryHelper;

    inline BML_Mod &BoundModuleHandleSlot() noexcept {
        static BML_Mod s_BoundModuleHandle = nullptr;
        return s_BoundModuleHandle;
    }

    inline BML_Mod GetBoundModuleHandle() noexcept {
        return BoundModuleHandleSlot();
    }
}

/**
 * @brief Base class for BML modules
 *
 * Derive from this class and override OnAttach(ModuleServices&)/OnDetach()
 * to implement module lifecycle. Use BML_DEFINE_MODULE(YourClass) after the
 * class definition to generate the entry point.
 */
class Module {
    template <typename T> friend class detail::ModuleEntryHelper;

public:
    virtual ~Module() = default;

    /**
     * @brief Called after API is loaded, m_Handle is set, and services are ready.
     * @param services Per-module service access (Log, Config, subscriptions, etc.)
     * @return BML_RESULT_OK on success; any other value aborts attach.
     */
    virtual BML_Result OnAttach(ModuleServices &services) { (void)services; return BML_RESULT_OK; }

    /**
     * @brief Called before detach gating to release outbound dependencies.
     * Return an error to abort unload/reload.
     */
    virtual BML_Result OnPrepareDetach() { return BML_RESULT_OK; }

    /**
     * @brief Called before the module instance is destroyed.
     * RAII members (SubscriptionManager, etc.) are still valid here.
     * Function pointers remain loaded until after destruction.
     */
    virtual void OnDetach() {}

    /** @brief Get the mod handle assigned by the runtime. */
    BML_Mod Handle() const noexcept { return m_Handle; }

    /** @brief Access the module's service view (valid after OnAttach). */
    const ModuleServices &Services() const noexcept { return m_Services; }

protected:
    BML_Mod m_Handle = nullptr;
    ModuleServices m_Services;

    template <typename T>
    InterfaceLease<T> Acquire(const char *id,
                              uint16_t major,
                              uint16_t minor = 0,
                              uint16_t patch = 0) const {
        return bml::AcquireInterface<T>(m_Handle, id, major, minor, patch);
    }

    PublishedInterface Publish(const BML_InterfaceDesc &desc) const {
        const auto *control = m_Services ? m_Services.Interfaces().InterfaceControl : nullptr;
        if (control && control->Register && control->Unregister) {
            return PublishedInterface(control->Register, control->Unregister, m_Handle, desc);
        }
        return PublishedInterface();
    }

    template <typename T>
    PublishedInterface Publish(const char *id,
                               const T *impl,
                               uint16_t major,
                               uint16_t minor = 0,
                               uint16_t patch = 0,
                               uint64_t flags = 0) const {
        return Publish(bml::MakeInterfaceDesc(id, impl, major, minor, patch, flags));
    }
};

namespace detail {

/**
 * @internal
 * @brief Generates the module entry point implementation for type T.
 *
 * The static s_Instance pointer serves as the single mutable "global"
 * managed entirely by the framework. Extension API trampolines can
 * access it via GetInstance().
 */
template <typename T>
class ModuleEntryHelper {
    static_assert(std::is_base_of_v<Module, T>,
                  "T must derive from bml::Module");

public:
    /** @brief Access the live module instance (nullptr when detached). */
    static T *GetInstance() noexcept { return s_Instance; }

    static BML_Result Entrypoint(BML_ModEntrypointCommand cmd, void *data) {
        switch (cmd) {
        case BML_MOD_ENTRYPOINT_ATTACH:
            return Attach(static_cast<const BML_ModAttachArgs *>(data));
        case BML_MOD_ENTRYPOINT_PREPARE_DETACH:
            return PrepareDetach(static_cast<const BML_ModDetachArgs *>(data));
        case BML_MOD_ENTRYPOINT_DETACH:
            return Detach(static_cast<const BML_ModDetachArgs *>(data));
        default:
            return BML_RESULT_INVALID_ARGUMENT;
        }
    }

private:
    static BML_Result Attach(const BML_ModAttachArgs *args) {
        if (!args || args->struct_size < sizeof(BML_ModAttachArgs) ||
            !args->mod || !args->services) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (args->api_version < BML_MOD_ENTRYPOINT_API_VERSION) {
            return BML_RESULT_VERSION_MISMATCH;
        }

        bmlBindServices(args->services);

        auto *instance = new (std::nothrow) T();
        if (!instance) {
            bmlUnloadAPI();
            return BML_RESULT_OUT_OF_MEMORY;
        }

        instance->m_Handle = args->mod;
        instance->m_Services = ModuleServices(args->mod, args->services);
        if (!instance->m_Services) {
            delete instance;
            bmlUnloadAPI();
            return BML_RESULT_NOT_FOUND;
        }

        BoundModuleHandleSlot() = args->mod;
        BML_Result result = instance->OnAttach(instance->m_Services);
        if (result != BML_RESULT_OK) {
            BoundModuleHandleSlot() = nullptr;
            delete instance;
            bmlUnloadAPI();
            return result;
        }

        s_Instance = instance;
        return BML_RESULT_OK;
    }

    static BML_Result PrepareDetach(const BML_ModDetachArgs *args) {
        if (!args || args->struct_size < sizeof(BML_ModDetachArgs) ||
            !args->mod) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (args->api_version < BML_MOD_ENTRYPOINT_API_VERSION) {
            return BML_RESULT_VERSION_MISMATCH;
        }

        if (!s_Instance) {
            return BML_RESULT_OK;
        }

        return s_Instance->OnPrepareDetach();
    }

    static BML_Result Detach(const BML_ModDetachArgs *args) {
        if (!args || args->struct_size < sizeof(BML_ModDetachArgs) ||
            !args->mod) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (args->api_version < BML_MOD_ENTRYPOINT_API_VERSION) {
            return BML_RESULT_VERSION_MISMATCH;
        }

        if (s_Instance) {
            s_Instance->OnDetach();
            BoundModuleHandleSlot() = nullptr;
            delete s_Instance;
            s_Instance = nullptr;
        }

        bmlUnloadAPI();
        return BML_RESULT_OK;
    }

    static inline T *s_Instance = nullptr;
};

} // namespace detail
} // namespace bml

/**
 * @brief Generate the module entry point for a bml::Module subclass.
 * Place this at file scope after the class definition.
 */
#define BML_DEFINE_MODULE(ClassName) \
    BML_MODULE_ENTRY BML_Result BML_ModEntrypoint( \
        BML_ModEntrypointCommand cmd, void *data) { \
        return ::bml::detail::ModuleEntryHelper<ClassName>::Entrypoint(cmd, data); \
    }

#endif /* BML_MODULE_HPP */
