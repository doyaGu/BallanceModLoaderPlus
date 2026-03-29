#include "LeaseManager.h"

#include "KernelServices.h"

#include <atomic>
#include <memory>
#include <new>
#include <sstream>

#include "CoreErrors.h"

struct BML_InterfaceLease_T {
    std::atomic<uint32_t> ref_count{1};
    uint64_t id{0};
    BML::Core::KernelServices *kernel{nullptr};
    std::string interface_id;
    std::string provider_id;
    std::string consumer_id;
};

struct BML_InterfaceRegistration_T {
    uint64_t id{0};
    BML::Core::KernelServices *kernel{nullptr};
};

namespace BML::Core {
    namespace {
        std::mutex g_HandleKernelRegistryMutex;
        std::unordered_map<void *, KernelServices *> g_HandleKernelRegistry;

        void RegisterHandleKernel(void *handle, KernelServices *kernel) {
            if (!handle || !kernel) {
                return;
            }

            std::lock_guard<std::mutex> lock(g_HandleKernelRegistryMutex);
            g_HandleKernelRegistry[handle] = kernel;
        }

        void UnregisterHandleKernel(void *handle) {
            if (!handle) {
                return;
            }

            std::lock_guard<std::mutex> lock(g_HandleKernelRegistryMutex);
            g_HandleKernelRegistry.erase(handle);
        }

        KernelServices *LookupHandleKernel(void *handle) noexcept {
            if (!handle) {
                return nullptr;
            }

            std::lock_guard<std::mutex> lock(g_HandleKernelRegistryMutex);
            auto it = g_HandleKernelRegistry.find(handle);
            return it != g_HandleKernelRegistry.end() ? it->second : nullptr;
        }

        template <typename HandleT, typename MapT, typename PredicateT>
        void DestroyMatchingHandles(MapT &records,
                                    size_t &outstanding_count,
                                    PredicateT &&predicate) {
            for (auto it = records.begin(); it != records.end();) {
                if (!predicate(it->second)) {
                    ++it;
                    continue;
                }
                auto *handle = static_cast<HandleT *>(it->first);
                UnregisterHandleKernel(handle);
                it = records.erase(it);
                delete handle;
                if (outstanding_count > 0) {
                    --outstanding_count;
                }
            }
        }

        template <typename HandleT, typename MapT>
        void DestroyAllHandles(MapT &records, size_t &outstanding_count) {
            for (auto &[handle, record] : records) {
                (void) record;
                UnregisterHandleKernel(handle);
                delete static_cast<HandleT *>(handle);
            }
            records.clear();
            outstanding_count = 0;
        }
    } // namespace

    BML_Result LeaseManager::CreateInterfaceLease(KernelServices *kernel,
                                                  const std::string &interface_id,
                                                  const std::string &provider_id,
                                                  const std::string &consumer_id,
                                                  BML_InterfaceLease *out_lease) {
        if (!kernel || !out_lease || interface_id.empty() || provider_id.empty() || consumer_id.empty()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *lease = new (std::nothrow) BML_InterfaceLease_T();
        if (!lease) {
            return BML_RESULT_OUT_OF_MEMORY;
        }

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            lease->id = m_NextId++;
            lease->kernel = kernel;
            lease->interface_id = interface_id;
            lease->provider_id = provider_id;
            lease->consumer_id = consumer_id;
            m_InterfaceLeases.emplace(lease->id, lease);
            RegisterHandleKernel(lease, kernel);
            ++m_OutstandingLeaseHandles;
        }

        *out_lease = lease;
        return BML_RESULT_OK;
    }

    void LeaseManager::AddRefInterfaceLease(BML_InterfaceLease lease) {
        if (!lease) {
            return;
        }
        lease->ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    BML_Result LeaseManager::ReleaseInterfaceLease(BML_InterfaceLease lease) {
        if (!lease) {
            return BML_RESULT_INVALID_HANDLE;
        }

        // Decrement ref_count. If not reaching zero, no registry work needed.
        const uint32_t prev = lease->ref_count.fetch_sub(1, std::memory_order_acq_rel);
        if (prev > 1) {
            return BML_RESULT_OK;
        }

        // ref_count reached zero -- destroy the lease
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            auto it = m_InterfaceLeases.find(lease->id);
            if (it != m_InterfaceLeases.end() && it->second == lease) {
                // Still tracked — remove from map and kernel registry
                UnregisterHandleKernel(lease);
                m_InterfaceLeases.erase(it);
                if (m_OutstandingLeaseHandles > 0) {
                    --m_OutstandingLeaseHandles;
                }
            }
            // If not in map, Cleanup already untracked it — just delete
        }

        delete lease;
        return BML_RESULT_OK;
    }

