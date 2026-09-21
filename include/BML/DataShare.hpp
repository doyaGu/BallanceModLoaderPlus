// RAII C++ facade for DataShare.h. All storage and callback ownership remain in
// the loader; this header only manages opaque C handles and request tokens.
#ifndef BML_DATA_SHARE_HPP
#define BML_DATA_SHARE_HPP

#include "BML/DataShare.h"

#include <cstddef>
#include <string>
#include <utility>

namespace BML {

class DataShare;

class DataShareRequest final {
public:
    DataShareRequest() noexcept = default;
    ~DataShareRequest() { Reset(); }

    DataShareRequest(const DataShareRequest &) = delete;
    DataShareRequest &operator=(const DataShareRequest &) = delete;

    DataShareRequest(DataShareRequest &&other) noexcept
        : m_Handle(std::exchange(other.m_Handle, nullptr)),
          m_Request(std::exchange(
              other.m_Request, BML_DATASHARE_INVALID_REQUEST)),
          m_OwnerId(std::move(other.m_OwnerId)) {}

    DataShareRequest &operator=(DataShareRequest &&other) noexcept {
        if (this != &other) {
            Reset();
            m_Handle = std::exchange(other.m_Handle, nullptr);
            m_Request = std::exchange(
                other.m_Request, BML_DATASHARE_INVALID_REQUEST);
            m_OwnerId = std::move(other.m_OwnerId);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Request != BML_DATASHARE_INVALID_REQUEST;
    }

    [[nodiscard]] BML_DataShareRequest Get() const noexcept {
        return m_Request;
    }

    // Cancellation runs the cleanup supplied when the request was created.
    // False also covers a request that fired immediately or was already retired.
    [[nodiscard]] bool Cancel() noexcept {
        if (!m_Handle || m_Request == BML_DATASHARE_INVALID_REQUEST)
            return false;

        const int cancelled = m_OwnerId.empty()
            ? BML_DataShare_CancelRequest(m_Handle, m_Request)
            : BML_DataShare_CancelRequestForOwner(
                  m_Handle, m_OwnerId.c_str(), m_Request);
        BML_DataShare_Release(m_Handle);
        m_Handle = nullptr;
        m_Request = BML_DATASHARE_INVALID_REQUEST;
        m_OwnerId.clear();
        return cancelled != 0;
    }

    void Reset() noexcept { (void) Cancel(); }

private:
    friend class DataShare;

    DataShareRequest(BML_DataShare *handle, BML_DataShareRequest request,
                     std::string ownerId) noexcept
        : m_Handle(request != BML_DATASHARE_INVALID_REQUEST ? handle : nullptr),
          m_Request(request), m_OwnerId(std::move(ownerId)) {
        if (m_Handle)
            BML_DataShare_AddRef(m_Handle);
    }

    BML_DataShare *m_Handle = nullptr;
    BML_DataShareRequest m_Request = BML_DATASHARE_INVALID_REQUEST;
    std::string m_OwnerId;
};

class DataShare final {
public:
    explicit DataShare(const char *name = nullptr) noexcept
        : m_Handle(BML_GetDataShare(name)) {}

    ~DataShare() { Reset(); }

    DataShare(const DataShare &other) noexcept : m_Handle(other.m_Handle) {
        if (m_Handle)
            BML_DataShare_AddRef(m_Handle);
    }

    DataShare &operator=(const DataShare &other) noexcept {
        if (this == &other)
            return *this;

        BML_DataShare *replacement = other.m_Handle;
        if (replacement)
            BML_DataShare_AddRef(replacement);
        Reset();
        m_Handle = replacement;
        return *this;
    }

    DataShare(DataShare &&other) noexcept
        : m_Handle(std::exchange(other.m_Handle, nullptr)) {}

    DataShare &operator=(DataShare &&other) noexcept {
        if (this != &other) {
            Reset();
            m_Handle = std::exchange(other.m_Handle, nullptr);
        }
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return m_Handle != nullptr;
    }

    [[nodiscard]] BML_DataShare *Get() const noexcept { return m_Handle; }

    void Reset() noexcept {
        if (m_Handle)
            BML_DataShare_Release(m_Handle);
        m_Handle = nullptr;
    }

    [[nodiscard]] bool Set(const char *key, const void *data,
                           std::size_t size) const noexcept {
        return BML_DataShare_Set(m_Handle, key, data, size) != 0;
    }

    void Remove(const char *key) const noexcept {
        BML_DataShare_Remove(m_Handle, key);
    }

    [[nodiscard]] const void *Get(
        const char *key, std::size_t *outSize = nullptr) const noexcept {
        return BML_DataShare_Get(m_Handle, key, outSize);
    }

    [[nodiscard]] bool Copy(const char *key, void *destination,
                            std::size_t destinationSize) const noexcept {
        return BML_DataShare_Copy(
                   m_Handle, key, destination, destinationSize) != 0;
    }

    [[nodiscard]] int CopyEx(const char *key, void *destination,
                             std::size_t destinationSize,
                             std::size_t *outFullSize = nullptr) const noexcept {
        return BML_DataShare_CopyEx(
            m_Handle, key, destination, destinationSize, outFullSize);
    }

    [[nodiscard]] bool Has(const char *key) const noexcept {
        return BML_DataShare_Has(m_Handle, key) != 0;
    }

    [[nodiscard]] std::size_t SizeOf(const char *key) const noexcept {
        return BML_DataShare_SizeOf(m_Handle, key);
    }

    [[nodiscard]] DataShareRequest Request(
        const char *key, BML_DataShareCallback callback, void *userdata,
        BML_DataShareCleanupCallback cleanup = nullptr) const noexcept {
        return DataShareRequest(
            m_Handle,
            BML_DataShare_Request(
                m_Handle, key, callback, userdata, cleanup),
            {});
    }

    [[nodiscard]] DataShareRequest RequestForOwner(
        const char *ownerId, const char *key,
        BML_DataShareCallback callback, void *userdata,
        BML_DataShareCleanupCallback cleanup = nullptr) const {
        std::string owner(ownerId ? ownerId : "");
        const BML_DataShareRequest request = BML_DataShare_RequestForOwner(
            m_Handle, owner.c_str(), key, callback, userdata, cleanup);
        return DataShareRequest(m_Handle, request, std::move(owner));
    }

private:
    BML_DataShare *m_Handle = nullptr;
};

} // namespace BML

#endif // BML_DATA_SHARE_HPP
