/**
 * @file TimerManagerTests.cpp
 * @brief Unit tests for the TimerManager
 */

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <thread>

#include "bml_timer.h"

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/CrashDumpWriter.h"
#include "Core/FaultTracker.h"
#include "Core/TimerManager.h"
#include "TestKernel.h"

using namespace BML::Core;
using BML::Core::Testing::TestKernel;

class TimerManagerTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->api_registry  = std::make_unique<ApiRegistry>();
        kernel_->config        = std::make_unique<ConfigStore>();
        kernel_->crash_dump    = std::make_unique<CrashDumpWriter>();
        kernel_->fault_tracker = std::make_unique<FaultTracker>();
        kernel_->context = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config, *kernel_->crash_dump, *kernel_->fault_tracker);
        kernel_->config->BindContext(*kernel_->context);
        kernel_->timers  = std::make_unique<TimerManager>(*kernel_->context);
        kernel_->context->Initialize(bmlMakeVersion(0, 4, 0));
        kernel_->timers->Shutdown();
    }

    void TearDown() override {
    }
};

static void IncrementCallback(BML_Context, BML_Timer, void *user_data) {
    auto *counter = static_cast<int *>(user_data);
    ++(*counter);
}

static void ThrowingCallback(BML_Context, BML_Timer, void *user_data) {
    auto *counter = static_cast<int *>(user_data);
    ++(*counter);
    throw std::runtime_error("timer callback failure");
}

TEST_F(TimerManagerTest, ScheduleOnceFiresOnTick) {
    int counter = 0;
    BML_Timer timer = nullptr;

    auto result = kernel_->timers->ScheduleOnce(
        "test.mod", 0, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);
    ASSERT_NE(timer, nullptr);

    kernel_->timers->Tick();
    EXPECT_EQ(counter, 1);

    // Should not fire again (one-shot)
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 1);
}

TEST_F(TimerManagerTest, ScheduleOnceWithDelay) {
    int counter = 0;
    BML_Timer timer = nullptr;

    auto result = kernel_->timers->ScheduleOnce(
        "test.mod", 50, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);

    // Should not fire immediately
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 1);
}

TEST_F(TimerManagerTest, ScheduleRepeatFiresMultipleTimes) {
    int counter = 0;
    BML_Timer timer = nullptr;

    auto result = kernel_->timers->ScheduleRepeat(
        "test.mod", 20, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    kernel_->timers->Tick();
    EXPECT_GE(counter, 1);

    int prev = counter;
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    kernel_->timers->Tick();
    EXPECT_GT(counter, prev);
}

TEST_F(TimerManagerTest, ScheduleRepeatRejectsZeroInterval) {
    BML_Timer timer = nullptr;
    auto result = kernel_->timers->ScheduleRepeat(
        "test.mod", 0, IncrementCallback, nullptr, &timer);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(TimerManagerTest, ScheduleFramesFires) {
    int counter = 0;
    BML_Timer timer = nullptr;

    auto result = kernel_->timers->ScheduleFrames(
        "test.mod", 3, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);

    // Frame 1: decrement 3->2
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 0);

    // Frame 2: decrement 2->1
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 0);

    // Frame 3: decrement 1->0
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 0);

    // Frame 4: frames_remaining == 0 -> fires
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 1);

    // Should not fire again
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 1);
}

TEST_F(TimerManagerTest, ScheduleFramesZeroFiresImmediately) {
    int counter = 0;
    BML_Timer timer = nullptr;

    auto result = kernel_->timers->ScheduleFrames(
        "test.mod", 0, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);

    // frame_count = 0: fires on first tick
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 1);
}

TEST_F(TimerManagerTest, CancelPreventsCallback) {
    int counter = 0;
    BML_Timer timer = nullptr;

    kernel_->timers->ScheduleOnce(
        "test.mod", 0, IncrementCallback, &counter, &timer);

    auto result = kernel_->timers->Cancel("test.mod", timer);
    EXPECT_EQ(result, BML_RESULT_OK);

    kernel_->timers->Tick();
    EXPECT_EQ(counter, 0);
}

