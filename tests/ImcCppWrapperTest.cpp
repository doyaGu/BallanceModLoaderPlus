/**
 * @file ImcCppWrapperTest.cpp
 * @brief Tests for the C++ IMC wrapper API
 */

// Define BML_LOADER_IMPLEMENTATION to get the function pointer definitions
// (they will all be NULL since we don't actually load the BML runtime)
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "bml_imc.hpp"

using namespace bml::imc;

namespace {
    const BML_Mod kOwner = reinterpret_cast<BML_Mod>(0x1);
    const BML_Context kServiceContext = reinterpret_cast<BML_Context>(0x2);

    #if defined(_MSC_VER)
        #define BML_TEST_NOINLINE __declspec(noinline)
    #else
        #define BML_TEST_NOINLINE __attribute__((noinline))
    #endif

    struct ImcMockState {
        BML_TopicId topic_id = 77u;
        BML_RpcId rpc_id = 91u;
        BML_Subscription subscription = reinterpret_cast<BML_Subscription>(0x3000);
        BML_Future future = reinterpret_cast<BML_Future>(0x4000);
        BML_TopicId last_topic = InvalidTopicId;
        BML_RpcId last_rpc = InvalidRpcId;
        std::string last_topic_name;
        std::string last_rpc_name;
        size_t last_publish_size = 0;
        int publish_calls = 0;
        int subscribe_calls = 0;
        int unsubscribe_calls = 0;
        int pump_calls = 0;
        int register_rpc_calls = 0;
        int unregister_rpc_calls = 0;
        int call_rpc_calls = 0;
        int future_release_calls = 0;
        int future_cancel_calls = 0;
        bool subscription_active = true;
        int future_result_value = 314;
        std::string topic_name = "test/topic";
        std::string rpc_name = "test/rpc";
    };

    ImcMockState g_ImcMockState;

    BML_Result MockGetTopicId(const char *name, BML_TopicId *outTopic) {
        if (!outTopic) return BML_RESULT_INVALID_ARGUMENT;
        g_ImcMockState.last_topic_name = name ? name : "";
        *outTopic = g_ImcMockState.topic_id;
        return BML_RESULT_OK;
    }
    BML_Result MockServiceGetTopicId(BML_Context, const char *name, BML_TopicId *outTopic) {
        return MockGetTopicId(name, outTopic);
    }

    BML_Result MockGetRpcId(const char *name, BML_RpcId *outRpc) {
        if (!outRpc) return BML_RESULT_INVALID_ARGUMENT;
        g_ImcMockState.last_rpc_name = name ? name : "";
        *outRpc = g_ImcMockState.rpc_id;
        return BML_RESULT_OK;
    }
    BML_Result MockServiceGetRpcId(BML_Context, const char *name, BML_RpcId *outRpc) {
        return MockGetRpcId(name, outRpc);
    }

    BML_Result MockPublish(BML_Mod, BML_TopicId topic, const void *, size_t size, BML_PayloadTypeId) {
        ++g_ImcMockState.publish_calls;
        g_ImcMockState.last_topic = topic;
        g_ImcMockState.last_publish_size = size;
        return BML_RESULT_OK;
    }

    BML_Result MockPublishEx(BML_Mod, BML_TopicId topic, const BML_ImcMessage *msg) {
        ++g_ImcMockState.publish_calls;
        g_ImcMockState.last_topic = topic;
        g_ImcMockState.last_publish_size = msg ? msg->size : 0;
        return BML_RESULT_OK;
    }

    BML_Result MockPublishBuffer(BML_Mod, BML_TopicId topic, const BML_ImcBuffer *buffer) {
        ++g_ImcMockState.publish_calls;
        g_ImcMockState.last_topic = topic;
        g_ImcMockState.last_publish_size = buffer ? buffer->size : 0;
        return BML_RESULT_OK;
    }

    BML_Result MockSubscribe(BML_Mod,
                             BML_TopicId topic,
                             BML_ImcHandler,
                             void *,
                             BML_Subscription *outSubscription) {
        if (!outSubscription) return BML_RESULT_INVALID_ARGUMENT;
        ++g_ImcMockState.subscribe_calls;
        g_ImcMockState.last_topic = topic;
        *outSubscription = g_ImcMockState.subscription;
        return BML_RESULT_OK;
    }

    BML_Result MockSubscribeEx(BML_Mod owner,
                               BML_TopicId topic,
                               BML_ImcHandler,
                               void *,
                               const BML_SubscribeOptions *,
                               BML_Subscription *outSubscription) {
        return MockSubscribe(owner, topic, nullptr, nullptr, outSubscription);
    }

    BML_Result MockUnsubscribe(BML_Subscription subscription) {
        if (subscription != g_ImcMockState.subscription) return BML_RESULT_INVALID_ARGUMENT;
        ++g_ImcMockState.unsubscribe_calls;
        g_ImcMockState.subscription_active = false;
        return BML_RESULT_OK;
    }

    BML_Result MockSubscriptionIsActive(BML_Subscription subscription, BML_Bool *outActive) {
        if (!outActive || subscription != g_ImcMockState.subscription) return BML_RESULT_INVALID_ARGUMENT;
        *outActive = g_ImcMockState.subscription_active ? BML_TRUE : BML_FALSE;
        return BML_RESULT_OK;
    }

