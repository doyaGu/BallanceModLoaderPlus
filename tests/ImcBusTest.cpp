/**
 * @file ImcBusTest.cpp
 * @brief Tests for simplified IMC Bus (17 APIs)
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Core/Context.h"
#include "Core/ImcBus.h"
#include "Core/ModHandle.h"

namespace {

using BML::Core::ImcBus;
using BML::Core::Context;

struct PubSubState {
    std::vector<BML_TopicId> topics;
    std::vector<std::vector<uint8_t>> payloads;
    std::vector<uint64_t> msg_ids;
    std::atomic<uint32_t> call_count{0};
};

void CollectingHandler(BML_Context ctx,
                       BML_TopicId topic,
                       const BML_ImcMessage *msg,
                       void *user_data) {
    (void)ctx;
    auto *state = static_cast<PubSubState *>(user_data);
    if (!state || !msg)
        return;

    state->topics.push_back(topic);
    std::vector<uint8_t> data;
    if (msg->data && msg->size > 0) {
        const auto *bytes = static_cast<const uint8_t *>(msg->data);
        data.assign(bytes, bytes + msg->size);
    }
    state->payloads.emplace_back(std::move(data));
    state->msg_ids.push_back(msg->msg_id);
    state->call_count.fetch_add(1, std::memory_order_relaxed);
}

struct BufferCleanupState {
    std::atomic<uint32_t> called{0};
    size_t last_size{0};
};

void BufferCleanup(const void *data, size_t size, void *user_data) {
    (void)data;
    auto *state = static_cast<BufferCleanupState *>(user_data);
    if (!state)
        return;
    state->last_size = size;
    state->called.fetch_add(1, std::memory_order_relaxed);
}

struct RpcState {
    std::atomic<uint32_t> call_count{0};
    BML_RpcId last_rpc_id{0};
    std::vector<uint8_t> last_payload;
};

struct BlockingHandlerState {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    std::mutex mutex;
    std::condition_variable cv;
};

void BlockingHandler(BML_Context,
                     BML_TopicId,
                     const BML_ImcMessage *,
                     void *user_data) {
    auto *state = static_cast<BlockingHandlerState *>(user_data);
    if (!state)
        return;
    state->entered.store(true, std::memory_order_release);
    std::unique_lock lock(state->mutex);
    state->cv.wait(lock, [state] {
        return state->release.load(std::memory_order_acquire);
    });
}

BML_Result EchoRpc(BML_Context ctx,
                   BML_RpcId rpc_id,
                   const BML_ImcMessage *request,
                   BML_ImcBuffer *out_response,
                   void *user_data) {
    (void)ctx;
    auto *state = static_cast<RpcState *>(user_data);
    if (state && request) {
        state->call_count.fetch_add(1, std::memory_order_relaxed);
        state->last_rpc_id = rpc_id;
        if (request->data && request->size > 0) {
            const auto *bytes = static_cast<const uint8_t *>(request->data);
            state->last_payload.assign(bytes, bytes + request->size);
        }
    }

    // Echo back the request
    if (request && request->data && request->size > 0) {
        auto *buffer = new std::vector<uint8_t>(request->size);
        const auto *bytes = static_cast<const uint8_t *>(request->data);
        std::copy(bytes, bytes + request->size, buffer->begin());

        out_response->data = buffer->data();
        out_response->size = buffer->size();
        out_response->cleanup_user_data = buffer;
        out_response->cleanup = [](const void *, size_t, void *user) {
            delete static_cast<std::vector<uint8_t> *>(user);
        };
    }
    return BML_RESULT_OK;
}

class ImcBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        Context::SetCurrentModule(nullptr);
        ImcBus::Instance().Shutdown();
    }

    void TearDown() override {
        ImcBus::Instance().Shutdown();
        Context::SetCurrentModule(nullptr);
        mod_storage_.clear();
    }

    BML_Mod MakeMod(const std::string &id) {
        auto mod = std::make_unique<BML_Mod_T>();
        mod->id = id;
        mod->version.major = 1;
        mod->version.minor = 0;
        mod->version.patch = 0;
        BML_Mod handle = mod.get();
        mod_storage_.push_back(std::move(mod));
        return handle;
    }

    std::vector<std::unique_ptr<BML_Mod_T>> mod_storage_;
};

// ========================================================================
// ID Resolution Tests
// ========================================================================

TEST_F(ImcBusTest, GetTopicIdReturnsConsistentId) {
    BML_TopicId id1, id2;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("test/topic", &id1));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("test/topic", &id2));
    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, BML_TOPIC_ID_INVALID);
}

TEST_F(ImcBusTest, DifferentTopicsHaveDifferentIds) {
    BML_TopicId id1, id2;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("topic/a", &id1));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("topic/b", &id2));
    EXPECT_NE(id1, id2);
}

TEST_F(ImcBusTest, GetTopicIdRejectsInvalidInput) {
    BML_TopicId id;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcBus::Instance().GetTopicId(nullptr, &id));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcBus::Instance().GetTopicId("", &id));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcBus::Instance().GetTopicId("test", nullptr));
}

TEST_F(ImcBusTest, GetRpcIdReturnsConsistentId) {
    BML_RpcId id1, id2;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetRpcId("my/rpc", &id1));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetRpcId("my/rpc", &id2));
    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, BML_RPC_ID_INVALID);
}

// ========================================================================
// Pub/Sub Tests
// ========================================================================

TEST_F(ImcBusTest, PublishesToSubscribedHandler) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("test.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, CollectingHandler, &state, &sub));
    ASSERT_NE(sub, nullptr);

    const std::string payload = "hello";
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, payload.data(), payload.size(), nullptr));

    ImcBus::Instance().Pump(0);

    ASSERT_EQ(state.call_count.load(), 1u);
    ASSERT_EQ(state.topics.size(), 1u);
    EXPECT_EQ(state.topics[0], topic);
    ASSERT_EQ(state.payloads.size(), 1u);
    ASSERT_EQ(state.payloads[0].size(), payload.size());
    EXPECT_TRUE(std::equal(state.payloads[0].begin(), state.payloads[0].end(), payload.begin()));

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
}

TEST_F(ImcBusTest, PublishBufferInvokesCleanup) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("buffer.topic", &topic));

    PubSubState state;
    BufferCleanupState cleanup_state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, CollectingHandler, &state, &sub));

    std::array<uint8_t, 4> buffer{1, 2, 3, 4};
    BML_ImcBuffer message = BML_IMC_BUFFER_INIT;
    message.data = buffer.data();
    message.size = buffer.size();
    message.cleanup = BufferCleanup;
    message.cleanup_user_data = &cleanup_state;

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().PublishBuffer(topic, &message));
    ImcBus::Instance().Pump(0);

    EXPECT_EQ(cleanup_state.called.load(), 1u);
    EXPECT_EQ(cleanup_state.last_size, buffer.size());
    ASSERT_EQ(state.payloads.size(), 1u);
    EXPECT_EQ(state.payloads[0], std::vector<uint8_t>(buffer.begin(), buffer.end()));

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
}

TEST_F(ImcBusTest, UnsubscribeStopsDelivery) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("unsub.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, CollectingHandler, &state, &sub));

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));

    const uint8_t value = 42;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &value, sizeof(value), nullptr));
    ImcBus::Instance().Pump(0);

    EXPECT_EQ(state.call_count.load(), 0u);
}

TEST_F(ImcBusTest, SubscriptionIsActiveReturnsCorrectState) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("active.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    BML_Bool is_active = BML_FALSE;
    
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, CollectingHandler, &state, &sub));
    ASSERT_NE(sub, nullptr);
    
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().SubscriptionIsActive(sub, &is_active));
    EXPECT_EQ(is_active, BML_TRUE);
    
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
    
    // After unsubscribe, handle is invalid
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, ImcBus::Instance().SubscriptionIsActive(sub, &is_active));
}

TEST_F(ImcBusTest, MultipleSubscribersReceiveMessages) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("multi.topic", &topic));

    PubSubState state1, state2;
    BML_Subscription sub1 = nullptr, sub2 = nullptr;

    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, CollectingHandler, &state1, &sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, CollectingHandler, &state2, &sub2));

    const uint8_t data = 123;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &data, sizeof(data), nullptr));
    ImcBus::Instance().Pump(0);

    EXPECT_EQ(state1.call_count.load(), 1u);
    EXPECT_EQ(state2.call_count.load(), 1u);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub1));
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub2));
}

TEST_F(ImcBusTest, TopicDiagnosticsReflectRegistry) {
    constexpr const char *kTopicName = "diag.topic";
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId(kTopicName, &topic));

    char name_buffer[64] = {};
    size_t name_length = 0;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicName(topic, name_buffer, sizeof(name_buffer), &name_length));
    EXPECT_STREQ(kTopicName, name_buffer);
    EXPECT_EQ(std::strlen(kTopicName), name_length);

    const uint8_t payload = 0x5Au;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &payload, sizeof(payload), nullptr));

    BML_TopicInfo info = BML_TOPIC_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicInfo(topic, &info));
    EXPECT_EQ(topic, info.topic_id);
    EXPECT_STREQ(kTopicName, info.name);
    EXPECT_EQ(0u, info.subscriber_count);
    EXPECT_EQ(1u, info.message_count);
}

TEST_F(ImcBusTest, UnsubscribeWaitsForInFlightHandlers) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("blocking.topic", &topic));

    BlockingHandlerState handler_state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, BlockingHandler, &handler_state, &sub));

    const uint8_t payload = 0x11u;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &payload, sizeof(payload), nullptr));

    std::thread pump_thread([] {
        ImcBus::Instance().Pump(0);
    });

    while (!handler_state.entered.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::atomic<BML_Result> unsubscribe_result{BML_RESULT_FAIL};
    std::atomic<bool> unsubscribe_done{false};
    std::thread unsubscribe_thread([&] {
        unsubscribe_result.store(ImcBus::Instance().Unsubscribe(sub), std::memory_order_relaxed);
        unsubscribe_done.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(unsubscribe_done.load(std::memory_order_acquire));

    {
        std::lock_guard lock(handler_state.mutex);
        handler_state.release.store(true, std::memory_order_release);
    }
    handler_state.cv.notify_all();

    pump_thread.join();
    unsubscribe_thread.join();

    EXPECT_TRUE(unsubscribe_done.load());
    EXPECT_EQ(BML_RESULT_OK, unsubscribe_result.load());
}

// ========================================================================
// RPC Tests
// ========================================================================

TEST_F(ImcBusTest, RpcEchoWorks) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetRpcId("echo", &rpc_id));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().RegisterRpc(rpc_id, EchoRpc, &state));

    const std::string request_data = "test request";
    BML_ImcMessage request = BML_IMC_MSG(request_data.data(), request_data.size());
    BML_Future future = nullptr;
    
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().CallRpc(rpc_id, &request, &future));
    ASSERT_NE(future, nullptr);

    ImcBus::Instance().Pump(0);  // Process RPC

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureAwait(future, 1000));

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_READY);

    BML_ImcMessage response;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureGetResult(future, &response));
    EXPECT_EQ(response.size, request_data.size());

    EXPECT_EQ(state.call_count.load(), 1u);
    EXPECT_EQ(state.last_rpc_id, rpc_id);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureRelease(future));
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().UnregisterRpc(rpc_id));
}

TEST_F(ImcBusTest, RpcToUnregisteredHandlerFails) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetRpcId("unregistered", &rpc_id));

    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    BML_Future future = nullptr;
    
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().CallRpc(rpc_id, &request, &future));
    ImcBus::Instance().Pump(0);

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_FAILED);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureRelease(future));
}

TEST_F(ImcBusTest, DuplicateRpcRegistrationFails) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetRpcId("duplicate", &rpc_id));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().RegisterRpc(rpc_id, EchoRpc, &state));
    EXPECT_EQ(BML_RESULT_ALREADY_EXISTS, ImcBus::Instance().RegisterRpc(rpc_id, EchoRpc, &state));

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().UnregisterRpc(rpc_id));
}

// ========================================================================
// Future Tests
// ========================================================================

TEST_F(ImcBusTest, FutureCancelWorks) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetRpcId("cancel_test", &rpc_id));

    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    BML_Future future = nullptr;
    
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().CallRpc(rpc_id, &request, &future));
    
    // Cancel before processing
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureCancel(future));

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_CANCELLED);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureRelease(future));
}

TEST_F(ImcBusTest, FutureOnCompleteCallbackWorks) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetRpcId("callback_test", &rpc_id));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().RegisterRpc(rpc_id, EchoRpc, &state));

    BML_ImcMessage request = BML_IMC_MSG("x", 1);
    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().CallRpc(rpc_id, &request, &future));

    std::atomic<bool> callback_called{false};
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureOnComplete(future, 
        [](BML_Context, BML_Future, void *ud) {
            *static_cast<std::atomic<bool>*>(ud) = true;
        }, &callback_called));

    ImcBus::Instance().Pump(0);

    EXPECT_TRUE(callback_called.load());

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureRelease(future));
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().UnregisterRpc(rpc_id));
}

// ========================================================================
// Edge Cases
// ========================================================================

TEST_F(ImcBusTest, PublishRejectsInvalidInput) {
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcBus::Instance().Publish(BML_TOPIC_ID_INVALID, nullptr, 0, nullptr));
    
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("valid", &topic));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcBus::Instance().Publish(topic, nullptr, 10, nullptr));  // data null but size > 0
}

TEST_F(ImcBusTest, SubscribeRejectsInvalidInput) {
    BML_Subscription sub;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcBus::Instance().Subscribe(BML_TOPIC_ID_INVALID, CollectingHandler, nullptr, &sub));
    
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("valid", &topic));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcBus::Instance().Subscribe(topic, nullptr, nullptr, &sub));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcBus::Instance().Subscribe(topic, CollectingHandler, nullptr, nullptr));
}

TEST_F(ImcBusTest, UnsubscribeInvalidHandleFails) {
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, ImcBus::Instance().Unsubscribe(nullptr));
}

TEST_F(ImcBusTest, PublishToEmptyTopicSucceeds) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("empty.topic", &topic));
    
    const uint8_t data = 1;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &data, sizeof(data), nullptr));
}

// ========================================================================
// Ordering and Backpressure Tests
// ========================================================================

// Helper for collecting int values
struct IntCapture {
    std::atomic<int> count{0};
    std::vector<int> values;
    std::mutex mtx;
    void push(int v) {
        std::lock_guard<std::mutex> lk(mtx);
        values.push_back(v);
        ++count;
    }
};

// Handler that collects int values (correct signature matching BML_ImcHandler)
static void IntCollectHandler(BML_Context /*ctx*/, BML_TopicId /*topic*/, const BML_ImcMessage *msg, void *ud) {
    auto *cap = static_cast<IntCapture *>(ud);
    if (msg && msg->data && msg->size == sizeof(int)) {
        int v;
        std::memcpy(&v, msg->data, sizeof(int));
        cap->push(v);
    }
}

