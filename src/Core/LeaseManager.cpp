#include "LeaseManager.h"

#include "KernelServices.h"

#include <memory>
#include <new>
#include <sstream>

#include "CoreErrors.h"

struct BML_InterfaceLease_T {
    uint64_t id{0};
};

struct BML_InterfaceRegistration_T {
    uint64_t id{0};
};

namespace BML::Core {
    BML_Result LeaseManager::CreateInterfaceLease(const std::string &interface_id,
                                                  const std::string &provider_id,
                                                  const std::string &consumer_id,
                                                  BML_InterfaceLease *out_lease) {
        if (!out_lease || interface_id.empty() || provider_id.empty() || consumer_id.empty()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *lease = new (std::nothrow) BML_InterfaceLease_T();
        if (!lease) {
            return BML_RESULT_OUT_OF_MEMORY;
        }

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            lease->id = m_NextId++;
            m_InterfaceLeases.emplace(lease, InterfaceLeaseRecord{
                lease->id, interface_id, provider_id, consumer_id
            });
        }

        *out_lease = lease;
        return BML_RESULT_OK;
    }

    BML_Result LeaseManager::ReleaseInterfaceLease(BML_InterfaceLease lease) {
        if (!lease) {
            return BML_RESULT_INVALID_HANDLE;
        }

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_InterfaceLeases.find(lease);
            if (it == m_InterfaceLeases.end()) {
                return BML_RESULT_INVALID_HANDLE;
            }
            m_InterfaceLeases.erase(it);
        }

        delete lease;
        return BML_RESULT_OK;
    }

    BML_Result LeaseManager::CreateInterfaceRegistration(const std::string &interface_id,
                                                         const std::string &provider_id,
                                                         const std::string &consumer_id,
                                                         BML_InterfaceRegistration *out_registration) {
        if (!out_registration || interface_id.empty() || provider_id.empty() || consumer_id.empty()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *registration = new (std::nothrow) BML_InterfaceRegistration_T();
        if (!registration) {
            return BML_RESULT_OUT_OF_MEMORY;
        }

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            registration->id = m_NextId++;
            m_InterfaceRegistrations.emplace(registration, InterfaceRegistrationRecord{
                registration->id, interface_id, provider_id, consumer_id
            });
        }

        *out_registration = registration;
        return BML_RESULT_OK;
    }

    BML_Result LeaseManager::ReleaseInterfaceRegistration(BML_InterfaceRegistration registration) {
        if (!registration) {
            return BML_RESULT_INVALID_HANDLE;
        }

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_InterfaceRegistrations.find(registration);
            if (it == m_InterfaceRegistrations.end()) {
                return BML_RESULT_INVALID_HANDLE;
            }
            m_InterfaceRegistrations.erase(it);
        }

        delete registration;
        return BML_RESULT_OK;
    }

    void LeaseManager::SetProviderBlocked(const std::string &provider_id, bool blocked) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (blocked) {
            m_BlockedProviders.insert(provider_id);
        } else {
            m_BlockedProviders.erase(provider_id);
        }
    }

    bool LeaseManager::IsProviderBlocked(const std::string &provider_id) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_BlockedProviders.find(provider_id) != m_BlockedProviders.end();
    }

    bool LeaseManager::HasActiveInterfaceLease(const std::string &consumer_id,
                                               const std::string &interface_id) const {
        if (consumer_id.empty() || interface_id.empty()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const auto &[handle, record] : m_InterfaceLeases) {
            (void) handle;
            if (record.consumer_id == consumer_id && record.interface_id == interface_id) {
                return true;
            }
        }
        return false;
    }

    bool LeaseManager::HasInboundDependencies(const std::string &provider_id, std::string *out_message) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const auto &[handle, record] : m_InterfaceLeases) {
            (void) handle;
            if (record.provider_id == provider_id) {
                if (out_message) {
                    std::ostringstream oss;
                    oss << "interface lease " << record.id << " on '" << record.interface_id
                        << "' held by '" << record.consumer_id << "'";
                    *out_message = oss.str();
                }
                return true;
            }
        }

        for (const auto &[handle, record] : m_InterfaceRegistrations) {
            (void) handle;
            if (record.provider_id == provider_id) {
                if (out_message) {
                    std::ostringstream oss;
                    oss << "registration " << record.id << " on '" << record.interface_id
                        << "' owned by '" << record.consumer_id << "'";
                    *out_message = oss.str();
                }
                return true;
            }
        }

        return false;
    }

    bool LeaseManager::HasOutboundDependencies(const std::string &consumer_id, std::string *out_message) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const auto &[handle, record] : m_InterfaceLeases) {
            (void) handle;
            if (record.consumer_id == consumer_id) {
                if (out_message) {
                    std::ostringstream oss;
                    oss << "outbound interface lease " << record.id << " on '" << record.interface_id
                        << "' provided by '" << record.provider_id << "'";
                    *out_message = oss.str();
                }
                return true;
            }
        }

        for (const auto &[handle, record] : m_InterfaceRegistrations) {
            (void) handle;
            if (record.consumer_id == consumer_id) {
                if (out_message) {
                    std::ostringstream oss;
                    oss << "outbound registration " << record.id << " on '" << record.interface_id
                        << "' hosted by '" << record.provider_id << "'";
                    *out_message = oss.str();
                }
                return true;
            }
        }

        return false;
    }

    void LeaseManager::CleanupProvider(const std::string &provider_id) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto it = m_InterfaceLeases.begin(); it != m_InterfaceLeases.end();) {
            if (it->second.provider_id == provider_id) {
                it = m_InterfaceLeases.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = m_InterfaceRegistrations.begin(); it != m_InterfaceRegistrations.end();) {
            if (it->second.provider_id == provider_id) {
                it = m_InterfaceRegistrations.erase(it);
            } else {
                ++it;
            }
        }

        m_BlockedProviders.erase(provider_id);
    }

    void LeaseManager::CleanupConsumer(const std::string &consumer_id) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto it = m_InterfaceLeases.begin(); it != m_InterfaceLeases.end();) {
            if (it->second.consumer_id == consumer_id) {
                it = m_InterfaceLeases.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = m_InterfaceRegistrations.begin(); it != m_InterfaceRegistrations.end();) {
            if (it->second.consumer_id == consumer_id) {
                it = m_InterfaceRegistrations.erase(it);
            } else {
                ++it;
            }
        }
    }

    uint32_t LeaseManager::GetLeaseCountForInterface(const std::string &interface_id) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        uint32_t count = 0;
        for (const auto &[handle, record] : m_InterfaceLeases) {
            (void) handle;
            if (record.interface_id == interface_id) {
                ++count;
            }
        }
        return count;
    }

    void LeaseManager::Reset() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_InterfaceLeases.clear();
        m_InterfaceRegistrations.clear();
        m_BlockedProviders.clear();
        m_NextId = 1;
    }
} // namespace BML::Core
