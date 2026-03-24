/**
 * @file EventInterceptionTests.cpp
 * @brief Unit tests for event interception/cancellation in the IMC bus
 */

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <vector>

#include "bml_imc.h"
#include "Core/Context.h"
#include "Core/ImcBus.h"
#include "TestKernel.h"
#include "TestKernelBuilder.h"
#include "TestModHelper.h"

using namespace BML::Core;
using BML::Core::Testing::TestKernel;
using BML::Core::Testing::TestKernelBuilder;
using BML::Core::Testing::TestModHelper;

class EventInterceptionTest : public ::testing::Test {
protected:
    TestKernel kernel_;
    std::unique_ptr<TestModHelper> mods_;

    void SetUp() override {
        kernel_ = TestKernelBuilder()
            .WithConfig()
            .WithImcBus()
            .Build();
        mods_ = std::make_unique<TestModHelper>(*kernel_);
        host_mod_ = mods_->HostMod();
        Context::SetLifecycleModule(host_mod_);
    }

    void TearDown() override {
        Context::SetLifecycleModule(nullptr);
    }

    BML_Mod host_mod_ = nullptr;

    BML_TopicId GetTopic(const char *name) {
        BML_TopicId id = BML_TOPIC_ID_INVALID;
        ImcGetTopicId(*kernel_, name, &id);
        return id;
    }

    void Pump(size_t maxPerSub = 0) {
        ImcPump(*kernel_, maxPerSub);
    }

    BML_Result SubscribeIntercept(BML_TopicId topic,
                                  BML_ImcInterceptHandler handler,
                                  void *userData,
                                  BML_Subscription *outSub) {
        return ImcSubscribeIntercept(host_mod_, topic, handler, userData, outSub);
    }

    BML_Result SubscribeInterceptEx(BML_TopicId topic,
                                    BML_ImcInterceptHandler handler,
                                    void *userData,
                                    const BML_SubscribeOptions *options,
                                    BML_Subscription *outSub) {
        return ImcSubscribeInterceptEx(host_mod_, topic, handler, userData, options, outSub);
    }

    BML_Result SubscribeRegular(BML_TopicId topic,
                                BML_ImcHandler handler,
                                void *userData,
                                BML_Subscription *outSub) {
        return ImcSubscribe(host_mod_, topic, handler, userData, outSub);
    }

    BML_Result PublishRegular(BML_TopicId topic, const void *data, size_t size) {
        return ImcPublish(host_mod_, topic, data, size);
    }

    BML_Result PublishInterceptable(BML_TopicId topic,
                                    BML_ImcMessage *msg,
                                    BML_EventResult *outResult) {
        return ImcPublishInterceptable(host_mod_, topic, msg, outResult);
    }
};

// Simple regular handler that records calls
static std::atomic<int> g_RegularCallCount{0};
static void RegularHandler(BML_Context, BML_TopicId, const BML_ImcMessage *, void *) {
    g_RegularCallCount.fetch_add(1);
}

// Intercept handler that continues
static BML_EventResult InterceptContinue(BML_Context, BML_TopicId, BML_ImcMessage *, void *) {
    return BML_EVENT_CONTINUE;
}

// Intercept handler that cancels
static BML_EventResult InterceptCancel(BML_Context, BML_TopicId, BML_ImcMessage *, void *user_data) {
    auto *counter = static_cast<int *>(user_data);
    if (counter) ++(*counter);
    return BML_EVENT_CANCEL;
}

// Intercept handler that marks as handled
static BML_EventResult InterceptHandled(BML_Context, BML_TopicId, BML_ImcMessage *, void *user_data) {
    auto *counter = static_cast<int *>(user_data);
    if (counter) ++(*counter);
    return BML_EVENT_HANDLED;
}

TEST_F(EventInterceptionTest, ContinuePassesToRegular) {
    auto topic = GetTopic("Test/Intercept/Continue");

    // Subscribe intercept handler
    BML_Subscription intercept_sub;
    ASSERT_EQ(SubscribeIntercept(topic, InterceptContinue, nullptr, &intercept_sub),
              BML_RESULT_OK);

    // Subscribe regular handler
    BML_Subscription reg_sub;
    ASSERT_EQ(SubscribeRegular(topic, RegularHandler, nullptr, &reg_sub),
              BML_RESULT_OK);

    g_RegularCallCount.store(0);

    // Publish interceptable
    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    BML_EventResult result;
    ASSERT_EQ(PublishInterceptable(topic, &msg, &result), BML_RESULT_OK);
    EXPECT_EQ(result, BML_EVENT_CONTINUE);

    // Pump to deliver to regular subscribers
    Pump(0);
    EXPECT_EQ(g_RegularCallCount.load(), 1);
}

TEST_F(EventInterceptionTest, CancelPreventsRegularDelivery) {
    auto topic = GetTopic("Test/Intercept/Cancel");

    int cancel_count = 0;
    BML_Subscription intercept_sub;
    ASSERT_EQ(SubscribeIntercept(topic, InterceptCancel, &cancel_count, &intercept_sub),
              BML_RESULT_OK);

    BML_Subscription reg_sub;
    ASSERT_EQ(SubscribeRegular(topic, RegularHandler, nullptr, &reg_sub),
              BML_RESULT_OK);

    g_RegularCallCount.store(0);

    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    BML_EventResult result;
    ASSERT_EQ(PublishInterceptable(topic, &msg, &result), BML_RESULT_OK);
    EXPECT_EQ(result, BML_EVENT_CANCEL);
    EXPECT_EQ(cancel_count, 1);

    // Pump -- regular handler should NOT have received the message
    Pump(0);
    EXPECT_EQ(g_RegularCallCount.load(), 0);
}

