/**
 * @file bml_module.hpp
 * @brief C++ Module Framework for BML
 *
 * Provides a base class and macro that eliminate all module entry point
 * boilerplate. Module authors derive from bml::Module, override
 * OnAttach() and OnDetach(), then use BML_DEFINE_MODULE(ClassName) to
 * generate the entry point.
 *
 * Usage:
 *   #include <bml_module.hpp>
 *
 *   class MyMod : public bml::Module {
 *       bml::imc::SubscriptionManager m_Subs;
 *   public:
 *       BML_Result OnAttach() override {
 *           m_Subs = Services().CreateSubscriptions();
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
 *           -> construct ModuleServices -> set s_Instance -> OnAttach() ->
 *           on failure: rollback s_Instance + delete + bmlUnloadAPI()
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
 * Derive from this class and override OnAttach()/OnDetach() to implement
 * module lifecycle. Use BML_DEFINE_MODULE(YourClass) after the class
 * definition to generate the entry point.
 */
class Module {
    template <typename T> friend class detail::ModuleEntryHelper;

public:
    virtual ~Module() = default;

    /**
     * @brief Called after API is loaded, m_Handle/m_Services are set.
     *
     * Services() is valid and can be used freely.
     * GetInstance() also returns this module during OnAttach.
     *
     * @return BML_RESULT_OK on success; any other value aborts attach.
     */
    virtual BML_Result OnAttach() { return BML_RESULT_OK; }

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

    /**
     * @brief Called instead of OnPrepareDetach when the module is about to be
     * hot-reloaded.  Return an error to abort the reload (falls back to full
     * reload).  Default: OK (allow reload).
     */
    virtual BML_Result OnPrepareReload() { return BML_RESULT_OK; }

    /**
     * @brief Called instead of OnAttach when the module is re-attached after a
     * hot-reload.  Default delegates to OnAttach().
     */
    virtual BML_Result OnReloadAttach() { return OnAttach(); }

    /** @brief Get the mod handle assigned by the runtime. */
    BML_Mod Handle() const noexcept { return m_Handle; }

    /** @brief Access the module's service view (valid after construction). */
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
        case BML_MOD_ENTRYPOINT_PREPARE_RELOAD:
            return PrepareReload(static_cast<const BML_ModPrepareReloadArgs *>(data));
        default:
            return BML_RESULT_INVALID_ARGUMENT;
        }
    }

private:
    static BML_Result Attach(const BML_ModAttachArgs *args) {
        // Use offsetof(is_reload) as the minimum accepted size so that
        // old runtimes that send the V1 struct (without is_reload) are
        // still accepted.  The is_reload field itself is gated by a
        // separate struct_size check below.
        if (!args || args->struct_size < offsetof(BML_ModAttachArgs, is_reload) ||
            !args->mod || !args->services) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (args->api_version < BML_MOD_ENTRYPOINT_API_VERSION) {
            return BML_RESULT_VERSION_MISMATCH;
        }
        if (s_Instance) {
            return BML_RESULT_ALREADY_INITIALIZED;
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

        // Set s_Instance before OnAttach so GetInstance() is available
        // during attach (e.g. for extension API trampoline registration).
        // BoundModuleHandleSlot bridges the gap for code that needs the
        // handle before m_Handle is accessible (e.g. during T's ctor).
        s_Instance = instance;
        BoundModuleHandleSlot() = args->mod;

        // Check if this is a reload re-attach (V2 extension field)
        bool is_reload = false;
        if (args->struct_size >= offsetof(BML_ModAttachArgs, is_reload) + sizeof(args->is_reload)) {
            is_reload = (args->is_reload == BML_TRUE);
        }
        BML_Result result = is_reload ? instance->OnReloadAttach()
                                      : instance->OnAttach();
        if (result != BML_RESULT_OK) {
            s_Instance = nullptr;
            BoundModuleHandleSlot() = nullptr;
            delete instance;
            bmlUnloadAPI();
            return result;
        }

        BoundModuleHandleSlot() = nullptr;
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

    static BML_Result PrepareReload(const BML_ModPrepareReloadArgs *args) {
        if (!args || args->struct_size < sizeof(BML_ModPrepareReloadArgs) ||
            !args->mod) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (args->api_version < BML_MOD_ENTRYPOINT_API_VERSION) {
            return BML_RESULT_VERSION_MISMATCH;
        }
        if (!s_Instance) {
            return BML_RESULT_OK;
        }
        return s_Instance->OnPrepareReload();
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
            delete s_Instance;
            s_Instance = nullptr;
        }

        bmlUnloadAPI();
        return BML_RESULT_OK;
    }

    static inline T *s_Instance = nullptr;
};

} // namespace detail

/**
 * @brief Access the live module instance (nullptr when detached).
 *
 * The framework sets the instance before OnAttach() and clears it after
 * OnDetach() + delete, so this is valid from any code that runs between
 * those two points (callbacks, hooks, IMC trampolines, etc.).
 *
 * Usage:
 * @code
 *   // From a static callback or trampoline:
 *   auto *self = bml::GetModuleInstance<MyMod>();
 *   if (self) self->DoSomething();
 * @endcode
 */
template <typename T>
T *GetModuleInstance() noexcept {
    static_assert(std::is_base_of_v<Module, T>,
                  "T must derive from bml::Module");
    return detail::ModuleEntryHelper<T>::GetInstance();
}

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

/**
 * @brief Access the live module instance from extension API trampolines.
 * Returns nullptr when the module is not attached.
 * @deprecated Prefer bml::GetModuleInstance<T>() instead.
 */
#define BML_GET_INSTANCE(ClassName) \
    ::bml::detail::ModuleEntryHelper<ClassName>::GetInstance()

#endif /* BML_MODULE_HPP */
