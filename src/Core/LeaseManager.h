#ifndef BML_CORE_LEASE_MANAGER_H
#define BML_CORE_LEASE_MANAGER_H

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "bml_interface.h"

namespace BML::Core {
    class LeaseManager {
    public:
        static LeaseManager &Instance();

        BML_Result CreateInterfaceLease(const std::string &interface_id,
                                        const std::string &provider_id,
                                        const std::string &consumer_id,
                                        BML_InterfaceLease *out_lease);
        BML_Result ReleaseInterfaceLease(BML_InterfaceLease lease);

        BML_Result CreateInterfaceRegistration(const std::string &interface_id,
                                               const std::string &provider_id,
                                               const std::string &consumer_id,
                                               BML_InterfaceRegistration *out_registration);
        BML_Result ReleaseInterfaceRegistration(BML_InterfaceRegistration registration);

        void SetProviderBlocked(const std::string &provider_id, bool blocked);
        bool IsProviderBlocked(const std::string &provider_id) const;
        bool HasActiveInterfaceLease(const std::string &consumer_id,
                                     const std::string &interface_id) const;

        bool HasInboundDependencies(const std::string &provider_id, std::string *out_message) const;
        bool HasOutboundDependencies(const std::string &consumer_id, std::string *out_message) const;

        uint32_t GetLeaseCountForInterface(const std::string &interface_id) const;

        void CleanupProvider(const std::string &provider_id);
        void CleanupConsumer(const std::string &consumer_id);
        void Reset();

        LeaseManager() = default;

    private:
        struct InterfaceLeaseRecord {
            uint64_t id{0};
            std::string interface_id;
            std::string provider_id;
            std::string consumer_id;
        };

        struct InterfaceRegistrationRecord {
            uint64_t id{0};
            std::string interface_id;
            std::string provider_id;
            std::string consumer_id;
        };

        mutable std::mutex m_Mutex;
        uint64_t m_NextId{1};
        std::unordered_map<BML_InterfaceLease, InterfaceLeaseRecord> m_InterfaceLeases;
        std::unordered_map<BML_InterfaceRegistration, InterfaceRegistrationRecord> m_InterfaceRegistrations;
        std::unordered_set<std::string> m_BlockedProviders;
    };
} // namespace BML::Core

#endif /* BML_CORE_LEASE_MANAGER_H */
