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
#define private public
#include "Core/ImcBusInternal.h"
#undef private
#include "TestKernel.h"
#include "TestKernelBuilder.h"
#include "TestModHelper.h"

namespace {

using namespace BML::Core;
using BML::Core::Context;
using BML::Core::Testing::TestKernel;
using BML::Core::Testing::TestKernelBuilder;
using BML::Core::Testing::TestModHelper;

struct PubSubState {
    std::vector<BML_TopicId> topics;
    std::vector<std::vector<uint8_t>> payloads;
    std::vector<uint64_t> msg_ids;
    std::vector<uint64_t> timestamps;
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
    state->timestamps.push_back(msg->timestamp);
    state->call_count.fetch_add(1, std::memory_order_relaxed);
}

BML_Bool RejectAllFilter(const BML_ImcMessage *, void *) {
    return BML_FALSE;
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

struct ShutdownFutureCallbackState {
    KernelServices *kernel{nullptr};
    std::atomic<bool> called{false};
    std::atomic<uintptr_t> context_value{0};
    std::atomic<bool> kernel_visible{false};
    std::atomic<int> future_state{BML_FUTURE_PENDING};
    std::atomic<int> get_state_result{BML_RESULT_FAIL};
};

struct BlockingHandlerState {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    std::mutex mutex;
    std::condition_variable cv;
};

struct SelfUnsubscribeState {
    BML_Subscription sub{nullptr};
    std::atomic<uint32_t> call_count{0};
    std::atomic<BML_Result> unsubscribe_result{BML_RESULT_FAIL};
};

struct CurrentModuleCaptureState {
    std::atomic<uintptr_t> current_module{0};
    std::atomic<uint32_t> call_count{0};
};

struct DelayedRepublishState {
    BML_Mod owner{nullptr};
    BML_TopicId output_topic{BML_TOPIC_ID_INVALID};
    uint32_t payload{0};
    uint64_t trigger_timestamp{0};
};

struct DelayedPublishStateState {
    BML_Mod owner{nullptr};
    BML_TopicId retained_topic{BML_TOPIC_ID_INVALID};
    uint32_t payload{0};
    uint64_t trigger_timestamp{0};
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

void SelfUnsubscribeHandler(BML_Context,
                            BML_TopicId,
                            const BML_ImcMessage *,
                            void *user_data) {
    auto *state = static_cast<SelfUnsubscribeState *>(user_data);
    if (!state)
        return;

    state->call_count.fetch_add(1, std::memory_order_relaxed);
    state->unsubscribe_result.store(ImcUnsubscribe(state->sub), std::memory_order_relaxed);
}

void CaptureCurrentModuleHandler(BML_Context,
                                 BML_TopicId,
                                 const BML_ImcMessage *,
                                 void *user_data) {
    auto *state = static_cast<CurrentModuleCaptureState *>(user_data);
    if (!state)
        return;
    state->current_module.store(
        reinterpret_cast<uintptr_t>(Context::GetLifecycleModule()), std::memory_order_release);
    state->call_count.fetch_add(1, std::memory_order_relaxed);
}

void DelayedRepublishHandler(BML_Context,
                             BML_TopicId,
                             const BML_ImcMessage *message,
                             void *user_data) {
    auto *state = static_cast<DelayedRepublishState *>(user_data);
    if (!state) {
        return;
    }

    state->trigger_timestamp = message ? message->timestamp : 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const BML_Result result = ImcPublish(state->owner, state->output_topic,
                                         &state->payload, sizeof(state->payload));
    EXPECT_EQ(result, BML_RESULT_OK);
}

void DelayedPublishStateHandler(BML_Context,
                                BML_TopicId,
                                const BML_ImcMessage *message,
                                void *user_data) {
    auto *state = static_cast<DelayedPublishStateState *>(user_data);
    if (!state) {
        return;
    }

    state->trigger_timestamp = message ? message->timestamp : 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    BML_ImcMessage msg = BML_IMC_MSG(&state->payload, sizeof(state->payload));
    const BML_Result result = ImcPublishState(state->owner, state->retained_topic, &msg);
    EXPECT_EQ(result, BML_RESULT_OK);
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

BML_Result CaptureCurrentModuleRpc(BML_Context,
                                   BML_RpcId,
                                   const BML_ImcMessage *,
                                   BML_ImcBuffer *,
                                   void *user_data) {
    auto *state = static_cast<CurrentModuleCaptureState *>(user_data);
    if (!state)
        return BML_RESULT_INVALID_ARGUMENT;
    state->current_module.store(
        reinterpret_cast<uintptr_t>(Context::GetLifecycleModule()), std::memory_order_release);
    state->call_count.fetch_add(1, std::memory_order_relaxed);
    return BML_RESULT_OK;
}

BML_Result NoopStreamingRpc(BML_Context,
                            BML_RpcId,
                            const BML_ImcMessage *,
                            BML_RpcStream,
                            void *) {
    return BML_RESULT_OK;
}

void CaptureCurrentModuleFutureCallback(BML_Context,
                                        BML_Future,
                                        void *user_data) {
    auto *state = static_cast<CurrentModuleCaptureState *>(user_data);
    if (!state)
        return;
    state->current_module.store(
        reinterpret_cast<uintptr_t>(Context::GetLifecycleModule()), std::memory_order_release);
    state->call_count.fetch_add(1, std::memory_order_relaxed);
}

void ShutdownFutureCallback(BML_Context ctx, BML_Future future, void *user_data) {
    auto *state = static_cast<ShutdownFutureCallbackState *>(user_data);
    if (!state)
        return;

    BML_FutureState future_state = BML_FUTURE_PENDING;
    BML_Result result = ImcFutureGetState(future, &future_state);
    state->get_state_result.store(result, std::memory_order_relaxed);
    state->future_state.store(static_cast<int>(future_state), std::memory_order_relaxed);
    state->context_value.store(reinterpret_cast<uintptr_t>(ctx), std::memory_order_relaxed);
    state->kernel_visible.store(
        state->kernel && state->kernel->context && state->kernel->context->GetHandle() == ctx,
        std::memory_order_relaxed);
    state->called.store(true, std::memory_order_release);
}

class ImcBusTest : public ::testing::Test {
protected:
    TestKernel kernel_;
    std::unique_ptr<TestModHelper> mods_;

    void SetUp() override {
        kernel_ = TestKernelBuilder()
            .WithConfig()
            .WithImcBus()
            .WithDiagnostics()
            .Build();
        mods_ = std::make_unique<TestModHelper>(*kernel_);
        host_mod_ = mods_->HostMod();
        ASSERT_NE(host_mod_, nullptr);
        Context::SetLifecycleModule(host_mod_);
    }

    void TearDown() override {
        Context::SetLifecycleModule(nullptr);
    }

    BML_Mod CreateTrackedMod(const std::string &id) {
        return mods_->CreateMod(id);
    }

    BML_Result Subscribe(BML_TopicId topic,
                         BML_ImcHandler handler,
                         void *user_data,
                         BML_Subscription *out_sub) {
        return ImcSubscribe(host_mod_, topic, handler, user_data, out_sub);
    }

    BML_Result SubscribeEx(BML_TopicId topic,
                           BML_ImcHandler handler,
                           void *user_data,
                           const BML_SubscribeOptions *options,
                           BML_Subscription *out_sub) {
        return ImcSubscribeEx(host_mod_, topic, handler, user_data, options, out_sub);
    }

    BML_Result Publish(BML_TopicId topic, const void *data, size_t size) {
        return ImcPublish(host_mod_, topic, data, size);
    }

    BML_Result PublishEx(BML_TopicId topic, const BML_ImcMessage *msg) {
        return ImcPublishEx(host_mod_, topic, msg);
    }

    BML_Result PublishBuffer(BML_TopicId topic, const BML_ImcBuffer *buffer) {
        return ImcPublishBuffer(host_mod_, topic, buffer);
    }

    BML_Result PublishState(BML_TopicId topic, const BML_ImcMessage *msg) {
        return ImcPublishState(host_mod_, topic, msg);
    }

    BML_Result RegisterRpc(BML_RpcId rpc_id, BML_RpcHandler handler, void *user_data) {
        return ImcRegisterRpc(host_mod_, rpc_id, handler, user_data);
    }

    BML_Result UnregisterRpc(BML_RpcId rpc_id) {
        return ImcUnregisterRpc(host_mod_, rpc_id);
    }

    BML_Result CallRpc(BML_RpcId rpc_id, const BML_ImcMessage *request, BML_Future *out_future) {
        return ImcCallRpc(host_mod_, rpc_id, request, out_future);
    }

    BML_Result FutureOnComplete(BML_Future future, BML_FutureCallback callback, void *user_data) {
        return ImcFutureOnComplete(host_mod_, future, callback, user_data);
    }

    BML_Result GetTopicId(const char *name, BML_TopicId *outId) {
        return ImcGetTopicId(*kernel_, name, outId);
    }

    BML_Result GetRpcId(const char *name, BML_RpcId *outId) {
        return ImcGetRpcId(*kernel_, name, outId);
    }

    BML_Result GetTopicName(BML_TopicId topic, char *buffer, size_t capacity, size_t *outLength) {
        return ImcGetTopicName(*kernel_, topic, buffer, capacity, outLength);
    }

    BML_Result GetTopicInfo(BML_TopicId topic, BML_TopicInfo *outInfo) {
        return ImcGetTopicInfo(*kernel_, topic, outInfo);
    }

    BML_Result GetRpcInfo(BML_RpcId rpcId, BML_RpcInfo *outInfo) {
        return ImcGetRpcInfo(*kernel_, rpcId, outInfo);
    }

    BML_Result CopyState(BML_TopicId topic,
                         void *dst,
                         size_t dstSize,
                         size_t *outSize,
                         BML_ImcStateMeta *outMeta) {
        return ImcCopyState(*kernel_, topic, dst, dstSize, outSize, outMeta);
    }

    BML_Result ClearState(BML_TopicId topic) {
        return ImcClearState(*kernel_, topic);
    }

    void Pump(size_t maxPerSub = 0) {
        ImcPump(*kernel_, maxPerSub);
    }

    BML_Mod host_mod_{nullptr};
};

// ========================================================================
// ID Resolution Tests
// ========================================================================

TEST_F(ImcBusTest, GetTopicIdReturnsConsistentId) {
    BML_TopicId id1, id2;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("test/topic", &id1));
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("test/topic", &id2));
    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, BML_TOPIC_ID_INVALID);
}

TEST_F(ImcBusTest, DifferentTopicsHaveDifferentIds) {
    BML_TopicId id1, id2;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("topic/a", &id1));
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("topic/b", &id2));
    EXPECT_NE(id1, id2);
}

