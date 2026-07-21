#ifndef BML_IMCCPP_HPP
#define BML_IMCCPP_HPP

#include "BML/ImcWire.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

namespace BML::Imc {

template <class Client>
class LazyClient {
public:
    int EnsureOpen(const char *ownerId = nullptr) noexcept {
        if (m_Ready.load(std::memory_order_acquire))
            return BML_OK;
        std::lock_guard lock(m_Mutex);
        if (m_Ready.load(std::memory_order_relaxed))
            return BML_OK;
        const int status = m_Client.Open(ownerId);
        if (status == BML_OK)
            m_Ready.store(true, std::memory_order_release);
        return status;
    }

    Client &Get() noexcept { return m_Client; }
    const Client &Get() const noexcept { return m_Client; }

private:
    Client m_Client;
    std::atomic<bool> m_Ready{false};
    std::mutex m_Mutex;
};

class MessageBuffer {
public:
    int Resize(std::size_t size) noexcept {
        m_Size = size;
        if (size <= m_Inline.size()) {
            m_Heap.clear();
            return BML_OK;
        }
        try { m_Heap.resize(size); }
        catch (...) { m_Size = 0; return BML_ERROR_OUT_OF_MEMORY; }
        return BML_OK;
    }

    void *Data() noexcept { return m_Size <= m_Inline.size() ? m_Inline.data() : m_Heap.data(); }
    const void *Data() const noexcept { return m_Size <= m_Inline.size() ? m_Inline.data() : m_Heap.data(); }
    std::size_t Size() const noexcept { return m_Size; }

private:
    std::array<std::uint8_t, BML_IMC_INLINE_PAYLOAD_SIZE> m_Inline{};
    std::vector<std::uint8_t> m_Heap;
    std::size_t m_Size = 0;
};

class FutureGuard {
public:
    explicit FutureGuard(BML_ImcFuture future = nullptr) noexcept : m_Future(future) {}
    ~FutureGuard() { if (m_Future) (void)BML_Imc_FutureRelease(m_Future); }
    FutureGuard(const FutureGuard &) = delete;
    FutureGuard &operator=(const FutureGuard &) = delete;
    BML_ImcFuture Get() const noexcept { return m_Future; }
private:
    BML_ImcFuture m_Future = nullptr;
};

template <class Value>
class RpcFuture {
public:
    using DecodeFunction = int (*)(const BML_ImcMessage &, Value &);

    RpcFuture() = default;
    ~RpcFuture() { (void)Release(); }
    RpcFuture(const RpcFuture &) = delete;
    RpcFuture &operator=(const RpcFuture &) = delete;

    RpcFuture(RpcFuture &&other) noexcept { MoveFrom(other); }
    RpcFuture &operator=(RpcFuture &&other) noexcept {
        if (this != &other) {
            (void)Release();
            MoveFrom(other);
        }
        return *this;
    }

    int Adopt(BML_ImcFuture future, BML_ImcPayloadTypeId payloadType,
              DecodeFunction decode) noexcept {
        if (m_Future)
            return BML_ERROR_BUSY;
        if (!future || payloadType == BML_IMC_INVALID_ID || !decode)
            return BML_ERROR_INVALID_PARAMETER;
        m_Future = future;
        m_PayloadType = payloadType;
        m_Decode = decode;
        return BML_OK;
    }

    bool IsValid() const noexcept { return m_Future != nullptr; }
    BML_ImcFuture Handle() const noexcept { return m_Future; }

    int GetState(BML_ImcFutureState &out) const noexcept {
        return m_Future ? BML_Imc_FutureGetState(m_Future, &out)
                        : BML_ERROR_INVALID_HANDLE;
    }

    int Await(std::uint32_t timeoutMs = 5000u) const noexcept {
        return m_Future ? BML_Imc_FutureAwait(m_Future, timeoutMs)
                        : BML_ERROR_INVALID_HANDLE;
    }

    int Cancel() noexcept {
        return m_Future ? BML_Imc_FutureCancel(m_Future)
                        : BML_ERROR_INVALID_HANDLE;
    }

    int GetError(int &out) const noexcept {
        return m_Future ? BML_Imc_FutureGetError(m_Future, &out)
                        : BML_ERROR_INVALID_HANDLE;
    }

