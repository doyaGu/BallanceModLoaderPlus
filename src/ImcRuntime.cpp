#include "ImcRuntime.h"

#include "ImcPrimitives.h"
#include "ModInvocationGate.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using BML::ImcDetail::BoundedQueue;
using BML::ImcDetail::BufferStorage;
using BML::ImcDetail::ObjectPool;

namespace {

uint64_t TimestampNs() noexcept {
    using namespace std::chrono;
    return static_cast<uint64_t>(duration_cast<nanoseconds>(
        steady_clock::now().time_since_epoch()).count());
}

struct SharedMessage;
struct RpcRequest;
struct CompletionItem;
std::atomic<std::uint64_t> g_NextOpaqueHandleToken{1};
constexpr std::uint64_t FutureTokenBlockSize = 65536;

bool ReserveOpaqueHandleTokens(std::uint64_t count, std::uint64_t &begin,
                               std::uint64_t &end) noexcept {
    std::uint64_t current = g_NextOpaqueHandleToken.load(std::memory_order_relaxed);
    for (;;) {
        if (current == 0 || count == 0 ||
            current > (std::numeric_limits<std::uint64_t>::max)() - count)
            return false;
        const std::uint64_t next = current + count;
        if (g_NextOpaqueHandleToken.compare_exchange_weak(
                current, next, std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            begin = current;
            end = next;
            return true;
        }
    }
}

template <class Handle>
Handle AllocateOpaqueHandle() noexcept {
    std::uint64_t token = 0;
    std::uint64_t end = 0;
    if (!ReserveOpaqueHandleTokens(1, token, end) ||
        token > (std::numeric_limits<std::uintptr_t>::max)())
        return nullptr;
    return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(token));
}

} // namespace

struct BML_ImcClient_T {
    BML::ImcRuntime *Runtime = nullptr;
    BML_ImcClient Handle = nullptr;
    std::string Owner;
    std::atomic<uint32_t> References{1};
    std::atomic<bool> PublicReference{true};
    std::atomic<bool> Active{true};
};

struct BML_ImcFuture_T {
    BML::ImcRuntime *Runtime = nullptr;
    BML_ImcFuture Handle = nullptr;
    BML_ImcClient Owner = nullptr;
    std::atomic<uint32_t> References{1};
    std::atomic<bool> PublicReference{true};
    std::mutex Mutex;
    std::condition_variable Condition;
    BML_ImcFutureState State = BML_IMC_FUTURE_PENDING;
    int Error = BML_OK;
    BufferStorage Result;
    BML_ImcPayloadTypeId PayloadType = BML_IMC_INVALID_ID;
    uint64_t MessageId = 0;
    uint64_t Timestamp = 0;
    BML_ImcFutureCallback Callback = nullptr;
    void *CallbackUserdata = nullptr;
    BML_ImcClient CallbackOwner = nullptr;
    bool CallbackQueued = false;
};

struct BML_ImcResponse_T {
    BufferStorage *Storage = nullptr;
    BML_ImcPayloadTypeId PayloadType = BML_IMC_INVALID_ID;
    bool Committed = false;
};

namespace {

struct SharedMessage {
    std::atomic<uint32_t> References{1};
    BufferStorage Payload;
    BML_ImcPayloadTypeId PayloadType = BML_IMC_INVALID_ID;
    uint32_t Flags = 0;
    uint64_t MessageId = 0;
    uint64_t Timestamp = 0;
};

struct BML_ImcSubscription_T_Impl {
    BML::ImcRuntime *Runtime = nullptr;
    BML_ImcSubscription Handle = nullptr;
    BML_ImcClient Owner = nullptr;
    BML_ImcTopicId Topic = BML_IMC_INVALID_ID;
    BML_ImcExecution Execution = BML_IMC_EXECUTION_GAME_THREAD;
    BML_ImcBackpressure Backpressure = BML_IMC_BACKPRESSURE_DROP_OLDEST;
    BML_ImcPayloadTypeId ExpectedPayloadType = BML_IMC_INVALID_ID;
    BML_ImcTopicHandler Handler = nullptr;
    void *Userdata = nullptr;
    std::atomic<uint32_t> References{1};
    std::atomic<bool> Active{true};
    std::atomic<uint64_t> Dropped{0};
    uint32_t Capacity = 0;
    std::atomic<uint32_t> Queued{0};
    std::unique_ptr<BoundedQueue<SharedMessage *>> Queue;

    bool TryReserveQueueSlot() noexcept {
        uint32_t queued = Queued.load(std::memory_order_relaxed);
        while (queued < Capacity) {
            if (Queued.compare_exchange_weak(queued, queued + 1,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed))
                return true;
        }
        return false;
    }

    void ReleaseQueueSlot() noexcept {
        Queued.fetch_sub(1, std::memory_order_relaxed);
    }
};

static_assert(sizeof(BML_ImcSubscription_T_Impl *) == sizeof(BML_ImcSubscription));

struct RpcRequest {
    BML_ImcRpcId RpcId = BML_IMC_INVALID_ID;
    BML_ImcFuture_T *Future = nullptr;
    BufferStorage Payload;
    BML_ImcPayloadTypeId PayloadType = BML_IMC_INVALID_ID;
    uint32_t Flags = 0;
    uint64_t MessageId = 0;
    uint64_t Timestamp = 0;
    uint64_t Deadline = 0;
};

struct CompletionItem {
    BML_ImcFuture_T *Future = nullptr;
    BML_ImcClient Owner = nullptr;
    BML_ImcFutureCallback Callback = nullptr;
    void *Userdata = nullptr;
};

class IdRegistry {
public:
    uint32_t GetOrCreate(const char *name) {
        if (!name || !*name)
            return BML_IMC_INVALID_ID;
        std::unique_lock lock(m_Mutex);
        const auto known = m_ByName.find(name);
        if (known != m_ByName.end())
            return known->second;

        uint32_t candidate = BML::ImcDetail::ComputeId(name);
        while (true) {
            const auto collision = m_ById.find(candidate);
            if (collision == m_ById.end())
                break;
            if (collision->second == name)
                return candidate;
            candidate = BML::ImcDetail::HashMix(candidate + 0x9e3779b9u);
            if (candidate == BML_IMC_INVALID_ID)
                candidate = 1;
        }
        try {
            m_ByName.emplace(name, candidate);
            m_ById.emplace(candidate, name);
            return candidate;
        } catch (...) {
            return BML_IMC_INVALID_ID;
        }
    }

private:
    std::mutex m_Mutex;
    std::unordered_map<std::string, uint32_t> m_ByName;
    std::unordered_map<uint32_t, std::string> m_ById;
};

struct RpcHandlerEntry {
    BML_ImcClient Owner = nullptr;
    BML_ImcRpcHandler Handler = nullptr;
    void *Userdata = nullptr;
    BML_ImcExecution Execution = BML_IMC_EXECUTION_GAME_THREAD;
};

} // namespace

/* Keep the public opaque tag distinct while using the internal implementation. */
struct BML_ImcSubscription_T : BML_ImcSubscription_T_Impl {};

namespace BML {

struct ImcRuntime::State {
    explicit State(ImcRuntime *runtime, ModInvocationGate *invocationGate)
        : Runtime(runtime), InvocationGate(invocationGate),
          MainThread(std::this_thread::get_id()),
          RpcQueue(256),
          CompletionQueue(4096) {
        (void)ReserveOpaqueHandleTokens(FutureTokenBlockSize, NextFutureToken,
                                        FutureTokenLimit);
    }

    ImcRuntime *Runtime;
    ModInvocationGate CallbackGate;
    ModInvocationGate *InvocationGate;
    const std::thread::id MainThread;
    std::atomic<bool> ShuttingDown{false};
    std::atomic<uint64_t> NextMessageId{1};

