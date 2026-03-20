#ifndef BML_CORE_HOOK_REGISTRY_H
#define BML_CORE_HOOK_REGISTRY_H

#include "bml_hook.h"

#include <string>

namespace BML::Core {
    class HookRegistry {
    public:
        static HookRegistry &Instance();

        BML_Result Register(const std::string &owner_module_id,
                            const BML_HookDesc *desc);
        BML_Result Unregister(const std::string &owner_module_id,
                              void *target_address);
        BML_Result Enumerate(BML_HookEnumCallback callback, void *user_data);

        void CleanupOwner(const std::string &owner_module_id);
        void Shutdown();

        HookRegistry();
    };

    void RegisterHookApis();
} // namespace BML::Core

#endif /* BML_CORE_HOOK_REGISTRY_H */