    BML_Result MockRegisterRpc(BML_Mod, BML_RpcId rpc, BML_RpcHandler, void *) {
        ++g_ImcMockState.register_rpc_calls;
        g_ImcMockState.last_rpc = rpc;
        return BML_RESULT_OK;
    }

    BML_Result MockUnregisterRpc(BML_Mod, BML_RpcId rpc) {
        ++g_ImcMockState.unregister_rpc_calls;
        g_ImcMockState.last_rpc = rpc;
        return BML_RESULT_OK;
    }

    BML_Result MockCallRpc(BML_Mod, BML_RpcId rpc, const BML_ImcMessage *, BML_Future *outFuture) {
        if (!outFuture) return BML_RESULT_INVALID_ARGUMENT;
        ++g_ImcMockState.call_rpc_calls;
        g_ImcMockState.last_rpc = rpc;
        *outFuture = g_ImcMockState.future;
        return BML_RESULT_OK;
    }

    BML_Result MockFutureAwait(BML_Future future, uint32_t) {
        return future == g_ImcMockState.future ? BML_RESULT_OK : BML_RESULT_INVALID_ARGUMENT;
    }

    BML_Result MockFutureGetResult(BML_Future future, BML_ImcMessage *outMessage) {
        if (!outMessage || future != g_ImcMockState.future) return BML_RESULT_INVALID_ARGUMENT;
        *outMessage = BML_IMC_MSG(&g_ImcMockState.future_result_value, sizeof(g_ImcMockState.future_result_value));
        return BML_RESULT_OK;
    }

    BML_Result MockFutureGetState(BML_Future future, BML_FutureState *outState) {
        if (!outState || future != g_ImcMockState.future) return BML_RESULT_INVALID_ARGUMENT;
        *outState = BML_FUTURE_READY;
        return BML_RESULT_OK;
    }

    BML_Result MockFutureCancel(BML_Future future) {
        if (future != g_ImcMockState.future) return BML_RESULT_INVALID_ARGUMENT;
        ++g_ImcMockState.future_cancel_calls;
        return BML_RESULT_OK;
    }

    BML_Result MockFutureOnComplete(BML_Mod, BML_Future future, BML_FutureCallback callback, void *userData) {
        if (future != g_ImcMockState.future) return BML_RESULT_INVALID_ARGUMENT;
        if (callback) {
            callback(nullptr, future, userData);
        }
        return BML_RESULT_OK;
    }

    BML_Result MockFutureRelease(BML_Future future) {
        if (future != g_ImcMockState.future) return BML_RESULT_INVALID_ARGUMENT;
        ++g_ImcMockState.future_release_calls;
        return BML_RESULT_OK;
    }

    void MockPump(size_t) {
        ++g_ImcMockState.pump_calls;
    }
    void MockServicePump(BML_Context, size_t maxPerSub) {
        MockPump(maxPerSub);
    }

    BML_Result MockGetSubscriptionStats(BML_Subscription subscription, BML_SubscriptionStats *outStats) {
        if (!outStats || subscription != g_ImcMockState.subscription) return BML_RESULT_INVALID_ARGUMENT;
        *outStats = BML_SUBSCRIPTION_STATS_INIT;
        outStats->messages_received = 5;
        outStats->messages_dropped = 1;
        outStats->queue_size = 2;
        return BML_RESULT_OK;
    }

    BML_Result MockGetStats(BML_ImcStats *outStats) {
        if (!outStats) return BML_RESULT_INVALID_ARGUMENT;
        *outStats = BML_IMC_STATS_INIT;
        outStats->total_messages_published = 10;
        outStats->active_topics = 1;
        return BML_RESULT_OK;
    }
    BML_Result MockServiceGetStats(BML_Context, BML_ImcStats *outStats) {
        return MockGetStats(outStats);
    }

    BML_Result MockResetStats() {
        return BML_RESULT_OK;
    }
    BML_Result MockServiceResetStats(BML_Context) {
        return MockResetStats();
    }

    BML_Result MockGetTopicInfo(BML_TopicId topic, BML_TopicInfo *outInfo) {
        if (!outInfo) return BML_RESULT_INVALID_ARGUMENT;
        *outInfo = BML_TOPIC_INFO_INIT;
        outInfo->topic_id = topic;
        std::strcpy(outInfo->name, "test/topic");
        outInfo->subscriber_count = 3;
        outInfo->message_count = 12;
        return BML_RESULT_OK;
    }
    BML_Result MockServiceGetTopicInfo(BML_Context, BML_TopicId topic, BML_TopicInfo *outInfo) {
        return MockGetTopicInfo(topic, outInfo);
    }

    BML_Result MockGetTopicName(BML_TopicId topic, char *buffer, size_t bufferSize, size_t *outLength) {
        const std::string &name = topic == g_ImcMockState.topic_id ? g_ImcMockState.topic_name : g_ImcMockState.topic_name;
        const size_t length = name.size();
        if (!buffer || bufferSize == 0) return BML_RESULT_INVALID_ARGUMENT;
        if (outLength) *outLength = length;
        if (bufferSize <= length) {
            if (bufferSize > 0) {
                std::memcpy(buffer, name.data(), bufferSize - 1);
                buffer[bufferSize - 1] = '\0';
            }
            return BML_RESULT_BUFFER_TOO_SMALL;
        }
        std::memcpy(buffer, name.data(), length);
        buffer[length] = '\0';
        return BML_RESULT_OK;
    }
    BML_Result MockServiceGetTopicName(BML_Context,
                                       BML_TopicId topic,
                                       char *buffer,
                                       size_t bufferSize,
                                       size_t *outLength) {
        return MockGetTopicName(topic, buffer, bufferSize, outLength);
    }