    IdRegistry RpcIds;
    IdRegistry TopicIds;
    IdRegistry PayloadIds;

    std::mutex ClientMutex;
    std::unordered_map<BML_ImcClient, BML_ImcClient> Clients;

    std::shared_mutex RpcMutex;
    std::unordered_map<BML_ImcRpcId, RpcHandlerEntry> RpcHandlers;
    BoundedQueue<RpcRequest *> RpcQueue;

    std::shared_mutex SubscriptionMutex;
    std::unordered_map<BML_ImcSubscription, BML_ImcSubscription> Subscriptions;

    std::mutex FutureMutex;
    std::unordered_map<BML_ImcFuture, BML_ImcFuture_T *> Futures;
    std::uint64_t NextFutureToken = 0;
    std::uint64_t FutureTokenLimit = 0;
    BoundedQueue<CompletionItem *> CompletionQueue;

    ObjectPool<BML_ImcFuture_T, 4096> FuturePool;
    ObjectPool<RpcRequest, 1024> RequestPool;
    ObjectPool<SharedMessage, 4096> MessagePool;
    ObjectPool<CompletionItem, 4096> CompletionPool;

    std::atomic<uint64_t> RpcCalls{0};
    std::atomic<uint64_t> RpcCompleted{0};
    std::atomic<uint64_t> RpcFailed{0};
    std::atomic<uint64_t> RpcQueueFull{0};
    std::atomic<uint64_t> MessagesPublished{0};
    std::atomic<uint64_t> MessagesDelivered{0};
    std::atomic<uint64_t> MessagesDropped{0};

    struct DeferredSubscriptionClose {
        BML_ImcClient Client = nullptr;
        BML_ImcSubscription Subscription = nullptr;
    };
    std::mutex DeferredTeardownMutex;
    std::vector<BML_ImcClient> DeferredClientCloses;
    std::vector<DeferredSubscriptionClose> DeferredSubscriptionCloses;
    std::atomic<bool> DeferredTeardownPending{false};

    class Operation {
    public:
        explicit Operation(State &state)
            : m_State(&state), m_Lock(state.CallbackGate.LockCall()) {}
        Operation(const Operation &) = delete;
        Operation &operator=(const Operation &) = delete;
        Operation(Operation &&other) noexcept
            : m_State(std::exchange(other.m_State, nullptr)),
              m_Lock(std::move(other.m_Lock)) {}
        Operation &operator=(Operation &&) = delete;
        ~Operation() {
            if (!m_State)
                return;
            const bool shouldDrain = m_Lock.IsOutermost();
            m_Lock.unlock();
            if (shouldDrain)
                m_State->DrainDeferredTeardown();
        }

    private:
        State *m_State = nullptr;
        ModInvocationGate::CallLock m_Lock;
    };

    Operation LockOperation() { return Operation(*this); }

    void AddClientRef(BML_ImcClient client) noexcept {
        client->References.fetch_add(1, std::memory_order_relaxed);
    }

    void ReleaseClientRef(BML_ImcClient client) noexcept {
        if (!client || client->References.fetch_sub(1, std::memory_order_acq_rel) != 1)
            return;
        {
            std::lock_guard lock(ClientMutex);
            Clients.erase(client->Handle);
        }
        delete client;
    }

    BML_ImcClient AcquireClient(BML_ImcClient client, bool requireActive = true) {
        if (!client)
            return nullptr;
        std::lock_guard lock(ClientMutex);
        const auto found = Clients.find(client);
        if (found == Clients.end() ||
            (requireActive &&
             !found->second->Active.load(std::memory_order_acquire)))
            return nullptr;
        AddClientRef(found->second);
        return found->second;
    }

    void AddFutureRef(BML_ImcFuture_T *future) noexcept {
        future->References.fetch_add(1, std::memory_order_relaxed);
    }

    BML_ImcFuture_T *AcquireFuture(BML_ImcFuture handle) {
        if (!handle)
            return nullptr;
        std::lock_guard lock(FutureMutex);
        const auto found = Futures.find(handle);
        if (found == Futures.end() ||
            !found->second->PublicReference.load(std::memory_order_acquire))
            return nullptr;
        auto *future = found->second;
        AddFutureRef(future);
        return future;
    }

    void ReleaseFutureRef(BML_ImcFuture_T *future) noexcept {
        if (!future)
            return;
        bool destroy = false;
        {
            std::lock_guard lock(FutureMutex);
            const auto found = Futures.find(future->Handle);
            if (found == Futures.end() || found->second != future)
                return;
            if (future->References.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                Futures.erase(found);
                destroy = true;
            }
        }
        if (!destroy)
            return;
        if (future->CallbackOwner)
            ReleaseClientRef(future->CallbackOwner);
        ReleaseClientRef(future->Owner);
        FuturePool.Destroy(future);
    }

    BML_ImcFuture_T *CreateFuture(BML_ImcClient owner) {
        auto *future = FuturePool.Construct();
        if (!future)
            return nullptr;
        future->Runtime = Runtime;
        future->Owner = owner;
        AddClientRef(owner);
        try {
            std::lock_guard lock(FutureMutex);
            BML_ImcFuture handle = nullptr;
            do {
                if (NextFutureToken == FutureTokenLimit) {
                    if (!ReserveOpaqueHandleTokens(FutureTokenBlockSize,
                                                   NextFutureToken,
                                                   FutureTokenLimit))
                        break;
                }
                const std::uint64_t token = NextFutureToken++;
                if (token > (std::numeric_limits<std::uintptr_t>::max)())
                    break;
                handle = reinterpret_cast<BML_ImcFuture>(
                    static_cast<std::uintptr_t>(token));
            } while (!handle || Futures.find(handle) != Futures.end());
            if (!handle) {
                ReleaseClientRef(owner);
                FuturePool.Destroy(future);
                return nullptr;
            }
            future->Handle = handle;
            Futures.emplace(handle, future);
        } catch (...) {
            ReleaseClientRef(owner);
            FuturePool.Destroy(future);
            return nullptr;
        }
        return future;
    }

    void AddMessageRef(SharedMessage *message) noexcept {
        message->References.fetch_add(1, std::memory_order_relaxed);
    }

    void ReleaseMessageRef(SharedMessage *message) noexcept {
        if (message && message->References.fetch_sub(1, std::memory_order_acq_rel) == 1)
            MessagePool.Destroy(message);
    }

    void AddSubscriptionRef(BML_ImcSubscription subscription) noexcept {
        subscription->References.fetch_add(1, std::memory_order_relaxed);
    }

    void ReleaseSubscriptionRef(BML_ImcSubscription subscription) noexcept {
        if (!subscription ||
            subscription->References.fetch_sub(1, std::memory_order_acq_rel) != 1)
            return;
        SharedMessage *message = nullptr;
        while (subscription->Queue && subscription->Queue->Dequeue(message)) {
            subscription->ReleaseQueueSlot();
            ReleaseMessageRef(message);
        }
        ReleaseClientRef(subscription->Owner);
        delete subscription;
    }

    int DeferClientClose(BML_ImcClient client) noexcept {
        try {
            std::lock_guard lock(DeferredTeardownMutex);
            if (!client->PublicReference.load(std::memory_order_acquire))
                return BML_ERROR_INVALID_HANDLE;
            DeferredClientCloses.push_back(client);
            client->PublicReference.store(false, std::memory_order_release);
            client->Active.store(false, std::memory_order_release);
            DeferredTeardownPending.store(true, std::memory_order_release);
        } catch (...) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
        ReleaseClientRef(client); // public reference; acquisition moves to queue
        return BML_OK;
    }