TEST_F(ImcBusTest, GetTopicIdRejectsInvalidInput) {
    BML_TopicId id;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, GetTopicId(nullptr, &id));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, GetTopicId("", &id));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, GetTopicId("test", nullptr));
}

TEST_F(ImcBusTest, GetRpcIdReturnsConsistentId) {
    BML_RpcId id1, id2;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("my/rpc", &id1));
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("my/rpc", &id2));
    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, BML_RPC_ID_INVALID);
}

// ========================================================================
// Pub/Sub Tests
// ========================================================================

TEST_F(ImcBusTest, PublishesToSubscribedHandler) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("test.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state, &sub));
    ASSERT_NE(sub, nullptr);

    const std::string payload = "hello";
    EXPECT_EQ(BML_RESULT_OK, Publish(topic, payload.data(), payload.size()));

    Pump(0);

    ASSERT_EQ(state.call_count.load(), 1u);
    ASSERT_EQ(state.topics.size(), 1u);
    EXPECT_EQ(state.topics[0], topic);
    ASSERT_EQ(state.payloads.size(), 1u);
    ASSERT_EQ(state.payloads[0].size(), payload.size());
    EXPECT_TRUE(std::equal(state.payloads[0].begin(), state.payloads[0].end(), payload.begin()));

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, PublishBufferInvokesCleanup) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("buffer.topic", &topic));

    PubSubState state;
    BufferCleanupState cleanup_state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state, &sub));

    std::array<uint8_t, 4> buffer{1, 2, 3, 4};
    BML_ImcBuffer message = BML_IMC_BUFFER_INIT;
    message.data = buffer.data();
    message.size = buffer.size();
    message.cleanup = BufferCleanup;
    message.cleanup_user_data = &cleanup_state;

    EXPECT_EQ(BML_RESULT_OK, PublishBuffer(topic, &message));
    Pump(0);

    EXPECT_EQ(cleanup_state.called.load(), 1u);
    EXPECT_EQ(cleanup_state.last_size, buffer.size());
    ASSERT_EQ(state.payloads.size(), 1u);
    EXPECT_EQ(state.payloads[0], std::vector<uint8_t>(buffer.begin(), buffer.end()));

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, PublishExPreservesExplicitTimestamp) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("timestamp.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state, &sub));

    constexpr uint64_t kTimestamp = 123456789ull;
    const int payload = 7;
    BML_ImcMessage msg = BML_IMC_MSG(&payload, sizeof(payload));
    msg.timestamp = kTimestamp;

    ASSERT_EQ(BML_RESULT_OK, PublishEx(topic, &msg));
    Pump(0);

    ASSERT_EQ(state.timestamps.size(), 1u);
    EXPECT_EQ(state.timestamps.front(), kTimestamp);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, HandlerPublishAssignsTimestampAtPublishTime) {
    BML_TopicId triggerTopic = BML_TOPIC_ID_INVALID;
    BML_TopicId outputTopic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("timestamp.handler.trigger", &triggerTopic));
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("timestamp.handler.output", &outputTopic));

    PubSubState outputState;
    BML_Subscription outputSub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(outputTopic, CollectingHandler, &outputState, &outputSub));

    DelayedRepublishState republishState;
    republishState.owner = host_mod_;
    republishState.output_topic = outputTopic;
    republishState.payload = 0xABCD1234u;

    BML_Subscription triggerSub = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              Subscribe(triggerTopic, DelayedRepublishHandler, &republishState, &triggerSub));

    const uint32_t triggerPayload = 1;
    ASSERT_EQ(BML_RESULT_OK, Publish(triggerTopic, &triggerPayload, sizeof(triggerPayload)));

    for (int i = 0; i < 4 && outputState.timestamps.empty(); ++i) {
        Pump(0);
    }

    ASSERT_EQ(outputState.timestamps.size(), 1u);
    ASSERT_NE(republishState.trigger_timestamp, 0u);
    EXPECT_GE(outputState.timestamps.front() - republishState.trigger_timestamp,
              10ull * 1000ull * 1000ull);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(triggerSub));
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(outputSub));
}

