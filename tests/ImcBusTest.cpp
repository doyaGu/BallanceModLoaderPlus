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

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/CrashDumpWriter.h"
#include "Core/DiagnosticManager.h"
#include "Core/FaultTracker.h"
#include "Core/ImcBus.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"
#include "TestKernel.h"

namespace {

using namespace BML::Core;
using BML::Core::Context;
using BML::Core::Testing::TestKernel;

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

void ShutdownFutureCallback(BML_Context ctx, BML_Future future, void *user_data) {
    auto *state = static_cast<ShutdownFutureCallbackState *>(user_data);
    if (!state)
        return;

    auto *kernel = GetKernelOrNull();
    BML_FutureState future_state = BML_FUTURE_PENDING;
    BML_Result result = ImcFutureGetState(future, &future_state);
    state->get_state_result.store(result, std::memory_order_relaxed);
    state->future_state.store(static_cast<int>(future_state), std::memory_order_relaxed);
    state->context_value.store(reinterpret_cast<uintptr_t>(ctx), std::memory_order_relaxed);
    state->kernel_visible.store(
        kernel && kernel->context && kernel->context->GetHandle() == ctx,
        std::memory_order_relaxed);
    state->called.store(true, std::memory_order_release);
}

class ImcBusTest : public ::testing::Test {
protected:
    std::vector<std::unique_ptr<BML::Core::ModManifest>> manifests_;
    TestKernel kernel_;

    void SetUp() override {
        kernel_->diagnostics = std::make_unique<DiagnosticManager>();
        kernel_->api_registry = std::make_unique<ApiRegistry>();
        kernel_->config = std::make_unique<ConfigStore>();
        kernel_->crash_dump = std::make_unique<CrashDumpWriter>();
        kernel_->fault_tracker = std::make_unique<FaultTracker>();
        kernel_->imc_bus = std::make_unique<ImcBus>();
        kernel_->context = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config, *kernel_->crash_dump, *kernel_->fault_tracker);
        kernel_->config->BindContext(*kernel_->context);

        auto &ctx = *kernel_->context;
        ctx.Initialize({0, 4, 0});
        kernel_->imc_bus->BindDeps(*kernel_->context);
        host_mod_ = ctx.GetSyntheticHostModule();
        ASSERT_NE(host_mod_, nullptr);
        Context::SetCurrentModule(host_mod_);
    }

    void TearDown() override {
        Context::SetCurrentModule(nullptr);
    }

    BML_Mod CreateTrackedMod(const std::string &id) {
        auto manifest = std::make_unique<BML::Core::ModManifest>();
        manifest->package.id = id;
        manifest->package.name = id;
        manifest->package.version = "1.0.0";
        manifest->package.parsed_version = {1, 0, 0};
        manifest->directory = L".";

        auto handle = kernel_->context->CreateModHandle(*manifest);
        LoadedModule module{};
        module.id = id;
        module.manifest = manifest.get();
        module.mod_handle = std::move(handle);
        kernel_->context->AddLoadedModule(std::move(module));

        BML_Mod mod = kernel_->context->GetModHandleById(id);
        manifests_.push_back(std::move(manifest));
        return mod;
    }

    BML_Mod host_mod_{nullptr};
};

// ========================================================================
// ID Resolution Tests
// ========================================================================

TEST_F(ImcBusTest, GetTopicIdReturnsConsistentId) {
    BML_TopicId id1, id2;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("test/topic", &id1));
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("test/topic", &id2));
    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, BML_TOPIC_ID_INVALID);
}

TEST_F(ImcBusTest, DifferentTopicsHaveDifferentIds) {
    BML_TopicId id1, id2;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("topic/a", &id1));
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("topic/b", &id2));
    EXPECT_NE(id1, id2);
}

TEST_F(ImcBusTest, GetTopicIdRejectsInvalidInput) {
    BML_TopicId id;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcGetTopicId(nullptr, &id));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcGetTopicId("", &id));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcGetTopicId("test", nullptr));
}

