/**
 * @file BMLIntegrationTests.cpp
 * @brief Integration tests that link against the real BML.dll target
 *
 * Unlike unit tests that compile individual .cpp files, these tests exercise
 * the exported API surface of the built BML shared library, verifying that
 * bootstrap, API lookup, IMC pub/sub, extension registration, and diagnostics
 * work end-to-end.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

#include "bml_export.h"
#include "bml_bootstrap.h"
#include "bml_imc.h"
#include "bml_extension.h"
#include "bml_topics.h"

// ========================================================================
// Test Fixture
// ========================================================================

class BMLIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Bootstrap core only, skip module discovery and loading
        BML_BootstrapConfig config = BML_BOOTSTRAP_CONFIG_INIT;
        config.flags = BML_BOOTSTRAP_FLAG_SKIP_DISCOVERY | BML_BOOTSTRAP_FLAG_SKIP_LOAD;
        BML_Result res = bmlBootstrap(&config);
        ASSERT_EQ(res, BML_RESULT_OK) << "Bootstrap failed";
    }

    void TearDown() override {
        bmlShutdown();
    }
};

// ========================================================================
// Test: CoreBootstrapAndState
// ========================================================================

TEST_F(BMLIntegrationTest, CoreBootstrapAndState) {
    BML_BootstrapState state = bmlGetBootstrapState();
    // With SKIP_DISCOVERY | SKIP_LOAD, state should be CORE_INITIALIZED
    EXPECT_EQ(state, BML_BOOTSTRAP_STATE_CORE_INITIALIZED);
}

// ========================================================================
// Test: ProcAddressLookup
// ========================================================================

TEST_F(BMLIntegrationTest, ProcAddressLookup) {
    // IMC APIs should be registered
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcGetTopicId"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcPublish"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcSubscribe"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcUnsubscribe"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlImcPump"));

    // Extension APIs should be registered
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlExtensionRegister"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlExtensionLoad"));
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlExtensionUnregister"));

    // Config APIs should be registered
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlConfigGet"));

    // Non-existent API returns null
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlNonExistentFunction"));
    EXPECT_EQ(nullptr, bmlGetProcAddress(nullptr));
}

// ========================================================================
// Test: ProcAddressByIdLookup
// ========================================================================

TEST_F(BMLIntegrationTest, ProcAddressByIdLookup) {
    // ID-based lookup should match string-based lookup
    void *byName = bmlGetProcAddress("bmlImcPublish");
    void *byId = bmlGetProcAddressById(BML_API_ID_bmlImcPublish);
    EXPECT_EQ(byName, byId);
    EXPECT_NE(nullptr, byId);

    // Another API
    void *topicByName = bmlGetProcAddress("bmlImcGetTopicId");
    void *topicById = bmlGetProcAddressById(BML_API_ID_bmlImcGetTopicId);
    EXPECT_EQ(topicByName, topicById);
    EXPECT_NE(nullptr, topicById);

    // Extension API
    void *extByName = bmlGetProcAddress("bmlExtensionRegister");
    void *extById = bmlGetProcAddressById(BML_API_ID_bmlExtensionRegister);
    EXPECT_EQ(extByName, extById);
}

// ========================================================================
// Test: FullIMCPublishSubscribeCycle
// ========================================================================

namespace {
    struct TestHandlerState {
        std::atomic<uint32_t> call_count{0};
        uint32_t last_payload{0};
    };

    void TestImcHandler(BML_Context ctx,
                        BML_TopicId topic,
                        const BML_ImcMessage *msg,
                        void *user_data) {
        (void)ctx;
        (void)topic;
        auto *state = static_cast<TestHandlerState *>(user_data);
        if (!state || !msg)
            return;
        if (msg->data && msg->size == sizeof(uint32_t)) {
            std::memcpy(&state->last_payload, msg->data, sizeof(uint32_t));
        }
        state->call_count.fetch_add(1, std::memory_order_relaxed);
    }
} // namespace

TEST_F(BMLIntegrationTest, FullIMCPublishSubscribeCycle) {
    auto getTopicId = (PFN_BML_ImcGetTopicId)bmlGetProcAddress("bmlImcGetTopicId");
    auto publish = (PFN_BML_ImcPublish)bmlGetProcAddress("bmlImcPublish");
    auto subscribe = (PFN_BML_ImcSubscribe)bmlGetProcAddress("bmlImcSubscribe");
    auto unsubscribe = (PFN_BML_ImcUnsubscribe)bmlGetProcAddress("bmlImcUnsubscribe");
    auto pump = (PFN_BML_ImcPump)bmlGetProcAddress("bmlImcPump");

    ASSERT_NE(nullptr, getTopicId);
    ASSERT_NE(nullptr, publish);
    ASSERT_NE(nullptr, subscribe);
    ASSERT_NE(nullptr, unsubscribe);
    ASSERT_NE(nullptr, pump);

    // Resolve topic
    BML_TopicId topic = 0;
    ASSERT_EQ(BML_RESULT_OK, getTopicId("test/integration/pubsub", &topic));
    EXPECT_NE(BML_TOPIC_ID_INVALID, topic);

    // Subscribe
    TestHandlerState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK, subscribe(topic, TestImcHandler, &state, &sub));
    ASSERT_NE(nullptr, sub);

    // Publish
    uint32_t payload = 42;
    ASSERT_EQ(BML_RESULT_OK, publish(topic, &payload, sizeof(payload)));

    // Pump to deliver
    pump(100);

    EXPECT_EQ(1u, state.call_count.load());
    EXPECT_EQ(42u, state.last_payload);

    // Cleanup
    ASSERT_EQ(BML_RESULT_OK, unsubscribe(sub));
}

// ========================================================================
// Test: TopicIdConsistency
// ========================================================================

TEST_F(BMLIntegrationTest, TopicIdConsistency) {
    auto getTopicId = (PFN_BML_ImcGetTopicId)bmlGetProcAddress("bmlImcGetTopicId");
    ASSERT_NE(nullptr, getTopicId);

    BML_TopicId id1 = 0, id2 = 0, id3 = 0;
    ASSERT_EQ(BML_RESULT_OK, getTopicId("test/consistency/a", &id1));
    ASSERT_EQ(BML_RESULT_OK, getTopicId("test/consistency/a", &id2));
    ASSERT_EQ(BML_RESULT_OK, getTopicId("test/consistency/b", &id3));

    // Same string -> same ID
    EXPECT_EQ(id1, id2);
    // Different strings -> different IDs
    EXPECT_NE(id1, id3);
}

// ========================================================================
// Test: ExtensionRegisterLoadCycle
// ========================================================================

namespace {
    struct TestExtApi {
        int (*add)(int a, int b);
        int (*multiply)(int a, int b);
    };

    int testAdd(int a, int b) { return a + b; }
    int testMultiply(int a, int b) { return a * b; }
} // namespace

TEST_F(BMLIntegrationTest, ExtensionRegisterLoadCycle) {
    auto extRegister = (PFN_BML_ExtensionRegister)bmlGetProcAddress("bmlExtensionRegister");
    auto extLoad = (PFN_BML_ExtensionLoad)bmlGetProcAddress("bmlExtensionLoad");
    auto extUnload = (PFN_BML_ExtensionUnload)bmlGetProcAddress("bmlExtensionUnload");
    auto extUnregister = (PFN_BML_ExtensionUnregister)bmlGetProcAddress("bmlExtensionUnregister");

    ASSERT_NE(nullptr, extRegister);
    ASSERT_NE(nullptr, extLoad);
    ASSERT_NE(nullptr, extUnload);
    ASSERT_NE(nullptr, extUnregister);

    // Register a test extension
    static TestExtApi s_api = {testAdd, testMultiply};

    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "BML_TEST_Integration";
    desc.version.major = 1;
    desc.version.minor = 0;
    desc.version.patch = 0;
    desc.api_table = &s_api;
    desc.api_size = sizeof(TestExtApi);
    desc.description = "Test extension for integration testing";

    ASSERT_EQ(BML_RESULT_OK, extRegister(&desc));

    // Load it back
    TestExtApi *loaded = nullptr;
    BML_Version req = {1, 0, 0};
    ASSERT_EQ(BML_RESULT_OK, extLoad("BML_TEST_Integration", &req, (void **)&loaded, nullptr));
    ASSERT_NE(nullptr, loaded);

    // Verify API table functions work
    EXPECT_EQ(5, loaded->add(2, 3));
    EXPECT_EQ(12, loaded->multiply(3, 4));

    // Cleanup
    ASSERT_EQ(BML_RESULT_OK, extUnload("BML_TEST_Integration"));
    ASSERT_EQ(BML_RESULT_OK, extUnregister("BML_TEST_Integration"));
}

// ========================================================================
// Test: DiagnosticsAvailable
// ========================================================================

TEST_F(BMLIntegrationTest, DiagnosticsAvailable) {
    const BML_BootstrapDiagnostics *diag = bmlGetBootstrapDiagnostics();
    ASSERT_NE(nullptr, diag);
}