TEST_F(ImcBusTest, HandlerPublishStateAssignsTimestampAtPublishTime) {
    BML_TopicId triggerTopic = BML_TOPIC_ID_INVALID;
    BML_TopicId retainedTopic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("timestamp.state.trigger", &triggerTopic));
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("timestamp.state.retained", &retainedTopic));

    DelayedPublishStateState publishState;
    publishState.owner = host_mod_;
    publishState.retained_topic = retainedTopic;
    publishState.payload = 0x42u;

    BML_Subscription triggerSub = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              Subscribe(triggerTopic, DelayedPublishStateHandler, &publishState, &triggerSub));

    const uint32_t triggerPayload = 2;
    ASSERT_EQ(BML_RESULT_OK, Publish(triggerTopic, &triggerPayload, sizeof(triggerPayload)));

    Pump(0);

    uint32_t retainedPayload = 0;
    size_t copiedSize = 0;
    BML_ImcStateMeta meta = BML_IMC_STATE_META_INIT;
    ASSERT_EQ(BML_RESULT_OK, CopyState(retainedTopic, &retainedPayload, sizeof(retainedPayload),
                                       &copiedSize, &meta));
    ASSERT_EQ(copiedSize, sizeof(retainedPayload));
    EXPECT_EQ(retainedPayload, publishState.payload);
    ASSERT_NE(publishState.trigger_timestamp, 0u);
    EXPECT_GE(meta.timestamp - publishState.trigger_timestamp, 10ull * 1000ull * 1000ull);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(triggerSub));
}

TEST_F(ImcBusTest, RetainedStateDeliveryHonorsSubscriptionOptions) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("retained.options.topic", &topic));

    const int payload = 99;
    BML_ImcMessage stateMessage = BML_IMC_MSG(&payload, sizeof(payload));
    stateMessage.priority = BML_IMC_PRIORITY_LOW;
    ASSERT_EQ(BML_RESULT_OK, PublishState(topic, &stateMessage));

    BML_SubscribeOptions priorityOpts = BML_SUBSCRIBE_OPTIONS_INIT;
    priorityOpts.flags = BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE;
    priorityOpts.min_priority = BML_IMC_PRIORITY_HIGH;

    PubSubState priorityState;
    BML_Subscription prioritySub = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              SubscribeEx(topic, CollectingHandler, &priorityState, &priorityOpts, &prioritySub));
    EXPECT_EQ(priorityState.call_count.load(std::memory_order_relaxed), 0u);

    BML_SubscribeOptions filterOpts = BML_SUBSCRIBE_OPTIONS_INIT;
    filterOpts.flags = BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE;
    filterOpts.filter = RejectAllFilter;

    PubSubState filteredState;
    BML_Subscription filteredSub = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              SubscribeEx(topic, CollectingHandler, &filteredState, &filterOpts, &filteredSub));
    EXPECT_EQ(filteredState.call_count.load(std::memory_order_relaxed), 0u);

    BML_SubscribeOptions acceptOpts = BML_SUBSCRIBE_OPTIONS_INIT;
    acceptOpts.flags = BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE;

    PubSubState acceptedState;
    BML_Subscription acceptedSub = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              SubscribeEx(topic, CollectingHandler, &acceptedState, &acceptOpts, &acceptedSub));
    EXPECT_EQ(acceptedState.call_count.load(std::memory_order_relaxed), 1u);
    ASSERT_EQ(acceptedState.payloads.size(), 1u);
    ASSERT_EQ(acceptedState.payloads.front().size(), sizeof(payload));

    int received = 0;
    std::memcpy(&received, acceptedState.payloads.front().data(), sizeof(received));
    EXPECT_EQ(received, payload);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(prioritySub));
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(filteredSub));
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(acceptedSub));
}

TEST_F(ImcBusTest, RetainedStateCanBeCopiedAndCleared) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("retained.copy.clear", &topic));

    const char payload[] = "custom-map.cmo";
    BML_ImcMessage stateMessage = BML_IMC_MSG(payload, sizeof(payload));
    ASSERT_EQ(BML_RESULT_OK, PublishState(topic, &stateMessage));

    char buffer[64] = {};
    size_t copiedSize = 0;
    BML_ImcStateMeta meta = BML_IMC_STATE_META_INIT;
    ASSERT_EQ(BML_RESULT_OK, CopyState(topic, buffer, sizeof(buffer), &copiedSize, &meta));
    EXPECT_EQ(sizeof(payload), copiedSize);
    EXPECT_STREQ(payload, buffer);
    EXPECT_EQ(sizeof(payload), meta.size);

    ASSERT_EQ(BML_RESULT_OK, ClearState(topic));
    EXPECT_EQ(BML_RESULT_NOT_FOUND, CopyState(topic, buffer, sizeof(buffer), &copiedSize, &meta));
}

TEST_F(ImcBusTest, UnsubscribeStopsDelivery) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("unsub.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state, &sub));

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));

    const uint8_t value = 42;
    EXPECT_EQ(BML_RESULT_OK, Publish(topic, &value, sizeof(value)));
    Pump(0);

    EXPECT_EQ(state.call_count.load(), 0u);
}

TEST_F(ImcBusTest, SubscriptionIsActiveReturnsCorrectState) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("active.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    BML_Bool is_active = BML_FALSE;
    
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state, &sub));
    ASSERT_NE(sub, nullptr);
    
    EXPECT_EQ(BML_RESULT_OK, ImcSubscriptionIsActive(sub, &is_active));
    EXPECT_EQ(is_active, BML_TRUE);
    
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
    
    // After unsubscribe, handle is invalid
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, ImcSubscriptionIsActive(sub, &is_active));
}

TEST_F(ImcBusTest, MultipleSubscribersReceiveMessages) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("multi.topic", &topic));

    PubSubState state1, state2;
    BML_Subscription sub1 = nullptr, sub2 = nullptr;

    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state1, &sub1));
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state2, &sub2));

    const uint8_t data = 123;
    EXPECT_EQ(BML_RESULT_OK, Publish(topic, &data, sizeof(data)));
    Pump(0);

    EXPECT_EQ(state1.call_count.load(), 1u);
    EXPECT_EQ(state2.call_count.load(), 1u);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub1));
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub2));
}

TEST_F(ImcBusTest, TopicDiagnosticsReflectRegistry) {
    constexpr const char *kTopicName = "diag.topic";
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId(kTopicName, &topic));

    char name_buffer[64] = {};
    size_t name_length = 0;
    ASSERT_EQ(BML_RESULT_OK, GetTopicName(topic, name_buffer, sizeof(name_buffer), &name_length));
    EXPECT_STREQ(kTopicName, name_buffer);
    EXPECT_EQ(std::strlen(kTopicName), name_length);

    const uint8_t payload = 0x5Au;
    EXPECT_EQ(BML_RESULT_OK, Publish(topic, &payload, sizeof(payload)));

    BML_TopicInfo info = BML_TOPIC_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, GetTopicInfo(topic, &info));
    EXPECT_EQ(topic, info.topic_id);
    EXPECT_STREQ(kTopicName, info.name);
    EXPECT_EQ(0u, info.subscriber_count);
    EXPECT_EQ(1u, info.message_count);
}