TEST_F(ImcBusTest, GetRpcIdReturnsConsistentId) {
    BML_RpcId id1, id2;
    ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("my/rpc", &id1));
    ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("my/rpc", &id2));
    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, BML_RPC_ID_INVALID);
}

// ========================================================================
// Pub/Sub Tests
// ========================================================================

TEST_F(ImcBusTest, PublishesToSubscribedHandler) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("test.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state, &sub));
    ASSERT_NE(sub, nullptr);

    const std::string payload = "hello";
    EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, payload.data(), payload.size()));

    ImcPump(0);

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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("buffer.topic", &topic));

    PubSubState state;
    BufferCleanupState cleanup_state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state, &sub));

    std::array<uint8_t, 4> buffer{1, 2, 3, 4};
    BML_ImcBuffer message = BML_IMC_BUFFER_INIT;
    message.data = buffer.data();
    message.size = buffer.size();
    message.cleanup = BufferCleanup;
    message.cleanup_user_data = &cleanup_state;

    EXPECT_EQ(BML_RESULT_OK, ImcPublishBuffer(topic, &message));
    ImcPump(0);

    EXPECT_EQ(cleanup_state.called.load(), 1u);
    EXPECT_EQ(cleanup_state.last_size, buffer.size());
    ASSERT_EQ(state.payloads.size(), 1u);
    EXPECT_EQ(state.payloads[0], std::vector<uint8_t>(buffer.begin(), buffer.end()));

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, PublishExPreservesExplicitTimestamp) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("timestamp.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state, &sub));

    constexpr uint64_t kTimestamp = 123456789ull;
    const int payload = 7;
    BML_ImcMessage msg = BML_IMC_MSG(&payload, sizeof(payload));
    msg.timestamp = kTimestamp;

    ASSERT_EQ(BML_RESULT_OK, ImcPublishEx(topic, &msg));
    ImcPump(0);

    ASSERT_EQ(state.timestamps.size(), 1u);
    EXPECT_EQ(state.timestamps.front(), kTimestamp);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, RetainedStateDeliveryHonorsSubscriptionOptions) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("retained.options.topic", &topic));

    const int payload = 99;
    BML_ImcMessage stateMessage = BML_IMC_MSG(&payload, sizeof(payload));
    stateMessage.priority = BML_IMC_PRIORITY_LOW;
    ASSERT_EQ(BML_RESULT_OK, ImcPublishState(topic, &stateMessage));

    BML_SubscribeOptions priorityOpts = BML_SUBSCRIBE_OPTIONS_INIT;
    priorityOpts.flags = BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE;
    priorityOpts.min_priority = BML_IMC_PRIORITY_HIGH;

    PubSubState priorityState;
    BML_Subscription prioritySub = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              ImcSubscribeEx(topic, CollectingHandler, &priorityState, &priorityOpts, &prioritySub));
    EXPECT_EQ(priorityState.call_count.load(std::memory_order_relaxed), 0u);

    BML_SubscribeOptions filterOpts = BML_SUBSCRIBE_OPTIONS_INIT;
    filterOpts.flags = BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE;
    filterOpts.filter = RejectAllFilter;

    PubSubState filteredState;
    BML_Subscription filteredSub = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              ImcSubscribeEx(topic, CollectingHandler, &filteredState, &filterOpts, &filteredSub));
    EXPECT_EQ(filteredState.call_count.load(std::memory_order_relaxed), 0u);

    BML_SubscribeOptions acceptOpts = BML_SUBSCRIBE_OPTIONS_INIT;
    acceptOpts.flags = BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE;

    PubSubState acceptedState;
    BML_Subscription acceptedSub = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              ImcSubscribeEx(topic, CollectingHandler, &acceptedState, &acceptOpts, &acceptedSub));
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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("retained.copy.clear", &topic));

    const char payload[] = "custom-map.cmo";
    BML_ImcMessage stateMessage = BML_IMC_MSG(payload, sizeof(payload));
    ASSERT_EQ(BML_RESULT_OK, ImcPublishState(topic, &stateMessage));

    char buffer[64] = {};
    size_t copiedSize = 0;
    BML_ImcStateMeta meta = BML_IMC_STATE_META_INIT;
    ASSERT_EQ(BML_RESULT_OK, ImcCopyState(topic, buffer, sizeof(buffer), &copiedSize, &meta));
    EXPECT_EQ(sizeof(payload), copiedSize);
    EXPECT_STREQ(payload, buffer);
    EXPECT_EQ(sizeof(payload), meta.size);

    ASSERT_EQ(BML_RESULT_OK, ImcClearState(topic));
    EXPECT_EQ(BML_RESULT_NOT_FOUND, ImcCopyState(topic, buffer, sizeof(buffer), &copiedSize, &meta));
}

