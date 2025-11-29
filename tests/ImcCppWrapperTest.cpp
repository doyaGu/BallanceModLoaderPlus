/**
 * @file ImcCppWrapperTest.cpp
 * @brief Tests for the C++ IMC wrapper API
 */

// Define BML_LOADER_IMPLEMENTATION to get the function pointer definitions
// (they will all be NULL since we don't actually load the BML runtime)
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

#include <gtest/gtest.h>
#include "bml_imc.hpp"

using namespace bml::imc;

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

// ============================================================================
// SubscribeOptions Tests
// ============================================================================

TEST(ImcCppWrapperTest, SubscribeOptionsBuilder) {
    auto opts = SubscribeOptions()
        .QueueCapacity(512)
        .Backpressure(backpressure::DropNewest)
        .MinPriority(priority::High);
    
    const auto& native = opts.Native();
    EXPECT_EQ(native.queue_capacity, 512u);
    EXPECT_EQ(native.backpressure, BML_BACKPRESSURE_DROP_NEWEST);
    EXPECT_EQ(native.min_priority, static_cast<uint32_t>(priority::High));
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

TEST(ImcCppWrapperTest, BusTopicRegistryAccess) {
    auto& registry = Bus::GetTopicRegistry();
    EXPECT_EQ(registry.Size(), registry.Size());  // Just verify it's accessible
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

// ============================================================================
// Convenience Function Tests
// ============================================================================

TEST(ImcCppWrapperTest, ConvenienceFunctions) {
    // These should compile and not crash (may return nullopt without backend)
    auto caps = bml::imc::Bus::GetCapabilities();
    auto stats = bml::imc::Bus::GetStatistics();
    
    // hasCapability should be callable
    bool hasPubSub = bml::imc::Bus::HasCapability(BML_IMC_CAP_PUBSUB);
    (void)hasPubSub;  // May be false without backend
}