TEST_F(ImcBusTest, UnsubscribeWaitsForInFlightHandlers) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("blocking.topic", &topic));

    BlockingHandlerState handler_state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, BlockingHandler, &handler_state, &sub));

    const uint8_t payload = 0x11u;
    EXPECT_EQ(BML_RESULT_OK, Publish(topic, &payload, sizeof(payload)));

    std::thread pump_thread([this] {
        Pump(0);
    });

    while (!handler_state.entered.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::atomic<BML_Result> unsubscribe_result{BML_RESULT_FAIL};
    std::atomic<bool> unsubscribe_done{false};
    std::thread unsubscribe_thread([&] {
        unsubscribe_result.store(ImcUnsubscribe(sub), std::memory_order_relaxed);
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

TEST_F(ImcBusTest, HandlerCanUnsubscribeItself) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("self.unsubscribe.topic", &topic));

    SelfUnsubscribeState state;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, SelfUnsubscribeHandler, &state, &state.sub));
    ASSERT_NE(state.sub, nullptr);

    const uint8_t payload = 0x22u;
    EXPECT_EQ(BML_RESULT_OK, Publish(topic, &payload, sizeof(payload)));

    Pump(0);

    EXPECT_EQ(1u, state.call_count.load(std::memory_order_relaxed));
    EXPECT_EQ(BML_RESULT_OK, state.unsubscribe_result.load(std::memory_order_relaxed));

    BML_Bool active = BML_TRUE;
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, ImcSubscriptionIsActive(state.sub, &active));

    EXPECT_EQ(BML_RESULT_OK, Publish(topic, &payload, sizeof(payload)));
    Pump(0);

    EXPECT_EQ(1u, state.call_count.load(std::memory_order_relaxed));
}

TEST_F(ImcBusTest, SubscribeRequiresBoundModule) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("missing.owner.subscription", &topic));

    Context::SetLifecycleModule(nullptr);

    PubSubState state;
    BML_Subscription sub = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcSubscribe(topic, CollectingHandler, &state, &sub));
    EXPECT_EQ(nullptr, sub);
}

TEST_F(ImcBusTest, PublishStateRequiresBoundModule) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("missing.owner.state", &topic));

    Context::SetLifecycleModule(nullptr);

    const uint32_t payload = 7;
    BML_ImcMessage state = BML_IMC_MSG(&payload, sizeof(payload));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcPublishState(topic, &state));
}

// ========================================================================
// RPC Tests
// ========================================================================

TEST_F(ImcBusTest, RpcEchoWorks) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("echo", &rpc_id));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, RegisterRpc(rpc_id, EchoRpc, &state));

    const std::string request_data = "test request";
    BML_ImcMessage request = BML_IMC_MSG(request_data.data(), request_data.size());
    BML_Future future = nullptr;
    
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc_id, &request, &future));
    ASSERT_NE(future, nullptr);

    Pump(0);  // Process RPC

    EXPECT_EQ(BML_RESULT_OK, ImcFutureAwait(future, 1000));

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcFutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_READY);

    BML_ImcMessage response;
    EXPECT_EQ(BML_RESULT_OK, ImcFutureGetResult(future, &response));
    EXPECT_EQ(response.size, request_data.size());

    EXPECT_EQ(state.call_count.load(), 1u);
    EXPECT_EQ(state.last_rpc_id, rpc_id);

    EXPECT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    EXPECT_EQ(BML_RESULT_OK, UnregisterRpc(rpc_id));
}

TEST_F(ImcBusTest, RpcToUnregisteredHandlerFails) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("unregistered", &rpc_id));

    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    BML_Future future = nullptr;
    
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc_id, &request, &future));
    Pump(0);

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcFutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_FAILED);

    EXPECT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
}

TEST_F(ImcBusTest, DuplicateRpcRegistrationFails) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("duplicate", &rpc_id));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, RegisterRpc(rpc_id, EchoRpc, &state));
    EXPECT_EQ(BML_RESULT_ALREADY_EXISTS, RegisterRpc(rpc_id, EchoRpc, &state));

    EXPECT_EQ(BML_RESULT_OK, UnregisterRpc(rpc_id));
}

TEST_F(ImcBusTest, RpcUnregisterRejectsNonOwner) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("rpc.owner.check", &rpc_id));

    auto owner_a = CreateTrackedMod("imc.rpc.owner.a");
    auto owner_b = CreateTrackedMod("imc.rpc.owner.b");

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(owner_a, rpc_id, EchoRpc, &state));
    EXPECT_EQ(BML_RESULT_PERMISSION_DENIED, ImcUnregisterRpc(owner_b, rpc_id));
    EXPECT_EQ(BML_RESULT_OK, ImcUnregisterRpc(owner_a, rpc_id));
}

// ========================================================================
// Future Tests
// ========================================================================

TEST_F(ImcBusTest, FutureCancelWorks) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("cancel_test", &rpc_id));

    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    BML_Future future = nullptr;
    
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc_id, &request, &future));
    
    // Cancel before processing
    EXPECT_EQ(BML_RESULT_OK, ImcFutureCancel(future));

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcFutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_CANCELLED);

    EXPECT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
}

TEST_F(ImcBusTest, FutureOnCompleteCallbackWorks) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("callback_test", &rpc_id));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, RegisterRpc(rpc_id, EchoRpc, &state));

    BML_ImcMessage request = BML_IMC_MSG("x", 1);
    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc_id, &request, &future));

    std::atomic<bool> callback_called{false};
    EXPECT_EQ(BML_RESULT_OK, FutureOnComplete(future,
        [](BML_Context, BML_Future, void *ud) {
            *static_cast<std::atomic<bool>*>(ud) = true;
        }, &callback_called));

    Pump(0);

    EXPECT_TRUE(callback_called.load());

    EXPECT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    EXPECT_EQ(BML_RESULT_OK, UnregisterRpc(rpc_id));
}

TEST_F(ImcBusTest, SubscriptionCallbackKeepsAmbientModuleContext) {
    BML_TopicId topic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("callback.owner.topic", &topic));

    auto owner = CreateTrackedMod("imc.callback.owner");
    CurrentModuleCaptureState state;
    BML_Subscription subscription = nullptr;

    ASSERT_EQ(BML_RESULT_OK,
              ImcSubscribe(owner, topic, CaptureCurrentModuleHandler, &state, &subscription));

    uint32_t payload = 7;
    ASSERT_EQ(BML_RESULT_OK, Publish(topic, &payload, sizeof(payload)));
    Pump(0);

    EXPECT_EQ(state.call_count.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(state.current_module.load(std::memory_order_acquire),
              reinterpret_cast<uintptr_t>(host_mod_));

    ASSERT_EQ(BML_RESULT_OK, ImcUnsubscribe(subscription));
}

TEST_F(ImcBusTest, RpcHandlerKeepsAmbientModuleContext) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("rpc.owner.context", &rpc_id));

    auto owner = CreateTrackedMod("imc.rpc.handler.owner");
    CurrentModuleCaptureState state;

    ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(owner, rpc_id, CaptureCurrentModuleRpc, &state));

    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc_id, nullptr, &future));
    Pump(0);

    EXPECT_EQ(state.call_count.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(state.current_module.load(std::memory_order_acquire),
              reinterpret_cast<uintptr_t>(host_mod_));

    ASSERT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    ASSERT_EQ(BML_RESULT_OK, ImcUnregisterRpc(owner, rpc_id));
}