TEST_F(ImcBusTest, UnsubscribeStopsDelivery) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("unsub.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state, &sub));

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));

    const uint8_t value = 42;
    EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, &value, sizeof(value)));
    ImcPump(0);

    EXPECT_EQ(state.call_count.load(), 0u);
}

TEST_F(ImcBusTest, SubscriptionIsActiveReturnsCorrectState) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("active.topic", &topic));

    PubSubState state;
    BML_Subscription sub = nullptr;
    BML_Bool is_active = BML_FALSE;
    
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state, &sub));
    ASSERT_NE(sub, nullptr);
    
    EXPECT_EQ(BML_RESULT_OK, ImcSubscriptionIsActive(sub, &is_active));
    EXPECT_EQ(is_active, BML_TRUE);
    
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
    
    // After unsubscribe, handle is invalid
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, ImcSubscriptionIsActive(sub, &is_active));
}

TEST_F(ImcBusTest, MultipleSubscribersReceiveMessages) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("multi.topic", &topic));

    PubSubState state1, state2;
    BML_Subscription sub1 = nullptr, sub2 = nullptr;

    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state1, &sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state2, &sub2));

    const uint8_t data = 123;
    EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, &data, sizeof(data)));
    ImcPump(0);

    EXPECT_EQ(state1.call_count.load(), 1u);
    EXPECT_EQ(state2.call_count.load(), 1u);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub1));
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub2));
}

TEST_F(ImcBusTest, TopicDiagnosticsReflectRegistry) {
    constexpr const char *kTopicName = "diag.topic";
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId(kTopicName, &topic));

    char name_buffer[64] = {};
    size_t name_length = 0;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicName(topic, name_buffer, sizeof(name_buffer), &name_length));
    EXPECT_STREQ(kTopicName, name_buffer);
    EXPECT_EQ(std::strlen(kTopicName), name_length);

    const uint8_t payload = 0x5Au;
    EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, &payload, sizeof(payload)));

    BML_TopicInfo info = BML_TOPIC_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicInfo(topic, &info));
    EXPECT_EQ(topic, info.topic_id);
    EXPECT_STREQ(kTopicName, info.name);
    EXPECT_EQ(0u, info.subscriber_count);
    EXPECT_EQ(1u, info.message_count);
}

TEST_F(ImcBusTest, UnsubscribeWaitsForInFlightHandlers) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("blocking.topic", &topic));

    BlockingHandlerState handler_state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, BlockingHandler, &handler_state, &sub));

    const uint8_t payload = 0x11u;
    EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, &payload, sizeof(payload)));

    std::thread pump_thread([] {
        ImcPump(0);
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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("self.unsubscribe.topic", &topic));

    SelfUnsubscribeState state;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, SelfUnsubscribeHandler, &state, &state.sub));
    ASSERT_NE(state.sub, nullptr);

    const uint8_t payload = 0x22u;
    EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, &payload, sizeof(payload)));

    ImcPump(0);

    EXPECT_EQ(1u, state.call_count.load(std::memory_order_relaxed));
    EXPECT_EQ(BML_RESULT_OK, state.unsubscribe_result.load(std::memory_order_relaxed));

    BML_Bool active = BML_TRUE;
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, ImcSubscriptionIsActive(state.sub, &active));

    EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, &payload, sizeof(payload)));
    ImcPump(0);

    EXPECT_EQ(1u, state.call_count.load(std::memory_order_relaxed));
}