    int DeferUnsubscribe(BML_ImcClient client,
                         BML_ImcSubscription subscription) noexcept {
        try {
            std::unique_lock subscriptionLock(SubscriptionMutex);
            const auto found = Subscriptions.find(subscription);
            if (found == Subscriptions.end())
                return BML_ERROR_INVALID_HANDLE;
            auto *subscriptionState = found->second;
            if (subscriptionState->Owner != client)
                return BML_ERROR_ACCESS_DENIED;
            if (!subscriptionState->Active.load(std::memory_order_acquire))
                return BML_ERROR_INVALID_HANDLE;

            std::lock_guard deferredLock(DeferredTeardownMutex);
            DeferredSubscriptionCloses.push_back({client->Handle, subscription});
            subscriptionState->Active.store(false, std::memory_order_release);
            DeferredTeardownPending.store(true, std::memory_order_release);
            return BML_OK;
        } catch (...) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    void RevokeClientRegistrations(BML_ImcClient client) noexcept {
        {
            std::unique_lock lock(RpcMutex);
            for (auto it = RpcHandlers.begin(); it != RpcHandlers.end();) {
                if (it->second.Owner != client) {
                    ++it;
                    continue;
                }
                BML_ImcClient owner = it->second.Owner;
                it = RpcHandlers.erase(it);
                ReleaseClientRef(owner);
            }
        }
        {
            std::unique_lock lock(SubscriptionMutex);
            for (auto it = Subscriptions.begin(); it != Subscriptions.end();) {
                auto *subscription = it->second;
                if (subscription->Owner != client) {
                    ++it;
                    continue;
                }
                subscription->Active.store(false, std::memory_order_release);
                it = Subscriptions.erase(it);
                ReleaseSubscriptionRef(subscription);
            }
        }
    }

    void RemoveDeferredSubscription(
            const DeferredSubscriptionClose &deferred) noexcept {
        BML_ImcSubscription removed = nullptr;
        {
            std::unique_lock lock(SubscriptionMutex);
            const auto found = Subscriptions.find(deferred.Subscription);
            if (found == Subscriptions.end() ||
                found->second->Owner->Handle != deferred.Client)
                return;
            removed = found->second;
            removed->Active.store(false, std::memory_order_release);
            Subscriptions.erase(found);
        }
        ReleaseSubscriptionRef(removed);
    }

    void DrainDeferredTeardown() noexcept {
        if (!DeferredTeardownPending.load(std::memory_order_acquire) ||
            CallbackGate.IsCallActiveOnCurrentThread())
            return;

        auto mutation = CallbackGate.LockMutation();
        std::vector<BML_ImcClient> clients;
        std::vector<DeferredSubscriptionClose> subscriptions;
        {
            std::lock_guard lock(DeferredTeardownMutex);
            clients.swap(DeferredClientCloses);
            subscriptions.swap(DeferredSubscriptionCloses);
            DeferredTeardownPending.store(false, std::memory_order_release);
        }
        for (const auto &subscription : subscriptions)
            RemoveDeferredSubscription(subscription);
        for (auto *client : clients) {
            RevokeClientRegistrations(client);
            ReleaseClientRef(client); // queued acquisition
        }
    }

    class SubscriptionSnapshot {
    public:
        void Prepare(size_t maximum) {
            if (maximum > m_Inline.size()) {
                m_Overflow.reserve(maximum);
                m_UseOverflow = true;
            }
        }
        void Push(BML_ImcSubscription subscription) {
            if (m_UseOverflow) m_Overflow.push_back(subscription);
            else m_Inline[m_Count] = subscription;
            ++m_Count;
        }
        BML_ImcSubscription *begin() noexcept { return m_UseOverflow ? m_Overflow.data() : m_Inline.data(); }
        BML_ImcSubscription *end() noexcept { return begin() + m_Count; }
        BML_ImcSubscription const *begin() const noexcept { return m_UseOverflow ? m_Overflow.data() : m_Inline.data(); }
        BML_ImcSubscription const *end() const noexcept { return begin() + m_Count; }
    private:
        std::array<BML_ImcSubscription, 16> m_Inline{};
        std::vector<BML_ImcSubscription> m_Overflow;
        size_t m_Count = 0;
        bool m_UseOverflow = false;
    };

    SubscriptionSnapshot SnapshotSubscriptions(BML_ImcTopicId topic = 0) {
        SubscriptionSnapshot result;
        std::shared_lock lock(SubscriptionMutex);
        result.Prepare(Subscriptions.size());
        for (const auto &[handle, subscription] : Subscriptions) {
            (void)handle;
            if (!subscription->Active.load(std::memory_order_acquire) ||
                (topic != 0 && subscription->Topic != topic))
                continue;
            AddSubscriptionRef(subscription);
            result.Push(subscription);
        }
        return result;
    }

    void ScheduleCallbackLocked(BML_ImcFuture_T *future) {
        if (!future->Callback || future->CallbackQueued)
            return;
        auto *item = CompletionPool.Construct();
        if (!item)
            return;
        item->Future = future;
        item->Owner = future->CallbackOwner;
        item->Callback = future->Callback;
        item->Userdata = future->CallbackUserdata;
        AddFutureRef(future);
        AddClientRef(item->Owner);
        if (!CompletionQueue.Enqueue(item)) {
            ReleaseClientRef(item->Owner);
            ReleaseFutureRef(future);
            CompletionPool.Destroy(item);
            return;
        }
        future->CallbackQueued = true;
    }

    void Complete(BML_ImcFuture_T *future, BML_ImcFutureState state, int error,
                  BML_ImcPayloadTypeId payloadType = BML_IMC_INVALID_ID) {
        {
            std::lock_guard lock(future->Mutex);
            if (future->State != BML_IMC_FUTURE_PENDING)
                return;
            future->State = state;
            future->Error = error;
            future->PayloadType = payloadType;
            future->Timestamp = TimestampNs();
            ScheduleCallbackLocked(future);
        }
        future->Condition.notify_all();
    }

    int Invoke(BML_ImcRpcId rpcId, const BML_ImcMessage &request,
               BML_ImcFuture_T *future, uint64_t deadline) {
        if (deadline != 0 && TimestampNs() >= deadline) {
            Complete(future, BML_IMC_FUTURE_TIMED_OUT, BML_ERROR_TIMEOUT);
            RpcFailed.fetch_add(1, std::memory_order_relaxed);
            return BML_ERROR_TIMEOUT;
        }

        std::optional<ModInvocationGate::CallLock> invocation;
        if (InvocationGate)
            invocation.emplace(InvocationGate->LockCall());
        auto operation = LockOperation();

        RpcHandlerEntry entry;
        {
            std::shared_lock lock(RpcMutex);
            const auto found = RpcHandlers.find(rpcId);
            if (found == RpcHandlers.end()) {
                Complete(future, BML_IMC_FUTURE_FAILED,
                         BML_ERROR_IMC_ENDPOINT_NOT_FOUND);
                RpcFailed.fetch_add(1, std::memory_order_relaxed);
                return BML_ERROR_IMC_ENDPOINT_NOT_FOUND;
            }
            entry = found->second;
            AddClientRef(entry.Owner);
        }

        if (!entry.Owner->Active.load(std::memory_order_acquire)) {
            ReleaseClientRef(entry.Owner);
            Complete(future, BML_IMC_FUTURE_FAILED,
                     BML_ERROR_IMC_PROVIDER_UNLOADED);
            RpcFailed.fetch_add(1, std::memory_order_relaxed);
            return BML_ERROR_IMC_PROVIDER_UNLOADED;
        }

        BML_ImcResponse response{};
        response.Storage = &future->Result;
        int status = BML_ERROR_IMC_TARGET_EXECUTION_FAILED;
        try {
            status = entry.Handler(rpcId, &request, &response, entry.Userdata);
        } catch (const std::bad_alloc &) {
            status = BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            status = BML_ERROR_IMC_TARGET_EXECUTION_FAILED;
        }
        ReleaseClientRef(entry.Owner);

        if (deadline != 0 && TimestampNs() >= deadline) {
            future->Result.Reset();
            Complete(future, BML_IMC_FUTURE_TIMED_OUT, BML_ERROR_TIMEOUT);
            RpcFailed.fetch_add(1, std::memory_order_relaxed);
            return BML_ERROR_TIMEOUT;
        }
        if (status == BML_OK) {
            if (!response.Committed)
                future->Result.Reset();
            Complete(future, BML_IMC_FUTURE_READY, BML_OK,
                     response.Committed ? response.PayloadType : BML_IMC_INVALID_ID);
            RpcCompleted.fetch_add(1, std::memory_order_relaxed);
        } else {
            future->Result.Reset();
            Complete(future, BML_IMC_FUTURE_FAILED, status);
            RpcFailed.fetch_add(1, std::memory_order_relaxed);
        }
        return status;
    }
};

ImcRuntime::ImcRuntime(ModInvocationGate *invocationGate)
    : m_State(std::make_unique<State>(this, invocationGate)) {}

ImcRuntime::~ImcRuntime() { Shutdown(); }

bool ImcRuntime::IsMainThread() const noexcept {
    return std::this_thread::get_id() == m_State->MainThread;
}

void ImcRuntime::SetInvocationGate(ModInvocationGate *invocationGate) noexcept {
    m_State->InvocationGate = invocationGate;
}

int ImcRuntime::OpenClient(const std::string &ownerId, BML_ImcClient *outClient) {
    auto operation = m_State->LockOperation();
    if (!outClient || ownerId.empty())
        return BML_ERROR_INVALID_PARAMETER;
    *outClient = nullptr;
    if (m_State->ShuttingDown.load(std::memory_order_acquire))
        return BML_ERROR_FROZEN;
    auto *client = new(std::nothrow) BML_ImcClient_T;
    if (!client)
        return BML_ERROR_OUT_OF_MEMORY;
    client->Runtime = this;
    client->Handle = AllocateOpaqueHandle<BML_ImcClient>();
    if (!client->Handle) {
        delete client;
        return BML_ERROR_OUT_OF_MEMORY;
    }
    try {
        client->Owner = ownerId;
        std::lock_guard lock(m_State->ClientMutex);
        m_State->Clients.emplace(client->Handle, client);
    } catch (...) {
        delete client;
        return BML_ERROR_OUT_OF_MEMORY;
    }
    *outClient = client->Handle;
    return BML_OK;
}

int ImcRuntime::CloseClient(BML_ImcClient client) {
    auto *owned = m_State->AcquireClient(client, false);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (m_State->CallbackGate.IsCallActiveOnCurrentThread()) {
        const int status = m_State->DeferClientClose(owned);
        if (status != BML_OK)
            m_State->ReleaseClientRef(owned);
        return status;
    }
    auto mutation = m_State->CallbackGate.LockMutation();
    if (!owned->PublicReference.exchange(false, std::memory_order_acq_rel)) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_HANDLE;
    }
    const bool wasActive = owned->Active.exchange(false, std::memory_order_acq_rel);
    if (wasActive)
        m_State->RevokeClientRegistrations(owned);
    m_State->ReleaseClientRef(owned); // acquisition
    m_State->ReleaseClientRef(owned); // public reference
    return wasActive ? BML_OK : BML_ERROR_INVALID_HANDLE;
}

int ImcRuntime::GetRpcId(BML_ImcClient client, const char *name,
                         BML_ImcRpcId *outId) {
    auto *owned = m_State->AcquireClient(client);
    if (!owned || !outId || !name || !*name) {
        if (owned) m_State->ReleaseClientRef(owned);
        return owned ? BML_ERROR_INVALID_PARAMETER : BML_ERROR_INVALID_HANDLE;
    }
    *outId = m_State->RpcIds.GetOrCreate(name);
    m_State->ReleaseClientRef(owned);
    return *outId ? BML_OK : BML_ERROR_OUT_OF_MEMORY;
}

int ImcRuntime::GetTopicId(BML_ImcClient client, const char *name,
                           BML_ImcTopicId *outId) {
    auto *owned = m_State->AcquireClient(client);
    if (!owned || !outId || !name || !*name) {
        if (owned) m_State->ReleaseClientRef(owned);
        return owned ? BML_ERROR_INVALID_PARAMETER : BML_ERROR_INVALID_HANDLE;
    }
    *outId = m_State->TopicIds.GetOrCreate(name);
    m_State->ReleaseClientRef(owned);
    return *outId ? BML_OK : BML_ERROR_OUT_OF_MEMORY;
}

int ImcRuntime::GetPayloadTypeId(BML_ImcClient client, const char *name,
                                 BML_ImcPayloadTypeId *outId) {
    auto *owned = m_State->AcquireClient(client);
    if (!owned || !outId || !name || !*name) {
        if (owned) m_State->ReleaseClientRef(owned);
        return owned ? BML_ERROR_INVALID_PARAMETER : BML_ERROR_INVALID_HANDLE;
    }
    *outId = m_State->PayloadIds.GetOrCreate(name);
    m_State->ReleaseClientRef(owned);
    return *outId ? BML_OK : BML_ERROR_OUT_OF_MEMORY;
}

int ImcRuntime::IsRpcAvailable(BML_ImcClient client, BML_ImcRpcId rpcId,
                               int *outAvailable) {
    auto *owned = m_State->AcquireClient(client);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (rpcId == BML_IMC_INVALID_ID || !outAvailable) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    {
        std::shared_lock lock(m_State->RpcMutex);
        *outAvailable = m_State->RpcHandlers.find(rpcId) !=
                        m_State->RpcHandlers.end() ? 1 : 0;
    }
    m_State->ReleaseClientRef(owned);
    return BML_OK;
}

int ImcRuntime::RegisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                            const BML_ImcRpcRegistrationOptions *options,
                            BML_ImcRpcHandler handler, void *userdata) {
    if (m_State->CallbackGate.IsCallActiveOnCurrentThread())
        return BML_ERROR_BUSY;
    auto operation = m_State->LockOperation();
    auto *owned = m_State->AcquireClient(client);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (!handler || rpcId == BML_IMC_INVALID_ID ||
        (options && options->Size < sizeof(BML_ImcRpcRegistrationOptions))) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    const BML_ImcExecution execution = options ? options->Execution
                                                : BML_IMC_EXECUTION_GAME_THREAD;
    if (execution != BML_IMC_EXECUTION_GAME_THREAD &&
        execution != BML_IMC_EXECUTION_CALLER_THREAD) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    std::unique_lock lock(m_State->RpcMutex);
    if (m_State->RpcHandlers.find(rpcId) != m_State->RpcHandlers.end()) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_ALREADY_EXISTS;
    }
    try {
        m_State->RpcHandlers.emplace(rpcId,
            RpcHandlerEntry{owned, handler, userdata, execution});
        m_State->AddClientRef(owned);
    } catch (...) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    m_State->ReleaseClientRef(owned);
    return BML_OK;
}

