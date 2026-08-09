#include "ImcRuntime.h"
#include "ModInvocationGate.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <thread>
#include <utility>
#include <vector>

namespace {

int EchoRpc(BML_ImcRpcId, const BML_ImcMessage *request,
            BML_ImcResponse *response, void *) {
    return BML::ImcRuntime::ResponseWrite(response, request->Data,
                                           request->DataSize,
                                           request->PayloadType);
}

int CountingRpc(BML_ImcRpcId, const BML_ImcMessage *,
                BML_ImcResponse *, void *userdata) {
    static_cast<std::atomic<int> *>(userdata)->fetch_add(1,
                                                        std::memory_order_relaxed);
    return BML_OK;
}

struct RpcMetadataObservation {
    std::atomic<std::uint64_t> MessageId{0};
    std::atomic<std::uint64_t> TimestampNs{0};
};

int ObserveRpcMetadata(BML_ImcRpcId, const BML_ImcMessage *request,
                       BML_ImcResponse *, void *userdata) {
    auto *observation = static_cast<RpcMetadataObservation *>(userdata);
    observation->MessageId.store(request->MessageId, std::memory_order_relaxed);
    observation->TimestampNs.store(request->TimestampNs,
                                   std::memory_order_relaxed);
    return BML_OK;
}

int ReserveResponseWithoutCommit(BML_ImcRpcId, const BML_ImcMessage *,
                                 BML_ImcResponse *response, void *) {
    void *data = nullptr;
    const int status = BML::ImcRuntime::ResponseReserve(response, sizeof(uint32_t),
                                                        &data);
    if (status != BML_OK)
        return status;
    const uint32_t partial = 0xdeadbeefu;
    std::memcpy(data, &partial, sizeof(partial));
    return BML_OK;
}

void CountTopic(BML_ImcTopicId, const BML_ImcMessage *message, void *userdata) {
    auto *count = static_cast<std::atomic<int> *>(userdata);
    count->fetch_add(static_cast<int>(message->DataSize),
                     std::memory_order_relaxed);
}

void ThrowingTopic(BML_ImcTopicId, const BML_ImcMessage *, void *) {
    throw 1;
}

struct CompletionObservation {
    std::atomic<int> Calls{0};
    BML_ImcFuture Future = nullptr;
};

struct CompletionChain {
    BML::ImcRuntime *Runtime = nullptr;
    BML_ImcClient Client = nullptr;
    BML_ImcRpcId Rpc = BML_IMC_INVALID_ID;
    int Limit = 0;
    std::atomic<int> Calls{0};
    std::atomic<int> HandlerCalls{0};
    std::atomic<int> Errors{0};
};

void CountCompletion(BML_ImcFuture future, void *userdata) {
    auto *observation = static_cast<CompletionObservation *>(userdata);
    observation->Future = future;
    observation->Calls.fetch_add(1, std::memory_order_relaxed);
}

void ContinueCompletionChain(BML_ImcFuture future, void *userdata) {
    auto *chain = static_cast<CompletionChain *>(userdata);
    if (chain->Runtime->FutureRelease(future) != BML_OK)
        chain->Errors.fetch_add(1, std::memory_order_relaxed);
    const int completed = chain->Calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (completed >= chain->Limit)
        return;

    BML_ImcFuture next = nullptr;
    if (chain->Runtime->CallRpc(chain->Client, chain->Rpc, nullptr, nullptr,
                                &next) != BML_OK) {
        chain->Errors.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (chain->Runtime->FutureOnComplete(chain->Client, next,
                                         ContinueCompletionChain,
                                         chain) != BML_OK) {
        chain->Errors.fetch_add(1, std::memory_order_relaxed);
        (void)chain->Runtime->FutureRelease(next);
    }
}

class ImcRuntimeTest : public testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(m_Runtime.OpenClient("test.provider", &m_Provider), BML_OK);
        ASSERT_EQ(m_Runtime.OpenClient("test.consumer", &m_Consumer), BML_OK);
    }

    void TearDown() override {
        if (m_Consumer)
            m_Runtime.CloseClient(m_Consumer);
        if (m_Provider)
            m_Runtime.CloseClient(m_Provider);
    }

    BML::ImcRuntime m_Runtime;
    BML_ImcClient m_Provider = nullptr;
    BML_ImcClient m_Consumer = nullptr;
};

TEST_F(ImcRuntimeTest, ResolvesStableIdsAndRejectsInvalidNames) {
    BML_ImcRpcId providerId = 0;
    BML_ImcRpcId consumerId = 0;
    EXPECT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/echo", &providerId),
              BML_OK);
    EXPECT_EQ(m_Runtime.GetRpcId(m_Consumer, "sample/v1/rpc/echo", &consumerId),
              BML_OK);
    EXPECT_NE(providerId, BML_IMC_INVALID_ID);
    EXPECT_EQ(providerId, consumerId);
    EXPECT_EQ(m_Runtime.GetRpcId(m_Consumer, "", &consumerId),
              BML_ERROR_INVALID_PARAMETER);
}

TEST_F(ImcRuntimeTest, RpcAvailabilityTracksTheCurrentHandlerWithoutInvokingIt) {
    BML_ImcRpcId rpc = BML_IMC_INVALID_ID;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Consumer, "sample/v1/rpc/optional", &rpc),
              BML_OK);

    int available = -1;
    ASSERT_EQ(m_Runtime.IsRpcAvailable(m_Consumer, rpc, &available), BML_OK);
    EXPECT_EQ(available, 0);
    EXPECT_EQ(m_Runtime.IsRpcAvailable(m_Consumer, BML_IMC_INVALID_ID, &available),
              BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(m_Runtime.IsRpcAvailable(m_Consumer, rpc, nullptr),
              BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(m_Runtime.IsRpcAvailable(nullptr, rpc, &available),
              BML_ERROR_INVALID_HANDLE);

    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, nullptr, EchoRpc, nullptr),
              BML_OK);
    ASSERT_EQ(m_Runtime.IsRpcAvailable(m_Consumer, rpc, &available), BML_OK);
    EXPECT_EQ(available, 1);

    ASSERT_EQ(m_Runtime.UnregisterRpc(m_Provider, rpc), BML_OK);
    ASSERT_EQ(m_Runtime.IsRpcAvailable(m_Consumer, rpc, &available), BML_OK);
    EXPECT_EQ(available, 0);
}