TEST_F(ImcBusTest, SubscribeRequiresBoundModule) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("missing.owner.subscription", &topic));

    Context::SetCurrentModule(nullptr);

    PubSubState state;
    BML_Subscription sub = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcSubscribe(topic, CollectingHandler, &state, &sub));
    EXPECT_EQ(nullptr, sub);
}

TEST_F(ImcBusTest, PublishStateRequiresBoundModule) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("missing.owner.state", &topic));

    Context::SetCurrentModule(nullptr);

    const uint32_t payload = 7;
    BML_ImcMessage state = BML_IMC_MSG(&payload, sizeof(payload));
    EXPECT_EQ(BML_RESULT_INVALID_CONTEXT, ImcPublishState(topic, &state));
}

// ========================================================================
// RPC Tests
// ========================================================================

TEST_F(ImcBusTest, RpcEchoWorks) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("echo", &rpc_id));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(rpc_id, EchoRpc, &state));

    const std::string request_data = "test request";
    BML_ImcMessage request = BML_IMC_MSG(request_data.data(), request_data.size());
    BML_Future future = nullptr;
    
    ASSERT_EQ(BML_RESULT_OK, ImcCallRpc(rpc_id, &request, &future));
    ASSERT_NE(future, nullptr);

    ImcPump(0);  // Process RPC

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
    EXPECT_EQ(BML_RESULT_OK, ImcUnregisterRpc(rpc_id));
}

TEST_F(ImcBusTest, RpcToUnregisteredHandlerFails) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("unregistered", &rpc_id));

    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    BML_Future future = nullptr;
    
    ASSERT_EQ(BML_RESULT_OK, ImcCallRpc(rpc_id, &request, &future));
    ImcPump(0);

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcFutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_FAILED);

    EXPECT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
}

TEST_F(ImcBusTest, DuplicateRpcRegistrationFails) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("duplicate", &rpc_id));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(rpc_id, EchoRpc, &state));
    EXPECT_EQ(BML_RESULT_ALREADY_EXISTS, ImcRegisterRpc(rpc_id, EchoRpc, &state));

    EXPECT_EQ(BML_RESULT_OK, ImcUnregisterRpc(rpc_id));
}

TEST_F(ImcBusTest, RpcUnregisterRejectsNonOwner) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("rpc.owner.check", &rpc_id));

    auto owner_a = CreateTrackedMod("imc.rpc.owner.a");
    auto owner_b = CreateTrackedMod("imc.rpc.owner.b");

    RpcState state;
    Context::SetCurrentModule(owner_a);
    ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(rpc_id, EchoRpc, &state));

    Context::SetCurrentModule(owner_b);
    EXPECT_EQ(BML_RESULT_PERMISSION_DENIED, ImcUnregisterRpc(rpc_id));

    Context::SetCurrentModule(owner_a);
    EXPECT_EQ(BML_RESULT_OK, ImcUnregisterRpc(rpc_id));
}

// ========================================================================
// Future Tests
// ========================================================================

TEST_F(ImcBusTest, FutureCancelWorks) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("cancel_test", &rpc_id));

    BML_ImcMessage request = BML_IMC_MESSAGE_INIT;
    BML_Future future = nullptr;
    
    ASSERT_EQ(BML_RESULT_OK, ImcCallRpc(rpc_id, &request, &future));
    
    // Cancel before processing
    EXPECT_EQ(BML_RESULT_OK, ImcFutureCancel(future));

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcFutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_CANCELLED);

    EXPECT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
}