    BML_Result MockRegisterRpcEx(BML_Mod, BML_RpcId, BML_RpcHandlerEx, void *) {
        return BML_RESULT_OK;
    }
    BML_Result MockCallRpcEx(BML_Mod, BML_RpcId rpc, const BML_ImcMessage *, const BML_RpcCallOptions *, BML_Future *o) {
        ++g_ImcMockState.call_rpc_calls;
        g_ImcMockState.last_rpc = rpc;
        if (o) *o = g_ImcMockState.future;
        return BML_RESULT_OK;
    }
    BML_Result MockFutureGetError(BML_Future, BML_Result *out_code, char *msg, size_t cap, size_t *out_len) {
        if (out_code) *out_code = BML_RESULT_OK;
        if (msg && cap > 0) msg[0] = '\0';
        if (out_len) *out_len = 0;
        return BML_RESULT_OK;
    }
    BML_Result MockGetRpcInfo(BML_RpcId id, BML_RpcInfo *o) {
        if (!o) return BML_RESULT_INVALID_ARGUMENT;
        *o = BML_RPC_INFO_INIT;
        o->rpc_id = id;
        o->has_handler = BML_TRUE;
        return BML_RESULT_OK;
    }
    BML_Result MockServiceGetRpcInfo(BML_Context, BML_RpcId id, BML_RpcInfo *o) {
        return MockGetRpcInfo(id, o);
    }
    BML_Result MockAddRpcMiddleware(BML_Mod, BML_RpcMiddleware, int32_t, void *) {
        return BML_RESULT_OK;
    }
    BML_Result MockRemoveRpcMiddleware(BML_Mod, BML_RpcMiddleware) {
        return BML_RESULT_OK;
    }

    BML_Result MockGetRpcName(BML_RpcId, char *buf, size_t cap, size_t *out_len) {
        size_t len = g_ImcMockState.rpc_name.size();
        if (!buf || cap == 0) return BML_RESULT_INVALID_ARGUMENT;
        if (out_len) *out_len = len;
        if (cap <= len) {
            std::memcpy(buf, g_ImcMockState.rpc_name.data(), cap - 1);
            buf[cap - 1] = '\0';
            return BML_RESULT_BUFFER_TOO_SMALL;
        }
        std::memcpy(buf, g_ImcMockState.rpc_name.data(), len);
        buf[len] = '\0';
        return BML_RESULT_OK;
    }
    BML_Result MockServiceGetRpcName(BML_Context,
                                     BML_RpcId rpcId,
                                     char *buf,
                                     size_t cap,
                                     size_t *out_len) {
        return MockGetRpcName(rpcId, buf, cap, out_len);
    }

    BML_ImcBusInterface MakeMockBus() {
        BML_ImcBusInterface bus{
            BML_IFACE_HEADER(BML_ImcBusInterface, BML_IMC_BUS_INTERFACE_ID, 1, 0)
        };
        bus.Context = kServiceContext;
        bus.GetTopicId = &MockServiceGetTopicId;
        bus.Publish = &MockPublish;
        bus.PublishEx = &MockPublishEx;
        bus.PublishBuffer = &MockPublishBuffer;
        bus.Subscribe = &MockSubscribe;
        bus.SubscribeEx = &MockSubscribeEx;
        bus.Unsubscribe = &MockUnsubscribe;
        bus.SubscriptionIsActive = &MockSubscriptionIsActive;
        bus.Pump = &MockServicePump;
        bus.GetSubscriptionStats = &MockGetSubscriptionStats;
        bus.GetStats = &MockServiceGetStats;
        bus.ResetStats = &MockServiceResetStats;
        bus.GetTopicInfo = &MockServiceGetTopicInfo;
        bus.GetTopicName = &MockServiceGetTopicName;
        return bus;
    }

    BML_ImcRpcInterface MakeMockRpc() {
        BML_ImcRpcInterface rpc{
            BML_IFACE_HEADER(BML_ImcRpcInterface, BML_IMC_RPC_INTERFACE_ID, 1, 0)
        };
        rpc.Context = kServiceContext;
        rpc.GetRpcId = &MockServiceGetRpcId;
        rpc.RegisterRpc = &MockRegisterRpc;
        rpc.UnregisterRpc = &MockUnregisterRpc;
        rpc.CallRpc = &MockCallRpc;
        rpc.FutureAwait = &MockFutureAwait;
        rpc.FutureGetResult = &MockFutureGetResult;
        rpc.FutureGetState = &MockFutureGetState;
        rpc.FutureCancel = &MockFutureCancel;
        rpc.FutureOnComplete = &MockFutureOnComplete;
        rpc.FutureRelease = &MockFutureRelease;
        rpc.RegisterRpcEx = &MockRegisterRpcEx;
        rpc.CallRpcEx = &MockCallRpcEx;
        rpc.FutureGetError = &MockFutureGetError;
        rpc.GetRpcInfo = &MockServiceGetRpcInfo;
        rpc.GetRpcName = &MockServiceGetRpcName;
        rpc.AddRpcMiddleware = &MockAddRpcMiddleware;
        rpc.RemoveRpcMiddleware = &MockRemoveRpcMiddleware;
        return rpc;
    }