TEST_F(ImcBusTest, FutureCompletionCallbackKeepsAmbientModuleContext) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("future.owner.context", &rpc_id));

    RpcState rpc_state;
    ASSERT_EQ(BML_RESULT_OK, RegisterRpc(rpc_id, EchoRpc, &rpc_state));

    auto owner = CreateTrackedMod("imc.future.callback.owner");
    CurrentModuleCaptureState state;
    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc_id, nullptr, &future));
    ASSERT_EQ(BML_RESULT_OK,
              ImcFutureOnComplete(owner, future, CaptureCurrentModuleFutureCallback, &state));

    Pump(0);

    EXPECT_EQ(state.call_count.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(state.current_module.load(std::memory_order_acquire),
              reinterpret_cast<uintptr_t>(host_mod_));

    ASSERT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    ASSERT_EQ(BML_RESULT_OK, UnregisterRpc(rpc_id));
}

TEST_F(ImcBusTest, SubscribeDoesNotRequireTlsBinding) {
    BML_TopicId topic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("owned.subscribe.topic", &topic));

    auto owner = CreateTrackedMod("imc.owned.subscribe.owner");
    CurrentModuleCaptureState state;
    BML_Subscription subscription = nullptr;

    Context::SetLifecycleModule(nullptr);
    ASSERT_EQ(BML_RESULT_OK,
              ImcSubscribe(owner, topic, CaptureCurrentModuleHandler, &state, &subscription));

    Context::SetLifecycleModule(host_mod_);
    uint32_t payload = 11;
    ASSERT_EQ(BML_RESULT_OK, Publish(topic, &payload, sizeof(payload)));
    Pump(0);

    EXPECT_EQ(state.call_count.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(state.current_module.load(std::memory_order_acquire),
              reinterpret_cast<uintptr_t>(host_mod_));

    Context::SetLifecycleModule(nullptr);
    ASSERT_EQ(BML_RESULT_OK, ImcUnsubscribe(subscription));
}

TEST_F(ImcBusTest, RpcRegistrationDoesNotRequireTlsBinding) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("owned.rpc.context", &rpc_id));

    auto owner = CreateTrackedMod("imc.owned.rpc.owner");
    CurrentModuleCaptureState state;

    Context::SetLifecycleModule(nullptr);
    ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(owner, rpc_id, CaptureCurrentModuleRpc, &state));

    Context::SetLifecycleModule(host_mod_);
    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc_id, nullptr, &future));
    Pump(0);

    EXPECT_EQ(state.call_count.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(state.current_module.load(std::memory_order_acquire),
              reinterpret_cast<uintptr_t>(host_mod_));

    ASSERT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    Context::SetLifecycleModule(nullptr);
    ASSERT_EQ(BML_RESULT_OK, ImcUnregisterRpc(owner, rpc_id));
}

TEST_F(ImcBusTest, FutureCallbackDoesNotRequireTlsBinding) {
    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("owned.future.context", &rpc_id));

    RpcState rpc_state;
    ASSERT_EQ(BML_RESULT_OK, RegisterRpc(rpc_id, EchoRpc, &rpc_state));

    auto owner = CreateTrackedMod("imc.owned.future.owner");
    CurrentModuleCaptureState state;
    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc_id, nullptr, &future));

    Context::SetLifecycleModule(nullptr);
    ASSERT_EQ(BML_RESULT_OK,
              ImcFutureOnComplete(owner, future, CaptureCurrentModuleFutureCallback, &state));

    Context::SetLifecycleModule(host_mod_);
    Pump(0);

    EXPECT_EQ(state.call_count.load(std::memory_order_acquire), 1u);
    EXPECT_EQ(state.current_module.load(std::memory_order_acquire),
              reinterpret_cast<uintptr_t>(host_mod_));

    ASSERT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    ASSERT_EQ(BML_RESULT_OK, UnregisterRpc(rpc_id));
}

TEST_F(ImcBusTest, RegistrationApisDoNotFallbackToTlsWhenOwnerIsNull) {
    auto owner = CreateTrackedMod("imc.owned.null.owner");
    Context::SetLifecycleModule(owner);

    BML_TopicId topic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("owned.null.owner.topic", &topic));

    CurrentModuleCaptureState state;
    BML_Subscription subscription = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT,
              ImcSubscribe(nullptr, topic, CaptureCurrentModuleHandler, &state, &subscription));

    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("owned.null.owner.rpc", &rpc_id));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcRegisterRpc(nullptr, rpc_id, CaptureCurrentModuleRpc, &state));

    RpcState rpc_state;
    ASSERT_EQ(BML_RESULT_OK, RegisterRpc(rpc_id, EchoRpc, &rpc_state));

    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc_id, nullptr, &future));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT,
              ImcFutureOnComplete(nullptr, future, CaptureCurrentModuleFutureCallback, &state));

    ASSERT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    ASSERT_EQ(BML_RESULT_OK, UnregisterRpc(rpc_id));
    Context::SetLifecycleModule(host_mod_);
}

TEST_F(ImcBusTest, ImcApisRejectNullOwnerWithoutAmbientFallback) {
    auto owner = CreateTrackedMod("imc.explicit.strict.owner");
    Context::SetLifecycleModule(owner);

    BML_TopicId topic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("imc.explicit.strict.topic", &topic));

    const uint32_t payload = 9;
    BML_ImcMessage message = BML_IMC_MSG(&payload, sizeof(payload));
    BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
    buffer.data = &payload;
    buffer.size = sizeof(payload);
    BML_EventResult event_result = BML_EVENT_CONTINUE;
    size_t delivered = 0;

    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcPublish(topic, &payload, sizeof(payload)));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcPublishEx(topic, &message));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcPublishBuffer(topic, &buffer));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcPublishInterceptable(topic, &message, &event_result));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT,
              ImcPublishMulti(&topic, 1, &payload, sizeof(payload), nullptr, &delivered));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcPublishState(topic, &message));

    BML_RpcId rpc_id = BML_RPC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("imc.explicit.strict.rpc", &rpc_id));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcRegisterRpc(rpc_id, EchoRpc, nullptr));

    BML_Future future = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcCallRpc(rpc_id, nullptr, &future));

    Context::SetLifecycleModule(host_mod_);
}

