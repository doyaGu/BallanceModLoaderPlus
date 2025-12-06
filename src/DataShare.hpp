/**
 * @file DataShare.hpp
 * @brief Thread-safe implementation of the DataShare backend with intrusive refcount.
 *
 * Guarantees / design:
 *  - All shared state is guarded by a single mutex.
 *  - User callbacks are never invoked while holding internal locks.
 *  - Copy() fails (returns false) on truncation; use SizeOf()/CopyEx() for robust copies.
 */
#ifndef BML_DATASHARE_HPP
#define BML_DATASHARE_HPP

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "BML/DataShare.h"
#include "BML/RefCount.h"

namespace BML {

    using BML_DataShareCallback = void (*)(const char*, const void*, size_t, void*);
    using BML_DataShareCleanupCallback = void (*)(const char*, void*);

    class RefCount;

    class DataShare {
    public:
        struct Callback {
            BML_DataShareCallback fn = nullptr;
            BML_DataShareCleanupCallback cleanup = nullptr;
            void* userdata = nullptr;
            Callback() = default;
            Callback(BML_DataShareCallback f, BML_DataShareCleanupCallback cl, void* ud) : fn(f), cleanup(cl), userdata(ud) {}
        };

        static constexpr size_t kMaxKeyLen = 255;

        explicit DataShare(std::string name);
        ~DataShare();

        static bool ValidateKey(const char *key) noexcept;

        uint32_t AddRef() const;
        uint32_t Release() const;

        // Data plane
        bool Set(const char *key, const void *data, size_t size);
        void Remove(const char *key);
        const void *Get(const char *key, size_t *outSize) const;
        bool Copy(const char *key, void *dst, size_t dstSize) const;
        // New: single-call copy with required-size reporting (-N on too small)
        int  CopyEx(const char *key, void *dst, size_t dstSize, size_t *outFullSize) const;
        bool Has(const char *key) const;
        size_t SizeOf(const char *key) const;

        // One-shot waiter: if key exists now, fires immediately; else enqueues
        void Request(const char *key, BML_DataShareCallback cb, BML_DataShareCleanupCallback cleanup, void *userdata);

        static DataShare *GetInstance(const char *name);
        static void DestroyAllInstances();

    private:
        // Contract: must be called while holding m_Mutex
        void AddCallbackLocked(const char *key, BML_DataShareCallback cb, BML_DataShareCleanupCallback cleanup, void *ud) const;
        void TriggerCallbacksUnlocked(const char *key, const void *data, size_t size) const;
        void CancelPendingCallbacks() const noexcept;

        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, std::vector<std::uint8_t>> m_Data;
        mutable std::unordered_map<std::string, std::vector<Callback>> m_Cbs;

        mutable RefCount m_Ref;
        const std::string m_Name;

        static std::mutex s_RegistryMutex;
        static std::unordered_map<std::string, DataShare *> s_Registry;
    };

} // namespace BML

#endif // BML_DATASHARE_HPP
