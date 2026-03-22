#include "InterfaceRegistry.h"

#include <algorithm>
#include <vector>

#include "Context.h"
#include "LeaseManager.h"

namespace BML::Core {
    InterfaceRegistry::InterfaceRegistry(Context &context, LeaseManager &leases)
        : m_Context(context), m_Leases(leases) {}

    namespace {
        bool AbiVersionIsCompatible(const BML_Version &actual, const BML_Version *required) {
            if (!required) {
                return true;
            }

            if (actual.major != required->major) {
                return false;
            }
            if (actual.minor < required->minor) {
                return false;
            }
            return actual.patch >= required->patch || actual.minor > required->minor;
        }

        bool HasCapability(const BML_Mod_T *mod, const char *capability_id) {
            if (!mod || !capability_id) {
                return false;
            }
            return std::find(mod->capabilities.begin(), mod->capabilities.end(), capability_id) !=
                mod->capabilities.end();
        }
    } // namespace

    BML_InterfaceRuntimeDesc InterfaceRegistry::BuildRuntimeDesc(
        const InterfaceEntry &entry, uint32_t leaseCount) {
        BML_InterfaceRuntimeDesc desc = BML_INTERFACE_RUNTIME_DESC_INIT;
        desc.interface_id = entry.desc.interface_id;
        desc.provider_id = entry.provider_id.c_str();
        desc.abi_version = entry.desc.abi_version;
        desc.implementation_size = entry.desc.implementation_size;
        desc.flags = entry.desc.flags;
        desc.capabilities = entry.desc.capabilities;
        desc.description = entry.description.empty() ? nullptr : entry.description.c_str();
        desc.category = entry.category.empty() ? nullptr : entry.category.c_str();
        desc.superseded_by = entry.superseded_by.empty() ? nullptr : entry.superseded_by.c_str();
        desc.metadata = entry.metadata_view.empty() ? nullptr : entry.metadata_view.data();
        desc.metadata_count = static_cast<uint32_t>(entry.metadata_view.size());
        desc.lease_count = leaseCount;
        return desc;
    }

    BML_Result InterfaceRegistry::Register(const BML_InterfaceDesc *desc, const BML_Mod_T *provider) {
        const std::string provider_id = provider ? provider->id : std::string{};
        if (!desc || desc->struct_size < sizeof(BML_InterfaceDesc) || !desc->interface_id ||
            !desc->implementation || desc->implementation_size == 0 || provider_id.empty()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        if ((desc->flags & BML_INTERFACE_FLAG_HOST_OWNED) != 0 && provider_id != "BML") {
            if (!provider || !m_Leases.HasActiveInterfaceLease(
                    provider->id, BML_CORE_HOST_RUNTIME_INTERFACE_ID)) {
                return BML_RESULT_PERMISSION_DENIED;
            }
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        const std::string key(desc->interface_id);
        if (m_Interfaces.find(key) != m_Interfaces.end()) {
            return BML_RESULT_ALREADY_EXISTS;
        }

        InterfaceEntry entry;
        entry.desc = *desc;
        entry.provider_id = provider_id;
        if (desc->description) {
            entry.description = desc->description;
        }
        if (desc->category) {
            entry.category = desc->category;
        }
        if (desc->superseded_by) {
            entry.superseded_by = desc->superseded_by;
        }
        for (uint32_t i = 0; i < desc->metadata_count && desc->metadata; ++i) {
            if (desc->metadata[i].key && desc->metadata[i].value) {
                entry.metadata.emplace_back(desc->metadata[i].key, desc->metadata[i].value);
            }
        }
        entry.RefreshStoredViews();
        m_Interfaces.emplace(key, std::move(entry));
        return BML_RESULT_OK;
    }

    BML_Result InterfaceRegistry::Acquire(const char *interface_id,
                                          const BML_Version *required_abi,
                                          const BML_Mod_T *consumer,
                                          const void **out_implementation,
                                          BML_InterfaceLease *out_lease) {
        if (!interface_id || !out_implementation || !out_lease) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (!consumer) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        *out_implementation = nullptr;
        *out_lease = nullptr;

        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Interfaces.find(interface_id);
        if (it == m_Interfaces.end()) {
            return BML_RESULT_NOT_FOUND;
        }
        if (!AbiVersionIsCompatible(it->second.desc.abi_version, required_abi)) {
            return BML_RESULT_VERSION_MISMATCH;
        }
        if (m_Leases.IsProviderBlocked(it->second.provider_id)) {
            return BML_RESULT_BUSY;
        }
        if ((it->second.desc.flags & BML_INTERFACE_FLAG_INTERNAL) != 0 && consumer->id != "BML" &&
            !HasCapability(consumer, "bml.internal.runtime")) {
            return BML_RESULT_PERMISSION_DENIED;
        }

        BML_CHECK(m_Leases.CreateInterfaceLease(
            interface_id, it->second.provider_id, consumer->id, out_lease));
        *out_implementation = it->second.desc.implementation;
        return BML_RESULT_OK;
    }

    BML_Result InterfaceRegistry::Release(BML_InterfaceLease lease) {
        return m_Leases.ReleaseInterfaceLease(lease);
    }

    BML_Result InterfaceRegistry::GetDescriptor(const char *interface_id,
                                                BML_InterfaceRuntimeDesc *out_desc) const {
        if (!interface_id || !out_desc) {
            return BML_RESULT_INVALID_ARGUMENT;
        }
        if (out_desc->struct_size != 0 && out_desc->struct_size < sizeof(BML_InterfaceRuntimeDesc)) {
            return BML_RESULT_INVALID_SIZE;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Interfaces.find(interface_id);
        if (it == m_Interfaces.end()) {
            return BML_RESULT_NOT_FOUND;
        }

        uint32_t leases = m_Leases.GetLeaseCountForInterface(interface_id);
        *out_desc = BuildRuntimeDesc(it->second, leases);
        return BML_RESULT_OK;
    }

    void InterfaceRegistry::Enumerate(PFN_BML_InterfaceRuntimeEnumerator callback,
                                      void *user_data,
                                      uint64_t required_flags_mask) const {
        if (!callback) {
            return;
        }

        std::vector<InterfaceEntry> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            snapshot.reserve(m_Interfaces.size());
            for (const auto &[id, entry] : m_Interfaces) {
                (void) id;
                if ((entry.desc.flags & required_flags_mask) != required_flags_mask) {
                    continue;
                }
                snapshot.push_back(entry);
                snapshot.back().RefreshStoredViews();
            }
        }

        for (const auto &entry : snapshot) {
            BML_InterfaceRuntimeDesc desc = BuildRuntimeDesc(entry);
            callback(&desc, user_data);
        }
    }

    BML_Result InterfaceRegistry::Unregister(const char *interface_id, const BML_Mod_T *provider) {
        const std::string provider_id = provider ? provider->id : std::string{};
        if (!interface_id || provider_id.empty()) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Interfaces.find(interface_id);
        if (it == m_Interfaces.end()) {
            return BML_RESULT_NOT_FOUND;
        }
        if (it->second.provider_id != provider_id) {
            return BML_RESULT_PERMISSION_DENIED;
        }

        std::string message;
        if (m_Leases.HasInboundDependencies(provider_id, &message)) {
            return BML_RESULT_BUSY;
        }
        m_Interfaces.erase(it);
        return BML_RESULT_OK;
    }

    void InterfaceRegistry::UnregisterByProvider(const std::string &provider_id) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto it = m_Interfaces.begin(); it != m_Interfaces.end();) {
            if (it->second.provider_id == provider_id) {
                it = m_Interfaces.erase(it);
            } else {
                ++it;
            }
        }
    }