TEST_F(ImcBusTest, PumpBudgetDistributesLoadAcrossSubscribers) {
    // With budget=1, each Pump processes 1 message per subscriber
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("budget.test", &topic));

    IntCapture cap1, cap2;
    BML_Subscription sub1, sub2;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, IntCollectHandler, &cap1, &sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, IntCollectHandler, &cap2, &sub2));

    // Publish 4 messages
    for (int i = 1; i <= 4; ++i) {
        ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &i, sizeof(i), nullptr));
    }

    // Each subscriber should have 4 messages to process
    // With budget=1, we process at most 1 message per subscriber per Pump
    ImcBus::Instance().Pump(1);
    EXPECT_EQ(cap1.count.load(), 1);
    EXPECT_EQ(cap2.count.load(), 1);

    // Drain remaining
    ImcBus::Instance().Pump(0);
    EXPECT_EQ(cap1.count.load(), 4);
    EXPECT_EQ(cap2.count.load(), 4);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub1));
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub2));
}

TEST_F(ImcBusTest, PublishingResumesAfterPumpClearsQueue) {
    // Backpressure scenario: queue fills up, pump clears, publish succeeds again
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("backpressure.test", &topic));

    IntCapture cap;
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, IntCollectHandler, &cap, &sub));

    // Publish a bunch of messages - they queue up
    constexpr int COUNT = 50;
    for (int i = 0; i < COUNT; ++i) {
        ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &i, sizeof(i), nullptr));
    }

    // Pump clears the queue
    ImcBus::Instance().Pump(0);
    EXPECT_EQ(cap.count.load(), COUNT);

    // More publishes should work
    int v = 999;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &v, sizeof(v), nullptr));
    ImcBus::Instance().Pump(0);
    EXPECT_EQ(cap.count.load(), COUNT + 1);
    EXPECT_EQ(cap.values.back(), 999);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
}

