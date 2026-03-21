#ifndef BML_CORE_HOOK_REGISTRY_H
#define BML_CORE_HOOK_REGISTRY_H

#include "bml_hook.h"

#include <memory>
#include <string>

namespace BML::Core {
    class HookRegistry {
    public:
        HookRegistry();
        ~HookRegistry();

        HookRegistry(const HookRegistry &) = delete;
        HookRegistry &operator=(const HookRegistry &) = delete;

        BML_Result Register(const std::string &owner_module_id,
                            const BML_HookDesc *desc);
        BML_Result Unregister(const std::string &owner_module_id,
                              void *target_address);
        BML_Result Enumerate(BML_HookEnumCallback callback, void *user_data);

        void CleanupOwner(const std::string &owner_module_id);
        void Shutdown();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };

    void RegisterHookApis();
} // namespace BML::Core

#endif /* BML_CORE_HOOK_REGISTRY_H */
