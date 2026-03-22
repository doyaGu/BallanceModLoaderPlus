#include "ApiRegistrationMacros.h"
#include "HookRegistry.h"

#include "Context.h"

#include "bml_hook.h"

namespace BML::Core {
    namespace {
        BML_Mod_T *ResolveBoundModule(Context &context, BML_Mod owner) {
            if (!owner) {
                return nullptr;
            }
            return context.ResolveModHandle(owner);
        }
    } // namespace

    BML_Result BML_API_HookRegisterOwned(BML_Mod owner, const BML_HookDesc *desc) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto *resolvedOwner = ResolveBoundModule(context, owner);
        if (!resolvedOwner) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.hooks->Register(resolvedOwner->id, desc);
    }

    BML_Result BML_API_HookRegister(const BML_HookDesc *desc) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto *owner = ResolveBoundModule(context, Context::GetCurrentModule());
        if (!owner) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.hooks->Register(owner->id, desc);
    }

    BML_Result BML_API_HookUnregisterOwned(BML_Mod owner, void *target_address) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto *resolvedOwner = ResolveBoundModule(context, owner);
        if (!resolvedOwner) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.hooks->Unregister(resolvedOwner->id, target_address);
    }

    BML_Result BML_API_HookUnregister(void *target_address) {
        auto &kernel = Kernel();
        auto &context = *kernel.context;
        auto *owner = ResolveBoundModule(context, Context::GetCurrentModule());
        if (!owner) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel.hooks->Unregister(owner->id, target_address);
    }

    BML_Result BML_API_HookEnumerate(BML_HookEnumCallback callback, void *user_data) {
        auto &hooks = *Kernel().hooks;
        return hooks.Enumerate(callback, user_data);
    }

    void RegisterHookApis() {
        BML_BEGIN_API_REGISTRATION();

        BML_REGISTER_API_GUARDED(bmlHookRegister, "hook", BML_API_HookRegister);
        BML_REGISTER_API_GUARDED(bmlHookRegisterOwned, "hook", BML_API_HookRegisterOwned);
        BML_REGISTER_API_GUARDED(bmlHookUnregister, "hook", BML_API_HookUnregister);
        BML_REGISTER_API_GUARDED(bmlHookUnregisterOwned, "hook", BML_API_HookUnregisterOwned);
        BML_REGISTER_API_GUARDED(bmlHookEnumerate, "hook", BML_API_HookEnumerate);
    }
} // namespace BML::Core