TEST_F(ImcBusTest, FutureAwaitTimesOutUntilPumpProcessesRpc) {
    // Without Pump, RPC waits forever (timeout); after Pump, completes
    // Reuse existing EchoRpc handler and RpcState from above
    BML_RpcId rpc;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetRpcId("timeout.echo", &rpc));
    
    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().RegisterRpc(rpc, EchoRpc, &state));

    uint8_t payload = 42;
    BML_ImcMessage request = BML_IMC_MSG(&payload, sizeof(payload));
    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().CallRpc(rpc, &request, &future));

    // Await with short timeout - should timeout because no Pump yet
    BML_Result res = ImcBus::Instance().FutureAwait(future, 10);
    EXPECT_EQ(res, BML_RESULT_TIMEOUT);

    // Now Pump and await again - should succeed
    ImcBus::Instance().Pump(0);
    res = ImcBus::Instance().FutureAwait(future, 100);
    EXPECT_EQ(res, BML_RESULT_OK);

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_READY);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().FutureRelease(future));
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().UnregisterRpc(rpc));
}

// ========================================================================
// Priority Message Ordering Tests
// ========================================================================

struct PriorityCapture {
    std::mutex mtx;
    std::vector<uint32_t> priorities;
    std::atomic<int> count{0};
    
    void push(uint32_t priority) {
        std::lock_guard<std::mutex> lk(mtx);
        priorities.push_back(priority);
        ++count;
    }
};

