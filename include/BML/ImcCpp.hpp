// The C++ side of Imc.h: the same calls, with the buffer and the handle looked after and
// the return code still the one to check. Header-only and inline, so including it costs
// nothing at link time and adds no export to the loader.
//
// A generated *_imc.hpp is written in terms of what is here, which is why a Mod using a
// generated client rarely names any of it directly. Reach for these when publishing an
// interface of one's own, or when calling something no .imc file describes.
//
// LazyClient opens a client the first time something needs it and hands the same one out
// afterwards, so a Mod does not have to find a place in its startup to open one. It locks
// while opening, so it can be reached from more than one thread.
//
// MessageBuffer holds a payload while it is being sent, on the stack up to
// BML_IMC_INLINE_PAYLOAD_SIZE bytes and on the heap past that. Because the loader borrows
// the bytes rather than copying them, the buffer has to outlive the call that reads it,
// which is what EncodeMessage assumes when it points a BML_ImcMessage into one.
//
// RpcFuture and FutureGuard release the future in their destructor, and RpcFuture also
// remembers which payload type was asked for and refuses a reply of another with
// BML_ERROR_TYPE_MISMATCH. RpcFuture moves and does not copy, so there is one owner of a
// future and no chance of a double release. Adopt on a future already holding one answers
// BML_ERROR_BUSY rather than dropping it.
//
// BeginRpc starts a call and leaves the waiting to the caller, CallRpc does both and is the
// one that blocks, so on the game thread BeginRpc plus a zero-timeout poll is the pair to
// use. WriteResponse encodes straight into the loader's own buffer from inside a handler,
// and Publish encodes and publishes in one step.
//
// ClientBase, TopicSubscription and RpcBinding are the machinery a generated client and
// provider used to carry a copy of each. A generated Client derives from ClientBase and
// passes its route table, a generated topic subscription is a TopicSubscription alias, and a
// generated provider registers an RpcBinding thunk per RPC, so what the generator writes is
// the interface's own names and nothing else.
//
// Nothing here throws out: an exception during a decode becomes BML_ERROR_OUT_OF_MEMORY or
// BML_ERROR_FAIL, which is the same as the C ABI does, since a handler is called from the
// loader and an exception must not cross back into it.
#ifndef BML_IMCCPP_HPP
#define BML_IMCCPP_HPP

#include "BML/ImcWire.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace BML::Imc {