TEST_F(TimerManagerTest, CancelNullReturnsError) {
    auto result = kernel_->timers->Cancel("test.mod", nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(TimerManagerTest, IsActiveReflectsState) {
    int counter = 0;
    BML_Timer timer = nullptr;
    BML_Bool active = BML_FALSE;

    kernel_->timers->ScheduleOnce(
        "test.mod", 0, IncrementCallback, &counter, &timer);

    kernel_->timers->IsActive("test.mod", timer, &active);
    EXPECT_EQ(active, BML_TRUE);

    kernel_->timers->Tick();

    kernel_->timers->IsActive("test.mod", timer, &active);
    EXPECT_EQ(active, BML_FALSE);
}

TEST_F(TimerManagerTest, CancelAllForModuleCancelsModuleTimers) {
    int counter_a = 0, counter_b = 0;
    BML_Timer timer_a = nullptr, timer_b = nullptr;

    kernel_->timers->ScheduleOnce(
        "mod.a", 0, IncrementCallback, &counter_a, &timer_a);
    kernel_->timers->ScheduleOnce(
        "mod.b", 0, IncrementCallback, &counter_b, &timer_b);

    kernel_->timers->CancelAllForModule("mod.a");

    kernel_->timers->Tick();
    EXPECT_EQ(counter_a, 0);  // cancelled
    EXPECT_EQ(counter_b, 1);  // still fires
}

TEST_F(TimerManagerTest, NullCallbackRejected) {
    BML_Timer timer = nullptr;
    auto result = kernel_->timers->ScheduleOnce(
        "test.mod", 0, nullptr, nullptr, &timer);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(TimerManagerTest, NullOutTimerRejected) {
    auto result = kernel_->timers->ScheduleOnce(
        "test.mod", 0, IncrementCallback, nullptr, nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(TimerManagerTest, ShutdownCancelsAll) {
    int counter = 0;
    BML_Timer timer = nullptr;

    kernel_->timers->ScheduleOnce(
        "test.mod", 0, IncrementCallback, &counter, &timer);

    kernel_->timers->Shutdown();
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 0);
}

TEST_F(TimerManagerTest, CallbackCanScheduleNewTimer) {
    int counter = 0;

    auto chain_callback = [](BML_Context, BML_Timer, void *ud) {
        auto *c = static_cast<int *>(ud);
        ++(*c);
        BML_Timer next = nullptr;
        BML::Core::Kernel().timers->ScheduleOnce("test.mod", 0, IncrementCallback, ud, &next);
    };

    BML_Timer timer = nullptr;
    kernel_->timers->ScheduleOnce(
        "test.mod", 0, chain_callback, &counter, &timer);

    // First tick: chain fires (counter=1), schedules IncrementCallback
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 1);

    // Second tick: IncrementCallback fires (counter=2)
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 2);

    // No more timers
    kernel_->timers->Tick();
    EXPECT_EQ(counter, 2);
}

TEST_F(TimerManagerTest, ThrowingCallbackIsContainedAndCancelsRepeatTimer) {
    int failing_counter = 0;
    int healthy_counter = 0;
    BML_Timer failing_timer = nullptr;
    BML_Timer healthy_timer = nullptr;
    BML_Bool active = BML_TRUE;

    ASSERT_EQ(kernel_->timers->ScheduleRepeat(
                  "test.mod", 1, ThrowingCallback, &failing_counter, &failing_timer),
              BML_RESULT_OK);
    ASSERT_EQ(kernel_->timers->ScheduleOnce(
                  "test.mod", 0, IncrementCallback, &healthy_counter, &healthy_timer),
              BML_RESULT_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_NO_THROW(kernel_->timers->Tick());
    EXPECT_EQ(failing_counter, 1);
    EXPECT_EQ(healthy_counter, 1);

    ASSERT_EQ(kernel_->timers->IsActive("test.mod", failing_timer, &active), BML_RESULT_OK);
    EXPECT_EQ(active, BML_FALSE);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_NO_THROW(kernel_->timers->Tick());
    EXPECT_EQ(failing_counter, 1);
}
