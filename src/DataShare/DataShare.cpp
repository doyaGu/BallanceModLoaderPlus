#include "DataShare/DataShare.h"

#include <atomic>
#include <climits>
#include <cstring>
#include <new>
#include <optional>
#include <string_view>
#include <utility>

#include <intrin.h>

#include "Loader/ModInvocationGate.h"

namespace BML {
    namespace {
        class OwnerCallbackScope {
        public:
            explicit OwnerCallbackScope(const std::string &owner) noexcept
                : m_Owner(owner), m_Previous(s_Current) {
                s_Current = this;
            }

            ~OwnerCallbackScope() { s_Current = m_Previous; }

            static bool IsActive(std::string_view owner) noexcept {
                for (const OwnerCallbackScope *scope = s_Current;
                     scope; scope = scope->m_Previous) {
                    if (scope->m_Owner == owner)
                        return true;
                }
                return false;
            }

        private:
            std::string_view m_Owner;
            const OwnerCallbackScope *m_Previous;
            static thread_local const OwnerCallbackScope *s_Current;
        };

        thread_local const OwnerCallbackScope *OwnerCallbackScope::s_Current = nullptr;

        void InvokeDataShareCallbackNoexcept(BML_DataShareCallback callback,
                                             const char *key,
                                             const void *data,
                                             size_t size,
                                             void *userdata) noexcept {
            if (!callback)
                return;
            try {
                callback(key, data, size, userdata);
            } catch (...) {
            }
        }

        void InvokeDataShareCleanupNoexcept(BML_DataShareCleanupCallback cleanup,
                                            const char *key,
                                            void *userdata) noexcept {
            if (!cleanup)
                return;
            try {
                cleanup(key, userdata);
            } catch (...) {
            }
        }
    }

    // ----------------------- Static registry ------------------------------------

    std::mutex DataShareStore::s_RegistryMutex;
    std::unordered_map<std::string, DataShareStore *> DataShareStore::s_Registry;
    std::recursive_mutex DataShareStore::s_CallbackMutex;
    std::unordered_map<std::string, bool> DataShareStore::s_OwnerStates;
    std::atomic<BML_DataShareRequest> DataShareStore::s_NextRequest{1};
    std::atomic<DataShareStore::OwnerResolver> DataShareStore::s_OwnerResolver{nullptr};
    std::atomic<ModInvocationGate *> DataShareStore::s_InvocationGate{nullptr};

    class DataShareStore::InvocationScope final {
    public:
        InvocationScope() {
            if (ModInvocationGate *gate =
                    s_InvocationGate.load(std::memory_order_acquire)) {
                m_Lock.emplace(gate->LockCall());
            }
        }

    private:
        std::optional<ModInvocationGate::CallLock> m_Lock;
    };

    // ----------------------- DataShare: lifecycle --------------------------------

    DataShareStore::DataShareStore(std::string name)
        : m_Ref(1), m_Name(std::move(name)) {}