TEST(ImcClientHandleTest, ClosedHandleNeverAliasesANewerClient) {
    BML::ImcRuntime runtime;
    BML_ImcClient stale = nullptr;
    ASSERT_EQ(runtime.OpenClient("stale.client", &stale), BML_OK);
    ASSERT_EQ(runtime.CloseClient(stale), BML_OK);

    for (int index = 0; index < 64; ++index) {
        BML_ImcClient live = nullptr;
        ASSERT_EQ(runtime.OpenClient("live.client", &live), BML_OK);
        ASSERT_NE(live, stale);
        BML_ImcRpcId id = BML_IMC_INVALID_ID;
        ASSERT_EQ(runtime.GetRpcId(stale, "sample/v1/rpc/stale-client", &id),
                  BML_ERROR_INVALID_HANDLE);
        EXPECT_EQ(runtime.CloseClient(live), BML_OK);
    }
}

TEST(ImcOpaqueHandleTokenTest, DoesNotAliasAcrossRuntimeLifetimes) {
    BML_ImcClient staleClient = nullptr;
    BML_ImcSubscription staleSubscription = nullptr;
    {
        BML::ImcRuntime runtime;
        BML_ImcClient provider = nullptr;
        ASSERT_EQ(runtime.OpenClient("first.provider", &provider), BML_OK);
        ASSERT_EQ(runtime.OpenClient("first.consumer", &staleClient), BML_OK);
        BML_ImcTopicId topic = BML_IMC_INVALID_ID;
        ASSERT_EQ(runtime.GetTopicId(provider, "sample/v1/topic/runtime-token", &topic),
                  BML_OK);
        std::atomic<int> delivered{0};
        ASSERT_EQ(runtime.Subscribe(staleClient, topic, nullptr, CountTopic,
                                    &delivered, &staleSubscription), BML_OK);
        ASSERT_EQ(runtime.Unsubscribe(staleClient, staleSubscription), BML_OK);
        ASSERT_EQ(runtime.CloseClient(staleClient), BML_OK);
        ASSERT_EQ(runtime.CloseClient(provider), BML_OK);
    }

    BML::ImcRuntime runtime;
    BML_ImcClient provider = nullptr;
    BML_ImcClient liveClient = nullptr;
    ASSERT_EQ(runtime.OpenClient("second.provider", &provider), BML_OK);
    ASSERT_EQ(runtime.OpenClient("second.consumer", &liveClient), BML_OK);
    EXPECT_NE(liveClient, staleClient);
    BML_ImcRpcId rpc = BML_IMC_INVALID_ID;
    EXPECT_EQ(runtime.GetRpcId(staleClient, "sample/v1/rpc/stale-runtime", &rpc),
              BML_ERROR_INVALID_HANDLE);
    std::uint64_t dropped = 0;
    EXPECT_EQ(runtime.GetSubscriptionDroppedCount(liveClient, staleSubscription,
                                                  &dropped),
              BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(runtime.CloseClient(liveClient), BML_OK);
    EXPECT_EQ(runtime.CloseClient(provider), BML_OK);
}

TEST_F(ImcRuntimeTest, CallerThreadRpcReturnsTypedPayloadWithoutQueueing) {
    BML_ImcRpcId rpc = 0;
    BML_ImcPayloadTypeId payloadType = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/echo", &rpc), BML_OK);
    ASSERT_EQ(m_Runtime.GetPayloadTypeId(m_Provider, "sample/v1/payload/bytes",
                                        &payloadType), BML_OK);
    BML_ImcRpcRegistrationOptions registration =
        BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    registration.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, &registration, EchoRpc, nullptr),
              BML_OK);

    const uint32_t value = 0x1234abcdu;
    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    request.Data = &value;
    request.DataSize = sizeof(value);
    request.PayloadType = payloadType;
    BML_ImcFuture future = nullptr;
    ASSERT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, &request, nullptr, &future), BML_OK);

    BML_ImcFutureState state = BML_IMC_FUTURE_PENDING;
    EXPECT_EQ(m_Runtime.FutureGetState(future, &state), BML_OK);
    EXPECT_EQ(state, BML_IMC_FUTURE_READY);
    BML_ImcMessage result = BML_IMC_MESSAGE_INIT;
    ASSERT_EQ(m_Runtime.FutureGetResult(future, &result), BML_OK);
    ASSERT_EQ(result.DataSize, sizeof(value));
    EXPECT_EQ(result.PayloadType, payloadType);
    EXPECT_EQ(std::memcmp(result.Data, &value, sizeof(value)), 0);
    EXPECT_EQ(m_Runtime.FutureRelease(future), BML_OK);
    EXPECT_EQ(m_Runtime.UnregisterRpc(m_Provider, rpc), BML_OK);
}

TEST_F(ImcRuntimeTest, UncommittedResponseBytesAreNotPublished) {
    BML_ImcRpcId rpc = BML_IMC_INVALID_ID;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider,
                                "sample/v1/rpc/uncommitted-response", &rpc),
              BML_OK);
    BML_ImcRpcRegistrationOptions registration =
        BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    registration.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, &registration,
                                    ReserveResponseWithoutCommit, nullptr),
              BML_OK);

    BML_ImcFuture future = nullptr;
    ASSERT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &future),
              BML_OK);
    BML_ImcMessage result = BML_IMC_MESSAGE_INIT;
    ASSERT_EQ(m_Runtime.FutureGetResult(future, &result), BML_OK);
    EXPECT_EQ(result.DataSize, 0u);
    EXPECT_EQ(result.PayloadType, BML_IMC_INVALID_ID);
    EXPECT_EQ(m_Runtime.FutureRelease(future), BML_OK);
}

TEST_F(ImcRuntimeTest, GameThreadRpcQueuesWorkersAndMainThreadNeverBlocks) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/queued", &rpc), BML_OK);
    std::atomic<int> calls{0};
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, nullptr, CountingRpc, &calls),
              BML_OK);

    BML_ImcFuture future = nullptr;
    std::thread worker([&] {
        EXPECT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &future),
                  BML_OK);
    });
    worker.join();
    ASSERT_NE(future, nullptr);
    EXPECT_EQ(m_Runtime.FutureAwait(future, 0), BML_ERROR_BUSY);
    EXPECT_EQ(m_Runtime.FutureAwait(future, 100), BML_ERROR_WRONG_THREAD);
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 0);
    m_Runtime.Pump();
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(m_Runtime.FutureAwait(future, 0), BML_OK);
    EXPECT_EQ(m_Runtime.FutureRelease(future), BML_OK);
}

