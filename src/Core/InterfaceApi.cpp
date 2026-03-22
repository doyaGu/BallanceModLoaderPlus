#include "bml_interface.h"

#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "InterfaceRegistry.h"

namespace BML::Core {
    namespace {
        BML_Mod_T *ResolveCaller(Context &context, BML_Mod owner) {
            if (!owner) {
                return nullptr;
            }
            return context.ResolveModHandle(owner);
        }

        BML_Mod_T *ResolveLegacyCaller(Context &context) {
            return ResolveCaller(context, Context::GetCurrentModule());
        }
    } // namespace

    BML_Result BML_API_InterfaceRegisterOwned(BML_Mod owner, const BML_InterfaceDesc *desc) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto *provider = ResolveCaller(context, owner);
        if (!provider) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.interface_registry->Register(desc, provider);
    }

    BML_Result BML_API_InterfaceRegister(const BML_InterfaceDesc *desc) {
        auto &context = *Kernel().context;
        auto *provider = ResolveLegacyCaller(context);
        if (!provider) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return BML_API_InterfaceRegisterOwned(provider, desc);
    }

    BML_Result BML_API_InterfaceAcquireOwned(BML_Mod owner,
                                             const char *interface_id,
                                             const BML_Version *required_abi,
                                             const void **out_implementation,
                                             BML_InterfaceLease *out_lease) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto *consumer = ResolveCaller(context, owner);
        if (!consumer) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.interface_registry->Acquire(
            interface_id, required_abi, consumer, out_implementation, out_lease);
    }

    BML_Result BML_API_InterfaceAcquire(const char *interface_id,
                                        const BML_Version *required_abi,
                                        const void **out_implementation,
                                        BML_InterfaceLease *out_lease) {
        auto &context = *Kernel().context;
        auto *consumer = ResolveLegacyCaller(context);
        if (!consumer) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return BML_API_InterfaceAcquireOwned(
            consumer, interface_id, required_abi, out_implementation, out_lease);
    }

    BML_Result BML_API_InterfaceRelease(BML_InterfaceLease lease) {
        auto &interfaceRegistry = *Kernel().interface_registry;
        return interfaceRegistry.Release(lease);
    }

    BML_Result BML_API_InterfaceUnregisterOwned(BML_Mod owner, const char *interface_id) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto *provider = ResolveCaller(context, owner);
        if (!provider) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.interface_registry->Unregister(interface_id, provider);
    }

    BML_Result BML_API_InterfaceUnregister(const char *interface_id) {
        auto &context = *Kernel().context;
        auto *provider = ResolveLegacyCaller(context);
        if (!provider) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return BML_API_InterfaceUnregisterOwned(provider, interface_id);
    }

    void RegisterInterfaceApis() {
        BML_BEGIN_API_REGISTRATION();
        BML_REGISTER_API_GUARDED(
            bmlInterfaceRegister, "interface", BML_API_InterfaceRegister);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceRegisterOwned, "interface", BML_API_InterfaceRegisterOwned);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceAcquire, "interface", BML_API_InterfaceAcquire);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceAcquireOwned, "interface", BML_API_InterfaceAcquireOwned);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceRelease, "interface", BML_API_InterfaceRelease);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceUnregister, "interface", BML_API_InterfaceUnregister);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceUnregisterOwned, "interface", BML_API_InterfaceUnregisterOwned);
    }
} // namespace BML::Core