TEST(ImcBusLifetimeTest, KernelDestructionCompletesPendingFutureBeforeContextDestruction) {
    ShutdownFutureCallbackState callback_state;

    {
        TestKernel kernel = TestKernelBuilder()
            .WithConfig()
            .WithImcBus()
            .WithDiagnostics()
            .Build();
        callback_state.kernel = kernel.get();
        TestModHelper mods(*kernel);

        BML_Mod host_mod = mods.HostMod();
        ASSERT_NE(host_mod, nullptr);
        Context::SetLifecycleModule(host_mod);

        BML_RpcId rpc_id = BML_RPC_ID_INVALID;
        ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId(*kernel, "shutdown_test", &rpc_id));
        ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(host_mod, rpc_id, EchoRpc, nullptr));

        BML_Future future = nullptr;
        ASSERT_EQ(BML_RESULT_OK, ImcCallRpc(host_mod, rpc_id, nullptr, &future));
        ASSERT_NE(future, nullptr);
        ASSERT_EQ(BML_RESULT_OK,
                  ImcFutureOnComplete(host_mod, future, ShutdownFutureCallback, &callback_state));

        // Leave the queued request owning the last reference so shutdown can
        // complete and release the future while the callback still runs.
        ASSERT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
        Context::SetLifecycleModule(nullptr);
    }

    EXPECT_TRUE(callback_state.called.load(std::memory_order_acquire));
    EXPECT_EQ(callback_state.get_state_result.load(std::memory_order_relaxed), BML_RESULT_OK);
    EXPECT_EQ(callback_state.future_state.load(std::memory_order_relaxed),
              static_cast<int>(BML_FUTURE_CANCELLED));
    EXPECT_TRUE(callback_state.kernel_visible.load(std::memory_order_relaxed));
    EXPECT_NE(callback_state.context_value.load(std::memory_order_relaxed),
              static_cast<uintptr_t>(0));
}

TEST(ImcBusLifetimeTest, ShutdownWaitsForRetiredSubscriptionRefsToDrain) {
    ImcBusImpl bus;

    auto sub = std::make_unique<BML_Subscription_T>();
    auto *raw_sub = sub.get();
    raw_sub->closed.store(true, std::memory_order_release);
    raw_sub->ref_count.store(1, std::memory_order_release);
    bus.m_PublishState.retired_subscriptions.push_back(std::move(sub));

    std::atomic<bool> shutdown_finished{false};
    std::thread shutdown_thread([&] {
        bus.Shutdown();
        shutdown_finished.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(5200));
    EXPECT_FALSE(shutdown_finished.load(std::memory_order_acquire));

    raw_sub->ref_count.store(0, std::memory_order_release);
    shutdown_thread.join();
}

TEST(ImcBusLifetimeTest, ShutdownWaitsForRetiredSnapshotRefsToDrain) {
    ImcBusImpl bus;

    auto *snapshot = new TopicSnapshot();
    snapshot->ref_count.store(1, std::memory_order_release);
    bus.m_PublishState.retired_snapshots.push_back(snapshot);

    std::atomic<bool> shutdown_finished{false};
    std::thread shutdown_thread([&] {
        bus.Shutdown();
        shutdown_finished.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(5200));
    EXPECT_FALSE(shutdown_finished.load(std::memory_order_acquire));

    snapshot->ref_count.store(0, std::memory_order_release);
    shutdown_thread.join();
}

// ========================================================================
// Edge Cases
// ========================================================================

TEST_F(ImcBusTest, PublishRejectsInvalidInput) {
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcPublish(host_mod_, BML_TOPIC_ID_INVALID, nullptr, 0));

    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("valid", &topic));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcPublish(host_mod_, topic, nullptr, 10));  // data null but size > 0
}

TEST_F(ImcBusTest, SubscribeRejectsInvalidInput) {
    BML_Subscription sub;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcSubscribe(host_mod_, BML_TOPIC_ID_INVALID, CollectingHandler, nullptr, &sub));

    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("valid", &topic));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcSubscribe(host_mod_, topic, nullptr, nullptr, &sub));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcSubscribe(host_mod_, topic, CollectingHandler, nullptr, nullptr));
}

TEST_F(ImcBusTest, UnsubscribeInvalidHandleFails) {
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, ImcUnsubscribe(nullptr));
}

TEST_F(ImcBusTest, PublishToEmptyTopicSucceeds) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("empty.topic", &topic));
    
    const uint8_t data = 1;
    EXPECT_EQ(BML_RESULT_OK, Publish(topic, &data, sizeof(data)));
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
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("budget.test", &topic));

    IntCapture cap1, cap2;
    BML_Subscription sub1, sub2;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, IntCollectHandler, &cap1, &sub1));
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, IntCollectHandler, &cap2, &sub2));

    // Publish 4 messages
    for (int i = 1; i <= 4; ++i) {
        ASSERT_EQ(BML_RESULT_OK, Publish(topic, &i, sizeof(i)));
    }

    // Each subscriber should have 4 messages to process
    // With budget=1, we process at most 1 message per subscriber per Pump
    Pump(1);
    EXPECT_EQ(cap1.count.load(), 1);
    EXPECT_EQ(cap2.count.load(), 1);

    BML_SubscriptionStats stats1 = BML_SUBSCRIPTION_STATS_INIT;
    BML_SubscriptionStats stats2 = BML_SUBSCRIPTION_STATS_INIT;
    ASSERT_EQ(BML_RESULT_OK, ImcGetSubscriptionStats(sub1, &stats1));
    ASSERT_EQ(BML_RESULT_OK, ImcGetSubscriptionStats(sub2, &stats2));
    EXPECT_EQ(stats1.queue_size, 3u);
    EXPECT_EQ(stats2.queue_size, 3u);

    // Drain remaining
    Pump(0);
    EXPECT_EQ(cap1.count.load(), 4);
    EXPECT_EQ(cap2.count.load(), 4);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub1));
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub2));
}

TEST_F(ImcBusTest, PublishingResumesAfterPumpClearsQueue) {
    // Backpressure scenario: queue fills up, pump clears, publish succeeds again
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("backpressure.test", &topic));

    IntCapture cap;
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, IntCollectHandler, &cap, &sub));

    // Publish a bunch of messages - they queue up
    constexpr int COUNT = 50;
    for (int i = 0; i < COUNT; ++i) {
        ASSERT_EQ(BML_RESULT_OK, Publish(topic, &i, sizeof(i)));
    }

    // Pump clears the queue
    Pump(0);
    EXPECT_EQ(cap.count.load(), COUNT);

    // More publishes should work
    int v = 999;
    ASSERT_EQ(BML_RESULT_OK, Publish(topic, &v, sizeof(v)));
    Pump(0);
    EXPECT_EQ(cap.count.load(), COUNT + 1);
    EXPECT_EQ(cap.values.back(), 999);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, FutureAwaitRejectsMainThreadButWorkerThreadCanWait) {
    BML_RpcId rpc;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("timeout.echo", &rpc));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, RegisterRpc(rpc, EchoRpc, &state));

    uint8_t payload = 42;
    BML_ImcMessage request = BML_IMC_MSG(&payload, sizeof(payload));
    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, CallRpc(rpc, &request, &future));

    EXPECT_EQ(ImcFutureAwait(future, 10), BML_RESULT_WRONG_THREAD);

    std::atomic<BML_Result> worker_result{BML_RESULT_FAIL};
    std::thread waiter([&] {
        worker_result.store(ImcFutureAwait(future, 1000), std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Pump(0);
    waiter.join();

    EXPECT_EQ(worker_result.load(std::memory_order_acquire), BML_RESULT_OK);

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcFutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_READY);

    EXPECT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    EXPECT_EQ(BML_RESULT_OK, UnregisterRpc(rpc));
}

TEST_F(ImcBusTest, GetRpcInfoReportsRegisteredStreamingHandler) {
    BML_RpcId rpc = BML_RPC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetRpcId("streaming.rpc.info", &rpc));

    ASSERT_EQ(BML_RESULT_OK,
              ImcRegisterStreamingRpc(host_mod_, rpc, NoopStreamingRpc, nullptr));

    BML_RpcInfo info = BML_RPC_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, GetRpcInfo(rpc, &info));
    EXPECT_EQ(info.rpc_id, rpc);
    EXPECT_EQ(info.has_handler, BML_TRUE);
    EXPECT_STREQ(info.name, "streaming.rpc.info");

    EXPECT_EQ(BML_RESULT_OK, ImcUnregisterRpc(host_mod_, rpc));
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