    DataShareStore::~DataShareStore() {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Data.clear();
        }
        CancelPendingCallbacks();
        // Unregister self
        std::lock_guard<std::mutex> registryLock(s_RegistryMutex);
        const auto it = s_Registry.find(m_Name);
        if (it != s_Registry.end() && it->second == this)
            s_Registry.erase(it);
    }

    std::uint32_t DataShareStore::AddRef() const { return m_Ref.AddRef(); }

    std::uint32_t DataShareStore::Release() const {
        const std::uint32_t newCount = m_Ref.Release();
        if (newCount == 0) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
        return newCount;
    }

    // ----------------------- Registry ops ----------------------------------------

    DataShareStore *DataShareStore::GetInstance(const char *name) {
        DataShareStore *discarded = nullptr;
        try {
            std::string key(name && *name ? name : "BML");
            {
                std::lock_guard<std::mutex> registryLock(s_RegistryMutex);
                const auto existing = s_Registry.find(key);
                if (existing != s_Registry.end())
                    return existing->second;

                DataShareStore *created = new (std::nothrow) DataShareStore(key);
                if (!created)
                    return nullptr;
                try {
                    s_Registry.emplace(std::move(key), created);
                    return created;
                } catch (...) {
                    discarded = created;
                }
            }
        } catch (...) {
        }
        delete discarded;
        return nullptr;
    }

    DataShareStore *DataShareStore::AcquireInstance(const char *name) {
        DataShareStore *discarded = nullptr;
        try {
            std::string key(name && *name ? name : "BML");
            {
                std::lock_guard<std::mutex> registryLock(s_RegistryMutex);
                const auto existing = s_Registry.find(key);
                if (existing != s_Registry.end()) {
                    existing->second->AddRef();
                    return existing->second;
                }

                DataShareStore *created = new (std::nothrow) DataShareStore(key);
                if (!created)
                    return nullptr;
                try {
                    s_Registry.emplace(std::move(key), created);
                    created->AddRef();
                    return created;
                } catch (...) {
                    discarded = created;
                }
            }
        } catch (...) {
        }
        delete discarded;
        return nullptr;
    }

    bool DataShareStore::RetireCallbacksFromOwner(const std::string &owner) noexcept {
        if (owner.empty())
            return true;
        if (OwnerCallbackScope::IsActive(owner))
            return false;

        try {
            InvocationScope invocation;
            std::lock_guard<std::recursive_mutex> callbackLock(s_CallbackMutex);
            const auto state = s_OwnerStates.find(owner);
            if (state == s_OwnerStates.end())
                return false;

            state->second = false;
            std::list<Callback> pending;
            {
                std::lock_guard<std::mutex> registryLock(s_RegistryMutex);
                for (const auto &entry : s_Registry)
                    entry.second->TakePendingCallbacks(pending, &owner);
            }

            for (Callback &callback : pending) {
                OwnerCallbackScope scope(callback.owner);
                InvokeDataShareCleanupNoexcept(
                    callback.cleanup, callback.key.c_str(), callback.userdata);
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    bool DataShareStore::ActivateCallbacksFromOwner(const std::string &owner) noexcept {
        if (owner.empty())
            return false;
        try {
            std::lock_guard<std::recursive_mutex> callbackLock(s_CallbackMutex);
            s_OwnerStates.insert_or_assign(owner, true);
            return true;
        } catch (...) {
            return false;
        }
    }

    void DataShareStore::SetOwnerResolver(OwnerResolver resolver) noexcept {
        s_OwnerResolver.store(resolver, std::memory_order_release);
    }

    void DataShareStore::SetInvocationGate(ModInvocationGate *gate) noexcept {
        s_InvocationGate.store(gate, std::memory_order_release);
    }

    bool DataShareStore::ResolveOwner(
        const void *callerAddress, const char *ownerId,
        const void *callbackAddress, const void *cleanupAddress,
        std::string &owner) {
        owner.clear();
        const OwnerResolver resolver =
            s_OwnerResolver.load(std::memory_order_acquire);
        return !resolver || resolver(callerAddress, ownerId, callbackAddress,
                                     cleanupAddress, owner);
    }

    void DataShareStore::CleanupRejectedRequest(
        const char *key, BML_DataShareCleanupCallback cleanup,
        void *userdata, const std::string &owner) noexcept {
        try {
            InvocationScope invocation;
            std::lock_guard<std::recursive_mutex> callbackLock(s_CallbackMutex);
            if (!owner.empty()) {
                const auto state = s_OwnerStates.find(owner);
                if (state == s_OwnerStates.end() || !state->second)
                    return;
            }

            OwnerCallbackScope scope(owner);
            InvokeDataShareCleanupNoexcept(cleanup, key, userdata);
        } catch (...) {
            return;
        }
    }

    void DataShareStore::ResetRegistryForTests() {
        std::unordered_map<std::string, DataShareStore *> victims;
        InvocationScope invocation;
        {
            std::lock_guard<std::recursive_mutex> callbackLock(s_CallbackMutex);
            s_OwnerStates.clear();
            {
                std::lock_guard<std::mutex> registryLock(s_RegistryMutex);
                victims.swap(s_Registry);
            }
            for (auto &entry : victims)
                entry.second->CancelPendingCallbacks();
        }
        for (auto &entry : victims)
            entry.second->Release();
    }

    // ----------------------- Key validation --------------------------------------

    bool DataShareStore::ValidateKey(const char *key) noexcept {
        if (!key || !*key)
            return false;

        std::size_t length = 0;
        while (length <= MaxKeyLength && key[length])
            ++length;
        return length <= MaxKeyLength;
    }

    // ----------------------- Helpers ---------------------------------------------

    BML_DataShareRequest DataShareStore::AddCallbackLocked(
        const char *key, BML_DataShareCallback callback,
        BML_DataShareCleanupCallback cleanup, void *userdata,
        const std::string &owner) const {
        const BML_DataShareRequest request =
            s_NextRequest.fetch_add(1, std::memory_order_relaxed);
        if (request == BML_DATASHARE_INVALID_REQUEST)
            return BML_DATASHARE_INVALID_REQUEST;
        try {
            m_Callbacks.emplace_back(
                request, std::string(key), callback, cleanup, userdata, owner);
        } catch (...) {
            return BML_DATASHARE_INVALID_REQUEST;
        }
        return request;
    }

    void DataShareStore::TriggerCallbacksUnlocked(
        const char *key, const void *data, std::size_t size) const {
        InvocationScope invocation;
        std::lock_guard<std::recursive_mutex> callbackLock(s_CallbackMutex);
        std::list<Callback> pending;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            for (auto it = m_Callbacks.begin(); it != m_Callbacks.end();) {
                auto current = it++;
                if (current->key == key)
                    pending.splice(pending.end(), m_Callbacks, current);
            }
        }
        // Callers guarantee data outlives this call, so no extra snapshot needed.
        for (Callback &callback : pending) {
            OwnerCallbackScope scope(callback.owner);
            InvokeDataShareCallbackNoexcept(
                callback.callback, key, data, size, callback.userdata);
            InvokeDataShareCleanupNoexcept(
                callback.cleanup, key, callback.userdata);
        }
    }

    void DataShareStore::TakePendingCallbacks(
        std::list<Callback> &out,
        const std::string *owner) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto it = m_Callbacks.begin(); it != m_Callbacks.end();) {
            auto current = it++;
            if (!owner || current->owner == *owner)
                out.splice(out.end(), m_Callbacks, current);
        }
    }

    void DataShareStore::CancelPendingCallbacks() const noexcept {
        std::list<Callback> pending;
        try {
            InvocationScope invocation;
            std::lock_guard<std::recursive_mutex> callbackLock(s_CallbackMutex);
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                pending.swap(m_Callbacks);
            }
            for (Callback &callback : pending) {
                OwnerCallbackScope scope(callback.owner);
                InvokeDataShareCleanupNoexcept(
                    callback.cleanup, callback.key.c_str(), callback.userdata);
            }
        } catch (...) {
            return;
        }
    }

    // ----------------------- Data plane ------------------------------------------

    bool DataShareStore::Set(
        const char *key, const void *data, std::size_t size) {
        if (!ValidateKey(key) || (size > 0 && !data))
            return false;

        std::vector<std::uint8_t> stored(size);
        if (size)
            std::memcpy(stored.data(), data, size);
        std::vector<std::uint8_t> snapshot(stored);
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            std::vector<std::uint8_t> &value = m_Data[std::string(key)];
            value.swap(stored);
        }
        const void *payload = snapshot.empty() ? nullptr : snapshot.data();
        TriggerCallbacksUnlocked(key, payload, snapshot.size());
        return true;
    }

    void DataShareStore::Remove(const char *key) {
        if (!ValidateKey(key))
            return;
        InvocationScope invocation;
        std::lock_guard<std::recursive_mutex> callbackLock(s_CallbackMutex);
        std::list<Callback> pending;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Data.erase(std::string(key));
            for (auto it = m_Callbacks.begin(); it != m_Callbacks.end();) {
                auto current = it++;
                if (current->key == key)
                    pending.splice(pending.end(), m_Callbacks, current);
            }
        }
        for (Callback &callback : pending) {
            OwnerCallbackScope scope(callback.owner);
            InvokeDataShareCallbackNoexcept(
                callback.callback, key, nullptr, 0, callback.userdata);
            InvokeDataShareCleanupNoexcept(
                callback.cleanup, key, callback.userdata);
        }
    }

    const void *DataShareStore::Get(
        const char *key, std::size_t *outSize) const {
        if (outSize)
            *outSize = 0;
        if (!ValidateKey(key))
            return nullptr;

        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto it = m_Data.find(std::string(key));
        if (it == m_Data.end())
            return nullptr;
        if (outSize)
            *outSize = it->second.size();
        return it->second.empty() ? nullptr : it->second.data();
    }

    bool DataShareStore::Copy(
        const char *key, void *dst, std::size_t dstSize) const {
        if (!ValidateKey(key))
            return false;

        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto it = m_Data.find(std::string(key));
        if (it == m_Data.end())
            return false;
        const std::vector<std::uint8_t> &value = it->second;
        if (dstSize < value.size() || (!value.empty() && !dst))
            return false;
        if (!value.empty())
            std::memcpy(dst, value.data(), value.size());
        return true;
    }

    int DataShareStore::CopyEx(const char *key, void *dst,
                               std::size_t dstSize,
                               std::size_t *outFullSize) const {
        if (outFullSize)
            *outFullSize = 0;
        if (!ValidateKey(key))
            return 0;

        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto it = m_Data.find(std::string(key));
        if (it == m_Data.end())
            return 0;
        const std::vector<std::uint8_t> &value = it->second;
        if (outFullSize)
            *outFullSize = value.size();
        if (dstSize < value.size())
            return value.size() <= static_cast<std::size_t>(INT_MAX)
                ? -static_cast<int>(value.size())
                : INT_MIN;
        if (!value.empty() && !dst)
            return 0;
        if (!value.empty())
            std::memcpy(dst, value.data(), value.size());
        return 1;
    }

    bool DataShareStore::Has(const char *key) const {
        if (!ValidateKey(key))
            return false;
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Data.find(std::string(key)) != m_Data.end();
    }

    std::size_t DataShareStore::SizeOf(const char *key) const {
        if (!ValidateKey(key))
            return 0;
        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto it = m_Data.find(std::string(key));
        return (it == m_Data.end()) ? 0 : it->second.size();
    }

    BML_DataShareRequest DataShareStore::Request(
        const char *key, BML_DataShareCallback callback,
        BML_DataShareCleanupCallback cleanup, void *userdata,
        std::string owner) {
        InvocationScope invocation;
        std::lock_guard<std::recursive_mutex> callbackLock(s_CallbackMutex);
        if (!ValidateKey(key)) {
            OwnerCallbackScope scope(owner);
            InvokeDataShareCleanupNoexcept(cleanup, key, userdata);
            return BML_DATASHARE_INVALID_REQUEST;
        }
        if (!callback) {
            OwnerCallbackScope scope(owner);
            InvokeDataShareCleanupNoexcept(cleanup, key, userdata);
            return BML_DATASHARE_INVALID_REQUEST;
        }

        if (!owner.empty()) {
            const auto state = s_OwnerStates.find(owner);
            if (state == s_OwnerStates.end() || !state->second) {
                OwnerCallbackScope scope(owner);
                InvokeDataShareCleanupNoexcept(cleanup, key, userdata);
                return BML_DATASHARE_INVALID_REQUEST;
            }
        }

        std::vector<std::uint8_t> snapshot;
        bool queueFailed = false;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            const auto it = m_Data.find(std::string(key));
            if (it != m_Data.end()) {
                snapshot = it->second; // fire immediately with snapshot
            } else {
                const BML_DataShareRequest request = AddCallbackLocked(
                    key, callback, cleanup, userdata, owner);
                if (request != BML_DATASHARE_INVALID_REQUEST)
                    return request;
                queueFailed = true;
            }
        }

        if (queueFailed) {
            OwnerCallbackScope scope(owner);
            InvokeDataShareCleanupNoexcept(cleanup, key, userdata);
            return BML_DATASHARE_INVALID_REQUEST;
        }

        const void *payload = snapshot.empty() ? nullptr : snapshot.data();
        OwnerCallbackScope scope(owner);
        InvokeDataShareCallbackNoexcept(
            callback, key, payload, snapshot.size(), userdata);
        InvokeDataShareCleanupNoexcept(cleanup, key, userdata);
        return BML_DATASHARE_INVALID_REQUEST;
    }

    bool DataShareStore::CancelRequest(BML_DataShareRequest request,
                                       const std::string *owner) {
        if (request == BML_DATASHARE_INVALID_REQUEST)
            return false;

        InvocationScope invocation;
        std::lock_guard<std::recursive_mutex> callbackLock(s_CallbackMutex);
        std::list<Callback> cancelled;
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            for (auto it = m_Callbacks.begin(); it != m_Callbacks.end(); ++it) {
                if (it->request == request && (!owner || it->owner == *owner)) {
                    cancelled.splice(cancelled.end(), m_Callbacks, it);
                    break;
                }
            }
        }
        if (cancelled.empty())
            return false;
        Callback &callback = cancelled.front();
        OwnerCallbackScope scope(callback.owner);
        InvokeDataShareCleanupNoexcept(
            callback.cleanup, callback.key.c_str(), callback.userdata);
        return true;
    }

} // namespace BML