int ImcRuntime::UnregisterRpc(BML_ImcClient client, BML_ImcRpcId rpcId) {
    auto *owned = m_State->AcquireClient(client, false);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (m_State->CallbackGate.IsCallActiveOnCurrentThread()) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_BUSY;
    }
    auto mutation = m_State->CallbackGate.LockMutation();
    BML_ImcClient registeredOwner = nullptr;
    {
        std::unique_lock lock(m_State->RpcMutex);
        const auto found = m_State->RpcHandlers.find(rpcId);
        if (found == m_State->RpcHandlers.end()) {
            m_State->ReleaseClientRef(owned);
            return BML_ERROR_NOT_FOUND;
        }
        if (found->second.Owner != owned) {
            m_State->ReleaseClientRef(owned);
            return BML_ERROR_ACCESS_DENIED;
        }
        registeredOwner = found->second.Owner;
        m_State->RpcHandlers.erase(found);
    }
    m_State->ReleaseClientRef(registeredOwner);
    m_State->ReleaseClientRef(owned);
    return BML_OK;
}

int ImcRuntime::CallRpc(BML_ImcClient client, BML_ImcRpcId rpcId,
                        const BML_ImcMessage *request,
                        const BML_ImcCallOptions *options,
                        BML_ImcFuture *outFuture) {
    auto operation = m_State->LockOperation();
    if (!outFuture)
        return BML_ERROR_INVALID_PARAMETER;
    *outFuture = nullptr;
    auto *owned = m_State->AcquireClient(client);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (rpcId == BML_IMC_INVALID_ID ||
        (request && (request->Size < sizeof(BML_ImcMessage) ||
                     (request->DataSize != 0 && !request->Data))) ||
        (options && options->Size < sizeof(BML_ImcCallOptions))) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }

    BML_ImcExecution execution;
    {
        std::shared_lock lock(m_State->RpcMutex);
        const auto handler = m_State->RpcHandlers.find(rpcId);
        if (handler == m_State->RpcHandlers.end()) {
            m_State->ReleaseClientRef(owned);
            return BML_ERROR_IMC_ENDPOINT_NOT_FOUND;
        }
        execution = handler->second.Execution;
    }

    auto *future = m_State->CreateFuture(owned);
    if (!future) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    const uint32_t timeout = options ? options->TimeoutMs : 5000u;
    const uint64_t deadline = timeout == std::numeric_limits<uint32_t>::max()
        ? 0 : TimestampNs() + static_cast<uint64_t>(timeout) * 1000000ull;
    BML_ImcMessage view = BML_IMC_MESSAGE_INIT;
    if (request)
        view = *request;
    if (view.MessageId == 0)
        view.MessageId = m_State->NextMessageId.fetch_add(1, std::memory_order_relaxed);
    if (view.TimestampNs == 0)
        view.TimestampNs = TimestampNs();
    future->MessageId = view.MessageId;

    m_State->RpcCalls.fetch_add(1, std::memory_order_relaxed);
    if (execution == BML_IMC_EXECUTION_CALLER_THREAD || IsMainThread()) {
        m_State->Invoke(rpcId, view, future, deadline);
        *outFuture = future->Handle;
        m_State->ReleaseClientRef(owned);
        return BML_OK;
    }

    auto *queued = m_State->RequestPool.Construct();
    if (!queued || !queued->Payload.CopyFrom(view.Data, view.DataSize)) {
        if (queued) m_State->RequestPool.Destroy(queued);
        m_State->ReleaseFutureRef(future);
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    queued->RpcId = rpcId;
    queued->Future = future;
    queued->PayloadType = view.PayloadType;
    queued->Flags = view.Flags;
    queued->MessageId = view.MessageId;
    queued->Timestamp = view.TimestampNs;
    queued->Deadline = deadline;
    m_State->AddFutureRef(future);
    if (!m_State->RpcQueue.Enqueue(queued)) {
        m_State->ReleaseFutureRef(future);
        m_State->RequestPool.Destroy(queued);
        m_State->ReleaseFutureRef(future);
        m_State->RpcQueueFull.fetch_add(1, std::memory_order_relaxed);
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_WOULD_BLOCK;
    }
    *outFuture = future->Handle;
    m_State->ReleaseClientRef(owned);
    return BML_OK;
}