    int GetResult(Value &out) const {
        if (!m_Future || !m_Decode)
            return BML_ERROR_INVALID_HANDLE;
        BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
        int status = BML_Imc_FutureGetResult(m_Future, &message);
        if (status != BML_OK)
            return status;
        if (message.PayloadType != m_PayloadType)
            return BML_ERROR_TYPE_MISMATCH;
        try {
            Value decoded{};
            status = m_Decode(message, decoded);
            if (status == BML_OK)
                out = std::move(decoded);
            return status;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            return BML_ERROR_FAIL;
        }
    }

    int AwaitResult(Value &out, std::uint32_t timeoutMs = 5000u) const {
        const int status = Await(timeoutMs);
        return status == BML_OK ? GetResult(out) : status;
    }

    int Release() noexcept {
        if (!m_Future)
            return BML_OK;
        const int status = BML_Imc_FutureRelease(m_Future);
        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE)
            Reset();
        return status;
    }

private:
    void Reset() noexcept {
        m_Future = nullptr;
        m_PayloadType = BML_IMC_INVALID_ID;
        m_Decode = nullptr;
    }

    void MoveFrom(RpcFuture &other) noexcept {
        m_Future = std::exchange(other.m_Future, nullptr);
        m_PayloadType = std::exchange(other.m_PayloadType, BML_IMC_INVALID_ID);
        m_Decode = std::exchange(other.m_Decode, nullptr);
    }

    BML_ImcFuture m_Future = nullptr;
    BML_ImcPayloadTypeId m_PayloadType = BML_IMC_INVALID_ID;
    DecodeFunction m_Decode = nullptr;
};

template <class Value, class SizeFunction, class EncodeFunction>
int EncodeMessage(const Value &value, BML_ImcPayloadTypeId payloadType,
                  MessageBuffer &buffer, BML_ImcMessage &message,
                  SizeFunction sizeFunction, EncodeFunction encodeFunction) noexcept {
    const std::size_t size = sizeFunction(value);
    if (size == 0) return BML_ERROR_INVALID_PARAMETER;
    int status = buffer.Resize(size);
    if (status != BML_OK) return status;
    status = encodeFunction(value, buffer.Data(), size);
    if (status != BML_OK) return status;
    message = {};
    message.Size = sizeof(BML_ImcMessage);
    message.Data = buffer.Data();
    message.DataSize = size;
    message.PayloadType = payloadType;
    return BML_OK;
}

inline constexpr std::uint32_t ObjectRequestSchema = 0xfffffff0u;
inline constexpr std::size_t ObjectRequestSize = Wire::HeaderSize + Wire::FieldHeaderSize + 12u;

inline int EncodeObjectRequest(const BML_ObjectRef &object, BML_ImcPayloadTypeId payloadType,
                               std::uint64_t descriptorHash, MessageBuffer &buffer,
                               BML_ImcMessage &message) noexcept {
    int status = buffer.Resize(ObjectRequestSize);
    if (status != BML_OK) return status;
    Wire::Writer writer(buffer.Data(), buffer.Size());
    status = writer.Begin(ObjectRequestSchema, descriptorHash, 1);
    if (status == BML_OK) status = writer.WriteObject(1, object);
    if (status == BML_OK) status = writer.Finish();
    if (status != BML_OK) return status;
    message = {};
    message.Size = sizeof(BML_ImcMessage); message.Data = buffer.Data();
    message.DataSize = buffer.Size(); message.PayloadType = payloadType;
    return BML_OK;
}

inline int DecodeObjectRequest(const BML_ImcMessage &message,
                               BML_ImcPayloadTypeId expectedPayload,
                               BML_ObjectRef &out, std::uint64_t &descriptorHash) noexcept {
    if (message.Size < sizeof(BML_ImcMessage) || message.PayloadType != expectedPayload)
        return BML_ERROR_TYPE_MISMATCH;
    Wire::Reader reader(message.Data, message.DataSize);
    int status = reader.Begin(ObjectRequestSchema);
    if (status != BML_OK) return status;
    descriptorHash = reader.DescriptorHash();
    BML_ObjectRef decoded{}; bool seen = false; Wire::FieldView field;
    while ((status = reader.Next(field)) == BML_OK) {
        if (field.Id != 1) continue;
        if (seen) return BML_ERROR_MALFORMED_MESSAGE;
        status = Wire::Reader::ReadObject(field, decoded);
        if (status != BML_OK) return status;
        seen = true;
    }
    if (status != BML_ERROR_NOT_FOUND) return status;
    status = reader.Finish();
    if (status != BML_OK) return status;
    if (!seen) return BML_ERROR_MALFORMED_MESSAGE;
    out = decoded; return BML_OK;
}