    void ResetImcMockState() {
        g_ImcMockState = ImcMockState{};
    }

    BML_TEST_NOINLINE void ClobberStack() {
        volatile uint8_t scratch[4096] = {};
        for (size_t i = 0; i < sizeof(scratch); ++i) {
            scratch[i] = static_cast<uint8_t>(i);
        }
    }
}

// ============================================================================
// Message Tests
// ============================================================================

TEST(ImcCppWrapperTest, MessageBuilderBasic) {
    int data = 42;
    MessageBuilder builder;
    builder.Typed(data).High().AddFlags(flags::Reliable);
    
    const auto& msg = builder.Build();
    EXPECT_EQ(msg.data, &data);
    EXPECT_EQ(msg.size, sizeof(int));
    EXPECT_EQ(msg.priority, priority::High);
    EXPECT_TRUE(msg.flags & flags::Reliable);
}

TEST(ImcCppWrapperTest, MessageBuilderString) {
    MessageBuilder builder;
    builder.String("Hello, World!").Normal();
    
    const auto& msg = builder.Build();
    EXPECT_EQ(msg.size, 13u);
    EXPECT_EQ(msg.priority, priority::Normal);
}

TEST(ImcCppWrapperTest, MessageBuilderChaining) {
    auto msg = MessageBuilder()
        .Copy("test", 4)
        .SetPriority(priority::Urgent)
        .SetFlags(flags::Ordered | flags::Reliable)
        .SetId(12345)
        .Build();
    
    EXPECT_EQ(msg.size, 4u);
    EXPECT_EQ(msg.priority, priority::Urgent);
    EXPECT_EQ(msg.msg_id, 12345u);
    EXPECT_TRUE(msg.flags & flags::Ordered);
    EXPECT_TRUE(msg.flags & flags::Reliable);
}

TEST(ImcCppWrapperTest, MessageViewAccess) {
    struct TestData {
        int value;
        float delta;
    };
    
    TestData data{42, 3.14f};
    BML_ImcMessage raw = BML_IMC_MSG(&data, sizeof(data));
    raw.priority = BML_IMC_PRIORITY_HIGH;
    
    Message view(&raw);
    
    EXPECT_TRUE(view.Valid());
    EXPECT_FALSE(view.Empty());
    EXPECT_EQ(view.Size(), sizeof(TestData));
    EXPECT_EQ(view.GetPriority(), priority::High);
    
    auto* typed = view.As<TestData>();
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value, 42);
    EXPECT_FLOAT_EQ(typed->delta, 3.14f);
    
    TestData copied{};
    EXPECT_TRUE(view.CopyTo(copied));
    EXPECT_EQ(copied.value, 42);
}

TEST(ImcCppWrapperTest, MessageViewBytes) {
    uint8_t data[] = {1, 2, 3, 4, 5};
    BML_ImcMessage raw = BML_IMC_MSG(data, sizeof(data));
    
    Message view(&raw);
    auto bytes = view.Bytes();
    
    EXPECT_EQ(bytes.size(), 5u);
    EXPECT_EQ(bytes[0], 1);
    EXPECT_EQ(bytes[4], 5);
}

// ============================================================================
// Topic Tests (without actual IMC backend)
// ============================================================================

TEST(ImcCppWrapperTest, TopicDefaultConstruction) {
    Topic topic;
    EXPECT_FALSE(topic.Valid());
    EXPECT_EQ(topic.Id(), InvalidTopicId);
}

TEST(ImcCppWrapperTest, TopicFromId) {
    Topic topic(123u, "TestTopic");
    EXPECT_TRUE(topic.Valid());
    EXPECT_EQ(topic.Id(), 123u);
    EXPECT_EQ(topic.Name(), "TestTopic");
}

TEST(ImcCppWrapperTest, TopicComparison) {
    Topic a(1u);
    Topic b(2u);
    Topic c(1u);
    
    EXPECT_EQ(a, c);
    EXPECT_NE(a, b);
    EXPECT_LT(a, b);
}

TEST(ImcCppWrapperTest, TopicUsesExplicitBus) {
    ResetImcMockState();
    BML_ImcBusInterface bus = MakeMockBus();

    Topic topic("test/topic", &bus, kOwner);
    ASSERT_TRUE(topic.Valid());
    EXPECT_EQ(g_ImcMockState.topic_id, topic.Id());
    EXPECT_EQ("test/topic", g_ImcMockState.last_topic_name);

    int payload = 42;
    EXPECT_TRUE(topic.Publish(payload));
    EXPECT_EQ(1, g_ImcMockState.publish_calls);
    EXPECT_EQ(g_ImcMockState.topic_id, g_ImcMockState.last_topic);

    auto info = topic.Info();
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(3u, info->subscriber_count);
    EXPECT_EQ(12u, info->message_count);
}

// ============================================================================
// SubscribeOptions Tests
// ============================================================================

TEST(ImcCppWrapperTest, SubscribeOptionsBuilder) {
    auto opts = SubscribeOptions()
        .QueueCapacity(512)
        .Backpressure(backpressure::DropNewest)
        .MinPriority(priority::High)
        .Flags(BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE);
    
    const auto& native = opts.Native();
    EXPECT_EQ(native.queue_capacity, 512u);
    EXPECT_EQ(native.backpressure, BML_BACKPRESSURE_DROP_NEWEST);
    EXPECT_EQ(native.min_priority, static_cast<uint32_t>(priority::High));
    EXPECT_EQ(native.flags, BML_IMC_SUBSCRIBE_FLAG_DELIVER_RETAINED_ON_SUBSCRIBE);
}

