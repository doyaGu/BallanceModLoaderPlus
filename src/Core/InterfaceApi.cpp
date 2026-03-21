#include "bml_interface.h"

#include "ApiRegistrationMacros.h"
#include "Context.h"
#include "InterfaceRegistry.h"

namespace BML::Core {
    namespace {
        const char *CurrentModuleId() {
            auto *current = GetKernelOrNull()->context->ResolveModHandle(Context::GetCurrentModule());
            return current ? current->id.c_str() : nullptr;
        }
    } // namespace

    BML_Result BML_API_InterfaceRegister(const BML_InterfaceDesc *desc) {
        const char *provider_id = CurrentModuleId();
        if (!provider_id) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return GetKernelOrNull()->interface_registry->Register(desc, provider_id);
    }

    BML_Result BML_API_InterfaceAcquire(const char *interface_id,
                                        const BML_Version *required_abi,
                                        const void **out_implementation,
                                        BML_InterfaceLease *out_lease) {
        return GetKernelOrNull()->interface_registry->Acquire(interface_id, required_abi, out_implementation, out_lease);
    }

    BML_Result BML_API_InterfaceRelease(BML_InterfaceLease lease) {
        return GetKernelOrNull()->interface_registry->Release(lease);
    }

    BML_Result BML_API_InterfaceUnregister(const char *interface_id) {
        const char *provider_id = CurrentModuleId();
        if (!provider_id) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return GetKernelOrNull()->interface_registry->Unregister(interface_id, provider_id);
    }

    void RegisterInterfaceApis() {
        BML_BEGIN_API_REGISTRATION();
        BML_REGISTER_API_GUARDED(
            bmlInterfaceRegister, "interface", BML_API_InterfaceRegister);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceAcquire, "interface", BML_API_InterfaceAcquire);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceRelease, "interface", BML_API_InterfaceRelease);
        BML_REGISTER_API_GUARDED(
            bmlInterfaceUnregister, "interface", BML_API_InterfaceUnregister);
    }
} // namespace BML::Core