TEST_F(ImcRuntimeTest, QueuedRpcPreservesCallerMessageMetadata) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/metadata", &rpc),
              BML_OK);
    RpcMetadataObservation observation;
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, nullptr, ObserveRpcMetadata,
                                    &observation), BML_OK);

    constexpr std::uint64_t MessageId = UINT64_C(0x123456789abcdef0);
    constexpr std::uint64_t Timestamp = UINT64_C(0x0fedcba987654321);
    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    request.MessageId = MessageId;
    request.TimestampNs = Timestamp;
    BML_ImcFuture future = nullptr;
    std::thread worker([&] {
        EXPECT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, &request, nullptr, &future),
                  BML_OK);
    });
    worker.join();

    m_Runtime.Pump();
    EXPECT_EQ(observation.MessageId.load(std::memory_order_relaxed), MessageId);
    EXPECT_EQ(observation.TimestampNs.load(std::memory_order_relaxed), Timestamp);
    EXPECT_EQ(m_Runtime.FutureRelease(future), BML_OK);
}

TEST_F(ImcRuntimeTest, ReleasedQueuedFutureIsImmediatelyInvalid) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/released", &rpc),
              BML_OK);
    std::atomic<int> calls{0};
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, nullptr, CountingRpc, &calls),
              BML_OK);

    BML_ImcFuture future = nullptr;
    std::thread worker([&] {
        EXPECT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &future),
                  BML_OK);
    });
    worker.join();
    ASSERT_NE(future, nullptr);
    ASSERT_EQ(m_Runtime.FutureRelease(future), BML_OK);
    BML_ImcFutureState state = BML_IMC_FUTURE_PENDING;
    EXPECT_EQ(m_Runtime.FutureGetState(future, &state), BML_ERROR_INVALID_HANDLE);
    m_Runtime.Pump();
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 1);
}

TEST_F(ImcRuntimeTest, RecycledFuturePoolSlotDoesNotReviveStaleHandle) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/future-generation", &rpc),
              BML_OK);
    BML_ImcRpcRegistrationOptions options = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    options.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    std::atomic<int> calls{0};
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, &options, CountingRpc, &calls),
              BML_OK);

    BML_ImcFuture stale = nullptr;
    ASSERT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &stale), BML_OK);
    ASSERT_EQ(m_Runtime.FutureRelease(stale), BML_OK);
    for (int index = 0; index < 4095; ++index) {
        BML_ImcFuture intermediate = nullptr;
        ASSERT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &intermediate),
                  BML_OK);
        ASSERT_EQ(m_Runtime.FutureRelease(intermediate), BML_OK);
    }
    BML_ImcFuture live = nullptr;
    ASSERT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &live), BML_OK);
    EXPECT_NE(live, stale);
    BML_ImcFutureState state = BML_IMC_FUTURE_PENDING;
    EXPECT_EQ(m_Runtime.FutureGetState(stale, &state), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(m_Runtime.FutureRelease(live), BML_OK);
}

TEST(ImcFutureTokenTest, DoesNotAliasAcrossRuntimeLifetimes) {
    BML_ImcFuture stale = nullptr;
    {
        BML::ImcRuntime runtime;
        BML_ImcClient provider = nullptr;
        BML_ImcClient consumer = nullptr;
        ASSERT_EQ(runtime.OpenClient("first.provider", &provider), BML_OK);
        ASSERT_EQ(runtime.OpenClient("first.consumer", &consumer), BML_OK);
        BML_ImcRpcId rpc = 0;
        ASSERT_EQ(runtime.GetRpcId(provider, "sample/v1/rpc/runtime-generation", &rpc),
                  BML_OK);
        BML_ImcRpcRegistrationOptions options = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
        options.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
        std::atomic<int> calls{0};
        ASSERT_EQ(runtime.RegisterRpc(provider, rpc, &options, CountingRpc, &calls),
                  BML_OK);
        ASSERT_EQ(runtime.CallRpc(consumer, rpc, nullptr, nullptr, &stale), BML_OK);
        ASSERT_EQ(runtime.FutureRelease(stale), BML_OK);
        ASSERT_EQ(runtime.CloseClient(consumer), BML_OK);
        ASSERT_EQ(runtime.CloseClient(provider), BML_OK);
    }

    BML::ImcRuntime runtime;
    BML_ImcClient provider = nullptr;
    BML_ImcClient consumer = nullptr;
    ASSERT_EQ(runtime.OpenClient("second.provider", &provider), BML_OK);
    ASSERT_EQ(runtime.OpenClient("second.consumer", &consumer), BML_OK);
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(runtime.GetRpcId(provider, "sample/v1/rpc/runtime-generation", &rpc),
              BML_OK);
    BML_ImcRpcRegistrationOptions options = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    options.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    std::atomic<int> calls{0};
    ASSERT_EQ(runtime.RegisterRpc(provider, rpc, &options, CountingRpc, &calls),
              BML_OK);
    BML_ImcFuture live = nullptr;
    ASSERT_EQ(runtime.CallRpc(consumer, rpc, nullptr, nullptr, &live), BML_OK);
    EXPECT_NE(live, stale);
    BML_ImcFutureState state = BML_IMC_FUTURE_PENDING;
    EXPECT_EQ(runtime.FutureGetState(stale, &state), BML_ERROR_INVALID_HANDLE);
    EXPECT_EQ(runtime.FutureRelease(live), BML_OK);
    EXPECT_EQ(runtime.CloseClient(consumer), BML_OK);
    EXPECT_EQ(runtime.CloseClient(provider), BML_OK);
}

TEST_F(ImcRuntimeTest, QueuedRpcCanBeCancelledOrExpireBeforeDispatch) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/cancellable", &rpc),
              BML_OK);
    std::atomic<int> calls{0};
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, nullptr, CountingRpc, &calls),
              BML_OK);

    BML_ImcFuture cancelled = nullptr;
    std::thread cancelWorker([&] {
        EXPECT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &cancelled),
                  BML_OK);
    });
    cancelWorker.join();
    ASSERT_EQ(m_Runtime.FutureCancel(cancelled), BML_OK);
    m_Runtime.Pump();
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(m_Runtime.FutureAwait(cancelled, 0), BML_ERROR_CANCELLED);
    EXPECT_EQ(m_Runtime.FutureRelease(cancelled), BML_OK);

    BML_ImcCallOptions immediateTimeout = BML_IMC_CALL_OPTIONS_INIT;
    immediateTimeout.TimeoutMs = 0;
    BML_ImcFuture expired = nullptr;
    std::thread timeoutWorker([&] {
        EXPECT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, &immediateTimeout,
                                   &expired), BML_OK);
    });
    timeoutWorker.join();
    m_Runtime.Pump();
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(m_Runtime.FutureAwait(expired, 0), BML_ERROR_TIMEOUT);
    EXPECT_EQ(m_Runtime.FutureRelease(expired), BML_OK);
}

