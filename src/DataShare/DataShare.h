/**
 * @file DataShare.h
 * @brief Private storage, wait queue, and owner-lifecycle implementation.
 */
#ifndef BML_PRIVATE_DATA_SHARE_H
#define BML_PRIVATE_DATA_SHARE_H

#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BML/DataShare.h"
#include "BML/RefCount.h"

namespace BML {
    class ModInvocationGate;

    class DataShareStore {
    public:
        using OwnerResolver = bool (*)(const void *callerAddress,
                                       const char *ownerId,
                                       const void *callbackAddress,
                                       const void *cleanupAddress,
                                       std::string &owner);

        explicit DataShareStore(std::string name);
        ~DataShareStore();

        static bool ValidateKey(const char *key) noexcept;

        std::uint32_t AddRef() const;
        std::uint32_t Release() const;

        bool Set(const char *key, const void *data, std::size_t size);
        void Remove(const char *key);
        const void *Get(const char *key, std::size_t *outSize) const;
        bool Copy(const char *key, void *dst, std::size_t dstSize) const;
        int CopyEx(const char *key, void *dst, std::size_t dstSize,
                   std::size_t *outFullSize) const;
        bool Has(const char *key) const;
        std::size_t SizeOf(const char *key) const;

        BML_DataShareRequest Request(const char *key,
                                     BML_DataShareCallback callback,
                                     BML_DataShareCleanupCallback cleanup,
                                     void *userdata, std::string owner);
        bool CancelRequest(BML_DataShareRequest request,
                           const std::string *owner = nullptr);

        static DataShareStore *GetInstance(const char *name);
        static DataShareStore *AcquireInstance(const char *name);
        static bool RetireCallbacksFromOwner(const std::string &owner) noexcept;
        static bool ActivateCallbacksFromOwner(const std::string &owner) noexcept;
        static void SetOwnerResolver(OwnerResolver resolver) noexcept;
        static void SetInvocationGate(ModInvocationGate *gate) noexcept;
        static bool ResolveOwner(const void *callerAddress, const char *ownerId,
                                 const void *callbackAddress,
                                 const void *cleanupAddress,
                                 std::string &owner);
        static void CleanupRejectedRequest(
            const char *key, BML_DataShareCleanupCallback cleanup,
            void *userdata, const std::string &owner) noexcept;
        static void ResetRegistryForTests();

    private:
        struct Callback {
            BML_DataShareRequest request = BML_DATASHARE_INVALID_REQUEST;
            std::string key;
            BML_DataShareCallback callback = nullptr;
            BML_DataShareCleanupCallback cleanup = nullptr;
            void *userdata = nullptr;
            std::string owner;

            Callback() = default;
            Callback(BML_DataShareRequest id, std::string requestKey,
                     BML_DataShareCallback requestCallback,
                     BML_DataShareCleanupCallback cleanupCallback,
                     void *data, std::string ownerId)
                : request(id), key(std::move(requestKey)),
                  callback(requestCallback), cleanup(cleanupCallback),
                  userdata(data), owner(std::move(ownerId)) {}
            Callback(const Callback &) = delete;
            Callback &operator=(const Callback &) = delete;
            Callback(Callback &&) noexcept = default;
            Callback &operator=(Callback &&) noexcept = default;
        };

        class InvocationScope;

        static constexpr std::size_t MaxKeyLength = 255;

        BML_DataShareRequest AddCallbackLocked(
            const char *key, BML_DataShareCallback callback,
            BML_DataShareCleanupCallback cleanup, void *userdata,
            const std::string &owner) const;
        void TriggerCallbacksUnlocked(const char *key, const void *data,
                                      std::size_t size) const;
        void TakePendingCallbacks(std::list<Callback> &out,
                                  const std::string *owner = nullptr) const;
        void CancelPendingCallbacks() const noexcept;

        mutable std::mutex m_Mutex;
        std::unordered_map<std::string, std::vector<std::uint8_t>> m_Data;
        mutable std::list<Callback> m_Callbacks;
        mutable RefCount m_Ref;
        const std::string m_Name;

        static std::mutex s_RegistryMutex;
        static std::unordered_map<std::string, DataShareStore *> s_Registry;
        static std::recursive_mutex s_CallbackMutex;
        static std::unordered_map<std::string, bool> s_OwnerStates;
        static std::atomic<BML_DataShareRequest> s_NextRequest;
        static std::atomic<OwnerResolver> s_OwnerResolver;
        static std::atomic<ModInvocationGate *> s_InvocationGate;
    };
}

#endif // BML_PRIVATE_DATA_SHARE_H
