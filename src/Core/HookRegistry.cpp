#include "HookRegistry.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Logging.h"

namespace BML::Core {
    namespace {
        constexpr char kLogCategory[] = "hook.registry";

        struct HookEntry {
            std::string owner_module_id;
            std::string target_name;
            void *target_address = nullptr;
            int32_t priority = 0;
            uint32_t flags = 0;
        };

        struct HookRegistryImpl {
            std::mutex mutex;
            // address -> list of entries (multiple modules can hook same address)
            std::unordered_map<uintptr_t, std::vector<HookEntry>> entries;
        };

        HookRegistryImpl &Impl() {
            static HookRegistryImpl impl;
            return impl;
        }
    } // namespace

    HookRegistry::HookRegistry() = default;

    HookRegistry &HookRegistry::Instance() {
        static HookRegistry instance;
        return instance;
    }

    BML_Result HookRegistry::Register(const std::string &owner_module_id,
                                      const BML_HookDesc *desc) {
        if (!desc || desc->struct_size < sizeof(BML_HookDesc))
            return BML_RESULT_INVALID_ARGUMENT;
        if (!desc->target_address)
            return BML_RESULT_INVALID_ARGUMENT;

        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);

        auto key = reinterpret_cast<uintptr_t>(desc->target_address);
        auto &vec = impl.entries[key];

        // Check for duplicate from same module
        for (auto &existing : vec) {
            if (existing.owner_module_id == owner_module_id) {
                // Update in-place
                existing.target_name = desc->target_name ? desc->target_name : "";
                existing.priority = desc->priority;
                existing.flags = desc->flags;
                return BML_RESULT_OK;
            }
        }

        // Check for conflict (different module, same address)
        bool conflict = !vec.empty();
        if (conflict) {
            for (auto &existing : vec) {
                existing.flags |= BML_HOOK_FLAG_CONFLICT;
                CoreLog(BML_LOG_WARN, kLogCategory,
                        "Hook conflict at %p ('%s'): '%s' vs '%s'",
                        desc->target_address,
                        desc->target_name ? desc->target_name : "?",
                        existing.owner_module_id.c_str(),
                        owner_module_id.c_str());
            }
        }

        HookEntry entry;
        entry.owner_module_id = owner_module_id;
        entry.target_name = desc->target_name ? desc->target_name : "";
        entry.target_address = desc->target_address;
        entry.priority = desc->priority;
        entry.flags = desc->flags | (conflict ? BML_HOOK_FLAG_CONFLICT : 0u);
        vec.push_back(std::move(entry));

        CoreLog(BML_LOG_DEBUG, kLogCategory,
                "Hook registered: '%s' at %p by '%s' (priority %d)%s",
                desc->target_name ? desc->target_name : "?",
                desc->target_address,
                owner_module_id.c_str(),
                desc->priority,
                conflict ? " [CONFLICT]" : "");

        return BML_RESULT_OK;
    }

    BML_Result HookRegistry::Unregister(const std::string &owner_module_id,
                                        void *target_address) {
        if (!target_address)
            return BML_RESULT_INVALID_ARGUMENT;

        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);

        auto key = reinterpret_cast<uintptr_t>(target_address);
        auto it = impl.entries.find(key);
        if (it == impl.entries.end())
            return BML_RESULT_NOT_FOUND;

        auto &vec = it->second;
        auto entry_it = std::find_if(vec.begin(), vec.end(),
            [&](const HookEntry &e) { return e.owner_module_id == owner_module_id; });

        if (entry_it == vec.end())
            return BML_RESULT_NOT_FOUND;

        CoreLog(BML_LOG_DEBUG, kLogCategory,
                "Hook unregistered: '%s' at %p by '%s'",
                entry_it->target_name.c_str(),
                target_address,
                owner_module_id.c_str());

        vec.erase(entry_it);

        // If only one entry remains, clear the conflict flag
        if (vec.size() == 1) {
            vec[0].flags &= ~BML_HOOK_FLAG_CONFLICT;
        }

        if (vec.empty()) {
            impl.entries.erase(it);
        }

        return BML_RESULT_OK;
    }

    BML_Result HookRegistry::Enumerate(BML_HookEnumCallback callback, void *user_data) {
        if (!callback)
            return BML_RESULT_INVALID_ARGUMENT;

        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);

        for (const auto &[addr, vec] : impl.entries) {
            for (const auto &entry : vec) {
                BML_HookDesc desc{};
                desc.struct_size = sizeof(BML_HookDesc);
                desc.target_name = entry.target_name.c_str();
                desc.target_address = entry.target_address;
                desc.priority = entry.priority;
                desc.flags = entry.flags;
                callback(&desc, entry.owner_module_id.c_str(), user_data);
            }
        }

        return BML_RESULT_OK;
    }

    void HookRegistry::CleanupOwner(const std::string &owner_module_id) {
        if (owner_module_id.empty())
            return;

        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);

        for (auto it = impl.entries.begin(); it != impl.entries.end(); ) {
            auto &vec = it->second;
            vec.erase(
                std::remove_if(vec.begin(), vec.end(),
                    [&](const HookEntry &e) { return e.owner_module_id == owner_module_id; }),
                vec.end());

            // Clear conflict flag if only one remains
            if (vec.size() == 1) {
                vec[0].flags &= ~BML_HOOK_FLAG_CONFLICT;
            }

            if (vec.empty()) {
                it = impl.entries.erase(it);
            } else {
                ++it;
            }
        }
    }

    void HookRegistry::Shutdown() {
        auto &impl = Impl();
        std::lock_guard lock(impl.mutex);
        impl.entries.clear();
    }
} // namespace BML::Core