TEST_F(ImcRuntimeTest, CompletionCallbacksRunFromPump) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/callback", &rpc),
              BML_OK);
    BML_ImcRpcRegistrationOptions registration =
        BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    registration.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, &registration, EchoRpc, nullptr),
              BML_OK);
    BML_ImcFuture future = nullptr;
    ASSERT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &future), BML_OK);
    CompletionObservation callbacks;
    ASSERT_EQ(m_Runtime.FutureOnComplete(m_Consumer, future, CountCompletion,
                                        &callbacks), BML_OK);
    EXPECT_EQ(callbacks.Calls.load(std::memory_order_relaxed), 0);
    m_Runtime.Pump();
    EXPECT_EQ(callbacks.Calls.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(callbacks.Future, future);
    EXPECT_EQ(m_Runtime.FutureRelease(future), BML_OK);
}

TEST_F(ImcRuntimeTest, PumpBoundsSelfReplenishingCompletionCallbacks) {
    BML_ImcRpcId rpc = BML_IMC_INVALID_ID;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider,
                                "sample/v1/rpc/completion-chain", &rpc),
              BML_OK);
    BML_ImcRpcRegistrationOptions registration =
        BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    registration.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    CompletionChain chain;
    chain.Runtime = &m_Runtime;
    chain.Client = m_Consumer;
    chain.Rpc = rpc;
    chain.Limit = 600;
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, &registration, CountingRpc,
                                    &chain.HandlerCalls), BML_OK);

    BML_ImcFuture first = nullptr;
    ASSERT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &first), BML_OK);
    ASSERT_EQ(m_Runtime.FutureOnComplete(m_Consumer, first,
                                        ContinueCompletionChain, &chain),
              BML_OK);

    m_Runtime.Pump();
    EXPECT_EQ(chain.Calls.load(std::memory_order_relaxed), 256);
    while (chain.Calls.load(std::memory_order_relaxed) < chain.Limit)
        m_Runtime.Pump();
    EXPECT_EQ(chain.Calls.load(std::memory_order_relaxed), chain.Limit);
    EXPECT_EQ(chain.HandlerCalls.load(std::memory_order_relaxed), chain.Limit);
    EXPECT_EQ(chain.Errors.load(std::memory_order_relaxed), 0);
}

TEST_F(ImcRuntimeTest, TopicBackpressureDropsOldestAndChecksPayloadType) {
    BML_ImcTopicId topic = 0;
    BML_ImcPayloadTypeId acceptedType = 0;
    BML_ImcPayloadTypeId rejectedType = 0;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider, "sample/v1/topic/updates", &topic),
              BML_OK);
    ASSERT_EQ(m_Runtime.GetPayloadTypeId(m_Provider, "sample/v1/payload/update",
                                        &acceptedType), BML_OK);
    ASSERT_EQ(m_Runtime.GetPayloadTypeId(m_Provider, "sample/v1/payload/other",
                                        &rejectedType), BML_OK);
    size_t subscribers = 99;
    ASSERT_EQ(m_Runtime.GetTopicSubscriberCount(m_Provider, topic, &subscribers), BML_OK);
    EXPECT_EQ(subscribers, 0u);
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Capacity = 2;
    options.ExpectedPayloadType = acceptedType;
    std::atomic<int> bytes{0};
    BML_ImcSubscription subscription = nullptr;
    ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, topic, &options, CountTopic, &bytes,
                                 &subscription), BML_OK);
    ASSERT_EQ(m_Runtime.GetTopicSubscriberCount(m_Provider, topic, &subscribers), BML_OK);
    EXPECT_EQ(subscribers, 1u);

    const uint8_t byte = 1;
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = &byte;
    message.DataSize = sizeof(byte);
    message.PayloadType = acceptedType;
    for (int i = 0; i < 3; ++i)
        EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
    message.PayloadType = rejectedType;
    EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
    m_Runtime.Pump();
    EXPECT_EQ(bytes.load(std::memory_order_relaxed), 2);

    BML_ImcStats stats{sizeof(BML_ImcStats)};
    ASSERT_EQ(m_Runtime.GetStats(m_Consumer, &stats), BML_OK);
    EXPECT_EQ(stats.MessagesPublished, 4u);
    EXPECT_GE(stats.MessagesDropped, 2u);
    uint64_t subscriptionDropped = 0;
    ASSERT_EQ(m_Runtime.GetSubscriptionDroppedCount(m_Consumer, subscription, &subscriptionDropped), BML_OK);
    EXPECT_EQ(subscriptionDropped, 2u);
    EXPECT_EQ(m_Runtime.Unsubscribe(m_Consumer, subscription), BML_OK);
    ASSERT_EQ(m_Runtime.GetTopicSubscriberCount(m_Provider, topic, &subscribers), BML_OK);
    EXPECT_EQ(subscribers, 0u);
}

TEST_F(ImcRuntimeTest, CallerThreadTopicReportsHandlerFailureAccurately) {
    BML_ImcTopicId topic = BML_IMC_INVALID_ID;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider,
                                  "sample/v1/topic/handler-failure", &topic),
              BML_OK);
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    BML_ImcSubscription subscription = nullptr;
    ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, topic, &options, ThrowingTopic,
                                  nullptr, &subscription), BML_OK);

    const std::uint8_t byte = 1;
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = &byte;
    message.DataSize = sizeof(byte);
    std::size_t delivered = 99;
    EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, &delivered),
              BML_ERROR_IMC_TARGET_EXECUTION_FAILED);
    EXPECT_EQ(delivered, 0u);
    std::uint64_t dropped = 0;
    ASSERT_EQ(m_Runtime.GetSubscriptionDroppedCount(m_Consumer, subscription,
                                                    &dropped), BML_OK);
    EXPECT_EQ(dropped, 1u);
    EXPECT_EQ(m_Runtime.Unsubscribe(m_Consumer, subscription), BML_OK);
}