int ImcRuntime::ResponseReserve(BML_ImcResponse *response, size_t size,
                                void **outData) {
    if (!response || !response->Storage || !outData)
        return BML_ERROR_INVALID_PARAMETER;
    *outData = nullptr;
    response->PayloadType = BML_IMC_INVALID_ID;
    response->Committed = false;
    if (!response->Storage->Resize(size))
        return BML_ERROR_OUT_OF_MEMORY;
    *outData = response->Storage->Data();
    return BML_OK;
}

int ImcRuntime::ResponseCommit(BML_ImcResponse *response, size_t size,
                               BML_ImcPayloadTypeId payloadType) {
    if (!response || !response->Storage || size != response->Storage->Size())
        return BML_ERROR_INVALID_PARAMETER;
    response->PayloadType = payloadType;
    response->Committed = true;
    return BML_OK;
}

int ImcRuntime::ResponseWrite(BML_ImcResponse *response, const void *data,
                              size_t size, BML_ImcPayloadTypeId payloadType) {
    if (!response || !response->Storage || (size != 0 && !data))
        return BML_ERROR_INVALID_PARAMETER;
    response->PayloadType = BML_IMC_INVALID_ID;
    response->Committed = false;
    if (!response->Storage->CopyFrom(data, size))
        return BML_ERROR_OUT_OF_MEMORY;
    response->PayloadType = payloadType;
    response->Committed = true;
    return BML_OK;
}

int ImcRuntime::FutureGetState(BML_ImcFuture future,
                               BML_ImcFutureState *outState) {
    auto *owned = m_State->AcquireFuture(future);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (!outState) {
        m_State->ReleaseFutureRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    {
        std::lock_guard lock(owned->Mutex);
        *outState = owned->State;
    }
    m_State->ReleaseFutureRef(owned);
    return BML_OK;
}

int ImcRuntime::FutureAwait(BML_ImcFuture future, uint32_t timeoutMs) {
    auto *owned = m_State->AcquireFuture(future);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    std::unique_lock lock(owned->Mutex);
    if (owned->State == BML_IMC_FUTURE_PENDING && IsMainThread() &&
        timeoutMs != 0) {
        lock.unlock();
        m_State->ReleaseFutureRef(owned);
        return BML_ERROR_WRONG_THREAD;
    }
    if (owned->State == BML_IMC_FUTURE_PENDING && timeoutMs != 0) {
        if (timeoutMs == std::numeric_limits<uint32_t>::max()) {
            owned->Condition.wait(lock, [&] { return owned->State != BML_IMC_FUTURE_PENDING; });
        } else if (!owned->Condition.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                             [&] { return owned->State != BML_IMC_FUTURE_PENDING; })) {
            lock.unlock();
            m_State->ReleaseFutureRef(owned);
            return BML_ERROR_TIMEOUT;
        }
    }
    const int result = owned->State == BML_IMC_FUTURE_PENDING
        ? BML_ERROR_BUSY : owned->Error;
    lock.unlock();
    m_State->ReleaseFutureRef(owned);
    return result;
}

int ImcRuntime::FutureCancel(BML_ImcFuture future) {
    auto *owned = m_State->AcquireFuture(future);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    int result = BML_ERROR_BUSY;
    {
        std::lock_guard lock(owned->Mutex);
        if (owned->State == BML_IMC_FUTURE_PENDING) {
            owned->State = BML_IMC_FUTURE_CANCELLED;
            owned->Error = BML_ERROR_CANCELLED;
            m_State->ScheduleCallbackLocked(owned);
            result = BML_OK;
        }
    }
    if (result == BML_OK)
        owned->Condition.notify_all();
    m_State->ReleaseFutureRef(owned);
    return result;
}