TEST_F(ImcBusTest, FutureOnCompleteCallbackWorks) {
    BML_RpcId rpc_id;
    ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("callback_test", &rpc_id));

    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(rpc_id, EchoRpc, &state));

    BML_ImcMessage request = BML_IMC_MSG("x", 1);
    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcCallRpc(rpc_id, &request, &future));

    std::atomic<bool> callback_called{false};
    EXPECT_EQ(BML_RESULT_OK, ImcFutureOnComplete(future, 
        [](BML_Context, BML_Future, void *ud) {
            *static_cast<std::atomic<bool>*>(ud) = true;
        }, &callback_called));

    ImcPump(0);

    EXPECT_TRUE(callback_called.load());

    EXPECT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    EXPECT_EQ(BML_RESULT_OK, ImcUnregisterRpc(rpc_id));
}

TEST(ImcBusLifetimeTest, KernelDestructionCompletesPendingFutureBeforeContextDestruction) {
    ShutdownFutureCallbackState callback_state;

    {
        TestKernel kernel;
        kernel->diagnostics = std::make_unique<DiagnosticManager>();
        kernel->api_registry = std::make_unique<ApiRegistry>();
        kernel->config = std::make_unique<ConfigStore>();
        kernel->crash_dump = std::make_unique<CrashDumpWriter>();
        kernel->fault_tracker = std::make_unique<FaultTracker>();
        kernel->imc_bus = std::make_unique<ImcBus>();
        kernel->context = std::make_unique<Context>(*kernel->api_registry, *kernel->config,
                                                    *kernel->crash_dump,
                                                    *kernel->fault_tracker);
        kernel->config->BindContext(*kernel->context);

        auto &ctx = *kernel->context;
        ctx.Initialize({0, 4, 0});
        kernel->imc_bus->BindDeps(ctx);

        BML_Mod host_mod = ctx.GetSyntheticHostModule();
        ASSERT_NE(host_mod, nullptr);
        Context::SetCurrentModule(host_mod);

        BML_RpcId rpc_id = BML_RPC_ID_INVALID;
        ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("shutdown_test", &rpc_id));
        ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(rpc_id, EchoRpc, nullptr));

        BML_Future future = nullptr;
        ASSERT_EQ(BML_RESULT_OK, ImcCallRpc(rpc_id, nullptr, &future));
        ASSERT_NE(future, nullptr);
        ASSERT_EQ(BML_RESULT_OK,
                  ImcFutureOnComplete(future, ShutdownFutureCallback, &callback_state));

        // Leave the queued request owning the last reference so shutdown can
        // complete and release the future while the callback still runs.
        ASSERT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
        Context::SetCurrentModule(nullptr);
    }

    EXPECT_TRUE(callback_state.called.load(std::memory_order_acquire));
    EXPECT_EQ(callback_state.get_state_result.load(std::memory_order_relaxed), BML_RESULT_OK);
    EXPECT_EQ(callback_state.future_state.load(std::memory_order_relaxed),
              static_cast<int>(BML_FUTURE_CANCELLED));
    EXPECT_TRUE(callback_state.kernel_visible.load(std::memory_order_relaxed));
    EXPECT_NE(callback_state.context_value.load(std::memory_order_relaxed),
              static_cast<uintptr_t>(0));
}

// ========================================================================
// Edge Cases
// ========================================================================

TEST_F(ImcBusTest, PublishRejectsInvalidInput) {
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcPublish(BML_TOPIC_ID_INVALID, nullptr, 0));
    
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("valid", &topic));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcPublish(topic, nullptr, 10));  // data null but size > 0
}

TEST_F(ImcBusTest, SubscribeRejectsInvalidInput) {
    BML_Subscription sub;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcSubscribe(BML_TOPIC_ID_INVALID, CollectingHandler, nullptr, &sub));
    
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("valid", &topic));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcSubscribe(topic, nullptr, nullptr, &sub));
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, ImcSubscribe(topic, CollectingHandler, nullptr, nullptr));
}