TEST_F(ImcRuntimeTest, UnsubscribedHandleNeverAliasesANewerSubscription) {
    BML_ImcTopicId topic = BML_IMC_INVALID_ID;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider,
                                  "sample/v1/topic/stale-subscription", &topic),
              BML_OK);
    std::atomic<int> delivered{0};
    BML_ImcSubscription stale = nullptr;
    ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, topic, nullptr, CountTopic,
                                  &delivered, &stale), BML_OK);
    ASSERT_EQ(m_Runtime.Unsubscribe(m_Consumer, stale), BML_OK);

    for (int index = 0; index < 64; ++index) {
        BML_ImcSubscription live = nullptr;
        ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, topic, nullptr, CountTopic,
                                      &delivered, &live), BML_OK);
        ASSERT_NE(live, stale);
        std::uint64_t dropped = 0;
        ASSERT_EQ(m_Runtime.GetSubscriptionDroppedCount(m_Consumer, stale,
                                                        &dropped),
                  BML_ERROR_INVALID_HANDLE);
        EXPECT_EQ(m_Runtime.Unsubscribe(m_Consumer, live), BML_OK);
    }
}

TEST_F(ImcRuntimeTest, SubscribeValidatesBackpressureAndAcceptsCapacityOne) {
    BML_ImcTopicId topic = 0;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider, "sample/v1/topic/options", &topic),
              BML_OK);
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Backpressure = static_cast<BML_ImcBackpressure>(99);
    BML_ImcSubscription subscription = nullptr;
    EXPECT_EQ(m_Runtime.Subscribe(m_Consumer, topic, &options, CountTopic, nullptr,
                                 &subscription),
              BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(subscription, nullptr);

    std::atomic<int> bytes{0};
    options.Backpressure = BML_IMC_BACKPRESSURE_DROP_OLDEST;
    options.Capacity = 1;
    ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, topic, &options, CountTopic, &bytes,
                                 &subscription),
              BML_OK);
    const std::uint8_t byte = 1;
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = &byte;
    message.DataSize = sizeof(byte);
    EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
    EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
    std::uint64_t dropped = 0;
    EXPECT_EQ(m_Runtime.GetSubscriptionDroppedCount(m_Consumer, subscription,
                                                    &dropped), BML_OK);
    EXPECT_EQ(dropped, 1u);
    m_Runtime.Pump();
    EXPECT_EQ(bytes.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(m_Runtime.Unsubscribe(m_Consumer, subscription), BML_OK);
}

TEST_F(ImcRuntimeTest, NonPowerOfTwoCapacityIsAnExactLogicalBound) {
    BML_ImcTopicId topic = 0;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider, "sample/v1/topic/capacity-three",
                                  &topic), BML_OK);
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Capacity = 3;
    std::atomic<int> bytes{0};
    BML_ImcSubscription subscription = nullptr;
    ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, topic, &options, CountTopic, &bytes,
                                 &subscription), BML_OK);
    const std::uint8_t byte = 1;
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = &byte;
    message.DataSize = sizeof(byte);
    for (int index = 0; index < 4; ++index)
        EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
    std::uint64_t dropped = 0;
    EXPECT_EQ(m_Runtime.GetSubscriptionDroppedCount(m_Consumer, subscription,
                                                    &dropped), BML_OK);
    EXPECT_EQ(dropped, 1u);
    m_Runtime.Pump();
    EXPECT_EQ(bytes.load(std::memory_order_relaxed), 3);
    EXPECT_EQ(m_Runtime.Unsubscribe(m_Consumer, subscription), BML_OK);
}

TEST_F(ImcRuntimeTest, TopicWithoutSubscribersDoesNotConsumeMessagePool) {
    BML_ImcTopicId saturatedTopic = BML_IMC_INVALID_ID;
    BML_ImcTopicId emptyTopic = BML_IMC_INVALID_ID;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider,
                                  "sample/v1/topic/saturated-pool",
                                  &saturatedTopic), BML_OK);
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider,
                                  "sample/v1/topic/no-subscribers",
                                  &emptyTopic), BML_OK);
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Capacity = 4096;
    options.Backpressure = BML_IMC_BACKPRESSURE_DROP_NEWEST;
    std::atomic<int> deliveredBytes{0};
    BML_ImcSubscription subscription = nullptr;
    ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, saturatedTopic, &options,
                                  CountTopic, &deliveredBytes, &subscription),
              BML_OK);

    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    for (int index = 0; index < 4096; ++index) {
        const int status = m_Runtime.Publish(m_Provider, saturatedTopic,
                                             &message, nullptr);
        ASSERT_EQ(status, BML_OK) << "message index " << index;
    }

    std::size_t delivered = 99;
    EXPECT_EQ(m_Runtime.Publish(m_Provider, emptyTopic, &message, &delivered),
              BML_OK);
    EXPECT_EQ(delivered, 0u);
    EXPECT_EQ(m_Runtime.Unsubscribe(m_Consumer, subscription), BML_OK);
}

TEST_F(ImcRuntimeTest, TopicBackpressurePoliciesHaveDistinctOverflowResults) {
    BML_ImcTopicId topic = 0;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider, "sample/v1/topic/policies", &topic),
              BML_OK);
    const std::uint8_t byte = 1;
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = &byte;
    message.DataSize = sizeof(byte);

    for (const auto [policy, overflowStatus] : {
             std::pair{BML_IMC_BACKPRESSURE_DROP_NEWEST, BML_OK},
             std::pair{BML_IMC_BACKPRESSURE_FAIL, BML_ERROR_WOULD_BLOCK}}) {
        BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
        options.Capacity = 2;
        options.Backpressure = policy;
        std::atomic<int> bytes{0};
        BML_ImcSubscription subscription = nullptr;
        ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, topic, &options, CountTopic, &bytes,
                                     &subscription), BML_OK);
        EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
        EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
        EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr),
                  overflowStatus);
        std::uint64_t dropped = 0;
        EXPECT_EQ(m_Runtime.GetSubscriptionDroppedCount(m_Consumer, subscription,
                                                        &dropped), BML_OK);
        EXPECT_EQ(dropped, 1u);
        m_Runtime.Pump();
        EXPECT_EQ(bytes.load(std::memory_order_relaxed), 2);
        EXPECT_EQ(m_Runtime.Unsubscribe(m_Consumer, subscription), BML_OK);
    }
}