// ============================================================================
// ZeroCopyBuffer Tests
// ============================================================================

TEST(ImcCppWrapperTest, ZeroCopyBufferCreate) {
    int data = 42;
    auto buffer = ZeroCopyBuffer::Create(&data, sizeof(data));
    
    EXPECT_EQ(buffer.Data(), &data);
    EXPECT_EQ(buffer.Size(), sizeof(int));
}

TEST(ImcCppWrapperTest, ZeroCopyBufferFromVector) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto* dataPtr = data.data();
    
    auto buffer = ZeroCopyBuffer::FromVector(std::move(data));
    
    // Note: data is moved, buffer owns the memory
    EXPECT_EQ(buffer.Size(), 5u);
    // Data pointer may or may not be the same after move
}

// ============================================================================
// Namespace and Type Alias Tests
// ============================================================================

TEST(ImcCppWrapperTest, NamespaceConstants) {
    // Priority constants
    EXPECT_EQ(priority::Low, BML_IMC_PRIORITY_LOW);
    EXPECT_EQ(priority::Normal, BML_IMC_PRIORITY_NORMAL);
    EXPECT_EQ(priority::High, BML_IMC_PRIORITY_HIGH);
    EXPECT_EQ(priority::Urgent, BML_IMC_PRIORITY_URGENT);
    
    // Flag constants
    EXPECT_EQ(flags::None, BML_IMC_FLAG_NONE);
    EXPECT_EQ(flags::NoCopy, BML_IMC_FLAG_NO_COPY);
    EXPECT_EQ(flags::Broadcast, BML_IMC_FLAG_BROADCAST);
    EXPECT_EQ(flags::Reliable, BML_IMC_FLAG_RELIABLE);
    EXPECT_EQ(flags::Ordered, BML_IMC_FLAG_ORDERED);
    
    // Backpressure constants
    EXPECT_EQ(backpressure::DropOldest, BML_BACKPRESSURE_DROP_OLDEST);
    EXPECT_EQ(backpressure::DropNewest, BML_BACKPRESSURE_DROP_NEWEST);
    EXPECT_EQ(backpressure::Block, BML_BACKPRESSURE_BLOCK);
    EXPECT_EQ(backpressure::Fail, BML_BACKPRESSURE_FAIL);
}

// ============================================================================
// Bus Facade Tests (without actual IMC backend)
// ============================================================================

TEST(ImcCppWrapperTest, FreeFunctionsUseExplicitBus) {
    ResetImcMockState();
    BML_ImcBusInterface bus = MakeMockBus();

    EXPECT_TRUE(bml::imc::publish(reinterpret_cast<BML_Mod>(0x1), "test/topic", uint32_t(99), &bus));

    auto stats = bml::imc::getStats(&bus);
    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(10u, stats->total_messages_published);

    bml::imc::pumpAll(&bus);
    EXPECT_EQ(1, g_ImcMockState.pump_calls);

    auto topicName = bml::imc::getTopicName(g_ImcMockState.topic_id, &bus);
    ASSERT_TRUE(topicName.has_value());
    EXPECT_EQ("test/topic", *topicName);
}

// ============================================================================
// RPC Types Tests
// ============================================================================

TEST(ImcCppWrapperTest, RpcDefaultConstruction) {
    Rpc rpc;
    EXPECT_FALSE(rpc.Valid());
    EXPECT_EQ(rpc.Id(), InvalidRpcId);
}

TEST(ImcCppWrapperTest, RpcFromId) {
    Rpc rpc(456u, "TestRpc");
    EXPECT_TRUE(rpc.Valid());
    EXPECT_EQ(rpc.Id(), 456u);
    EXPECT_EQ(rpc.Name(), "TestRpc");
}

TEST(ImcCppWrapperTest, RpcUsesExplicitBus) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    Rpc rpc("test/rpc", &rpcIface);
    ASSERT_TRUE(rpc.Valid());
    EXPECT_EQ(g_ImcMockState.rpc_id, rpc.Id());

    RpcClient client(rpc, &rpcIface, kOwner);
    auto future = client.Call(int(7));
    ASSERT_TRUE(future.Valid());
    EXPECT_TRUE(future.Wait(10));

    auto result = future.Result<int>();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(314, *result);

    bool callbackInvoked = false;
    EXPECT_TRUE(future.OnComplete([](BML_Context, BML_Future, void *userData) {
        auto *flag = static_cast<bool *>(userData);
        *flag = true;
    }, &callbackInvoked));
    EXPECT_TRUE(callbackInvoked);
    EXPECT_TRUE(future.Cancel());
    future.Release();
    EXPECT_EQ(1, g_ImcMockState.future_cancel_calls);
    EXPECT_EQ(1, g_ImcMockState.future_release_calls);

    {
        RpcServer server("test/rpc", [](const Message &) {
            return std::vector<uint8_t>{};
        }, &rpcIface, kOwner);
        EXPECT_TRUE(server.Valid());
    }

    EXPECT_EQ(1, g_ImcMockState.register_rpc_calls);
    EXPECT_EQ(1, g_ImcMockState.unregister_rpc_calls);
}