TEST_F(ImcBusTest, UnsubscribeInvalidHandleFails) {
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, ImcUnsubscribe(nullptr));
}

TEST_F(ImcBusTest, PublishToEmptyTopicSucceeds) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("empty.topic", &topic));
    
    const uint8_t data = 1;
    EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, &data, sizeof(data)));
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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("budget.test", &topic));

    IntCapture cap1, cap2;
    BML_Subscription sub1, sub2;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, IntCollectHandler, &cap1, &sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, IntCollectHandler, &cap2, &sub2));

    // Publish 4 messages
    for (int i = 1; i <= 4; ++i) {
        ASSERT_EQ(BML_RESULT_OK, ImcPublish(topic, &i, sizeof(i)));
    }

    // Each subscriber should have 4 messages to process
    // With budget=1, we process at most 1 message per subscriber per Pump
    ImcPump(1);
    EXPECT_EQ(cap1.count.load(), 1);
    EXPECT_EQ(cap2.count.load(), 1);

    // Drain remaining
    ImcPump(0);
    EXPECT_EQ(cap1.count.load(), 4);
    EXPECT_EQ(cap2.count.load(), 4);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub1));
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub2));
}

TEST_F(ImcBusTest, PublishingResumesAfterPumpClearsQueue) {
    // Backpressure scenario: queue fills up, pump clears, publish succeeds again
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("backpressure.test", &topic));

    IntCapture cap;
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, IntCollectHandler, &cap, &sub));

    // Publish a bunch of messages - they queue up
    constexpr int COUNT = 50;
    for (int i = 0; i < COUNT; ++i) {
        ASSERT_EQ(BML_RESULT_OK, ImcPublish(topic, &i, sizeof(i)));
    }

    // Pump clears the queue
    ImcPump(0);
    EXPECT_EQ(cap.count.load(), COUNT);

    // More publishes should work
    int v = 999;
    ASSERT_EQ(BML_RESULT_OK, ImcPublish(topic, &v, sizeof(v)));
    ImcPump(0);
    EXPECT_EQ(cap.count.load(), COUNT + 1);
    EXPECT_EQ(cap.values.back(), 999);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, FutureAwaitTimesOutUntilPumpProcessesRpc) {
    // Without Pump, RPC waits forever (timeout); after Pump, completes
    // Reuse existing EchoRpc handler and RpcState from above
    BML_RpcId rpc;
    ASSERT_EQ(BML_RESULT_OK, ImcGetRpcId("timeout.echo", &rpc));
    
    RpcState state;
    ASSERT_EQ(BML_RESULT_OK, ImcRegisterRpc(rpc, EchoRpc, &state));

    uint8_t payload = 42;
    BML_ImcMessage request = BML_IMC_MSG(&payload, sizeof(payload));
    BML_Future future = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcCallRpc(rpc, &request, &future));

    // Await with short timeout - should timeout because no Pump yet
    BML_Result res = ImcFutureAwait(future, 10);
    EXPECT_EQ(res, BML_RESULT_TIMEOUT);

    // Now Pump and await again - should succeed
    ImcPump(0);
    res = ImcFutureAwait(future, 100);
    EXPECT_EQ(res, BML_RESULT_OK);

    BML_FutureState fstate;
    EXPECT_EQ(BML_RESULT_OK, ImcFutureGetState(future, &fstate));
    EXPECT_EQ(fstate, BML_FUTURE_READY);

    EXPECT_EQ(BML_RESULT_OK, ImcFutureRelease(future));
    EXPECT_EQ(BML_RESULT_OK, ImcUnregisterRpc(rpc));
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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("priority.order.test", &topic));

    PriorityCapture cap;
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, PriorityCollectHandler, &cap, &sub));

    // Publish messages with different priorities (LOW first, URGENT last)
    BML_ImcMessage msg_low = BML_IMC_MESSAGE_INIT;
    msg_low.priority = BML_IMC_PRIORITY_LOW;
    ASSERT_EQ(BML_RESULT_OK, ImcPublishEx(topic, &msg_low));

    BML_ImcMessage msg_normal = BML_IMC_MESSAGE_INIT;
    msg_normal.priority = BML_IMC_PRIORITY_NORMAL;
    ASSERT_EQ(BML_RESULT_OK, ImcPublishEx(topic, &msg_normal));

    BML_ImcMessage msg_high = BML_IMC_MESSAGE_INIT;
    msg_high.priority = BML_IMC_PRIORITY_HIGH;
    ASSERT_EQ(BML_RESULT_OK, ImcPublishEx(topic, &msg_high));

    BML_ImcMessage msg_urgent = BML_IMC_MESSAGE_INIT;
    msg_urgent.priority = BML_IMC_PRIORITY_URGENT;
    ASSERT_EQ(BML_RESULT_OK, ImcPublishEx(topic, &msg_urgent));

    // Pump all messages
    ImcPump(0);

    ASSERT_EQ(cap.count.load(), 4);
    // Urgent should be first (index 0)
    EXPECT_EQ(cap.priorities[0], BML_IMC_PRIORITY_URGENT);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, PriorityFilterRespectsMinPriority) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("priority.filter.test", &topic));

    PriorityCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.min_priority = BML_IMC_PRIORITY_HIGH;  // Only HIGH and URGENT
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribeEx(topic, PriorityCollectHandler, &cap, &opts, &sub));

    // Publish all priority levels
    for (uint32_t p = BML_IMC_PRIORITY_LOW; p <= BML_IMC_PRIORITY_URGENT; ++p) {
        BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
        msg.priority = p;
        ASSERT_EQ(BML_RESULT_OK, ImcPublishEx(topic, &msg));
    }

    ImcPump(0);

    // Should only receive HIGH and URGENT
    EXPECT_EQ(cap.count.load(), 2);
    
    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, MessageFilterSkipsRejectedMessages) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("message.filter.test", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.filter = EvenValueFilter;

    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    for (int value = 1; value <= 4; ++value) {
        ASSERT_EQ(BML_RESULT_OK, ImcPublish(topic, &value, sizeof(value)));
    }

    ImcPump(0);

    ASSERT_EQ(2, cap.count.load());
    ASSERT_EQ(2u, cap.values.size());
    EXPECT_EQ(2, cap.values[0]);
    EXPECT_EQ(4, cap.values[1]);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(sub));
}