struct LegacySubscribeOptions {
    size_t struct_size;
    uint32_t queue_capacity;
    BML_BackpressurePolicy backpressure;
    BML_ImcFilter filter;
    void *filter_user_data;
    uint32_t min_priority;
};

static_assert(offsetof(LegacySubscribeOptions, min_priority) ==
                  offsetof(BML_SubscribeOptions, min_priority),
              "Legacy subscribe options layout must match the old ABI prefix");

static void PriorityCollectHandler(BML_Context, BML_TopicId, const BML_ImcMessage *msg, void *ud) {
    auto *cap = static_cast<PriorityCapture *>(ud);
    if (msg) {
        cap->push(msg->priority);
    }
}

static BML_Bool EvenValueFilter(const BML_ImcMessage *msg, void *) {
    if (!msg || !msg->data || msg->size != sizeof(int)) {
        return BML_FALSE;
    }

    int value = 0;
    std::memcpy(&value, msg->data, sizeof(value));
    return (value % 2) == 0 ? BML_TRUE : BML_FALSE;
}

TEST_F(ImcBusTest, HighPriorityMessagesProcessedFirst) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("priority.order.test", &topic));

    PriorityCapture cap;
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, PriorityCollectHandler, &cap, &sub));

    // Publish messages with different priorities (LOW first, URGENT last)
    BML_ImcMessage msg_low = BML_IMC_MESSAGE_INIT;
    msg_low.priority = BML_IMC_PRIORITY_LOW;
    ASSERT_EQ(BML_RESULT_OK, PublishEx(topic, &msg_low));

    BML_ImcMessage msg_normal = BML_IMC_MESSAGE_INIT;
    msg_normal.priority = BML_IMC_PRIORITY_NORMAL;
    ASSERT_EQ(BML_RESULT_OK, PublishEx(topic, &msg_normal));

    BML_ImcMessage msg_high = BML_IMC_MESSAGE_INIT;
    msg_high.priority = BML_IMC_PRIORITY_HIGH;
    ASSERT_EQ(BML_RESULT_OK, PublishEx(topic, &msg_high));

    BML_ImcMessage msg_urgent = BML_IMC_MESSAGE_INIT;
    msg_urgent.priority = BML_IMC_PRIORITY_URGENT;
    ASSERT_EQ(BML_RESULT_OK, PublishEx(topic, &msg_urgent));

    // Pump all messages
    Pump(0);

    ASSERT_EQ(cap.count.load(), 4);
    // Urgent should be first (index 0)
    EXPECT_EQ(cap.priorities[0], BML_IMC_PRIORITY_URGENT);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, PriorityFilterRespectsMinPriority) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("priority.filter.test", &topic));

    PriorityCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.min_priority = BML_IMC_PRIORITY_HIGH;  // Only HIGH and URGENT
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, SubscribeEx(topic, PriorityCollectHandler, &cap, &opts, &sub));

    // Publish all priority levels
    for (uint32_t p = BML_IMC_PRIORITY_LOW; p <= BML_IMC_PRIORITY_URGENT; ++p) {
        BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
        msg.priority = p;
        ASSERT_EQ(BML_RESULT_OK, PublishEx(topic, &msg));
    }

    Pump(0);

    // Should only receive HIGH and URGENT
    EXPECT_EQ(cap.count.load(), 2);
    
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, MessageFilterSkipsRejectedMessages) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("message.filter.test", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.filter = EvenValueFilter;

    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, SubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    for (int value = 1; value <= 4; ++value) {
        ASSERT_EQ(BML_RESULT_OK, Publish(topic, &value, sizeof(value)));
    }

    Pump(0);

    ASSERT_EQ(2, cap.count.load());
    ASSERT_EQ(2u, cap.values.size());
    EXPECT_EQ(2, cap.values[0]);
    EXPECT_EQ(4, cap.values[1]);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, LegacySubscribeOptionsRemainCompatible) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("legacy.subscribe.options", &topic));

    PriorityCapture cap;
    LegacySubscribeOptions legacy_opts = {};
    legacy_opts.struct_size = sizeof(LegacySubscribeOptions);
    legacy_opts.queue_capacity = 16;
    legacy_opts.backpressure = BML_BACKPRESSURE_FAIL;
    legacy_opts.filter = nullptr;
    legacy_opts.filter_user_data = nullptr;
    legacy_opts.min_priority = BML_IMC_PRIORITY_HIGH;

    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK,
              SubscribeEx(
                  topic,
                  PriorityCollectHandler,
                  &cap,
                  reinterpret_cast<const BML_SubscribeOptions *>(&legacy_opts),
                  &sub));

    for (uint32_t priority = BML_IMC_PRIORITY_LOW; priority <= BML_IMC_PRIORITY_URGENT; ++priority) {
        BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
        msg.priority = priority;
        ASSERT_EQ(BML_RESULT_OK, PublishEx(topic, &msg));
    }

    Pump(0);

    ASSERT_EQ(2, cap.count.load());
    ASSERT_EQ(2u, cap.priorities.size());
    EXPECT_EQ(BML_IMC_PRIORITY_URGENT, cap.priorities[0]);
    EXPECT_EQ(BML_IMC_PRIORITY_HIGH, cap.priorities[1]);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

// ========================================================================
// Backpressure Policy Tests
// ========================================================================