namespace {

BML::DataShareStore *DataShareStoreFromHandle(BML_DataShare *handle) noexcept {
    return reinterpret_cast<BML::DataShareStore *>(handle);
}

const BML::DataShareStore *DataShareStoreFromHandle(
    const BML_DataShare *handle) noexcept {
    return reinterpret_cast<const BML::DataShareStore *>(handle);
}

void CleanupRejectedDataShareRequest(
    const void *callerAddress, const char *ownerId, const char *key,
    BML_DataShareCleanupCallback cleanup, void *userdata) noexcept {
    if (!cleanup)
        return;

    try {
        std::string owner;
        if (BML::DataShareStore::ResolveOwner(
                callerAddress, ownerId, nullptr,
                reinterpret_cast<const void *>(cleanup), owner)) {
            BML::DataShareStore::CleanupRejectedRequest(
                key, cleanup, userdata, owner);
        }
    } catch (...) {
    }
}

BML_DataShareRequest RequestDataShare(
    BML_DataShare *handle, const void *callerAddress, const char *ownerId,
    bool requireOwner, const char *key, BML_DataShareCallback callback,
    void *userdata, BML_DataShareCleanupCallback cleanup) noexcept {
    std::string owner;
    bool ownerResolved = false;
    try {
        if (requireOwner && (!ownerId || !*ownerId)) {
            CleanupRejectedDataShareRequest(
                callerAddress, nullptr, key, cleanup, userdata);
            return BML_DATASHARE_INVALID_REQUEST;
        }

        ownerResolved = BML::DataShareStore::ResolveOwner(
            callerAddress, ownerId, reinterpret_cast<const void *>(callback),
            reinterpret_cast<const void *>(cleanup), owner);
        if (!ownerResolved) {
            CleanupRejectedDataShareRequest(
                callerAddress, ownerId, key, cleanup, userdata);
            return BML_DATASHARE_INVALID_REQUEST;
        }
        if (!handle) {
            BML::DataShareStore::CleanupRejectedRequest(
                key, cleanup, userdata, owner);
            return BML_DATASHARE_INVALID_REQUEST;
        }
        return DataShareStoreFromHandle(handle)->Request(
            key, callback, cleanup, userdata, owner);
    } catch (...) {
        if (ownerResolved) {
            BML::DataShareStore::CleanupRejectedRequest(
                key, cleanup, userdata, owner);
        }
        return BML_DATASHARE_INVALID_REQUEST;
    }
}

} // namespace