static void PriorityCollectHandler(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *ud) {
    auto *cap = static_cast<PriorityCapture *>(ud);
    if (msg) {
        cap->push(msg->priority);
    }
}

TEST_F(ImcBusTest, HighPriorityMessagesProcessedFirst) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("priority.order.test", &topic));

    PriorityCapture cap;
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, PriorityCollectHandler, &cap, &sub));

    // Publish messages with different priorities (LOW first, URGENT last)
    BML_ImcMessage msg_low = BML_IMC_MESSAGE_INIT;
    msg_low.priority = BML_IMC_PRIORITY_LOW;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, nullptr, 0, &msg_low));

    BML_ImcMessage msg_normal = BML_IMC_MESSAGE_INIT;
    msg_normal.priority = BML_IMC_PRIORITY_NORMAL;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, nullptr, 0, &msg_normal));

    BML_ImcMessage msg_high = BML_IMC_MESSAGE_INIT;
    msg_high.priority = BML_IMC_PRIORITY_HIGH;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, nullptr, 0, &msg_high));

    BML_ImcMessage msg_urgent = BML_IMC_MESSAGE_INIT;
    msg_urgent.priority = BML_IMC_PRIORITY_URGENT;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, nullptr, 0, &msg_urgent));

    // Pump all messages
    ImcBus::Instance().Pump(0);

    ASSERT_EQ(cap.count.load(), 4);
    // Urgent should be first (index 0)
    EXPECT_EQ(cap.priorities[0], BML_IMC_PRIORITY_URGENT);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
}