TEST(ImcCppWrapperTest, RpcClientFromRpcReusesSourceBus) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    Rpc rpc("test/rpc", &rpcIface);
    ASSERT_TRUE(rpc.Valid());

    RpcClient client(rpc, nullptr, kOwner);
    auto future = client.Call(int(7));
    ASSERT_TRUE(future.Valid());
    EXPECT_EQ(1, g_ImcMockState.call_rpc_calls);
}

TEST(ImcCppWrapperTest, RpcFutureResultMessageSurvivesStackClobber) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    RpcClient client("test/rpc", &rpcIface, kOwner);
    auto future = client.Call(int(7));
    ASSERT_TRUE(future.Valid());
    ASSERT_TRUE(future.Wait(10));

    auto result = future.ResultMessage();
    ASSERT_TRUE(result.has_value());

    ClobberStack();

    int value = 0;
    ASSERT_TRUE(result->CopyTo(value));
    EXPECT_EQ(314, value);
}

TEST(ImcCppWrapperTest, RpcHandlerContextReturnsManagedResponseBuffer) {
    bml::imc::detail::RpcHandlerContext ctx;
    ctx.handler = [](const Message &) {
        constexpr int kResponse = 1234;
        std::vector<uint8_t> bytes(sizeof(kResponse));
        std::memcpy(bytes.data(), &kResponse, sizeof(kResponse));
        return bytes;
    };

    int requestValue = 9;
    BML_ImcMessage request = BML_IMC_MSG(&requestValue, sizeof(requestValue));
    BML_ImcBuffer response = BML_IMC_BUFFER_INIT;

    ASSERT_EQ(BML_RESULT_OK,
              bml::imc::detail::RpcHandlerContext::Invoke(nullptr, 0, &request, &response, &ctx));
    ASSERT_NE(nullptr, response.data);
    ASSERT_EQ(sizeof(int), response.size);
    ASSERT_NE(nullptr, response.cleanup);

    int value = 0;
    std::memcpy(&value, response.data, sizeof(value));
    EXPECT_EQ(1234, value);

    response.cleanup(response.data, response.size, response.cleanup_user_data);
}

TEST(ImcCppWrapperTest, RpcFutureDefaultConstruction) {
    RpcFuture future;
    EXPECT_FALSE(future.Valid());
    EXPECT_EQ(future.State(), BML_FUTURE_FAILED);
}

// ============================================================================
// Publisher Tests (without actual IMC backend)
// ============================================================================

TEST(ImcCppWrapperTest, PublisherDefaultConstruction) {
    Publisher<int> pub;
    EXPECT_FALSE(pub.Valid());
}

TEST(ImcCppWrapperTest, MultiPublisherOperations) {
    MultiPublisher multi;
    EXPECT_TRUE(multi.Empty());
    EXPECT_EQ(multi.Count(), 0u);
    
    // Can't add topics without IMC backend, but verify methods exist
    multi.Clear();
    EXPECT_TRUE(multi.Empty());
}

// ============================================================================
// Subscription Manager Tests (without actual IMC backend)
// ============================================================================

TEST(ImcCppWrapperTest, SubscriptionManagerOperations) {
    SubscriptionManager manager;
    EXPECT_TRUE(manager.Empty());
    EXPECT_EQ(manager.Count(), 0u);
    
    manager.Clear();
    EXPECT_TRUE(manager.Empty());
}

TEST(ImcCppWrapperTest, SubscriptionUsesExplicitBus) {
    ResetImcMockState();
    BML_ImcBusInterface bus = MakeMockBus();

    auto sub = Subscription::create("test/topic", [](const Message &) {}, nullptr, &bus, kOwner);
    ASSERT_TRUE(sub.has_value());
    EXPECT_TRUE(sub->IsActive());

    auto stats = sub->Stats();
    ASSERT_TRUE(stats.has_value());
    EXPECT_EQ(5u, stats->messages_received);
    EXPECT_EQ(1u, stats->messages_dropped);

    sub->Unsubscribe();
    EXPECT_EQ(1, g_ImcMockState.subscribe_calls);
    EXPECT_EQ(1, g_ImcMockState.unsubscribe_calls);
}

TEST(ImcCppWrapperTest, SubscriptionCanUseExtendedOnlyBus) {
    ResetImcMockState();
    BML_ImcBusInterface bus = MakeMockBus();
    bus.Subscribe = nullptr;

    auto sub = Subscription::create("test/topic", [](const Message &) {}, nullptr, &bus, kOwner);
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ(1, g_ImcMockState.subscribe_calls);
}

TEST(ImcCppWrapperTest, WrapperGuardsV11MembersByInterfaceSize) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();
    rpcIface.header.struct_size = offsetof(BML_ImcRpcInterface, RegisterRpcEx);

    EXPECT_FALSE(bml::imc::getRpcInfo(g_ImcMockState.rpc_id, &rpcIface).has_value());
    EXPECT_FALSE(bml::imc::getRpcName(g_ImcMockState.rpc_id, &rpcIface).has_value());

    RpcClient client("test/rpc", &rpcIface, kOwner);
    RpcCallOptions opts;
    EXPECT_FALSE(client.Call(int(7), opts).Valid());

    auto future = client.Call(int(7));
    ASSERT_TRUE(future.Valid());
    EXPECT_FALSE(future.Error().has_value());
}