TEST_F(ImcRuntimeTest, ConcurrentDropOldestKeepsNewestAndAccountsEveryDrop) {
    BML_ImcTopicId topic = 0;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider, "sample/v1/topic/concurrent-drop", &topic),
              BML_OK);
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Capacity = 3;
    options.Backpressure = BML_IMC_BACKPRESSURE_DROP_OLDEST;
    std::atomic<int> bytes{0};
    BML_ImcSubscription subscription = nullptr;
    ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, topic, &options, CountTopic, &bytes,
                                 &subscription),
              BML_OK);

    const std::uint8_t byte = 1;
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = &byte;
    message.DataSize = sizeof(byte);
    ASSERT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
    ASSERT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
    ASSERT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);

    constexpr int ThreadCount = 8;
    constexpr int Iterations = 2000;
    std::atomic<bool> start{false};
    std::atomic<int> failures{0};
    std::vector<std::thread> publishers;
    publishers.reserve(ThreadCount);
    for (int thread = 0; thread < ThreadCount; ++thread) {
        publishers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (int index = 0; index < Iterations; ++index) {
                if (m_Runtime.Publish(m_Provider, topic, &message, nullptr) != BML_OK)
                    failures.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (auto &publisher : publishers) publisher.join();

    std::uint64_t dropped = 0;
    ASSERT_EQ(m_Runtime.GetSubscriptionDroppedCount(m_Consumer, subscription, &dropped),
              BML_OK);
    EXPECT_EQ(failures.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(dropped, static_cast<std::uint64_t>(ThreadCount * Iterations));
    EXPECT_EQ(m_Runtime.Unsubscribe(m_Consumer, subscription), BML_OK);
}

TEST_F(ImcRuntimeTest, ConcurrentPublishAndPumpPreserveEveryMessageOutcome) {
    BML_ImcTopicId topic = 0;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider, "sample/v1/topic/concurrent-pump",
                                  &topic), BML_OK);
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Capacity = 3;
    options.Backpressure = BML_IMC_BACKPRESSURE_DROP_OLDEST;
    std::atomic<int> delivered{0};
    BML_ImcSubscription subscription = nullptr;
    ASSERT_EQ(m_Runtime.Subscribe(m_Consumer, topic, &options, CountTopic,
                                 &delivered, &subscription), BML_OK);

    const std::uint8_t byte = 1;
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = &byte;
    message.DataSize = sizeof(byte);
    constexpr int ThreadCount = 4;
    constexpr int Iterations = 5000;
    std::atomic<bool> start{false};
    std::atomic<int> active{ThreadCount};
    std::atomic<int> failures{0};
    std::vector<std::thread> publishers;
    publishers.reserve(ThreadCount);
    for (int thread = 0; thread < ThreadCount; ++thread) {
        publishers.emplace_back([&] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (int index = 0; index < Iterations; ++index) {
                if (m_Runtime.Publish(m_Provider, topic, &message, nullptr) != BML_OK)
                    failures.fetch_add(1, std::memory_order_relaxed);
            }
            active.fetch_sub(1, std::memory_order_release);
        });
    }
    start.store(true, std::memory_order_release);
    while (active.load(std::memory_order_acquire) != 0)
        m_Runtime.Pump(0, 2);
    for (auto &publisher : publishers) publisher.join();
    m_Runtime.Pump(0, 16);

    std::uint64_t dropped = 0;
    ASSERT_EQ(m_Runtime.GetSubscriptionDroppedCount(m_Consumer, subscription,
                                                    &dropped), BML_OK);
    EXPECT_EQ(failures.load(std::memory_order_relaxed), 0);
    EXPECT_EQ(static_cast<std::uint64_t>(delivered.load(std::memory_order_relaxed)) +
                  dropped,
              static_cast<std::uint64_t>(ThreadCount * Iterations));
    EXPECT_EQ(m_Runtime.Unsubscribe(m_Consumer, subscription), BML_OK);
}

TEST_F(ImcRuntimeTest, CleanupOwnerRevokesProvidersBeforeDispatch) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/unload", &rpc), BML_OK);
    std::atomic<int> calls{0};
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, nullptr, CountingRpc, &calls),
              BML_OK);
    m_Runtime.CleanupOwner("test.provider");
    m_Provider = nullptr;

    BML_ImcFuture future = nullptr;
    EXPECT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &future),
              BML_ERROR_IMC_ENDPOINT_NOT_FOUND);
    EXPECT_EQ(future, nullptr);
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 0);
}

TEST_F(ImcRuntimeTest, CleanupOwnerReleasesQueuedConsumerHandlesSafely) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/consumer-unload", &rpc),
              BML_OK);
    std::atomic<int> calls{0};
    ASSERT_EQ(m_Runtime.RegisterRpc(m_Provider, rpc, nullptr, CountingRpc, &calls),
              BML_OK);

    BML_ImcFuture future = nullptr;
    std::thread worker([&] {
        EXPECT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &future),
                  BML_OK);
    });
    worker.join();
    ASSERT_NE(future, nullptr);
    m_Runtime.CleanupOwner("test.consumer");
    m_Consumer = nullptr;
    m_Runtime.Pump();
    EXPECT_EQ(calls.load(std::memory_order_relaxed), 0);
    BML_ImcFutureState state = BML_IMC_FUTURE_PENDING;
    EXPECT_EQ(m_Runtime.FutureGetState(future, &state), BML_ERROR_INVALID_HANDLE);
}

TEST(ImcRuntimeShutdownTest, InvalidatesLeakedPublicHandlesWithoutCrashing) {
    BML::ImcRuntime runtime;
    BML_ImcClient provider = nullptr;
    BML_ImcClient consumer = nullptr;
    ASSERT_EQ(runtime.OpenClient("shutdown.provider", &provider), BML_OK);
    ASSERT_EQ(runtime.OpenClient("shutdown.consumer", &consumer), BML_OK);
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(runtime.GetRpcId(provider, "sample/v1/rpc/shutdown", &rpc), BML_OK);
    BML_ImcRpcRegistrationOptions registration =
        BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    registration.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    ASSERT_EQ(runtime.RegisterRpc(provider, rpc, &registration, EchoRpc, nullptr),
              BML_OK);
    BML_ImcFuture future = nullptr;
    ASSERT_EQ(runtime.CallRpc(consumer, rpc, nullptr, nullptr, &future), BML_OK);
    runtime.Shutdown();
}

