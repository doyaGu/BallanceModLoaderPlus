#include "bml_interface.h"

#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "InterfaceRegistry.h"
#include "LeaseManager.h"

namespace BML::Core {
    namespace {
        BML_Mod_T *ResolveCaller(BML_Mod owner, Context **out_context = nullptr) {
            auto *context = Context::ContextFromMod(owner);
            if (out_context) {
                *out_context = context;
            }
            if (!context || !owner) {
                return nullptr;
            }
            return context->ResolveModHandle(owner);
        }
    } // namespace

    BML_Result BML_API_InterfaceRegister(BML_Mod owner, const BML_InterfaceDesc *desc) {
        auto *kernel = Context::KernelFromMod(owner);
        Context *context = nullptr;
        auto *provider = ResolveCaller(owner, &context);
        if (!kernel || !context) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        if (!provider) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel->interface_registry->Register(desc, provider);
    }

    BML_Result BML_API_InterfaceAcquire(BML_Mod owner,
                                        const char *interface_id,
                                        const BML_Version *required_abi,
                                        const void **out_implementation,
                                        BML_InterfaceLease *out_lease) {
        auto *kernel = Context::KernelFromMod(owner);
        Context *context = nullptr;
        auto *consumer = ResolveCaller(owner, &context);
        if (!kernel || !context) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        if (!consumer) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel->interface_registry->Acquire(
            interface_id, required_abi, consumer, out_implementation, out_lease);
    }

    BML_Result BML_API_InterfaceRelease(BML_InterfaceLease lease) {
        auto *kernel = LeaseManager::KernelFromLease(lease);
        if (!kernel || !kernel->interface_registry) {
            return BML_RESULT_INVALID_HANDLE;
        }
        return kernel->interface_registry->Release(lease);
    }

    void BML_API_InterfaceAddRef(BML_InterfaceLease lease) {
        auto *kernel = LeaseManager::KernelFromLease(lease);
        if (!kernel || !kernel->leases) {
            return;
        }
        kernel->leases->AddRefInterfaceLease(lease);
    }

    BML_Result BML_API_InterfaceUnregister(BML_Mod owner, const char *interface_id) {
        auto *kernel = Context::KernelFromMod(owner);
        Context *context = nullptr;
        auto *provider = ResolveCaller(owner, &context);
        if (!kernel || !context) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        if (!provider) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel->interface_registry->Unregister(interface_id, provider);
    }

    void RegisterInterfaceApis(ApiRegistry &apiRegistry) {
        BML_BEGIN_API_REGISTRATION(apiRegistry);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceRegister, "interface", BML_API_InterfaceRegister);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceAcquire, "interface", BML_API_InterfaceAcquire);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceRelease, "interface", BML_API_InterfaceRelease);
        BML_REGISTER_API_VOID_GUARDED(
            bmlInterfaceAddRef, "interface", BML_API_InterfaceAddRef);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceUnregister, "interface", BML_API_InterfaceUnregister);
    }
} // namespace BML::Core
