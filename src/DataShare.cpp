#include "DataShare.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <utility>

namespace BML {
    // ----------------------- Static registry ------------------------------------

    std::mutex DataShare::s_RegMutex;
    std::unordered_map<std::string, DataShare *> DataShare::s_Registry;

    // ----------------------- DataShare: lifecycle --------------------------------

    DataShare::DataShare(std::string name) : m_Ref(1), m_Name(std::move(name)) {}

    DataShare::~DataShare() {
        {
            std::lock_guard<std::mutex> g(m_Mutex);
            m_Data.clear();
        }
        CancelPendingCallbacks();
        // Unregister self
        std::lock_guard<std::mutex> g(s_RegMutex);
        const auto it = s_Registry.find(m_Name);
        if (it != s_Registry.end() && it->second == this)
            s_Registry.erase(it);
    }

    uint32_t DataShare::AddRef() const { return m_Ref.AddRef(); }

    uint32_t DataShare::Release() const {
        uint32_t newCount = m_Ref.Release();
        if (newCount == 0) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete this;
        }
        return newCount;
    }

    // ----------------------- Registry ops ----------------------------------------

    DataShare *DataShare::GetInstance(const char *name) {
        if (!name) name = "BML";
        std::lock_guard<std::mutex> g(s_RegMutex);
        auto &p = s_Registry[std::string(name)];
        if (!p) p = new DataShare(std::string(name));
        return p;
    }

    void DataShare::DestroyAllInstances() {
        std::unordered_map<std::string, DataShare*> victims;
        {
            std::lock_guard<std::mutex> g(s_RegMutex);
            if (s_Registry.empty()) return;
            victims.swap(s_Registry);
        }
        for (auto &kv : victims) {
            delete kv.second; // HARD KILL: bypasses refcounts (see header warning)
        }
    }

    // ----------------------- Key validation --------------------------------------

    bool DataShare::ValidateKey(const char *key) noexcept {
        if (!key || !*key) return false;
        constexpr size_t MAX_KEY_LEN = kMaxKeyLen;
        size_t n = 0;
        while (key[n] && n <= MAX_KEY_LEN) ++n;
        return n > 0 && n <= MAX_KEY_LEN;
    }

    // ----------------------- Helpers ---------------------------------------------

    void DataShare::AddCallbackLocked(const char *key, BML_DataShareCallback cb, BML_DataShareCleanupCallback cleanup, void *ud) const {
        if (!cb) {
            if (cleanup) cleanup(key, ud);
            return;
        }
        m_Cbs[std::string(key)].emplace_back(cb, cleanup, ud);
    }

    void DataShare::TriggerCallbacksUnlocked(const char *key, const void *data, size_t size) const {
        std::vector<Callback> pending;
        std::vector<std::uint8_t> snapshot;
        {
            std::lock_guard<std::mutex> g(m_Mutex);
            const auto it = m_Cbs.find(key);
            if (it != m_Cbs.end()) {
                pending.swap(it->second);
                m_Cbs.erase(it);
            }
            if (data && size) {
                const auto *p = static_cast<const std::uint8_t *>(data);
                snapshot.assign(p, p + size);
            }
        }
        if (!pending.empty()) {
            const void *payload = snapshot.empty() ? nullptr : snapshot.data();
            const size_t payloadSize = snapshot.size();
            for (auto &cb : pending) {
                if (cb.fn) cb.fn(key, payload, payloadSize, cb.userdata);
                if (cb.cleanup) cb.cleanup(key, cb.userdata);
            }
        }
    }

    void DataShare::CancelPendingCallbacks() const noexcept {
        std::unordered_map<std::string, std::vector<Callback>> pending;
        {
            std::lock_guard<std::mutex> g(m_Mutex);
            if (m_Cbs.empty()) return;
            pending.swap(m_Cbs);
        }
        for (auto &kv : pending) {
            const char *key = kv.first.c_str();
            for (const auto &cb : kv.second) {
                if (cb.cleanup) cb.cleanup(key, cb.userdata);
            }
        }
    }

    // ----------------------- Data plane ------------------------------------------

    bool DataShare::Set(const char *key, const void *data, size_t size) {
        if (!ValidateKey(key) || (size > 0 && data == nullptr)) return false;

        std::vector<std::uint8_t> localCopy;
        {
            std::lock_guard<std::mutex> g(m_Mutex);
            auto &buf = m_Data[std::string(key)];
            buf.resize(size);
            if (size) std::memcpy(buf.data(), data, size);
            // take snapshot for out-of-lock callbacks with a single extra copy
            localCopy = buf;
        }
        TriggerCallbacksUnlocked(key, localCopy.data(), localCopy.size());
        return true;
    }

    void DataShare::Remove(const char *key) {
        if (!ValidateKey(key)) return;
        bool hadWaiters = false;
        {
            std::lock_guard<std::mutex> g(m_Mutex);
            m_Data.erase(std::string(key));
            hadWaiters = (m_Cbs.find(std::string(key)) != m_Cbs.end());
        }
        if (hadWaiters) {
            // Wake pending callbacks with a negative result (nullptr payload)
            TriggerCallbacksUnlocked(key, nullptr, 0);
        }
    }

    const void *DataShare::Get(const char *key, size_t *outSize) const {
        if (outSize) *outSize = 0;
        if (!ValidateKey(key)) return nullptr;

        std::lock_guard<std::mutex> g(m_Mutex);
        const auto it = m_Data.find(std::string(key));
        if (it == m_Data.end()) return nullptr;
        if (outSize) *outSize = it->second.size();
        return it->second.data(); // BORROWED pointer, see header docs
    }

    bool DataShare::Copy(const char *key, void *dst, size_t dstSize) const {
        if (!ValidateKey(key)) return false;

        std::lock_guard<std::mutex> g(m_Mutex);
        const auto it = m_Data.find(std::string(key));
        if (it == m_Data.end()) return false;
        const auto &src = it->second;
        if (dstSize < src.size()) return false;
        if (!src.empty()) std::memcpy(dst, src.data(), src.size());
        return true;
    }

    int DataShare::CopyEx(const char *key, void *dst, size_t dstSize, size_t *outFullSize) const {
        if (!ValidateKey(key)) { if (outFullSize) *outFullSize = 0; return 0; }
        std::lock_guard<std::mutex> g(m_Mutex);
        const auto it = m_Data.find(std::string(key));
        if (it == m_Data.end()) { if (outFullSize) *outFullSize = 0; return 0; }
        const auto &src = it->second;
        if (outFullSize) *outFullSize = src.size();
        if (dstSize < src.size()) return -int(src.size());
        if (!src.empty()) std::memcpy(dst, src.data(), src.size());
        return 1;
    }

    bool DataShare::Has(const char *key) const {
        if (!ValidateKey(key)) return false;
        std::lock_guard<std::mutex> g(m_Mutex);
        return m_Data.find(std::string(key)) != m_Data.end();
    }

    size_t DataShare::SizeOf(const char *key) const {
        if (!ValidateKey(key)) return 0;
        std::lock_guard<std::mutex> g(m_Mutex);
        const auto it = m_Data.find(std::string(key));
        return (it == m_Data.end()) ? 0 : it->second.size();
    }

    void DataShare::Request(const char *key, BML_DataShareCallback cb, BML_DataShareCleanupCallback cleanup, void *userdata) {
        if (!ValidateKey(key)) {
            if (cleanup) cleanup(key, userdata);
            return;
        }
        if (!cb) {
            if (cleanup) cleanup(key, userdata);
            return;
        }
        std::vector<std::uint8_t> snapshot;
        {
            std::lock_guard<std::mutex> g(m_Mutex);
            const auto it = m_Data.find(std::string(key));
            if (it != m_Data.end()) {
                snapshot = it->second; // fire immediately with snapshot
            } else {
                AddCallbackLocked(key, cb, cleanup, userdata);
                return;
            }
        }

        const void *payload = snapshot.empty() ? nullptr : snapshot.data();
        cb(key, payload, snapshot.size(), userdata);
        if (cleanup) cleanup(key, userdata);
    }

} // namespace BML