TEST(ImcCppWrapperTest, WrapperRetriesLongNames) {
    ResetImcMockState();
    BML_ImcBusInterface bus = MakeMockBus();
    g_ImcMockState.topic_name.assign(320, 't');
    g_ImcMockState.rpc_name.assign(320, 'r');

    auto topicName = bml::imc::getTopicName(g_ImcMockState.topic_id, &bus);
    ASSERT_TRUE(topicName.has_value());
    EXPECT_EQ(g_ImcMockState.topic_name, *topicName);

    BML_ImcRpcInterface rpcIface = MakeMockRpc();
    g_ImcMockState.rpc_name.assign(320, 'r');
    auto rpcName = bml::imc::getRpcName(g_ImcMockState.rpc_id, &rpcIface);
    ASSERT_TRUE(rpcName.has_value());
    EXPECT_EQ(g_ImcMockState.rpc_name, *rpcName);
}

TEST(ImcCppWrapperTest, SubscriptionCreateCopiesStringViewBeforeLookup) {
    ResetImcMockState();
    BML_ImcBusInterface bus = MakeMockBus();

    std::string storage = "test/topic suffix";
    std::string_view topicView(storage.data(), std::strlen("test/topic"));

    auto sub = Subscription::create(topicView, [](const Message &) {}, nullptr, &bus, kOwner);
    ASSERT_TRUE(sub.has_value());
    EXPECT_EQ("test/topic", g_ImcMockState.last_topic_name);
}

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST(ImcCppWrapperTest, ConvenienceFunctions) {
    // These should compile and not crash (may return nullopt without backend)
    auto stats = bml::imc::getStats();
    (void)stats;
}

TEST(ImcCppWrapperTest, RpcServerCopiesStringViewBeforeLookup) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    std::string storage = "test/rpc suffix";
    std::string_view rpcView(storage.data(), std::strlen("test/rpc"));

    RpcServer server(rpcView, [](const Message &) {
        return std::vector<uint8_t>{};
    }, &rpcIface, kOwner);
    ASSERT_TRUE(server.Valid());
    EXPECT_EQ("test/rpc", g_ImcMockState.last_rpc_name);
}

// ============================================================================
// RPC v1.1 Wrapper Tests
// ============================================================================

TEST(ImcCppWrapperTest, RpcCallOptionsBuilder) {
    RpcCallOptions opts;
    opts.Timeout(500);
    EXPECT_EQ(opts.GetTimeout(), 500u);
    EXPECT_EQ(opts.Native().timeout_ms, 500u);
    EXPECT_EQ(opts.Native().struct_size, sizeof(BML_RpcCallOptions));
}

TEST(ImcCppWrapperTest, RpcClientCallWithOptions) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    RpcClient client("test/rpc", &rpcIface, kOwner);
    ASSERT_TRUE(client.Valid());

    RpcCallOptions opts;
    opts.Timeout(1000);
    int request = 42;
    auto future = client.Call(request, opts);
    EXPECT_EQ(g_ImcMockState.call_rpc_calls, 1);
}

TEST(ImcCppWrapperTest, RpcFutureError) {
    ResetImcMockState();
    BML_ImcBusInterface bus = MakeMockBus();

    // MockFutureGetError returns OK/empty, so Error() should return nullopt
    BML_ImcRpcInterface rpcIface = MakeMockRpc();
    RpcFuture future(g_ImcMockState.future, &rpcIface);
    auto err = future.Error();
    EXPECT_FALSE(err.has_value());

    // Prevent double-release (future handle is a mock pointer)
    (void)future.Native();
}

TEST(ImcCppWrapperTest, RpcServerInfo) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    RpcServer server("test/rpc", [](const Message &) {
        return std::vector<uint8_t>{};
    }, &rpcIface, kOwner);
    ASSERT_TRUE(server.Valid());

    auto info = server.Info();
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->has_handler, BML_TRUE);
}

TEST(ImcCppWrapperTest, RpcServiceManagerDefaultEmpty) {
    RpcServiceManager mgr;
    EXPECT_TRUE(mgr.Empty());
    EXPECT_EQ(mgr.Count(), 0u);
}

TEST(ImcCppWrapperTest, RpcServiceManagerAddAndClear) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    RpcServiceManager mgr(&rpcIface, kOwner);
    EXPECT_TRUE(mgr.AddServer("test/rpc", [](const Message &) {
        return std::vector<uint8_t>{};
    }));
    EXPECT_EQ(mgr.Count(), 1u);

    mgr.Clear();
    EXPECT_TRUE(mgr.Empty());
}

TEST(ImcCppWrapperTest, RpcGetInfoFreeFunction) {
    ResetImcMockState();
    BML_ImcBusInterface bus = MakeMockBus();

    BML_ImcRpcInterface rpcIface = MakeMockRpc();
    auto info = bml::imc::getRpcInfo(g_ImcMockState.rpc_id, &rpcIface);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->rpc_id, g_ImcMockState.rpc_id);
}

TEST(ImcCppWrapperTest, RpcGetNameFreeFunction) {
    ResetImcMockState();
    BML_ImcBusInterface bus = MakeMockBus();

    BML_ImcRpcInterface rpcIface = MakeMockRpc();
    auto name = bml::imc::getRpcName(g_ImcMockState.rpc_id, &rpcIface);
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "test/rpc");
}