BML_BEGIN_CDECLS

BML_EXPORT BML_DataShare *BML_CDECL BML_GetDataShare(const char *name) {
    try {
        return reinterpret_cast<BML_DataShare *>(
            BML::DataShareStore::AcquireInstance(name));
    } catch (...) {
        return nullptr;
    }
}

BML_EXPORT std::uint32_t BML_CDECL BML_DataShare_AddRef(
    BML_DataShare *handle) {
    try {
        return handle ? DataShareStoreFromHandle(handle)->AddRef() : 0;
    } catch (...) {
        return 0;
    }
}

BML_EXPORT std::uint32_t BML_CDECL BML_DataShare_Release(
    BML_DataShare *handle) {
    try {
        return handle ? DataShareStoreFromHandle(handle)->Release() : 0;
    } catch (...) {
        return 0;
    }
}

BML_EXPORT int BML_CDECL BML_DataShare_Set(BML_DataShare *handle, const char *key,
                                            const void *data, std::size_t size) {
    try {
        const bool stored = handle &&
            DataShareStoreFromHandle(handle)->Set(key, data, size);
        return stored ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

BML_EXPORT void BML_CDECL BML_DataShare_Remove(BML_DataShare *handle,
                                                const char *key) {
    try {
        if (handle)
            DataShareStoreFromHandle(handle)->Remove(key);
    } catch (...) {
    }
}

BML_EXPORT const void *BML_CDECL BML_DataShare_Get(
    const BML_DataShare *handle, const char *key, std::size_t *outSize) {
    try {
        if (!handle) {
            if (outSize)
                *outSize = 0;
            return nullptr;
        }
        return DataShareStoreFromHandle(handle)->Get(key, outSize);
    } catch (...) {
        if (outSize)
            *outSize = 0;
        return nullptr;
    }
}

BML_EXPORT int BML_CDECL BML_DataShare_Copy(
    const BML_DataShare *handle, const char *key, void *dst,
    std::size_t dstSize) {
    try {
        const bool copied = handle &&
            DataShareStoreFromHandle(handle)->Copy(key, dst, dstSize);
        return copied ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

BML_EXPORT int BML_CDECL BML_DataShare_CopyEx(
    const BML_DataShare *handle, const char *key, void *dst,
    std::size_t dstSize, std::size_t *outFullSize) {
    try {
        if (!handle) {
            if (outFullSize)
                *outFullSize = 0;
            return 0;
        }
        return DataShareStoreFromHandle(handle)->CopyEx(
            key, dst, dstSize, outFullSize);
    } catch (...) {
        if (outFullSize)
            *outFullSize = 0;
        return 0;
    }
}

BML_EXPORT int BML_CDECL BML_DataShare_Has(const BML_DataShare *handle,
                                            const char *key) {
    try {
        const bool present = handle && DataShareStoreFromHandle(handle)->Has(key);
        return present ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

BML_EXPORT std::size_t BML_CDECL BML_DataShare_SizeOf(
    const BML_DataShare *handle, const char *key) {
    try {
        return handle ? DataShareStoreFromHandle(handle)->SizeOf(key) : 0;
    } catch (...) {
        return 0;
    }
}

BML_EXPORT BML_DataShareRequest BML_CDECL BML_DataShare_Request(
    BML_DataShare *handle, const char *key, BML_DataShareCallback callback,
    void *userdata, BML_DataShareCleanupCallback cleanup) {
    return RequestDataShare(handle, _ReturnAddress(), nullptr, false, key,
                            callback, userdata, cleanup);
}

BML_EXPORT BML_DataShareRequest BML_CDECL BML_DataShare_RequestForOwner(
    BML_DataShare *handle, const char *ownerId, const char *key,
    BML_DataShareCallback callback, void *userdata,
    BML_DataShareCleanupCallback cleanup) {
    return RequestDataShare(handle, _ReturnAddress(), ownerId, true, key,
                            callback, userdata, cleanup);
}

BML_EXPORT int BML_CDECL BML_DataShare_CancelRequest(
    BML_DataShare *handle, BML_DataShareRequest request) {
    const void *const callerAddress = _ReturnAddress();
    try {
        if (!handle)
            return 0;
        std::string owner;
        if (!BML::DataShareStore::ResolveOwner(
                callerAddress, nullptr, nullptr, nullptr, owner))
            return 0;
        const std::string *ownerFilter = owner.empty() ? nullptr : &owner;
        return DataShareStoreFromHandle(handle)->CancelRequest(
                   request, ownerFilter)
            ? 1
            : 0;
    } catch (...) {
        return 0;
    }
}

BML_EXPORT int BML_CDECL BML_DataShare_CancelRequestForOwner(
    BML_DataShare *handle, const char *ownerId,
    BML_DataShareRequest request) {
    const void *const callerAddress = _ReturnAddress();
    try {
        if (!handle || !ownerId || !*ownerId)
            return 0;
        std::string owner;
        if (!BML::DataShareStore::ResolveOwner(
                callerAddress, ownerId, nullptr, nullptr, owner))
            return 0;
        return DataShareStoreFromHandle(handle)->CancelRequest(request, &owner)
            ? 1
            : 0;
    } catch (...) {
        return 0;
    }
}

BML_END_CDECLS
