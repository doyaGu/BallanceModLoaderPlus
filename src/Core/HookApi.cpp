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

    BML_Result BML_API_HookRegister(BML_Mod owner, const BML_HookDesc *desc) {
        auto *kernel = Context::KernelFromMod(owner);
        auto *context = Context::ContextFromMod(owner);
        if (!kernel || !context) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        auto *resolvedOwner = ResolveBoundModule(*context, owner);
        if (!resolvedOwner) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel->hooks->Register(resolvedOwner->id, desc);
    }

    BML_Result BML_API_HookUnregister(BML_Mod owner, void *target_address) {
        auto *kernel = Context::KernelFromMod(owner);
        auto *context = Context::ContextFromMod(owner);
        if (!kernel || !context) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        auto *resolvedOwner = ResolveBoundModule(*context, owner);
        if (!resolvedOwner) {
            return BML_RESULT_INVALID_CONTEXT;
        }
        return kernel->hooks->Unregister(resolvedOwner->id, target_address);
    }

    void RegisterHookApis(ApiRegistry &apiRegistry) {
        BML_BEGIN_API_REGISTRATION(apiRegistry);

        BML_REGISTER_API_GUARDED(bmlHookRegister, "hook", BML_API_HookRegister);
        BML_REGISTER_API_GUARDED(bmlHookUnregister, "hook", BML_API_HookUnregister);
    }
} // namespace BML::Core
