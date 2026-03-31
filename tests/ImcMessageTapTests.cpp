/**
 * @file ImcMessageTapTests.cpp
 * @brief Tests for IMC Message Tap diagnostic API
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <vector>

#include "Core/Context.h"
#include "Core/ImcBus.h"
#include "Core/ImcBusInternal.h"
#include "TestKernel.h"
#include "TestKernelBuilder.h"
#include "TestModHelper.h"

namespace {

using namespace BML::Core;
using BML::Core::Context;
using BML::Core::Testing::TestKernel;
using BML::Core::Testing::TestKernelBuilder;
using BML::Core::Testing::TestModHelper;

struct TapCapture {
    std::vector<BML_ImcMessageTrace> traces;
    std::atomic<uint32_t> call_count{0};
};

void CollectingTap(const BML_ImcMessageTrace *trace, void *user_data) {
    auto *state = static_cast<TapCapture *>(user_data);
    if (!state || !trace) return;
    state->traces.push_back(*trace);
    state->call_count.fetch_add(1, std::memory_order_relaxed);
}

class ImcMessageTapTest : public ::testing::Test {
protected:
    TestKernel kernel_;
    std::unique_ptr<TestModHelper> mods_;
    BML_Mod host_mod_{nullptr};

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
};

TEST_F(ImcMessageTapTest, TapReceivesPublishedMessage) {
    TapCapture cap;
    ASSERT_EQ(BML_RESULT_OK,
              ImcRegisterMessageTap(*kernel_, host_mod_, CollectingTap, &cap));

    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId(*kernel_, "test/tap", &topic));

    uint32_t payload = 42;
    ASSERT_EQ(BML_RESULT_OK,
              ImcPublish(host_mod_, topic, &payload, sizeof(payload)));

    ASSERT_EQ(1u, cap.call_count.load());
    EXPECT_EQ(topic, cap.traces[0].topic);
    EXPECT_EQ(host_mod_, cap.traces[0].owner);
    EXPECT_EQ(sizeof(payload), cap.traces[0].payload_size);

    ASSERT_EQ(BML_RESULT_OK, ImcUnregisterMessageTap(*kernel_, host_mod_));
}

TEST_F(ImcMessageTapTest, UnregisterStopsTap) {
    TapCapture cap;
    ASSERT_EQ(BML_RESULT_OK,
              ImcRegisterMessageTap(*kernel_, host_mod_, CollectingTap, &cap));

    BML_TopicId topic;
    ASSERT_EQ(BML_RESULT_OK, ImcGetTopicId(*kernel_, "test/stop", &topic));

    ASSERT_EQ(BML_RESULT_OK, ImcUnregisterMessageTap(*kernel_, host_mod_));

    uint32_t payload = 1;
    ImcPublish(host_mod_, topic, &payload, sizeof(payload));
    EXPECT_EQ(0u, cap.call_count.load());
}

TEST_F(ImcMessageTapTest, OwnerMismatchOnUnregister) {
    BML_Mod mod2 = mods_->CreateMod("test.mod2");

    TapCapture cap;
    ASSERT_EQ(BML_RESULT_OK,
              ImcRegisterMessageTap(*kernel_, host_mod_, CollectingTap, &cap));

    EXPECT_EQ(BML_RESULT_NOT_FOUND, ImcUnregisterMessageTap(*kernel_, mod2));
    EXPECT_EQ(BML_RESULT_OK, ImcUnregisterMessageTap(*kernel_, host_mod_));
}

TEST_F(ImcMessageTapTest, CleanupOwnerRemovesTap) {
    TapCapture cap;
    ASSERT_EQ(BML_RESULT_OK,
              ImcRegisterMessageTap(*kernel_, host_mod_, CollectingTap, &cap));

    ImcCleanupOwner(host_mod_);

    // Tap should be gone - new registration should succeed
    TapCapture cap2;
    BML_Mod mod2 = mods_->CreateMod("test.mod3");
    EXPECT_EQ(BML_RESULT_OK,
              ImcRegisterMessageTap(*kernel_, mod2, CollectingTap, &cap2));
    ImcUnregisterMessageTap(*kernel_, mod2);
}

} // namespace