inline constexpr std::uint32_t CollectionMagic = 0x314c4349u;
inline constexpr std::uint16_t CollectionVersion = 1u;
inline constexpr std::size_t CollectionHeaderSize = 20u;

template <class Value, class SizeFunction>
std::size_t EncodedCollectionSize(const std::vector<Value> &values, SizeFunction sizeFunction) noexcept {
    constexpr auto Max = (std::numeric_limits<std::size_t>::max)();
    if (values.size() > (std::numeric_limits<std::uint32_t>::max)()) return 0;
    std::size_t size = CollectionHeaderSize;
    for (const auto &value : values) {
        const std::size_t itemSize = sizeFunction(value);
        if (itemSize == 0 || itemSize > (std::numeric_limits<std::uint32_t>::max)() || size > Max - 4u || itemSize > Max - size - 4u) return 0;
        size += 4u + itemSize;
    }
    return size;
}

template <class Value, class SizeFunction, class EncodeFunction>
int EncodeCollection(const std::vector<Value> &values, void *data, std::size_t size,
                     std::uint64_t descriptorHash, SizeFunction sizeFunction,
                     EncodeFunction encodeFunction) noexcept {
    if (!data || descriptorHash == 0 || size != EncodedCollectionSize(values, sizeFunction))
        return BML_ERROR_INVALID_PARAMETER;
    auto *out = static_cast<std::uint8_t *>(data);
    Wire::Detail::Store32(out, CollectionMagic); Wire::Detail::Store16(out + 4, CollectionVersion);
    Wire::Detail::Store16(out + 6, 0); Wire::Detail::Store32(out + 8, static_cast<std::uint32_t>(values.size()));
    Wire::Detail::Store64(out + 12, descriptorHash); out += CollectionHeaderSize;
    for (const auto &value : values) {
        const std::size_t itemSize = sizeFunction(value);
        Wire::Detail::Store32(out, static_cast<std::uint32_t>(itemSize)); out += 4;
        const int status = encodeFunction(value, out, itemSize);
        if (status != BML_OK) return status;
        out += itemSize;
    }
    return BML_OK;
}

template <class Value, class DecodeFunction>
int DecodeCollection(const BML_ImcMessage &message, std::vector<Value> &out,
                     std::uint64_t &descriptorHash, DecodeFunction decodeFunction) {
    if (message.Size < sizeof(BML_ImcMessage) || !message.Data || message.DataSize < CollectionHeaderSize)
        return BML_ERROR_MALFORMED_MESSAGE;
    const auto *data = static_cast<const std::uint8_t *>(message.Data);
    if (Wire::Detail::Load32(data) != CollectionMagic || Wire::Detail::Load16(data + 4) != CollectionVersion)
        return BML_ERROR_MALFORMED_MESSAGE;
    const std::uint32_t count = Wire::Detail::Load32(data + 8);
    descriptorHash = Wire::Detail::Load64(data + 12);
    const auto *cursor = data + CollectionHeaderSize; std::size_t remaining = message.DataSize - CollectionHeaderSize;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (remaining < 4) return BML_ERROR_MALFORMED_MESSAGE;
        const std::uint32_t itemSize = Wire::Detail::Load32(cursor); cursor += 4; remaining -= 4;
        if (itemSize < Wire::HeaderSize || itemSize > remaining) return BML_ERROR_MALFORMED_MESSAGE;
        cursor += itemSize; remaining -= itemSize;
    }
    if (remaining != 0) return BML_ERROR_MALFORMED_MESSAGE;
    std::vector<Value> decoded;
    try { decoded.reserve(count); }
    catch (...) { return BML_ERROR_OUT_OF_MEMORY; }
    cursor = data + CollectionHeaderSize;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint32_t itemSize = Wire::Detail::Load32(cursor); cursor += 4;
        BML_ImcMessage item = BML_IMC_MESSAGE_INIT; item.Data = cursor; item.DataSize = itemSize;
        Value value{}; const int status = decodeFunction(item, value);
        if (status != BML_OK) return status;
        try { decoded.push_back(std::move(value)); }
        catch (...) { return BML_ERROR_OUT_OF_MEMORY; }
        cursor += itemSize;
    }
    out = std::move(decoded); return BML_OK;
}
template <class Output>
int BeginRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
             const BML_ImcMessage *request, BML_ImcPayloadTypeId outputPayload,
             RpcFuture<Output> &out, typename RpcFuture<Output>::DecodeFunction decodeFunction,
             std::uint32_t timeoutMs = 5000u) noexcept {
    if (out.IsValid())
        return BML_ERROR_BUSY;
    if (!client || rpcId == BML_IMC_INVALID_ID ||
        outputPayload == BML_IMC_INVALID_ID || !decodeFunction)
        return BML_ERROR_INVALID_PARAMETER;
    BML_ImcCallOptions options = BML_IMC_CALL_OPTIONS_INIT;
    options.TimeoutMs = timeoutMs;
    BML_ImcFuture future = nullptr;
    int status = BML_Imc_CallRpc(client, rpcId, request, &options, &future);
    if (status != BML_OK)
        return status;
    status = out.Adopt(future, outputPayload, decodeFunction);
    if (status != BML_OK)
        (void)BML_Imc_FutureRelease(future);
    return status;
}