TEST_F(ImcBusTest, BackpressureDropNewestPolicy) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("backpressure.drop.newest", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 4;  // Very small queue
    opts.backpressure = BML_BACKPRESSURE_DROP_NEWEST;
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, SubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    // Fill queue beyond capacity - new messages should be dropped
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(BML_RESULT_OK, Publish(topic, &i, sizeof(i)));
    }

    Pump(0);

    // Check subscription stats for dropped messages
    BML_SubscriptionStats stats;
    ASSERT_EQ(BML_RESULT_OK, ImcGetSubscriptionStats(sub, &stats));
    EXPECT_GT(stats.messages_dropped, 0u);

    EXPECT_EQ(cap.values.size(), 4u);
    EXPECT_EQ(cap.values[0], 0);
    EXPECT_EQ(cap.values[1], 1);
    EXPECT_EQ(cap.values[2], 2);
    EXPECT_EQ(cap.values[3], 3);
    
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, BackpressureDropOldestPolicy) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("backpressure.drop.oldest", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 4;  // Very small queue
    opts.backpressure = BML_BACKPRESSURE_DROP_OLDEST;
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, SubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    // Fill queue and overflow - old messages should be dropped
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(BML_RESULT_OK, Publish(topic, &i, sizeof(i)));
    }

    Pump(0);

    // Check that some messages were dropped
    BML_SubscriptionStats stats;
    ASSERT_EQ(BML_RESULT_OK, ImcGetSubscriptionStats(sub, &stats));
    EXPECT_GT(stats.messages_dropped, 0u);

    EXPECT_EQ(cap.values.size(), 4u);
    EXPECT_EQ(cap.values[0], 16);
    EXPECT_EQ(cap.values[1], 17);
    EXPECT_EQ(cap.values[2], 18);
    EXPECT_EQ(cap.values[3], 19);
    
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, BackpressureFailPolicy) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("backpressure.fail", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 4;  // Very small queue
    opts.backpressure = BML_BACKPRESSURE_FAIL;
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, SubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    // Overflow the queue - should start returning WOULD_BLOCK
    bool got_would_block = false;
    for (int i = 0; i < 100; ++i) {
        BML_Result res = Publish(topic, &i, sizeof(i));
        if (res == BML_RESULT_WOULD_BLOCK) {
            got_would_block = true;
            break;
        }
    }

    EXPECT_TRUE(got_would_block);

    Pump(0);

    BML_SubscriptionStats stats;
    ASSERT_EQ(BML_RESULT_OK, ImcGetSubscriptionStats(sub, &stats));
    EXPECT_EQ(stats.messages_dropped, 0u);
    EXPECT_LE(cap.values.size(), 4u);
    
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

// ========================================================================
// Zero-Copy Buffer Lifecycle Tests
// ========================================================================

TEST_F(ImcBusTest, ZeroCopyBufferCleanupAfterAllSubscribersProcess) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("zerocopy.lifecycle", &topic));

    // Subscribe multiple handlers
    PubSubState state1, state2, state3;
    BML_Subscription sub1, sub2, sub3;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state1, &sub1));
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state2, &sub2));
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, CollectingHandler, &state3, &sub3));

    BufferCleanupState cleanup_state;
    std::array<uint8_t, 64> data;
    std::fill(data.begin(), data.end(), 0xAB);

    BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
    buffer.data = data.data();
    buffer.size = data.size();
    buffer.cleanup = BufferCleanup;
    buffer.cleanup_user_data = &cleanup_state;

    ASSERT_EQ(BML_RESULT_OK, PublishBuffer(topic, &buffer));
    
    // Pump to process all
    Pump(0);

    // All three subscribers should have received the message
    EXPECT_EQ(state1.call_count.load(), 1u);
    EXPECT_EQ(state2.call_count.load(), 1u);
    EXPECT_EQ(state3.call_count.load(), 1u);

    // Cleanup should have been called exactly once (after last ref released)
    EXPECT_EQ(cleanup_state.called.load(), 1u);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub1));
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub2));
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub3));
}

// ========================================================================
// Concurrent Publishing Tests
// ========================================================================

TEST_F(ImcBusTest, ConcurrentPublishersDoNotCrash) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("concurrent.publishers", &topic));

    std::atomic<uint32_t> received{0};
    
    // Use a larger queue capacity to avoid backpressure drops
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 1024;  // Much larger than total messages
    opts.backpressure = BML_BACKPRESSURE_FAIL;  // Fail instead of drop
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, SubscribeEx(topic,
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
                BML_Result res = Publish(topic, &value, sizeof(value));
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
        Pump(0);
    }

    // Verify no publish failures occurred
    EXPECT_EQ(publish_failed.load(), 0) << "Some publishes failed due to queue pressure";
    
    // Should have received all messages
    EXPECT_EQ(received.load(), kThreads * kMessagesPerThread);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

// ========================================================================
// RCU Snapshot Concurrency Stress Test
// ========================================================================

TEST_F(ImcBusTest, ConcurrentPublishWithSubscribeUnsubscribeRace) {
    // Stress test: multiple publisher threads race against subscribe/unsubscribe
    // to verify the RCU snapshot mechanism handles concurrent mutation safely.
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("stress.rcu.race", &topic));

    std::atomic<uint32_t> total_received{0};
    std::atomic<bool> stop{false};

    auto counting_handler = [](BML_Context, BML_TopicId, const BML_ImcMessage *, void *ud) {
        static_cast<std::atomic<uint32_t> *>(ud)->fetch_add(1, std::memory_order_relaxed);
    };

    // Start with one subscription
    BML_Subscription initial_sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, Subscribe(topic, counting_handler, &total_received, &initial_sub));

    constexpr int kPublishThreads = 3;
    constexpr int kMessagesPerThread = 200;
    std::vector<std::thread> threads;

    // Publisher threads: continuously publish while subs change
    for (int t = 0; t < kPublishThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                int value = t * 1000 + i;
                Publish(topic, &value, sizeof(value));
            }
        });
    }

    // Subscribe/unsubscribe churn thread
    threads.emplace_back([&]() {
        for (int cycle = 0; cycle < 20 && !stop.load(std::memory_order_relaxed); ++cycle) {
            BML_Subscription sub = nullptr;
            BML_Result res = Subscribe(topic, counting_handler, &total_received, &sub);
            if (res == BML_RESULT_OK && sub) {
                std::this_thread::yield();
                ImcUnsubscribe(sub);
            }
        }
    });

    // Pump thread: drain messages concurrently
    threads.emplace_back([&]() {
        for (int i = 0; i < 50 && !stop.load(std::memory_order_relaxed); ++i) {
            Pump(0);
            std::this_thread::yield();
        }
    });

    for (auto &t : threads) {
        t.join();
    }
    stop.store(true, std::memory_order_release);

    // Final drain
    for (int i = 0; i < 10; ++i) {
        Pump(0);
    }

    // The initial subscriber should have received some messages
    EXPECT_GT(total_received.load(), 0u);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(initial_sub));
}

// ============================================================================
// Typed Message Tests
// ============================================================================

TEST_F(ImcBusTest, PublishRejectsTypeMismatch) {
    // Create a topic and set an expected type on it
    BML_TopicId topic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("Test/Typed", &topic));
    ASSERT_NE(BML_TOPIC_ID_INVALID, topic);

    // Access the internal bus to set expected type
    auto &bus = kernel_->imc_bus->GetImpl();
    bus.m_PublishState.topic_registry.SetExpectedTypeId(topic, 0xDEADBEEF);

    // Publish with matching type_id should succeed
    uint32_t data = 42;
    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    msg.data = &data;
    msg.size = sizeof(data);
    msg.payload_type_id = 0xDEADBEEF;
    EXPECT_EQ(BML_RESULT_OK, PublishEx(topic, &msg));

    // Publish with mismatching type_id should fail
    msg.payload_type_id = 0xCAFEBABE;
    EXPECT_EQ(BML_RESULT_IMC_TYPE_MISMATCH, PublishEx(topic, &msg));

    // Publish with BML_PAYLOAD_TYPE_NONE should bypass validation
    msg.payload_type_id = BML_PAYLOAD_TYPE_NONE;
    EXPECT_EQ(BML_RESULT_OK, PublishEx(topic, &msg));

    // Raw Publish (via C API with default type_id=0) should bypass validation
    EXPECT_EQ(BML_RESULT_OK, Publish(topic, &data, sizeof(data)));
}

TEST_F(ImcBusTest, PublishWithTypeIdAndNoExpectedTypeSucceeds) {
    // Topic with no expected type set should accept any type_id
    BML_TopicId topic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, GetTopicId("Test/Untyped", &topic));

    uint32_t data = 42;
    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    msg.data = &data;
    msg.size = sizeof(data);
    msg.payload_type_id = 0x12345678;
    EXPECT_EQ(BML_RESULT_OK, PublishEx(topic, &msg));
}

} // namespace