    void InterfaceRegistry::Clear() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Interfaces.clear();
    }

    bool InterfaceRegistry::Exists(const char *interface_id) const {
        if (!interface_id) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Interfaces.find(interface_id) != m_Interfaces.end();
    }

    bool InterfaceRegistry::IsCompatible(const char *interface_id,
                                         const BML_Version *required) const {
        if (!interface_id) {
            return false;
        }
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Interfaces.find(interface_id);
        if (it == m_Interfaces.end()) {
            return false;
        }
        return AbiVersionIsCompatible(it->second.desc.abi_version, required);
    }

    void InterfaceRegistry::EnumerateByProvider(const char *provider_id,
                                                PFN_BML_InterfaceRuntimeEnumerator callback,
                                                void *user_data) const {
        if (!callback || !provider_id) {
            return;
        }

        std::vector<InterfaceEntry> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            snapshot.reserve(m_Interfaces.size());
            for (const auto &[id, entry] : m_Interfaces) {
                (void) id;
                if (entry.provider_id == provider_id) {
                    snapshot.push_back(entry);
                    snapshot.back().RefreshStoredViews();
                }
            }
        }

        for (const auto &entry : snapshot) {
            BML_InterfaceRuntimeDesc desc = BuildRuntimeDesc(entry);
            callback(&desc, user_data);
        }
    }

    void InterfaceRegistry::EnumerateByCapability(uint64_t required_caps,
                                                  PFN_BML_InterfaceRuntimeEnumerator callback,
                                                  void *user_data) const {
        if (!callback) {
            return;
        }

        std::vector<InterfaceEntry> snapshot;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            snapshot.reserve(m_Interfaces.size());
            for (const auto &[id, entry] : m_Interfaces) {
                (void) id;
                if ((entry.desc.capabilities & required_caps) == required_caps) {
                    snapshot.push_back(entry);
                    snapshot.back().RefreshStoredViews();
                }
            }
        }

        for (const auto &entry : snapshot) {
            BML_InterfaceRuntimeDesc desc = BuildRuntimeDesc(entry);
            callback(&desc, user_data);
        }
    }

    uint32_t InterfaceRegistry::GetCount() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return static_cast<uint32_t>(m_Interfaces.size());
    }

    uint32_t InterfaceRegistry::GetLeaseCount(const char *interface_id) const {
        if (!interface_id) {
            return 0;
        }
        return m_Leases.GetLeaseCountForInterface(interface_id);
    }
} // namespace BML::Core