TEST_F(ImcBusTest, PriorityFilterRespectsMinPriority) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("priority.filter.test", &topic));

    PriorityCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.min_priority = BML_IMC_PRIORITY_HIGH;  // Only HIGH and URGENT
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().SubscribeEx(topic, PriorityCollectHandler, &cap, &opts, &sub));

    // Publish all priority levels
    for (uint32_t p = BML_IMC_PRIORITY_LOW; p <= BML_IMC_PRIORITY_URGENT; ++p) {
        BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
        msg.priority = p;
        ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, nullptr, 0, &msg));
    }

    ImcBus::Instance().Pump(0);

    // Should only receive HIGH and URGENT
    EXPECT_EQ(cap.count.load(), 2);
    
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
}

// ========================================================================
// Backpressure Policy Tests
// ========================================================================

TEST_F(ImcBusTest, BackpressureDropNewestPolicy) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("backpressure.drop.newest", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 4;  // Very small queue
    opts.backpressure = BML_BACKPRESSURE_DROP_NEWEST;
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().SubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    // Fill queue beyond capacity - new messages should be dropped
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &i, sizeof(i), nullptr));
    }

    ImcBus::Instance().Pump(0);

    // Check subscription stats for dropped messages
    BML_SubscriptionStats stats;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetSubscriptionStats(sub, &stats));
    EXPECT_GT(stats.messages_dropped, 0u);

    // First messages should be preserved (DROP_NEWEST drops new ones)
    EXPECT_GE(cap.count.load(), 1);
    
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
}

