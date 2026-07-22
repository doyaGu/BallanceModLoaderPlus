#ifndef BML_IMCCPP_HPP
#define BML_IMCCPP_HPP

#include "BML/ImcWire.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
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

namespace Detail {
class RpcFutureHandle {
public:
    RpcFutureHandle() = default;
    ~RpcFutureHandle() { (void)Release(); }
    RpcFutureHandle(const RpcFutureHandle &) = delete;
    RpcFutureHandle &operator=(const RpcFutureHandle &) = delete;

    RpcFutureHandle(RpcFutureHandle &&other) noexcept { MoveFrom(other); }
    RpcFutureHandle &operator=(RpcFutureHandle &&other) noexcept {
        if (this != &other) {
            (void)Release();
            MoveFrom(other);
        }
        return *this;
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

    int Release() noexcept {
        if (!m_Future)
            return BML_OK;
        const int status = BML_Imc_FutureRelease(m_Future);
        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE)
            m_Future = nullptr;
        return status;
    }

protected:
    int AdoptHandle(BML_ImcFuture future) noexcept {
        if (m_Future)
            return BML_ERROR_BUSY;
        if (!future)
            return BML_ERROR_INVALID_PARAMETER;
        m_Future = future;
        return BML_OK;
    }

private:
    void MoveFrom(RpcFutureHandle &other) noexcept {
        m_Future = std::exchange(other.m_Future, nullptr);
    }

    BML_ImcFuture m_Future = nullptr;
};
} // namespace Detail

template <class Value>
class RpcFuture : public Detail::RpcFutureHandle {
public:
    using DecodeFunction = int (*)(const BML_ImcMessage &, Value &);

    RpcFuture() = default;
    RpcFuture(RpcFuture &&) noexcept = default;
    RpcFuture &operator=(RpcFuture &&) noexcept = default;

    int Adopt(BML_ImcFuture future, BML_ImcPayloadTypeId payloadType,
              DecodeFunction decode) noexcept {
        if (IsValid())
            return BML_ERROR_BUSY;
        if (payloadType == BML_IMC_INVALID_ID || !decode)
            return BML_ERROR_INVALID_PARAMETER;
        const int status = AdoptHandle(future);
        if (status == BML_OK) {
            m_PayloadType = payloadType;
            m_Decode = decode;
        }
        return status;
    }

    int GetResult(Value &out) const {
        if (!IsValid() || !m_Decode)
            return BML_ERROR_INVALID_HANDLE;
        BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
        int status = BML_Imc_FutureGetResult(Handle(), &message);
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

private:
    BML_ImcPayloadTypeId m_PayloadType = BML_IMC_INVALID_ID;
    DecodeFunction m_Decode = nullptr;
};

template <>
class RpcFuture<void> : public Detail::RpcFutureHandle {
public:
    RpcFuture() = default;
    RpcFuture(RpcFuture &&) noexcept = default;
    RpcFuture &operator=(RpcFuture &&) noexcept = default;

    int Adopt(BML_ImcFuture future) noexcept { return AdoptHandle(future); }
    int AwaitResult(std::uint32_t timeoutMs = 5000u) const noexcept {
        return Await(timeoutMs);
    }
};

template <class Value, class SizeFunction, class EncodeFunction>
int EncodeMessage(const Value &value, BML_ImcPayloadTypeId payloadType,
                  MessageBuffer &buffer, BML_ImcMessage &message,
                  SizeFunction sizeFunction, EncodeFunction encodeFunction) noexcept {
    const std::size_t size = sizeFunction(value);
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

inline int BeginRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                    const BML_ImcMessage *request, RpcFuture<void> &out,
                    std::uint32_t timeoutMs = 5000u) noexcept {
    if (out.IsValid())
        return BML_ERROR_BUSY;
    if (!client || rpcId == BML_IMC_INVALID_ID)
        return BML_ERROR_INVALID_PARAMETER;
    BML_ImcCallOptions options = BML_IMC_CALL_OPTIONS_INIT;
    options.TimeoutMs = timeoutMs;
    BML_ImcFuture future = nullptr;
    int status = BML_Imc_CallRpc(client, rpcId, request, &options, &future);
    if (status != BML_OK)
        return status;
    status = out.Adopt(future);
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
    void *data = nullptr;
    int status = BML_Imc_ResponseReserve(response, size, &data);
    if (status != BML_OK) return status;
    status = encodeFunction(value, data, size);
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