TEST_F(ImcRuntimeTest, CloseClientCompletesAfterReentrantImcCallback) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/reentrant-close", &rpc),
              BML_OK);
    struct CallbackState {
        BML::ImcRuntime *Runtime;
        BML_ImcClient Client;
        int CloseStatus = BML_OK;
    } state{&m_Runtime, m_Provider};
    BML_ImcRpcRegistrationOptions options = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    options.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    ASSERT_EQ(m_Runtime.RegisterRpc(
                  m_Provider, rpc, &options,
                  [](BML_ImcRpcId, const BML_ImcMessage *, BML_ImcResponse *, void *userdata) {
                      auto *callback = static_cast<CallbackState *>(userdata);
                      callback->CloseStatus =
                          callback->Runtime->CloseClient(callback->Client);
                      return BML_OK;
                  },
                  &state),
              BML_OK);

    BML_ImcFuture future = nullptr;
    ASSERT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &future), BML_OK);
    EXPECT_EQ(state.CloseStatus, BML_OK);
    EXPECT_EQ(m_Runtime.FutureRelease(future), BML_OK);
    int available = 1;
    EXPECT_EQ(m_Runtime.IsRpcAvailable(m_Consumer, rpc, &available), BML_OK);
    EXPECT_EQ(available, 0);
    m_Provider = nullptr;
}

TEST_F(ImcRuntimeTest, UnsubscribeCompletesAfterReentrantTopicCallback) {
    BML_ImcTopicId topic = 0;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider, "sample/v1/topic/reentrant-close",
                                   &topic),
              BML_OK);
    struct CallbackState {
        BML::ImcRuntime *Runtime;
        BML_ImcClient Client;
        BML_ImcSubscription Subscription = nullptr;
        int CloseStatus = BML_ERROR_FAIL;
        int Calls = 0;
    } state{&m_Runtime, m_Consumer};
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    ASSERT_EQ(m_Runtime.Subscribe(
                  m_Consumer, topic, &options,
                  [](BML_ImcTopicId, const BML_ImcMessage *, void *userdata) {
                      auto *callback = static_cast<CallbackState *>(userdata);
                      ++callback->Calls;
                      callback->CloseStatus = callback->Runtime->Unsubscribe(
                          callback->Client, callback->Subscription);
                  },
                  &state, &state.Subscription),
              BML_OK);

    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    std::size_t delivered = 0;
    ASSERT_EQ(m_Runtime.Publish(m_Provider, topic, &message, &delivered), BML_OK);
    EXPECT_EQ(state.CloseStatus, BML_OK);
    EXPECT_EQ(state.Calls, 1);
    EXPECT_EQ(delivered, 1u);

    delivered = 1;
    ASSERT_EQ(m_Runtime.Publish(m_Provider, topic, &message, &delivered), BML_OK);
    EXPECT_EQ(state.Calls, 1);
    EXPECT_EQ(delivered, 0u);
}

TEST_F(ImcRuntimeTest, UnregisterWaitsForConcurrentCallerThreadRpc) {
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(m_Runtime.GetRpcId(m_Provider, "sample/v1/rpc/blocking", &rpc), BML_OK);
    std::promise<void> entered;
    std::promise<void> release;
    std::shared_future<void> releaseFuture = release.get_future().share();
    struct CallbackState {
        std::promise<void> *Entered;
        std::shared_future<void> *Release;
    } state{&entered, &releaseFuture};
    BML_ImcRpcRegistrationOptions options = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    options.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    ASSERT_EQ(m_Runtime.RegisterRpc(
                  m_Provider, rpc, &options,
                  [](BML_ImcRpcId, const BML_ImcMessage *, BML_ImcResponse *, void *userdata) {
                      auto *callback = static_cast<CallbackState *>(userdata);
                      callback->Entered->set_value();
                      callback->Release->wait();
                      return BML_OK;
                  },
                  &state),
              BML_OK);

    BML_ImcFuture future = nullptr;
    std::thread caller([&] {
        EXPECT_EQ(m_Runtime.CallRpc(m_Consumer, rpc, nullptr, nullptr, &future), BML_OK);
    });
    entered.get_future().wait();
    auto unregister = std::async(std::launch::async, [&] {
        return m_Runtime.UnregisterRpc(m_Provider, rpc);
    });
    EXPECT_EQ(unregister.wait_for(std::chrono::milliseconds(20)),
              std::future_status::timeout);
    release.set_value();
    caller.join();
    EXPECT_EQ(unregister.get(), BML_OK);
    ASSERT_NE(future, nullptr);
    EXPECT_EQ(m_Runtime.FutureRelease(future), BML_OK);
}

TEST_F(ImcRuntimeTest, UnsubscribeWaitsForConcurrentCallerThreadTopic) {
    BML_ImcTopicId topic = 0;
    ASSERT_EQ(m_Runtime.GetTopicId(m_Provider, "sample/v1/topic/blocking", &topic), BML_OK);
    std::promise<void> entered;
    std::promise<void> release;
    std::shared_future<void> releaseFuture = release.get_future().share();
    struct CallbackState {
        std::promise<void> *Entered;
        std::shared_future<void> *Release;
    } state{&entered, &releaseFuture};
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    BML_ImcSubscription subscription = nullptr;
    ASSERT_EQ(m_Runtime.Subscribe(
                  m_Consumer, topic, &options,
                  [](BML_ImcTopicId, const BML_ImcMessage *, void *userdata) {
                      auto *callback = static_cast<CallbackState *>(userdata);
                      callback->Entered->set_value();
                      callback->Release->wait();
                  },
                  &state, &subscription),
              BML_OK);

    const std::uint8_t byte = 1;
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = &byte;
    message.DataSize = sizeof(byte);
    std::thread publisher([&] {
        EXPECT_EQ(m_Runtime.Publish(m_Provider, topic, &message, nullptr), BML_OK);
    });
    entered.get_future().wait();
    auto unsubscribe = std::async(std::launch::async, [&] {
        return m_Runtime.Unsubscribe(m_Consumer, subscription);
    });
    EXPECT_EQ(unsubscribe.wait_for(std::chrono::milliseconds(20)),
              std::future_status::timeout);
    release.set_value();
    publisher.join();
    EXPECT_EQ(unsubscribe.get(), BML_OK);
}