// ----------------------- C API wrappers ----------------------------------------

#include "BML/Defines.h"

BML_BEGIN_CDECLS

BML_EXPORT BML_DataShare *BML_GetDataShare(const char *name) {
    if (!name) name = "BML";
    auto *ds = BML::DataShare::GetInstance(name);
    if (!ds) return nullptr;
    ds->AddRef();
    return reinterpret_cast<BML_DataShare *>(ds);
}

BML_EXPORT uint32_t BML_DataShare_AddRef(BML_DataShare *handle) {
    if (!handle) return 0;
    return reinterpret_cast<BML::DataShare *>(handle)->AddRef();
}

BML_EXPORT uint32_t BML_DataShare_Release(BML_DataShare *handle) {
    if (!handle) return 0;
    return reinterpret_cast<BML::DataShare *>(handle)->Release();
}

BML_EXPORT int BML_DataShare_Set(BML_DataShare *handle, const char *key, const void *data, size_t size) {
    if (!handle) return 0;
    return reinterpret_cast<BML::DataShare *>(handle)->Set(key, data, size) ? 1 : 0;
}

BML_EXPORT void BML_DataShare_Remove(BML_DataShare *handle, const char *key) {
    if (!handle) return;
    reinterpret_cast<BML::DataShare *>(handle)->Remove(key);
}

BML_EXPORT const void *BML_DataShare_Get(const BML_DataShare *handle, const char *key, size_t *outSize) {
    if (!handle) {
        if (outSize) *outSize = 0;
        return nullptr;
    }
    return reinterpret_cast<const BML::DataShare *>(handle)->Get(key, outSize);
}

BML_EXPORT int BML_DataShare_Copy(const BML_DataShare *handle, const char *key, void *dst, size_t dstSize) {
    if (!handle) return 0;
    return reinterpret_cast<const BML::DataShare *>(handle)->Copy(key, dst, dstSize) ? 1 : 0;
}

BML_EXPORT int BML_DataShare_CopyEx(const BML_DataShare *handle, const char *key, void *dst, size_t dstSize, size_t *outFullSize) {
    if (!handle) { if (outFullSize) *outFullSize = 0; return 0; }
    return reinterpret_cast<const BML::DataShare *>(handle)->CopyEx(key, dst, dstSize, outFullSize);
}

BML_EXPORT int BML_DataShare_Has(const BML_DataShare *handle, const char *key) {
    if (!handle) return 0;
    return reinterpret_cast<const BML::DataShare *>(handle)->Has(key) ? 1 : 0;
}

BML_EXPORT size_t BML_DataShare_SizeOf(const BML_DataShare *handle, const char *key) {
    if (!handle) return 0;
    return reinterpret_cast<const BML::DataShare *>(handle)->SizeOf(key);
}

BML_EXPORT void BML_DataShare_Request(BML_DataShare *handle, const char *key,
                                      BML_DataShareCallback callback, void *userdata,
                                      BML_DataShareCleanupCallback cleanup) {
    if (!handle) {
        if (cleanup) cleanup(key, userdata);
        return;
    }
    reinterpret_cast<BML::DataShare *>(handle)->Request(key, callback, cleanup, userdata);
}

BML_EXPORT void BML_DataShare_DestroyAll(void) {
    BML::DataShare::DestroyAllInstances();
}

BML_END_CDECLS