TEST_F(ImcBusTest, LegacySubscribeOptionsRemainCompatible) {
    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("legacy.subscribe.options", &topic));

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
              ImcSubscribeEx(
                  topic,
                  PriorityCollectHandler,
                  &cap,
                  reinterpret_cast<const BML_SubscribeOptions *>(&legacy_opts),
                  &sub));

    for (uint32_t priority = BML_IMC_PRIORITY_LOW; priority <= BML_IMC_PRIORITY_URGENT; ++priority) {
        BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
        msg.priority = priority;
        ASSERT_EQ(BML_RESULT_OK, ImcPublishEx(topic, &msg));
    }

    ImcPump(0);

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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("backpressure.drop.newest", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 4;  // Very small queue
    opts.backpressure = BML_BACKPRESSURE_DROP_NEWEST;
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    // Fill queue beyond capacity - new messages should be dropped
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, &i, sizeof(i)));
    }

    ImcPump(0);

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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("backpressure.drop.oldest", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 4;  // Very small queue
    opts.backpressure = BML_BACKPRESSURE_DROP_OLDEST;
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    // Fill queue and overflow - old messages should be dropped
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(BML_RESULT_OK, ImcPublish(topic, &i, sizeof(i)));
    }

    ImcPump(0);

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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("backpressure.fail", &topic));

    IntCapture cap;
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 4;  // Very small queue
    opts.backpressure = BML_BACKPRESSURE_FAIL;
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribeEx(topic, IntCollectHandler, &cap, &opts, &sub));

    // Overflow the queue - should start returning WOULD_BLOCK
    bool got_would_block = false;
    for (int i = 0; i < 100; ++i) {
        BML_Result res = ImcPublish(topic, &i, sizeof(i));
        if (res == BML_RESULT_WOULD_BLOCK) {
            got_would_block = true;
            break;
        }
    }

    EXPECT_TRUE(got_would_block);

    ImcPump(0);

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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("zerocopy.lifecycle", &topic));

    // Subscribe multiple handlers
    PubSubState state1, state2, state3;
    BML_Subscription sub1, sub2, sub3;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state1, &sub1));
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state2, &sub2));
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, CollectingHandler, &state3, &sub3));

    BufferCleanupState cleanup_state;
    std::array<uint8_t, 64> data;
    std::fill(data.begin(), data.end(), 0xAB);

    BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
    buffer.data = data.data();
    buffer.size = data.size();
    buffer.cleanup = BufferCleanup;
    buffer.cleanup_user_data = &cleanup_state;

    ASSERT_EQ(BML_RESULT_OK, ImcPublishBuffer(topic, &buffer));
    
    // Pump to process all
    ImcPump(0);

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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("concurrent.publishers", &topic));

    std::atomic<uint32_t> received{0};
    
    // Use a larger queue capacity to avoid backpressure drops
    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;
    opts.queue_capacity = 1024;  // Much larger than total messages
    opts.backpressure = BML_BACKPRESSURE_FAIL;  // Fail instead of drop
    
    BML_Subscription sub;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribeEx(topic, 
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
                BML_Result res = ImcPublish(topic, &value, sizeof(value));
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
        ImcPump(0);
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
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId("stress.rcu.race", &topic));

    std::atomic<uint32_t> total_received{0};
    std::atomic<bool> stop{false};

    auto counting_handler = [](BML_Context, BML_TopicId, const BML_ImcMessage *, void *ud) {
        static_cast<std::atomic<uint32_t> *>(ud)->fetch_add(1, std::memory_order_relaxed);
    };

    // Start with one subscription
    BML_Subscription initial_sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, ImcSubscribe(topic, counting_handler, &total_received, &initial_sub));

    constexpr int kPublishThreads = 3;
    constexpr int kMessagesPerThread = 200;
    std::vector<std::thread> threads;

    // Publisher threads: continuously publish while subs change
    for (int t = 0; t < kPublishThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                int value = t * 1000 + i;
                ImcPublish(topic, &value, sizeof(value));
            }
        });
    }

    // Subscribe/unsubscribe churn thread
    threads.emplace_back([&]() {
        for (int cycle = 0; cycle < 20 && !stop.load(std::memory_order_relaxed); ++cycle) {
            BML_Subscription sub = nullptr;
            BML_Result res = ImcSubscribe(topic, counting_handler, &total_received, &sub);
            if (res == BML_RESULT_OK && sub) {
                std::this_thread::yield();
                ImcUnsubscribe(sub);
            }
        }
    });

    // Pump thread: drain messages concurrently
    threads.emplace_back([&]() {
        for (int i = 0; i < 50 && !stop.load(std::memory_order_relaxed); ++i) {
            ImcPump(0);
            std::this_thread::yield();
        }
    });

    for (auto &t : threads) {
        t.join();
    }
    stop.store(true, std::memory_order_release);

    // Final drain
    for (int i = 0; i < 10; ++i) {
        ImcPump(0);
    }

    // The initial subscriber should have received some messages
    EXPECT_GT(total_received.load(), 0u);

    EXPECT_EQ(BML_RESULT_OK, ImcUnsubscribe(initial_sub));
}

} // namespace