TEST(ImcPerformanceGate, DirectRpcMeetsReleaseThreshold) {
#ifndef NDEBUG
    GTEST_SKIP() << "Performance gate runs only in Release builds";
#else
    BML::ModInvocationGate invocationGate;
    BML::ImcRuntime runtime(&invocationGate);
    BML_ImcClient provider = nullptr;
    BML_ImcClient consumer = nullptr;
    ASSERT_EQ(runtime.OpenClient("perf.provider", &provider), BML_OK);
    ASSERT_EQ(runtime.OpenClient("perf.consumer", &consumer), BML_OK);
    BML_ImcRpcId rpc = 0;
    ASSERT_EQ(runtime.GetRpcId(provider, "perf/v1/rpc/noop", &rpc), BML_OK);
    BML_ImcRpcRegistrationOptions registration = BML_IMC_RPC_REGISTRATION_OPTIONS_INIT;
    registration.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    ASSERT_EQ(runtime.RegisterRpc(provider, rpc, &registration,
        [](BML_ImcRpcId, const BML_ImcMessage *, BML_ImcResponse *, void *) {
            return BML_OK;
        }, nullptr), BML_OK);

    auto invoke = [&] {
        BML_ImcFuture future = nullptr;
        return runtime.CallRpc(consumer, rpc, nullptr, nullptr, &future) == BML_OK &&
               runtime.FutureRelease(future) == BML_OK;
    };
    for (int i = 0; i < 10000; ++i)
        ASSERT_TRUE(invoke());

    constexpr int ThroughputIterations = 200000;
    const auto throughputStart = std::chrono::steady_clock::now();
    for (int i = 0; i < ThroughputIterations; ++i)
        ASSERT_TRUE(invoke());
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - throughputStart).count();
    const double callsPerSecond = ThroughputIterations / seconds;

    constexpr int LatencyIterations = 20000;
    std::vector<std::uint64_t> latencies;
    latencies.reserve(LatencyIterations);
    for (int i = 0; i < LatencyIterations; ++i) {
        const auto start = std::chrono::steady_clock::now();
        ASSERT_TRUE(invoke());
        latencies.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count()));
    }
    const auto percentile = latencies.begin() +
        static_cast<std::ptrdiff_t>(latencies.size() * 99 / 100);
    std::nth_element(latencies.begin(), percentile, latencies.end());
    const std::uint64_t p99Ns = *percentile;
    RecordProperty("calls_per_second", callsPerSecond);
    RecordProperty("p99_nanoseconds", p99Ns);
    EXPECT_GE(callsPerSecond, 1000000.0);
    EXPECT_LE(p99Ns, 5000u);
    EXPECT_EQ(runtime.UnregisterRpc(provider, rpc), BML_OK);
    EXPECT_EQ(runtime.CloseClient(consumer), BML_OK);
    EXPECT_EQ(runtime.CloseClient(provider), BML_OK);
#endif
}

TEST(ImcPerformanceGate, CallerThreadTopicMeetsReleaseThreshold) {
#ifndef NDEBUG
    GTEST_SKIP() << "Performance gate runs only in Release builds";
#else
    BML::ModInvocationGate invocationGate;
    BML::ImcRuntime runtime(&invocationGate);
    BML_ImcClient publisher = nullptr;
    BML_ImcClient consumer = nullptr;
    ASSERT_EQ(runtime.OpenClient("topic-perf.publisher", &publisher), BML_OK);
    ASSERT_EQ(runtime.OpenClient("topic-perf.consumer", &consumer), BML_OK);

    BML_ImcTopicId topic = 0;
    BML_ImcPayloadTypeId payloadType = 0;
    ASSERT_EQ(runtime.GetTopicId(publisher, "perf/v1/topic/noop", &topic), BML_OK);
    ASSERT_EQ(runtime.GetPayloadTypeId(publisher, "perf/v1/payload/noop", &payloadType),
              BML_OK);

    std::uint64_t deliveries = 0;
    BML_ImcSubscribeOptions options = BML_IMC_SUBSCRIBE_OPTIONS_INIT;
    options.Execution = BML_IMC_EXECUTION_CALLER_THREAD;
    options.ExpectedPayloadType = payloadType;
    BML_ImcSubscription subscription = nullptr;
    ASSERT_EQ(runtime.Subscribe(
                  consumer, topic, &options,
                  [](BML_ImcTopicId, const BML_ImcMessage *, void *userData) {
                      ++*static_cast<std::uint64_t *>(userData);
                  },
                  &deliveries, &subscription),
              BML_OK);

    const std::uint8_t byte = 1;
    BML_ImcMessage message = BML_IMC_MESSAGE_INIT;
    message.Data = &byte;
    message.DataSize = sizeof(byte);
    message.PayloadType = payloadType;
    auto publish = [&] {
        return runtime.Publish(publisher, topic, &message, nullptr) == BML_OK;
    };

    for (int i = 0; i < 10000; ++i)
        ASSERT_TRUE(publish());

    constexpr int ThroughputIterations = 200000;
    const auto throughputStart = std::chrono::steady_clock::now();
    for (int i = 0; i < ThroughputIterations; ++i)
        ASSERT_TRUE(publish());
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - throughputStart).count();
    const double messagesPerSecond = ThroughputIterations / seconds;

    constexpr int LatencyIterations = 20000;
    std::vector<std::uint64_t> latencies;
    latencies.reserve(LatencyIterations);
    for (int i = 0; i < LatencyIterations; ++i) {
        const auto start = std::chrono::steady_clock::now();
        ASSERT_TRUE(publish());
        latencies.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count()));
    }
    const auto percentile = latencies.begin() +
        static_cast<std::ptrdiff_t>(latencies.size() * 99 / 100);
    std::nth_element(latencies.begin(), percentile, latencies.end());
    const std::uint64_t p99Ns = *percentile;
    RecordProperty("messages_per_second", messagesPerSecond);
    RecordProperty("p99_nanoseconds", p99Ns);
    EXPECT_GE(messagesPerSecond, 500000.0);
    EXPECT_LE(p99Ns, 10000u);
    EXPECT_EQ(deliveries,
              static_cast<std::uint64_t>(10000 + ThroughputIterations +
                                         LatencyIterations));

    EXPECT_EQ(runtime.Unsubscribe(consumer, subscription), BML_OK);
    EXPECT_EQ(runtime.CloseClient(consumer), BML_OK);
    EXPECT_EQ(runtime.CloseClient(publisher), BML_OK);
#endif
}
} // namespace