TEST_F(EventInterceptionTest, HandledStopsPropagationButDelivers) {
    auto topic = GetTopic("Test/Intercept/Handled");

    int handled_count = 0;
    BML_Subscription intercept_sub;
    ASSERT_EQ(SubscribeIntercept(topic, InterceptHandled, &handled_count, &intercept_sub),
              BML_RESULT_OK);

    BML_Subscription reg_sub;
    ASSERT_EQ(SubscribeRegular(topic, RegularHandler, nullptr, &reg_sub),
              BML_RESULT_OK);

    g_RegularCallCount.store(0);

    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    BML_EventResult result;
    ASSERT_EQ(PublishInterceptable(topic, &msg, &result), BML_RESULT_OK);
    EXPECT_EQ(result, BML_EVENT_HANDLED);
    EXPECT_EQ(handled_count, 1);

    // HANDLED still delivers to regular subscribers
    Pump(0);
    EXPECT_EQ(g_RegularCallCount.load(), 1);
}

// Order tracking for execution_order test
static std::vector<int> g_OrderLog;
static std::mutex g_OrderMutex;

static BML_EventResult InterceptRecordOrder(BML_Context, BML_TopicId, BML_ImcMessage *, void *user_data) {
    int order = *static_cast<int *>(user_data);
    std::lock_guard lock(g_OrderMutex);
    g_OrderLog.push_back(order);
    return BML_EVENT_CONTINUE;
}

TEST_F(EventInterceptionTest, ExecutionOrderRespected) {
    auto topic = GetTopic("Test/Intercept/Order");

    {
        std::lock_guard lock(g_OrderMutex);
        g_OrderLog.clear();
    }

    // Subscribe in reverse order with explicit execution_order
    static int order_high = 100;
    static int order_low = -50;
    static int order_mid = 0;

    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;

    opts.execution_order = 100;
    BML_Subscription sub_high;
    ASSERT_EQ(SubscribeInterceptEx(topic, InterceptRecordOrder, &order_high, &opts, &sub_high),
              BML_RESULT_OK);

    opts.execution_order = -50;
    BML_Subscription sub_low;
    ASSERT_EQ(SubscribeInterceptEx(topic, InterceptRecordOrder, &order_low, &opts, &sub_low),
              BML_RESULT_OK);

    opts.execution_order = 0;
    BML_Subscription sub_mid;
    ASSERT_EQ(SubscribeInterceptEx(topic, InterceptRecordOrder, &order_mid, &opts, &sub_mid),
              BML_RESULT_OK);

    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    BML_EventResult result;
    ASSERT_EQ(PublishInterceptable(topic, &msg, &result), BML_RESULT_OK);

    std::lock_guard lock(g_OrderMutex);
    ASSERT_EQ(g_OrderLog.size(), 3u);
    // Should be sorted by execution_order: -50, 0, 100
    EXPECT_EQ(g_OrderLog[0], -50);
    EXPECT_EQ(g_OrderLog[1], 0);
    EXPECT_EQ(g_OrderLog[2], 100);
}

TEST_F(EventInterceptionTest, CancelStopsLaterInterceptors) {
    auto topic = GetTopic("Test/Intercept/CancelChain");

    {
        std::lock_guard lock(g_OrderMutex);
        g_OrderLog.clear();
    }

    static int order_first = 1;
    static int order_second = 2;

    BML_SubscribeOptions opts = BML_SUBSCRIBE_OPTIONS_INIT;

    // First interceptor cancels
    opts.execution_order = 0;
    BML_Subscription sub1;
    ASSERT_EQ(SubscribeInterceptEx(topic, InterceptCancel, &order_first, &opts, &sub1),
              BML_RESULT_OK);

    // Second interceptor should never be called
    opts.execution_order = 10;
    BML_Subscription sub2;
    ASSERT_EQ(SubscribeInterceptEx(topic, InterceptRecordOrder, &order_second, &opts, &sub2),
              BML_RESULT_OK);

    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    BML_EventResult result;
    ASSERT_EQ(PublishInterceptable(topic, &msg, &result), BML_RESULT_OK);
    EXPECT_EQ(result, BML_EVENT_CANCEL);

    // Second interceptor was never called
    std::lock_guard lock(g_OrderMutex);
    EXPECT_TRUE(g_OrderLog.empty());
}

TEST_F(EventInterceptionTest, RegularPublishIgnoresInterceptors) {
    auto topic = GetTopic("Test/Intercept/RegularIgnores");

    int cancel_count = 0;
    BML_Subscription intercept_sub;
    ASSERT_EQ(SubscribeIntercept(topic, InterceptCancel, &cancel_count, &intercept_sub),
              BML_RESULT_OK);

    BML_Subscription reg_sub;
    ASSERT_EQ(SubscribeRegular(topic, RegularHandler, nullptr, &reg_sub),
              BML_RESULT_OK);

    g_RegularCallCount.store(0);

    // Regular Publish should NOT invoke intercept handlers
    int data = 42;
    ASSERT_EQ(BML_RESULT_OK, PublishRegular(topic, &data, sizeof(data)));
    Pump(0);

    // Regular handler should have been called, intercept should not
    EXPECT_EQ(g_RegularCallCount.load(), 1);
    EXPECT_EQ(cancel_count, 0);
}

TEST_F(EventInterceptionTest, NullOutResultOk) {
    auto topic = GetTopic("Test/Intercept/NullResult");

    BML_ImcMessage msg = BML_IMC_MESSAGE_INIT;
    // Should succeed even with null out_result
    EXPECT_EQ(PublishInterceptable(topic, &msg, nullptr), BML_RESULT_OK);
}