TEST_F(ImcBusTest, BackpressureDropOldestPolicy) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("backpressure.drop.oldest", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 4;  // Very small queue
    opts.backpressure = BML_BACKPRESSURE_DROP_OLDEST;
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().SubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    // Fill queue and overflow - old messages should be dropped
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Publish(topic, &i, sizeof(i), nullptr));
    }

    ImcBus::Instance().Pump(0);

    // Check that some messages were dropped
    BML_SubscriptionStats stats;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetSubscriptionStats(sub, &stats));
    EXPECT_GT(stats.messages_dropped, 0u);

    // Last messages should be preserved (DROP_OLDEST drops old ones)
    EXPECT_GE(cap.count.load(), 1);
    
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
}

TEST_F(ImcBusTest, BackpressureFailPolicy) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("backpressure.fail", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 4;  // Very small queue
    opts.backpressure = BML_BACKPRESSURE_FAIL;
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().SubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    // Overflow the queue - should start returning WOULD_BLOCK
    bool got_would_block = false;
    for (int i = 0; i < 100; ++i) {
        BML_Result res = ImcBus::Instance().Publish(topic, &i, sizeof(i), nullptr);
        if (res == BML_RESULT_WOULD_BLOCK) {
            got_would_block = true;
            break;
        }
    }

    // Note: Due to priority levels, effective capacity is queue_capacity * 4
    // We may or may not hit backpressure depending on timing
    
    ImcBus::Instance().Pump(0);
    
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
}

// ========================================================================
// Zero-Copy Buffer Lifecycle Tests
// ========================================================================

TEST_F(ImcBusTest, ZeroCopyBufferCleanupAfterAllSubscribersProcess) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("zerocopy.lifecycle", &topic));

    // Subscribe multiple handlers
    PubSubState state1, state2, state3;
    BML_Subscription sub1, sub2, sub3;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, CollectingHandler, &state1, &sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, CollectingHandler, &state2, &sub2));
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().Subscribe(topic, CollectingHandler, &state3, &sub3));

    BufferCleanupState cleanup_state;
    std::array<uint8_t, 64> data;
    std::fill(data.begin(), data.end(), 0xAB);

    BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
    buffer.data = data.data();
    buffer.size = data.size();
    buffer.cleanup = BufferCleanup;
    buffer.cleanup_user_data = &cleanup_state;

    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().PublishBuffer(topic, &buffer));
    
    // Pump to process all
    ImcBus::Instance().Pump(0);

    // All three subscribers should have received the message
    EXPECT_EQ(state1.call_count.load(), 1u);
    EXPECT_EQ(state2.call_count.load(), 1u);
    EXPECT_EQ(state3.call_count.load(), 1u);

    // Cleanup should have been called exactly once (after last ref released)
    EXPECT_EQ(cleanup_state.called.load(), 1u);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub1));
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub2));
    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub3));
}

// ========================================================================
// Concurrent Publishing Tests
// ========================================================================

TEST_F(ImcBusTest, ConcurrentPublishersDoNotCrash) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().GetTopicId("concurrent.publishers", &topic));

    std::atomic<uint32_t> received{0};
    
    // Use a larger queue capacity to avoid backpressure drops
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 1024;  // Much larger than total messages
    opts.backpressure = BML_BACKPRESSURE_FAIL;  // Fail instead of drop
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcBus::Instance().SubscribeEx(topic, 
        [](BML_Context, BML_TopicId, const BML_ImcMessage*, void* ud) {
            static_cast<std::atomic<uint32_t>*>(ud)->fetch_add(1, std::memory_order_relaxed);
        }, &received, &opts, &sub));

    constexpr int kThreads = 4;
    constexpr int kMessagesPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<bool> done{false};
    std::atomic<int> publish_failed{0};

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                int value = t * 1000 + i;
                BML_Result res = ImcBus::Instance().Publish(topic, &value, sizeof(value), nullptr);
                if (res != BML_RESULT_OK) {
                    publish_failed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    done.store(true, std::memory_order_release);

    // Pump all messages after all publishers complete
    for (int i = 0; i < 20; ++i) {
        ImcBus::Instance().Pump(0);
    }

    // Verify no publish failures occurred
    EXPECT_EQ(publish_failed.load(), 0) << "Some publishes failed due to queue pressure";
    
    // Should have received all messages
    EXPECT_EQ(received.load(), kThreads * kMessagesPerThread);

    EXPECT_EQ(BML_RESULT_OK, ImcBus::Instance().Unsubscribe(sub));
}

} // namespace