TEST(ImcCppWrapperTest, RpcServiceManagerAddServerAndCall) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    RpcServiceManager mgr(&rpcIface, kOwner);
    EXPECT_TRUE(mgr.AddServer("test/rpc", [](const Message &) {
        return std::vector<uint8_t>{1, 2, 3};
    }));

    auto client = mgr.CreateClient("test/rpc");
    EXPECT_TRUE(client.Valid());
    auto future = client.Call(int(42));
    EXPECT_TRUE(future.Valid());
    EXPECT_EQ(g_ImcMockState.call_rpc_calls, 1);
}

TEST(ImcCppWrapperTest, RpcMiddlewareRegistrationRAII) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    // AddRpcMiddleware mock just returns OK
    {
    RpcMiddlewareRegistration reg(
            [](BML_Context, BML_RpcId, BML_Bool, const BML_ImcMessage *,
               const BML_ImcBuffer *, BML_Result, void *) -> BML_Result {
                return BML_RESULT_OK;
            },
            0, nullptr, &rpcIface, kOwner);
        EXPECT_TRUE(reg.Valid());
    }
    // Destructor calls RemoveRpcMiddleware -- no crash = success
}

TEST(ImcCppWrapperTest, RpcMiddlewareRegistrationMoveOnly) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    auto mw = [](BML_Context, BML_RpcId, BML_Bool, const BML_ImcMessage *,
                  const BML_ImcBuffer *, BML_Result, void *) -> BML_Result {
        return BML_RESULT_OK;
    };

    RpcMiddlewareRegistration reg1(mw, 0, nullptr, &rpcIface, kOwner);
    EXPECT_TRUE(reg1.Valid());

    RpcMiddlewareRegistration reg2(std::move(reg1));
    EXPECT_TRUE(reg2.Valid());
    EXPECT_FALSE(reg1.Valid()); // NOLINT(bugprone-use-after-move)
}

TEST(ImcCppWrapperTest, RpcServiceManagerManagedMiddleware) {
    ResetImcMockState();
    BML_ImcRpcInterface rpcIface = MakeMockRpc();

    RpcServiceManager mgr(&rpcIface, kOwner);
    EXPECT_TRUE(mgr.AddMiddleware(
        [](BML_Context, BML_RpcId, BML_Bool, const BML_ImcMessage *,
           const BML_ImcBuffer *, BML_Result, void *) -> BML_Result {
            return BML_RESULT_OK;
        },
        0));
    EXPECT_EQ(mgr.Count(), 0u); // Count only counts servers, not middleware

    mgr.Clear(); // Should remove middleware without crash
    EXPECT_TRUE(mgr.Empty());
}

// ============================================================================
// Typed Message Tests
// ============================================================================

TEST(ImcMessageTest, AsReturnsNullptrOnTypeMismatch) {
    struct FooEvent { uint32_t value; };
    struct BarEvent { float x; float y; };

    FooEvent foo{42};
    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    msg.data = &foo;
    msg.size = sizeof(foo);
    msg.payload_type_id = bml::imc::PayloadType<FooEvent>::Id;

    bml::imc::Message wrapped(&msg);

    // Correct type: should succeed
    const FooEvent *result = wrapped.As<FooEvent>();
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->value, 42u);

    // Wrong type: should return nullptr
    const BarEvent *wrong = wrapped.As<BarEvent>();
    EXPECT_EQ(wrong, nullptr);

    // Untyped message: As<T>() should still work (type_id=0 bypasses type check)
    BML_ImcMessage untyped = BML_IMC_MESSAGE_INIT;
    untyped.data = &foo;
    untyped.size = sizeof(foo);
    untyped.payload_type_id = BML_PAYLOAD_TYPE_NONE;
    bml::imc::Message untypedWrapped(&untyped);
    EXPECT_NE(untypedWrapped.As<FooEvent>(), nullptr);  // same size, no type check
    // BarEvent is 8 bytes but buffer is only 4, so size check rejects it
    // (type check is bypassed since payload_type_id==0, but size check still applies)
    EXPECT_EQ(untypedWrapped.As<BarEvent>(), nullptr);

    // Verify that untyped message with sufficient size allows any type cast
    struct AltEvent { uint32_t v; };
    EXPECT_NE(untypedWrapped.As<AltEvent>(), nullptr);  // same size, untyped allows cast
}

TEST(PayloadTypeTest, DifferentTypesHaveDifferentIds) {
    struct TypeA { int x; };
    struct TypeB { float y; };

    constexpr uint32_t idA = bml::imc::PayloadType<TypeA>::Id;
    constexpr uint32_t idB = bml::imc::PayloadType<TypeB>::Id;
    constexpr uint32_t idVoid = bml::imc::PayloadType<void>::Id;

    EXPECT_NE(idA, idB);
    EXPECT_NE(idA, idVoid);
    EXPECT_NE(idB, idVoid);
    EXPECT_EQ(idVoid, BML_PAYLOAD_TYPE_NONE);

    // Same type should always produce the same ID
    EXPECT_EQ(idA, bml::imc::PayloadType<TypeA>::Id);
}

TEST(PayloadTypeTest, PayloadTypeIdFlowsThroughMessage) {
    struct MyPayload { int32_t data; };

    MyPayload payload{123};
    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    msg.data = &payload;
    msg.size = sizeof(payload);
    msg.payload_type_id = bml::imc::PayloadType<MyPayload>::Id;

    bml::imc::Message wrapped(&msg);
    EXPECT_EQ(wrapped.PayloadTypeId(), bml::imc::PayloadType<MyPayload>::Id);
    EXPECT_NE(wrapped.PayloadTypeId(), BML_PAYLOAD_TYPE_NONE);
}