    BML_Result LeaseManager::CreateInterfaceRegistration(KernelServices *kernel,
                                                         const std::string &interface_id,
                                                         const std::string &provider_id,
                                                         const std::string &consumer_id,
                                                         BML_InterfaceRegistration *out_registration) {
        if (!kernel || !out_registration || interface_id.empty() || provider_id.empty() || consumer_id.empty()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *registration = new (std::nothrow) BML_InterfaceRegistration_T();
        if (!registration) {
            return BML_RESULT_OUT_OF_MEMORY;
        }

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            registration->id = m_NextId++;
            registration->kernel = kernel;
            m_InterfaceRegistrations.emplace(registration, InterfaceRegistrationRecord{
                registration->id, interface_id, provider_id, consumer_id
            });
            RegisterHandleKernel(registration, kernel);
            ++m_OutstandingRegistrationHandles;
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
            UnregisterHandleKernel(registration);
            m_InterfaceRegistrations.erase(it);
            if (m_OutstandingRegistrationHandles > 0) {
                --m_OutstandingRegistrationHandles;
            }
        }

        delete registration;
        return BML_RESULT_OK;
    }

    KernelServices *LeaseManager::KernelFromLease(BML_InterfaceLease lease) noexcept {
        return LookupHandleKernel(lease);
    }

    KernelServices *LeaseManager::KernelFromRegistration(BML_InterfaceRegistration registration) noexcept {
        return LookupHandleKernel(registration);
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
        for (const auto &[id, lease] : m_InterfaceLeases) {
            (void) id;
            if (lease->consumer_id == consumer_id && lease->interface_id == interface_id) {
                return true;
            }
        }
        return false;
    }

    bool LeaseManager::HasInboundDependencies(const std::string &provider_id, std::string *out_message) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (const auto &[id, lease] : m_InterfaceLeases) {
            (void) id;
            if (lease->provider_id == provider_id) {
                if (out_message) {
                    std::ostringstream oss;
                    oss << "interface lease " << lease->id << " on '" << lease->interface_id
                        << "' held by '" << lease->consumer_id << "'";
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
        for (const auto &[id, lease] : m_InterfaceLeases) {
            (void) id;
            if (lease->consumer_id == consumer_id) {
                if (out_message) {
                    std::ostringstream oss;
                    oss << "outbound interface lease " << lease->id << " on '" << lease->interface_id
                        << "' provided by '" << lease->provider_id << "'";
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
            if (it->second->provider_id != provider_id) {
                ++it;
                continue;
            }
            // Untrack from map and kernel registry, but DON'T delete.
            // Outstanding InterfaceLease<T> copies may still hold this pointer.
            // The ref_count system will delete when the last copy releases.
            UnregisterHandleKernel(it->second);
            it->second->kernel = nullptr;
            it = m_InterfaceLeases.erase(it);
            if (m_OutstandingLeaseHandles > 0) {
                --m_OutstandingLeaseHandles;
            }
        }
        DestroyMatchingHandles<BML_InterfaceRegistration_T>(
            m_InterfaceRegistrations,
            m_OutstandingRegistrationHandles,
            [&](const InterfaceRegistrationRecord &record) { return record.provider_id == provider_id; });

        m_BlockedProviders.erase(provider_id);
    }

    void LeaseManager::CleanupConsumer(const std::string &consumer_id) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto it = m_InterfaceLeases.begin(); it != m_InterfaceLeases.end();) {
            if (it->second->consumer_id != consumer_id) {
                ++it;
                continue;
            }
            UnregisterHandleKernel(it->second);
            it->second->kernel = nullptr;
            it = m_InterfaceLeases.erase(it);
            if (m_OutstandingLeaseHandles > 0) {
                --m_OutstandingLeaseHandles;
            }
        }
        DestroyMatchingHandles<BML_InterfaceRegistration_T>(
            m_InterfaceRegistrations,
            m_OutstandingRegistrationHandles,
            [&](const InterfaceRegistrationRecord &record) { return record.consumer_id == consumer_id; });
    }

    uint32_t LeaseManager::GetLeaseCountForInterface(const std::string &interface_id) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        uint32_t count = 0;
        for (const auto &[id, lease] : m_InterfaceLeases) {
            (void) id;
            if (lease->interface_id == interface_id) {
                ++count;
            }
        }
        return count;
    }

    void LeaseManager::Reset() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto &[id, lease] : m_InterfaceLeases) {
            (void) id;
            UnregisterHandleKernel(lease);
            lease->kernel = nullptr;
        }
        m_InterfaceLeases.clear();
        m_OutstandingLeaseHandles = 0;
        DestroyAllHandles<BML_InterfaceRegistration_T>(
            m_InterfaceRegistrations, m_OutstandingRegistrationHandles);
        m_BlockedProviders.clear();
        m_NextId = 1;
    }

    size_t LeaseManager::GetOutstandingLeaseHandlesForTest() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_OutstandingLeaseHandles;
    }

    size_t LeaseManager::GetOutstandingRegistrationHandlesForTest() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_OutstandingRegistrationHandles;
    }
} // namespace BML::Core