template <class Client>
class LazyClient {
public:
    [[nodiscard]] int EnsureOpen(const char *ownerId = nullptr) noexcept {
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
    [[nodiscard]] int Resize(std::size_t size) noexcept {
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

    [[nodiscard]] int GetState(BML_ImcFutureState &out) const noexcept {
        return m_Future ? BML_Imc_FutureGetState(m_Future, &out)
                        : BML_ERROR_INVALID_HANDLE;
    }

    [[nodiscard]] int Await(std::uint32_t timeoutMs = 5000u) const noexcept {
        return m_Future ? BML_Imc_FutureAwait(m_Future, timeoutMs)
                        : BML_ERROR_INVALID_HANDLE;
    }

    [[nodiscard]] int Cancel() noexcept {
        return m_Future ? BML_Imc_FutureCancel(m_Future)
                        : BML_ERROR_INVALID_HANDLE;
    }

    [[nodiscard]] int GetError(int &out) const noexcept {
        return m_Future ? BML_Imc_FutureGetError(m_Future, &out)
                        : BML_ERROR_INVALID_HANDLE;
    }

    [[nodiscard]] int Release() noexcept {
        if (!m_Future)
            return BML_OK;
        const int status = BML_Imc_FutureRelease(m_Future);
        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE)
            m_Future = nullptr;
        return status;
    }

protected:
    [[nodiscard]] int AdoptHandle(BML_ImcFuture future) noexcept {
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

    [[nodiscard]] int Adopt(BML_ImcFuture future, BML_ImcPayloadTypeId payloadType,
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

    [[nodiscard]] int GetResult(Value &out) const {
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

    [[nodiscard]] int AwaitResult(Value &out, std::uint32_t timeoutMs = 5000u) const {
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

    [[nodiscard]] int Adopt(BML_ImcFuture future) noexcept { return AdoptHandle(future); }
    [[nodiscard]] int AwaitResult(std::uint32_t timeoutMs = 5000u) const noexcept {
        return Await(timeoutMs);
    }
};

template <class Value, class SizeFunction, class EncodeFunction>
[[nodiscard]] int EncodeMessage(const Value &value, BML_ImcPayloadTypeId payloadType,
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
[[nodiscard]] int BeginRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
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

[[nodiscard]] inline int BeginRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
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
[[nodiscard]] int CallRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
            const BML_ImcMessage *request, BML_ImcPayloadTypeId outputPayload,
            Output &out, typename RpcFuture<Output>::DecodeFunction decodeFunction,
            std::uint32_t timeoutMs = 5000u) {
    RpcFuture<Output> future;
    const int status = BeginRpc(client, rpcId, request, outputPayload, future,
                                decodeFunction, timeoutMs);
    return status == BML_OK ? future.AwaitResult(out, timeoutMs) : status;
}

template <class Value, class SizeFunction, class EncodeFunction>
[[nodiscard]] int WriteResponse(BML_ImcResponse *response, BML_ImcPayloadTypeId payloadType,
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
[[nodiscard]] int Publish(BML_ImcClient client, BML_ImcTopicId topicId,
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

enum class RouteKind : std::uint8_t {
    Payload,
    Rpc,
    Topic,
};

struct RouteBinding {
    RouteKind Kind = RouteKind::Payload;
    const char *Name = nullptr;
};

// Every payload type, RPC and topic name of one interface, resolved to numeric
// ids once when the client opens so a call names an id rather than a string. A
// generated Client hands its own route table in and reads the ids back by index,
// which is why the ids are here and not repeated per route in generated code.
template <std::size_t Count>
class ClientBase {
public:
    ClientBase(const ClientBase &) = delete;
    ClientBase &operator=(const ClientBase &) = delete;

    [[nodiscard]] int Open(const char *ownerId = nullptr) noexcept {
        const int closeStatus = Close();
        if (m_Client) return closeStatus;
        BML_ImcClient client = nullptr;
        const int status = BML_Imc_OpenClient(ownerId, &client);
        return status == BML_OK ? Adopt(client) : status;
    }

    [[nodiscard]] int Adopt(BML_ImcClient client) noexcept {
        const int closeStatus = Close();
        if (m_Client) return closeStatus;
        if (!client) return BML_ERROR_INVALID_PARAMETER;
        m_Client = client;
        int status = BML_OK;
        for (std::size_t index = 0; status == BML_OK && index < Count; ++index) {
            const RouteBinding &route = m_Routes[index];
            switch (route.Kind) {
            case RouteKind::Payload:
                status = BML_Imc_GetPayloadTypeId(m_Client, route.Name, &m_Ids[index]);
                break;
            case RouteKind::Rpc:
                status = BML_Imc_GetRpcId(m_Client, route.Name, &m_Ids[index]);
                break;
            case RouteKind::Topic:
                status = BML_Imc_GetTopicId(m_Client, route.Name, &m_Ids[index]);
                break;
            }
        }
        if (status != BML_OK) (void)Close();
        return status;
    }

    [[nodiscard]] int Close() noexcept {
        if (!m_Client) return BML_OK;
        const int status = BML_Imc_CloseClient(m_Client);
        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE) {
            m_Client = nullptr;
            m_Ids.fill(BML_IMC_INVALID_ID);
        }
        return status;
    }

    BML_ImcClient Handle() const noexcept { return m_Client; }
    bool IsOpen() const noexcept { return m_Client != nullptr; }

    [[nodiscard]] int EnsureOpen(const char *ownerId = nullptr) noexcept {
        return m_Client ? BML_OK : Open(ownerId);
    }

protected:
    explicit ClientBase(const RouteBinding (&routes)[Count]) noexcept : m_Routes(routes) {}
    ~ClientBase() { (void)Close(); }

    std::uint32_t RouteId(std::size_t index) const noexcept { return m_Ids[index]; }

    [[nodiscard]] int RpcAvailable(std::size_t index, bool &out) const noexcept {
        int available = 0;
        const int status = BML_Imc_IsRpcAvailable(m_Client, m_Ids[index], &available);
        if (status == BML_OK) out = available != 0;
        return status;
    }

private:
    const RouteBinding *m_Routes = nullptr;
    std::array<std::uint32_t, Count> m_Ids{};
    BML_ImcClient m_Client = nullptr;
};

// One subscription to one topic, decoding each message to the topic's payload
// type before the handler sees it. A message of another payload type is
// BML_ERROR_TYPE_MISMATCH and an undecodable one keeps its decode status, so a
// handler is told what went wrong rather than handed a half-filled value.
template <class Value, int (*DecodeFunction)(const BML_ImcMessage &, Value &)>
class TopicSubscription {
public:
    using Handler = void (*)(int status, Value *value, const BML_ImcMessage *message, void *userdata);

    TopicSubscription() = default;
    ~TopicSubscription() { (void)Close(); }
    TopicSubscription(const TopicSubscription &) = delete;
    TopicSubscription &operator=(const TopicSubscription &) = delete;

    [[nodiscard]] int Open(BML_ImcClient client, BML_ImcTopicId topic, BML_ImcPayloadTypeId payload,
                           Handler handler, void *userdata = nullptr, std::uint32_t capacity = 256u,
                           BML_ImcBackpressure backpressure = BML_IMC_BACKPRESSURE_DROP_OLDEST,
                           BML_ImcExecution execution = BML_IMC_EXECUTION_GAME_THREAD) noexcept {
        const int closeStatus = Close();
        if (IsOpen()) return closeStatus;
        if (!client || topic == BML_IMC_INVALID_ID || payload == BML_IMC_INVALID_ID || !handler
                || capacity == 0)
            return BML_ERROR_INVALID_PARAMETER;
        m_Client = client;
        m_Payload = payload;
        m_Handler = handler;
        m_Userdata = userdata;
        BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
        options.Execution = execution;
        options.Backpressure = backpressure;
        options.Capacity = capacity;
        options.ExpectedPayloadType = payload;
        const int status = BML_Imc_Subscribe(client, topic, &options, &Dispatch, this, &m_Subscription);
        if (status != BML_OK) Reset();
        return status;
    }

    [[nodiscard]] int Close() noexcept {
        if (!m_Subscription) return BML_OK;
        const int status = BML_Imc_Unsubscribe(m_Client, m_Subscription);
        if (status == BML_OK || status == BML_ERROR_INVALID_HANDLE) Reset();
        return status;
    }

    bool IsOpen() const noexcept { return m_Subscription != nullptr; }

    [[nodiscard]] int DroppedCount(std::uint64_t &out) const noexcept {
        return m_Subscription ? BML_Imc_GetSubscriptionDroppedCount(m_Client, m_Subscription, &out)
                              : BML_ERROR_INVALID_HANDLE;
    }

private:
    static void Dispatch(BML_ImcTopicId, const BML_ImcMessage *message, void *userdata) noexcept {
        auto *self = static_cast<TopicSubscription *>(userdata);
        if (!self || !self->m_Handler) return;
        Value value{};
        int status = BML_ERROR_MALFORMED_MESSAGE;
        try {
            if (message && message->PayloadType == self->m_Payload)
                status = DecodeFunction(*message, value);
            else if (message)
                status = BML_ERROR_TYPE_MISMATCH;
            self->m_Handler(status, status == BML_OK ? &value : nullptr, message, self->m_Userdata);
        } catch (...) {}
    }

    void Reset() noexcept {
        m_Client = nullptr;
        m_Subscription = nullptr;
        m_Payload = BML_IMC_INVALID_ID;
        m_Handler = nullptr;
        m_Userdata = nullptr;
    }

    BML_ImcClient m_Client = nullptr;
    BML_ImcSubscription m_Subscription = nullptr;
    BML_ImcPayloadTypeId m_Payload = BML_IMC_INVALID_ID;
    Handler m_Handler = nullptr;
    void *m_Userdata = nullptr;
};

// What one registered RPC needs at dispatch time: the handler, its userdata and
// the payload types the request must carry and the reply will carry. The thunk
// reads nothing else, which is what lets one thunk serve every interface.
template <class Handler>
struct RpcSlot {
    Handler Function = nullptr;
    void *Userdata = nullptr;
    BML_ImcPayloadTypeId RequestPayload = BML_IMC_INVALID_ID;
    BML_ImcPayloadTypeId ResponsePayload = BML_IMC_INVALID_ID;
    bool Registered = false;
};

// The four shapes an RPC comes in, as one thunk: Input and Output are void when
// the RPC has no request or no reply payload, and the codec functions are the
// generated ones for the types that are there. An exception from a handler
// becomes BML_ERROR_IMC_TARGET_EXECUTION_FAILED rather than crossing back into
// the loader.
template <class Input, class Output, class Handler,
          auto DecodeInput = nullptr, auto EncodedOutputSize = nullptr, auto EncodeOutput = nullptr>
struct RpcBinding {
    using Slot = RpcSlot<Handler>;

    [[nodiscard]] static int Thunk(BML_ImcRpcId, const BML_ImcMessage *request,
                                   BML_ImcResponse *response, void *userdata) noexcept {
        auto *slot = static_cast<Slot *>(userdata);
        if (!slot || !slot->Function) return BML_ERROR_INVALID_PARAMETER;
        // A handler is allowed to destroy its own provider, which destroys this slot. Copy
        // everything the thunk still needs before the handler runs.
        const Handler function = slot->Function;
        void *handlerUserdata = slot->Userdata;
        const BML_ImcPayloadTypeId requestPayload = slot->RequestPayload;
        const BML_ImcPayloadTypeId responsePayload = slot->ResponsePayload;
        (void)requestPayload;
        (void)responsePayload;
        (void)response;
        try {
            if constexpr (std::is_void_v<Input>) {
                if (request && (request->Size < sizeof(BML_ImcMessage) || request->DataSize != 0))
                    return BML_ERROR_MALFORMED_MESSAGE;
                if constexpr (std::is_void_v<Output>) {
                    return function(handlerUserdata);
                } else {
                    Output output{};
                    const int status = function(output, handlerUserdata);
                    return status == BML_OK
                        ? WriteResponse(response, responsePayload, output,
                                        EncodedOutputSize, EncodeOutput)
                        : status;
                }
            } else {
                if (!request || request->Size < sizeof(BML_ImcMessage))
                    return BML_ERROR_MALFORMED_MESSAGE;
                if (request->PayloadType != requestPayload)
                    return BML_ERROR_TYPE_MISMATCH;
                Input input{};
                int status = DecodeInput(*request, input);
                if (status != BML_OK) return status;
                if constexpr (std::is_void_v<Output>) {
                    return function(input, handlerUserdata);
                } else {
                    Output output{};
                    status = function(input, output, handlerUserdata);
                    return status == BML_OK
                        ? WriteResponse(response, responsePayload, output,
                                        EncodedOutputSize, EncodeOutput)
                        : status;
                }
            }
        } catch (...) {
            return BML_ERROR_IMC_TARGET_EXECUTION_FAILED;
        }
    }
};

template <class Binding, class Handler>
[[nodiscard]] int RegisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId, RpcSlot<Handler> &slot,
                              Handler handler, void *userdata, BML_ImcExecution execution,
                              BML_ImcPayloadTypeId requestPayload,
                              BML_ImcPayloadTypeId responsePayload) noexcept {
    if (!client || !handler) return BML_ERROR_INVALID_PARAMETER;
    if (slot.Registered) return BML_ERROR_ALREADY_EXISTS;
    slot = {handler, userdata, requestPayload, responsePayload, false};
    BML_ImcRpcRegistrationOptions options = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    options.Execution = execution;
    const int status = BML_Imc_RegisterRpc(client, rpcId, &options, &Binding::Thunk, &slot);
    if (status == BML_OK) slot.Registered = true;
    else slot = {};
    return status;
}

template <class Handler>
[[nodiscard]] int UnregisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                                RpcSlot<Handler> &slot) noexcept {
    if (!slot.Registered) return BML_ERROR_NOT_FOUND;
    const int status = BML_Imc_UnregisterRpc(client, rpcId);
    if (status == BML_OK) slot = {};
    return status;
}

} // namespace BML::Imc

#endif // BML_IMCCPP_HPP
