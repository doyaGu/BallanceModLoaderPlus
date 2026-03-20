/**
 * @file TimerManagerTests.cpp
 * @brief Unit tests for the TimerManager
 */

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <thread>

#include "bml_timer.h"

#include "Core/TimerManager.h"
#include "Core/Context.h"
#include "TestKernel.h"

using namespace BML::Core;
using BML::Core::Testing::TestKernel;

class TimerManagerTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->context = std::make_unique<Context>();
        kernel_->timers  = std::make_unique<TimerManager>();
        Context::Instance().Initialize(bmlMakeVersion(0, 4, 0));
        TimerManager::Instance().Shutdown();
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

    auto result = TimerManager::Instance().ScheduleOnce(
        "test.mod", 0, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);
    ASSERT_NE(timer, nullptr);

    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 1);

    // Should not fire again (one-shot)
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 1);
}

TEST_F(TimerManagerTest, ScheduleOnceWithDelay) {
    int counter = 0;
    BML_Timer timer = nullptr;

    auto result = TimerManager::Instance().ScheduleOnce(
        "test.mod", 50, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);

    // Should not fire immediately
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 1);
}

TEST_F(TimerManagerTest, ScheduleRepeatFiresMultipleTimes) {
    int counter = 0;
    BML_Timer timer = nullptr;

    auto result = TimerManager::Instance().ScheduleRepeat(
        "test.mod", 20, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    TimerManager::Instance().Tick();
    EXPECT_GE(counter, 1);

    int prev = counter;
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TimerManager::Instance().Tick();
    EXPECT_GT(counter, prev);
}

TEST_F(TimerManagerTest, ScheduleRepeatRejectsZeroInterval) {
    BML_Timer timer = nullptr;
    auto result = TimerManager::Instance().ScheduleRepeat(
        "test.mod", 0, IncrementCallback, nullptr, &timer);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(TimerManagerTest, ScheduleFramesFires) {
    int counter = 0;
    BML_Timer timer = nullptr;

    auto result = TimerManager::Instance().ScheduleFrames(
        "test.mod", 3, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);

    // Frame 1: decrement 3->2
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 0);

    // Frame 2: decrement 2->1
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 0);

    // Frame 3: decrement 1->0
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 0);

    // Frame 4: frames_remaining == 0 -> fires
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 1);

    // Should not fire again
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 1);
}

TEST_F(TimerManagerTest, ScheduleFramesZeroFiresImmediately) {
    int counter = 0;
    BML_Timer timer = nullptr;

    auto result = TimerManager::Instance().ScheduleFrames(
        "test.mod", 0, IncrementCallback, &counter, &timer);
    ASSERT_EQ(result, BML_RESULT_OK);

    // frame_count = 0: fires on first tick
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 1);
}

TEST_F(TimerManagerTest, CancelPreventsCallback) {
    int counter = 0;
    BML_Timer timer = nullptr;

    TimerManager::Instance().ScheduleOnce(
        "test.mod", 0, IncrementCallback, &counter, &timer);

    auto result = TimerManager::Instance().Cancel(timer);
    EXPECT_EQ(result, BML_RESULT_OK);

    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 0);
}

TEST_F(TimerManagerTest, CancelNullReturnsError) {
    auto result = TimerManager::Instance().Cancel(nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(TimerManagerTest, IsActiveReflectsState) {
    int counter = 0;
    BML_Timer timer = nullptr;
    BML_Bool active = BML_FALSE;

    TimerManager::Instance().ScheduleOnce(
        "test.mod", 0, IncrementCallback, &counter, &timer);

    TimerManager::Instance().IsActive(timer, &active);
    EXPECT_EQ(active, BML_TRUE);

    TimerManager::Instance().Tick();

    TimerManager::Instance().IsActive(timer, &active);
    EXPECT_EQ(active, BML_FALSE);
}

TEST_F(TimerManagerTest, CancelAllForModuleCancelsOwned) {
    int counter_a = 0, counter_b = 0;
    BML_Timer timer_a = nullptr, timer_b = nullptr;

    TimerManager::Instance().ScheduleOnce(
        "mod.a", 0, IncrementCallback, &counter_a, &timer_a);
    TimerManager::Instance().ScheduleOnce(
        "mod.b", 0, IncrementCallback, &counter_b, &timer_b);

    TimerManager::Instance().CancelAllForModule("mod.a");

    TimerManager::Instance().Tick();
    EXPECT_EQ(counter_a, 0);  // cancelled
    EXPECT_EQ(counter_b, 1);  // still fires
}

TEST_F(TimerManagerTest, NullCallbackRejected) {
    BML_Timer timer = nullptr;
    auto result = TimerManager::Instance().ScheduleOnce(
        "test.mod", 0, nullptr, nullptr, &timer);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(TimerManagerTest, NullOutTimerRejected) {
    auto result = TimerManager::Instance().ScheduleOnce(
        "test.mod", 0, IncrementCallback, nullptr, nullptr);
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(TimerManagerTest, ShutdownCancelsAll) {
    int counter = 0;
    BML_Timer timer = nullptr;

    TimerManager::Instance().ScheduleOnce(
        "test.mod", 0, IncrementCallback, &counter, &timer);

    TimerManager::Instance().Shutdown();
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 0);
}

TEST_F(TimerManagerTest, CallbackCanScheduleNewTimer) {
    int counter = 0;

    auto chain_callback = [](BML_Context, BML_Timer, void *ud) {
        auto *c = static_cast<int *>(ud);
        ++(*c);
        BML_Timer next = nullptr;
        TimerManager::Instance().ScheduleOnce("test.mod", 0, IncrementCallback, ud, &next);
    };

    BML_Timer timer = nullptr;
    TimerManager::Instance().ScheduleOnce(
        "test.mod", 0, chain_callback, &counter, &timer);

    // First tick: chain fires (counter=1), schedules IncrementCallback
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 1);

    // Second tick: IncrementCallback fires (counter=2)
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 2);

    // No more timers
    TimerManager::Instance().Tick();
    EXPECT_EQ(counter, 2);
}

TEST_F(TimerManagerTest, ThrowingCallbackIsContainedAndCancelsRepeatTimer) {
    int failing_counter = 0;
    int healthy_counter = 0;
    BML_Timer failing_timer = nullptr;
    BML_Timer healthy_timer = nullptr;
    BML_Bool active = BML_TRUE;

    ASSERT_EQ(TimerManager::Instance().ScheduleRepeat(
                  "test.mod", 1, ThrowingCallback, &failing_counter, &failing_timer),
              BML_RESULT_OK);
    ASSERT_EQ(TimerManager::Instance().ScheduleOnce(
                  "test.mod", 0, IncrementCallback, &healthy_counter, &healthy_timer),
              BML_RESULT_OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_NO_THROW(TimerManager::Instance().Tick());
    EXPECT_EQ(failing_counter, 1);
    EXPECT_EQ(healthy_counter, 1);

    ASSERT_EQ(TimerManager::Instance().IsActive(failing_timer, &active), BML_RESULT_OK);
    EXPECT_EQ(active, BML_FALSE);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_NO_THROW(TimerManager::Instance().Tick());
    EXPECT_EQ(failing_counter, 1);
}
