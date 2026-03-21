#include "ApiRegistrationMacros.h"
#include "HookRegistry.h"

#include "Context.h"

#include "bml_hook.h"

namespace BML::Core {
    namespace {
        BML_Mod_T *ResolveBoundModule() {
            auto current = Context::GetCurrentModule();
            if (!current) {
                return nullptr;
            }
            return GetKernelOrNull()->context->ResolveModHandle(current);
        }
    } // namespace

    BML_Result BML_API_HookRegister(const BML_HookDesc *desc) {
        auto *owner = ResolveBoundModule();
        if (!owner) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return GetKernelOrNull()->hooks->Register(owner->id, desc);
    }

    BML_Result BML_API_HookUnregister(void *target_address) {
        auto *owner = ResolveBoundModule();
        if (!owner) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return GetKernelOrNull()->hooks->Unregister(owner->id, target_address);
    }

    BML_Result BML_API_HookEnumerate(BML_HookEnumCallback callback, void *user_data) {
        return GetKernelOrNull()->hooks->Enumerate(callback, user_data);
    }

    void RegisterHookApis() {
        BML_BEGIN_API_REGISTRATION();

        BML_REGISTER_API_GUARDED(bmlHookRegister, "hook", BML_API_HookRegister);
        BML_REGISTER_API_GUARDED(bmlHookUnregister, "hook", BML_API_HookUnregister);
        BML_REGISTER_API_GUARDED(bmlHookEnumerate, "hook", BML_API_HookEnumerate);
    }
} // namespace BML::Core