int ImcRuntime::FutureGetResult(BML_ImcFuture future, BML_ImcMessage *outMessage) {
    auto *owned = m_State->AcquireFuture(future);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (!outMessage || outMessage->Size < sizeof(BML_ImcMessage)) {
        m_State->ReleaseFutureRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    int result = BML_OK;
    {
        std::lock_guard lock(owned->Mutex);
        if (owned->State != BML_IMC_FUTURE_READY) {
            result = owned->State == BML_IMC_FUTURE_PENDING ? BML_ERROR_BUSY
                                                             : owned->Error;
        } else {
            outMessage->Data = owned->Result.Data();
            outMessage->DataSize = owned->Result.Size();
            outMessage->PayloadType = owned->PayloadType;
            outMessage->Flags = 0;
            outMessage->MessageId = owned->MessageId;
            outMessage->TimestampNs = owned->Timestamp;
        }
    }
    m_State->ReleaseFutureRef(owned);
    return result;
}

int ImcRuntime::FutureGetError(BML_ImcFuture future, int *outError) {
    auto *owned = m_State->AcquireFuture(future);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (!outError) {
        m_State->ReleaseFutureRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    {
        std::lock_guard lock(owned->Mutex);
        *outError = owned->Error;
    }
    m_State->ReleaseFutureRef(owned);
    return BML_OK;
}

int ImcRuntime::FutureOnComplete(BML_ImcClient client, BML_ImcFuture future,
                                 BML_ImcFutureCallback callback, void *userdata) {
    auto operation = m_State->LockOperation();
    auto *callbackOwner = m_State->AcquireClient(client);
    auto *owned = m_State->AcquireFuture(future);
    if (!callbackOwner || !owned) {
        if (callbackOwner) m_State->ReleaseClientRef(callbackOwner);
        if (owned) m_State->ReleaseFutureRef(owned);
        return BML_ERROR_INVALID_HANDLE;
    }
    if (!callback) {
        m_State->ReleaseClientRef(callbackOwner);
        m_State->ReleaseFutureRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    bool alreadyRegistered = false;
    {
        std::lock_guard lock(owned->Mutex);
        if (owned->Callback) {
            alreadyRegistered = true;
        } else {
            owned->Callback = callback;
            owned->CallbackUserdata = userdata;
            owned->CallbackOwner = callbackOwner;
            m_State->AddClientRef(callbackOwner);
            if (owned->State != BML_IMC_FUTURE_PENDING)
                m_State->ScheduleCallbackLocked(owned);
        }
    }
    m_State->ReleaseClientRef(callbackOwner);
    m_State->ReleaseFutureRef(owned);
    return alreadyRegistered ? BML_ERROR_ALREADY_EXISTS : BML_OK;
}

int ImcRuntime::FutureRelease(BML_ImcFuture future) {
    BML_ImcFuture_T *owned = nullptr;
    {
        std::lock_guard lock(m_State->FutureMutex);
        const auto found = m_State->Futures.find(future);
        if (!future || found == m_State->Futures.end())
            return BML_ERROR_INVALID_HANDLE;
        owned = found->second;
        if (!owned->PublicReference.exchange(false, std::memory_order_acq_rel))
            return BML_ERROR_INVALID_HANDLE;
    }
    m_State->ReleaseFutureRef(owned);
    return BML_OK;
}

int ImcRuntime::Subscribe(BML_ImcClient client, BML_ImcTopicId topicId,
                          const BML_ImcSubscribeOptions *options,
                          BML_ImcTopicHandler handler, void *userdata,
                          BML_ImcSubscription *outSubscription) {
    auto operation = m_State->LockOperation();
    if (!outSubscription)
        return BML_ERROR_INVALID_PARAMETER;
    *outSubscription = nullptr;
    auto *owned = m_State->AcquireClient(client);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (!handler || topicId == BML_IMC_INVALID_ID ||
        (options && options->Size < sizeof(BML_ImcSubscribeOptions))) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    BML_ImcSubscribeOptions effective = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    if (options)
        effective = *options;
    if (effective.Capacity < 1 || effective.Capacity > 16384 ||
        (effective.Execution != BML_IMC_EXECUTION_GAME_THREAD &&
         effective.Execution != BML_IMC_EXECUTION_CALLER_THREAD) ||
        (effective.Backpressure != BML_IMC_BACKPRESSURE_DROP_OLDEST &&
         effective.Backpressure != BML_IMC_BACKPRESSURE_DROP_NEWEST &&
         effective.Backpressure != BML_IMC_BACKPRESSURE_FAIL)) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    auto *subscription = new(std::nothrow) BML_ImcSubscription_T;
    if (!subscription) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    subscription->Handle = AllocateOpaqueHandle<BML_ImcSubscription>();
    if (!subscription->Handle) {
        delete subscription;
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    bool ownerRetained = false;
    try {
        subscription->Runtime = this;
        subscription->Owner = owned;
        subscription->Topic = topicId;
        subscription->Execution = effective.Execution;
        subscription->Backpressure = effective.Backpressure;
        subscription->ExpectedPayloadType = effective.ExpectedPayloadType;
        subscription->Handler = handler;
        subscription->Userdata = userdata;
        subscription->Capacity = effective.Capacity;
        subscription->Queue = std::make_unique<BoundedQueue<SharedMessage *>>(effective.Capacity);
        m_State->AddClientRef(owned);
        ownerRetained = true;
        std::unique_lock lock(m_State->SubscriptionMutex);
        m_State->Subscriptions.emplace(subscription->Handle, subscription);
    } catch (...) {
        if (ownerRetained)
            m_State->ReleaseClientRef(owned);
        delete subscription;
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    *outSubscription = subscription->Handle;
    m_State->ReleaseClientRef(owned);
    return BML_OK;
}

int ImcRuntime::Unsubscribe(BML_ImcClient client,
                            BML_ImcSubscription subscription) {
    auto *owned = m_State->AcquireClient(client, false);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (m_State->CallbackGate.IsCallActiveOnCurrentThread()) {
        const int status = m_State->DeferUnsubscribe(owned, subscription);
        m_State->ReleaseClientRef(owned);
        return status;
    }
    auto mutation = m_State->CallbackGate.LockMutation();
    {
        std::unique_lock lock(m_State->SubscriptionMutex);
        const auto found = m_State->Subscriptions.find(subscription);
        if (found == m_State->Subscriptions.end()) {
            m_State->ReleaseClientRef(owned);
            return BML_ERROR_INVALID_HANDLE;
        }
        auto *subscriptionState = found->second;
        if (subscriptionState->Owner != owned) {
            m_State->ReleaseClientRef(owned);
            return BML_ERROR_ACCESS_DENIED;
        }
        subscriptionState->Active.store(false, std::memory_order_release);
        m_State->Subscriptions.erase(found);
        subscription = subscriptionState;
    }
    m_State->ReleaseSubscriptionRef(subscription);
    m_State->ReleaseClientRef(owned);
    return BML_OK;
}

int ImcRuntime::GetSubscriptionDroppedCount(BML_ImcClient client,
                                             BML_ImcSubscription subscription,
                                             uint64_t *outCount) {
    if (outCount) *outCount = 0;
    auto *owned = m_State->AcquireClient(client);
    if (!owned) return BML_ERROR_INVALID_HANDLE;
    if (!subscription || !outCount) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    int status = BML_OK;
    {
        std::shared_lock lock(m_State->SubscriptionMutex);
        const auto found = m_State->Subscriptions.find(subscription);
        if (found == m_State->Subscriptions.end()) status = BML_ERROR_INVALID_HANDLE;
        else if (found->second->Owner != owned) status = BML_ERROR_ACCESS_DENIED;
        else *outCount = found->second->Dropped.load(std::memory_order_relaxed);
    }
    m_State->ReleaseClientRef(owned);
    return status;
}
int ImcRuntime::Publish(BML_ImcClient client, BML_ImcTopicId topicId,
                        const BML_ImcMessage *message, size_t *outDelivered) {
    auto operation = m_State->LockOperation();
    if (outDelivered)
        *outDelivered = 0;
    auto *owned = m_State->AcquireClient(client);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (topicId == BML_IMC_INVALID_ID || !message ||
        message->Size < sizeof(BML_ImcMessage) ||
        (message->DataSize != 0 && !message->Data)) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    State::SubscriptionSnapshot subscriptions;
    try {
        subscriptions = m_State->SnapshotSubscriptions(topicId);
    } catch (...) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    if (subscriptions.begin() == subscriptions.end()) {
        m_State->MessagesPublished.fetch_add(1, std::memory_order_relaxed);
        m_State->ReleaseClientRef(owned);
        return BML_OK;
    }

    auto *shared = m_State->MessagePool.Construct();
    if (!shared || !shared->Payload.CopyFrom(message->Data, message->DataSize)) {
        if (shared) m_State->MessagePool.Destroy(shared);
        for (auto *subscription : subscriptions)
            m_State->ReleaseSubscriptionRef(subscription);
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_OUT_OF_MEMORY;
    }
    shared->PayloadType = message->PayloadType;
    shared->Flags = message->Flags;
    shared->MessageId = message->MessageId ? message->MessageId
        : m_State->NextMessageId.fetch_add(1, std::memory_order_relaxed);
    shared->Timestamp = message->TimestampNs ? message->TimestampNs : TimestampNs();

    size_t delivered = 0;
    int failure = BML_OK;
    for (auto *subscription : subscriptions) {
        if (subscription->ExpectedPayloadType != BML_IMC_INVALID_ID &&
            subscription->ExpectedPayloadType != shared->PayloadType) {
            subscription->Dropped.fetch_add(1, std::memory_order_relaxed);
            m_State->MessagesDropped.fetch_add(1, std::memory_order_relaxed);
            m_State->ReleaseSubscriptionRef(subscription);
            continue;
        }
        if (subscription->Execution == BML_IMC_EXECUTION_CALLER_THREAD) {
            BML_ImcMessage view = BML_IMC_MESSAGE_INIT;
            view.Data = shared->Payload.Data();
            view.DataSize = shared->Payload.Size();
            view.PayloadType = shared->PayloadType;
            view.Flags = shared->Flags;
            view.MessageId = shared->MessageId;
            view.TimestampNs = shared->Timestamp;
            try {
                std::optional<ModInvocationGate::CallLock> invocation;
                if (m_State->InvocationGate)
                    invocation.emplace(m_State->InvocationGate->LockCall());
                auto operation = m_State->LockOperation();
                if (subscription->Active.load(std::memory_order_acquire) &&
                    subscription->Owner->Active.load(std::memory_order_acquire)) {
                    subscription->Handler(topicId, &view, subscription->Userdata);
                    ++delivered;
                }
            } catch (const std::bad_alloc &) {
                failure = BML_ERROR_OUT_OF_MEMORY;
                subscription->Dropped.fetch_add(1, std::memory_order_relaxed);
                m_State->MessagesDropped.fetch_add(1, std::memory_order_relaxed);
            } catch (...) {
                if (failure == BML_OK)
                    failure = BML_ERROR_IMC_TARGET_EXECUTION_FAILED;
                subscription->Dropped.fetch_add(1, std::memory_order_relaxed);
                m_State->MessagesDropped.fetch_add(1, std::memory_order_relaxed);
            }
            m_State->ReleaseSubscriptionRef(subscription);
            continue;
        }

        m_State->AddMessageRef(shared);
        if (subscription->Backpressure == BML_IMC_BACKPRESSURE_DROP_OLDEST) {
            for (;;) {
                if (subscription->TryReserveQueueSlot()) {
                    while (!subscription->Queue->Enqueue(shared))
                        std::this_thread::yield();
                    ++delivered;
                    break;
                }
                SharedMessage *dropped = nullptr;
                if (!subscription->Queue->Dequeue(dropped)) {
                    std::this_thread::yield();
                    continue;
                }
                subscription->ReleaseQueueSlot();
                m_State->ReleaseMessageRef(dropped);
                subscription->Dropped.fetch_add(1, std::memory_order_relaxed);
                m_State->MessagesDropped.fetch_add(1, std::memory_order_relaxed);
            }
        } else if (!subscription->TryReserveQueueSlot()) {
            if (subscription->Backpressure == BML_IMC_BACKPRESSURE_FAIL &&
                failure == BML_OK)
                failure = BML_ERROR_WOULD_BLOCK;
            m_State->ReleaseMessageRef(shared);
            subscription->Dropped.fetch_add(1, std::memory_order_relaxed);
            m_State->MessagesDropped.fetch_add(1, std::memory_order_relaxed);
        } else {
            while (!subscription->Queue->Enqueue(shared))
                std::this_thread::yield();
            ++delivered;
        }
        m_State->ReleaseSubscriptionRef(subscription);
    }
    m_State->MessagesPublished.fetch_add(1, std::memory_order_relaxed);
    m_State->MessagesDelivered.fetch_add(delivered, std::memory_order_relaxed);
    m_State->ReleaseMessageRef(shared);
    m_State->ReleaseClientRef(owned);
    if (outDelivered)
        *outDelivered = delivered;
    return failure;
}

void ImcRuntime::Pump(size_t rpcBudget, size_t messageBudgetPerSubscription,
                      size_t completionBudget) {
    if (!IsMainThread() || m_State->ShuttingDown.load(std::memory_order_acquire))
        return;
    RpcRequest *request = nullptr;
    for (size_t processed = 0;
         processed < rpcBudget && m_State->RpcQueue.Dequeue(request); ++processed) {
        BML_ImcFutureState state;
        {
            std::lock_guard lock(request->Future->Mutex);
            state = request->Future->State;
        }
        if (state == BML_IMC_FUTURE_PENDING) {
            BML_ImcMessage view = BML_IMC_MESSAGE_INIT;
            view.Data = request->Payload.Data();
            view.DataSize = request->Payload.Size();
            view.PayloadType = request->PayloadType;
            view.Flags = request->Flags;
            view.MessageId = request->MessageId;
            view.TimestampNs = request->Timestamp;
            m_State->Invoke(request->RpcId, view, request->Future,
                            request->Deadline);
        }
        m_State->ReleaseFutureRef(request->Future);
        m_State->RequestPool.Destroy(request);
    }

    const auto subscriptions = m_State->SnapshotSubscriptions();
    for (auto *subscription : subscriptions) {
        if (subscription->Execution != BML_IMC_EXECUTION_GAME_THREAD) {
            m_State->ReleaseSubscriptionRef(subscription);
            continue;
        }
        SharedMessage *message = nullptr;
        for (size_t processed = 0;
             processed < messageBudgetPerSubscription &&
             subscription->Queue->Dequeue(message); ++processed) {
            subscription->ReleaseQueueSlot();
            {
                BML_ImcMessage view = BML_IMC_MESSAGE_INIT;
                view.Data = message->Payload.Data();
                view.DataSize = message->Payload.Size();
                view.PayloadType = message->PayloadType;
                view.Flags = message->Flags;
                view.MessageId = message->MessageId;
                view.TimestampNs = message->Timestamp;
                try {
                    std::optional<ModInvocationGate::CallLock> invocation;
                    if (m_State->InvocationGate)
                        invocation.emplace(m_State->InvocationGate->LockCall());
                    auto operation = m_State->LockOperation();
                    if (subscription->Active.load(std::memory_order_acquire) &&
                        subscription->Owner->Active.load(std::memory_order_acquire))
                        subscription->Handler(subscription->Topic, &view,
                                              subscription->Userdata);
                } catch (...) {
                }
            }
            m_State->ReleaseMessageRef(message);
        }
        m_State->ReleaseSubscriptionRef(subscription);
    }

    CompletionItem *completion = nullptr;
    for (size_t processed = 0;
         processed < completionBudget &&
         m_State->CompletionQueue.Dequeue(completion); ++processed) {
        try {
            std::optional<ModInvocationGate::CallLock> invocation;
            if (m_State->InvocationGate)
                invocation.emplace(m_State->InvocationGate->LockCall());
            auto operation = m_State->LockOperation();
            if (completion->Owner->Active.load(std::memory_order_acquire))
                completion->Callback(completion->Future->Handle, completion->Userdata);
        } catch (...) {
        }
        m_State->ReleaseClientRef(completion->Owner);
        m_State->ReleaseFutureRef(completion->Future);
        m_State->CompletionPool.Destroy(completion);
    }
}

void ImcRuntime::CleanupOwner(const std::string &ownerId) {
    if (m_State->CallbackGate.IsCallActiveOnCurrentThread())
        return;
    auto mutation = m_State->CallbackGate.LockMutation();
    std::vector<BML_ImcClient> clients;
    {
        std::lock_guard lock(m_State->ClientMutex);
        for (const auto &[handle, client] : m_State->Clients) {
            (void)handle;
            if (client->Owner == ownerId) {
                m_State->AddClientRef(client);
                clients.push_back(client);
            }
        }
    }
    for (auto *client : clients) {
        client->Active.store(false, std::memory_order_release);
        m_State->RevokeClientRegistrations(client);

        std::vector<BML_ImcFuture_T *> futures;
        {
            std::lock_guard lock(m_State->FutureMutex);
            futures.reserve(m_State->Futures.size());
            for (const auto &[handle, future] : m_State->Futures) {
                (void) handle;
                m_State->AddFutureRef(future);
                futures.push_back(future);
            }
        }
        for (auto *future : futures) {
            BML_ImcClient detachedCallbackOwner = nullptr;
            {
                std::lock_guard lock(future->Mutex);
                if (future->CallbackOwner == client) {
                    detachedCallbackOwner = future->CallbackOwner;
                    future->CallbackOwner = nullptr;
                    future->Callback = nullptr;
                    future->CallbackUserdata = nullptr;
                }
            }
            if (detachedCallbackOwner)
                m_State->ReleaseClientRef(detachedCallbackOwner);

            if (future->Owner == client) {
                m_State->Complete(future, BML_IMC_FUTURE_CANCELLED,
                                  BML_ERROR_CANCELLED);
                if (future->PublicReference.exchange(false,
                        std::memory_order_acq_rel))
                    m_State->ReleaseFutureRef(future);
            }
            m_State->ReleaseFutureRef(future);
        }

        const bool hadPublicReference = client->PublicReference.exchange(
            false, std::memory_order_acq_rel);
        m_State->ReleaseClientRef(client); // cleanup snapshot
        if (hadPublicReference)
            m_State->ReleaseClientRef(client);
    }
}
void ImcRuntime::Shutdown() {
    if (!m_State || m_State->ShuttingDown.exchange(true, std::memory_order_acq_rel))
        return;
    auto mutation = m_State->CallbackGate.LockMutation();

    /* Shutdown is a terminal invalidation point. ModContext has already stopped
     * dispatch and unloaded mods, so no opaque handle may remain usable after
     * this pass. First detach every object that can retain a client/future, then
     * destroy the pool objects and finally any public client handles that were
     * not explicitly closed by their owner. */
    {
        std::lock_guard lock(m_State->ClientMutex);
        for (const auto &[handle, client] : m_State->Clients) {
            (void)handle;
            client->Active.store(false, std::memory_order_release);
        }
    }

    std::vector<BML_ImcClient> rpcOwners;
    {
        std::unique_lock lock(m_State->RpcMutex);
        rpcOwners.reserve(m_State->RpcHandlers.size());
        for (const auto &[id, entry] : m_State->RpcHandlers) {
            (void) id;
            rpcOwners.push_back(entry.Owner);
        }
        m_State->RpcHandlers.clear();
    }
    for (auto *owner : rpcOwners)
        m_State->ReleaseClientRef(owner);

    std::vector<BML_ImcSubscription> subscriptions;
    {
        std::unique_lock lock(m_State->SubscriptionMutex);
        subscriptions.reserve(m_State->Subscriptions.size());
        for (const auto &[handle, subscription] : m_State->Subscriptions) {
            (void)handle;
            subscription->Active.store(false, std::memory_order_release);
            subscriptions.push_back(subscription);
        }
        m_State->Subscriptions.clear();
    }
    for (auto *subscription : subscriptions)
        m_State->ReleaseSubscriptionRef(subscription);

    RpcRequest *request = nullptr;
    while (m_State->RpcQueue.Dequeue(request)) {
        m_State->Complete(request->Future, BML_IMC_FUTURE_FAILED, BML_ERROR_FROZEN);
        m_State->ReleaseFutureRef(request->Future);
        m_State->RequestPool.Destroy(request);
    }

    CompletionItem *completion = nullptr;
    while (m_State->CompletionQueue.Dequeue(completion)) {
        m_State->ReleaseClientRef(completion->Owner);
        m_State->ReleaseFutureRef(completion->Future);
        m_State->CompletionPool.Destroy(completion);
    }

    std::vector<BML_ImcFuture_T *> futures;
    {
        std::lock_guard lock(m_State->FutureMutex);
        futures.reserve(m_State->Futures.size());
        for (const auto &[handle, future] : m_State->Futures) {
            (void) handle;
            futures.push_back(future);
        }
        m_State->Futures.clear();
    }
    for (auto *future : futures) {
        if (future->CallbackOwner)
            m_State->ReleaseClientRef(future->CallbackOwner);
        m_State->ReleaseClientRef(future->Owner);
        m_State->FuturePool.Destroy(future);
    }

    std::vector<BML_ImcClient> clients;
    {
        std::lock_guard lock(m_State->ClientMutex);
        clients.reserve(m_State->Clients.size());
        for (const auto &[handle, client] : m_State->Clients) {
            (void)handle;
            clients.push_back(client);
        }
        m_State->Clients.clear();
    }
    for (auto *client : clients)
        delete client;
}

int ImcRuntime::GetTopicSubscriberCount(BML_ImcClient client, BML_ImcTopicId topicId,
                                        size_t *outCount) {
    if (outCount)
        *outCount = 0;
    auto *owned = m_State->AcquireClient(client);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (!outCount || topicId == BML_IMC_INVALID_ID) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    size_t count = 0;
    {
        std::shared_lock lock(m_State->SubscriptionMutex);
        for (const auto &[handle, subscription] : m_State->Subscriptions) {
            (void)handle;
            if (subscription->Topic == topicId && subscription->Active.load(std::memory_order_acquire))
                ++count;
        }
    }
    *outCount = count;
    m_State->ReleaseClientRef(owned);
    return BML_OK;
}
int ImcRuntime::GetStats(BML_ImcClient client, BML_ImcStats *outStats) {
    auto *owned = m_State->AcquireClient(client);
    if (!owned)
        return BML_ERROR_INVALID_HANDLE;
    if (!outStats || outStats->Size < sizeof(BML_ImcStats)) {
        m_State->ReleaseClientRef(owned);
        return BML_ERROR_INVALID_PARAMETER;
    }
    outStats->RpcCalls = m_State->RpcCalls.load(std::memory_order_relaxed);
    outStats->RpcCompleted = m_State->RpcCompleted.load(std::memory_order_relaxed);
    outStats->RpcFailed = m_State->RpcFailed.load(std::memory_order_relaxed);
    outStats->RpcQueueFull = m_State->RpcQueueFull.load(std::memory_order_relaxed);
    outStats->MessagesPublished = m_State->MessagesPublished.load(std::memory_order_relaxed);
    outStats->MessagesDelivered = m_State->MessagesDelivered.load(std::memory_order_relaxed);
    outStats->MessagesDropped = m_State->MessagesDropped.load(std::memory_order_relaxed);
    {
        std::shared_lock lock(m_State->RpcMutex);
        outStats->ActiveRpcHandlers = static_cast<uint32_t>(m_State->RpcHandlers.size());
    }
    {
        std::shared_lock lock(m_State->SubscriptionMutex);
        outStats->ActiveSubscriptions = static_cast<uint32_t>(m_State->Subscriptions.size());
    }
    outStats->PendingRpcCalls = static_cast<uint32_t>(m_State->RpcQueue.ApproximateSize());
    m_State->ReleaseClientRef(owned);
    return BML_OK;
}

} // namespace BML