template <class Output>
int CallRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
            const BML_ImcMessage *request, BML_ImcPayloadTypeId outputPayload,
            Output &out, typename RpcFuture<Output>::DecodeFunction decodeFunction,
            std::uint32_t timeoutMs = 5000u) {
    RpcFuture<Output> future;
    const int status = BeginRpc(client, rpcId, request, outputPayload, future,
                                decodeFunction, timeoutMs);
    return status == BML_OK ? future.AwaitResult(out, timeoutMs) : status;
}

template <class Value, class SizeFunction, class EncodeFunction>
int WriteResponse(BML_ImcResponse *response, BML_ImcPayloadTypeId payloadType,
                  const Value &value, SizeFunction sizeFunction,
                  EncodeFunction encodeFunction) noexcept {
    if (!response || payloadType == BML_IMC_INVALID_ID) return BML_ERROR_INVALID_PARAMETER;
    const std::size_t size = sizeFunction(value);
    if (size == 0) return BML_ERROR_INVALID_PARAMETER;
    void *data = nullptr;
    int status = BML_Imc_ResponseReserve(response, size, &data);
    if (status != BML_OK) return status;
    status = encodeFunction(value, data, size);
    return status == BML_OK ? BML_Imc_ResponseCommit(response, size, payloadType) : status;
}

template <class Value, class SizeFunction, class EncodeFunction>
int WriteCollectionResponse(BML_ImcResponse *response, BML_ImcPayloadTypeId payloadType,
                            const std::vector<Value> &values, std::uint64_t descriptorHash,
                            SizeFunction sizeFunction, EncodeFunction encodeFunction) noexcept {
    if (!response || payloadType == BML_IMC_INVALID_ID) return BML_ERROR_INVALID_PARAMETER;
    const std::size_t size = EncodedCollectionSize(values, sizeFunction);
    if (size == 0) return BML_ERROR_INVALID_PARAMETER;
    void *data = nullptr; int status = BML_Imc_ResponseReserve(response, size, &data);
    if (status != BML_OK) return status;
    status = EncodeCollection(values, data, size, descriptorHash, sizeFunction, encodeFunction);
    return status == BML_OK ? BML_Imc_ResponseCommit(response, size, payloadType) : status;
}
template <class Value, class SizeFunction, class EncodeFunction>
int Publish(BML_ImcClient client, BML_ImcTopicId topicId,
            BML_ImcPayloadTypeId payloadType, const Value &value,
            SizeFunction sizeFunction, EncodeFunction encodeFunction,
            std::size_t *outDelivered = nullptr) noexcept {
    if (!client || topicId == BML_IMC_INVALID_ID || payloadType == BML_IMC_INVALID_ID)
        return BML_ERROR_INVALID_PARAMETER;
    MessageBuffer buffer;
    BML_ImcMessage message{};
    const int status = EncodeMessage(value, payloadType, buffer, message, sizeFunction, encodeFunction);
    return status == BML_OK ? BML_Imc_Publish(client, topicId, &message, outDelivered) : status;
}

} // namespace BML::Imc

#endif // BML_IMCCPP_HPP
