#ifndef BML_CORE_INTERFACE_REGISTRY_H
#define BML_CORE_INTERFACE_REGISTRY_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bml_builtin_interfaces.h"
#include "bml_interface.h"

namespace BML::Core {
    class InterfaceRegistry {
    public:
        BML_Result Register(const BML_InterfaceDesc *desc, const BML_Mod_T *provider);
        BML_Result Acquire(const char *interface_id,
                           const BML_Version *required_abi,
                           const BML_Mod_T *consumer,
                           const void **out_implementation,
                           BML_InterfaceLease *out_lease);
        BML_Result Release(BML_InterfaceLease lease);
        BML_Result GetDescriptor(const char *interface_id, BML_InterfaceRuntimeDesc *out_desc) const;
        void Enumerate(PFN_BML_InterfaceRuntimeEnumerator callback,
                       void *user_data,
                       uint64_t required_flags_mask) const;
        BML_Result Unregister(const char *interface_id, const BML_Mod_T *provider);
        void UnregisterByProvider(const std::string &provider_id);
        void Clear();

        bool Exists(const char *interface_id) const;
        bool IsCompatible(const char *interface_id, const BML_Version *required) const;
        void EnumerateByProvider(const char *provider_id,
                                 PFN_BML_InterfaceRuntimeEnumerator callback,
                                 void *user_data) const;
        void EnumerateByCapability(uint64_t required_caps,
                                   PFN_BML_InterfaceRuntimeEnumerator callback,
                                   void *user_data) const;
        uint32_t GetCount() const;
        uint32_t GetLeaseCount(const char *interface_id) const;

        explicit InterfaceRegistry(class Context &context, class LeaseManager &leases);

    private:
        class Context &m_Context;
        class LeaseManager &m_Leases;
        struct InterfaceEntry {
            BML_InterfaceDesc desc{};
            std::string provider_id;
            std::string description;
            std::string category;
            std::string superseded_by;
            std::vector<std::pair<std::string, std::string>> metadata;
            std::vector<BML_InterfaceMetadata> metadata_view;

            void RefreshStoredViews() {
                metadata_view.clear();
                metadata_view.reserve(metadata.size());
                for (const auto &[key, value] : metadata) {
                    metadata_view.push_back({key.c_str(), value.c_str()});
                }

                desc.description = description.empty() ? nullptr : description.c_str();
                desc.category = category.empty() ? nullptr : category.c_str();
                desc.superseded_by = superseded_by.empty() ? nullptr : superseded_by.c_str();
                desc.metadata = metadata_view.empty() ? nullptr : metadata_view.data();
                desc.metadata_count = static_cast<uint32_t>(metadata_view.size());
            }
        };

        static BML_InterfaceRuntimeDesc BuildRuntimeDesc(const InterfaceEntry &entry, uint32_t leaseCount = 0);

        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, InterfaceEntry> m_Interfaces;
    };
} // namespace BML::Core

#endif /* BML_CORE_INTERFACE_REGISTRY_H */
