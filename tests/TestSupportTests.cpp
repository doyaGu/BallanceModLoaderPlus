/**
 * @file TestSupportTests.cpp
 * @brief Tests for the bml_test infrastructure itself
 *
 * Validates that TestContext bootstraps/shuts down cleanly, can create
 * mock modules, tick IMC, do pub/sub round-trips, and capture logs.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <string>

#include "bml_test.hpp"
#include "bml_imc.h"

// ============================================================================
// Test: Bootstrap and Shutdown
// ============================================================================

TEST(TestSupportTests, TestContext_BootstrapAndShutdown) {
    // Constructor bootstraps, destructor shuts down -- should not crash
    bml::test::TestContext ctx;
    EXPECT_NE(nullptr, ctx.Handle());
}

// ============================================================================
// Test: CreateMockMod
// ============================================================================

TEST(TestSupportTests, TestContext_CreateMockMod) {
    bml::test::TestContext ctx;
    BML_Mod mod = ctx.CreateMockMod("com.test.mockmod");
    ASSERT_NE(nullptr, mod);

    // Creating a second mock with a different ID should also work
    BML_Mod mod2 = ctx.CreateMockMod("com.test.mockmod2");
    ASSERT_NE(nullptr, mod2);
    EXPECT_NE(mod, mod2);
}

// ============================================================================
// Test: Tick
// ============================================================================

TEST(TestSupportTests, TestContext_Tick) {
    bml::test::TestContext ctx;
    // Tick should not crash even with no subscribers or timers
    ctx.Tick();
    ctx.Tick();
    ctx.Tick();
}

// ============================================================================
// Test: Publish and Subscribe Round-Trip
// ============================================================================

namespace {
    struct PubSubState {
        std::atomic<uint32_t> call_count{0};
        uint32_t last_value{0};
    };

    void TestPubSubHandler(BML_Context /*ctx*/,
                           BML_TopicId /*topic*/,
                           const BML_ImcMessage *msg,
                           void *user_data) {
        auto *state = static_cast<PubSubState *>(user_data);
        if (!state || !msg || !msg->data || msg->size != sizeof(uint32_t)) {
            return;
        }
        std::memcpy(&state->last_value, msg->data, sizeof(uint32_t));
        state->call_count.fetch_add(1, std::memory_order_relaxed);
    }
} // namespace

TEST(TestSupportTests, TestContext_PublishSubscribeRoundTrip) {
    bml::test::TestContext ctx;

    // Resolve topic
    auto getTopicId = reinterpret_cast<PFN_BML_ImcGetTopicId>(
        bmlGetProcAddress("bmlImcGetTopicId"));
    auto subscribe = reinterpret_cast<PFN_BML_ImcSubscribe>(
        bmlGetProcAddress("bmlImcSubscribe"));
    auto unsubscribe = reinterpret_cast<PFN_BML_ImcUnsubscribe>(
        bmlGetProcAddress("bmlImcUnsubscribe"));
    ASSERT_NE(nullptr, getTopicId);
    ASSERT_NE(nullptr, subscribe);
    ASSERT_NE(nullptr, unsubscribe);

    BML_TopicId topic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK, getTopicId("test/support/pubsub", &topic));
    ASSERT_NE(BML_TOPIC_ID_INVALID, topic);

    // Subscribe
    PubSubState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, subscribe(topic, TestPubSubHandler, &state, &sub));
    ASSERT_NE(nullptr, sub);

    // Publish via TestContext convenience
    uint32_t payload = 99;
    ASSERT_TRUE(ctx.Publish("test/support/pubsub", payload));

    // Tick to deliver
    ctx.Tick();

    EXPECT_EQ(1u, state.call_count.load());
    EXPECT_EQ(99u, state.last_value);

    // Cleanup
    ASSERT_EQ(BML_RESULT_OK, unsubscribe(sub));
}

// ============================================================================
// Test: Log Capture
// ============================================================================

TEST(TestSupportTests, TestContext_LogCapture) {
    bml::test::TestContext ctx;
    ctx.EnableLogCapture(BML_LOG_TRACE);

    // Log a message via the API
    auto logFn = reinterpret_cast<PFN_BML_Log>(
        bmlGetProcAddress("bmlLog"));
    ASSERT_NE(nullptr, logFn);

    logFn(ctx.Handle(), BML_LOG_INFO, "test.capture", "hello from test %d", 42);

    EXPECT_FALSE(ctx.CapturedLogs().empty());
    EXPECT_TRUE(ctx.HasLog("hello from test 42"));

    // Clear and verify
    ctx.ClearLogs();
    EXPECT_TRUE(ctx.CapturedLogs().empty());
}

// ============================================================================
// Test: Multiple Sequential Contexts
// ============================================================================

TEST(TestSupportTests, TestContext_MultipleSequentialContexts) {
    // First context
    {
        bml::test::TestContext ctx1;
        EXPECT_NE(nullptr, ctx1.Handle());
        BML_Mod mod1 = ctx1.CreateMockMod("com.test.first");
        EXPECT_NE(nullptr, mod1);
        ctx1.Tick();
    }

    // Second context after first is destroyed
    {
        bml::test::TestContext ctx2;
        EXPECT_NE(nullptr, ctx2.Handle());
        BML_Mod mod2 = ctx2.CreateMockMod("com.test.second");
        EXPECT_NE(nullptr, mod2);
        ctx2.Tick();
    }
}

// ============================================================================
// Test: Hub and Services Access
// ============================================================================

TEST(TestSupportTests, TestContext_HubAndServicesAccess) {
    bml::test::TestContext ctx;
    ASSERT_NE(nullptr, ctx.Hub());

    const auto &builtins = ctx.Hub()->Builtins();
    EXPECT_NE(nullptr, builtins.Context);
    EXPECT_NE(nullptr, builtins.Logging);
    EXPECT_NE(nullptr, builtins.ImcBus);

    BML_Mod mod = ctx.CreateMockMod("com.test.services");
    ASSERT_NE(nullptr, mod);

    auto services = ctx.CreateServices(mod);
    EXPECT_TRUE(static_cast<bool>(services));
}
