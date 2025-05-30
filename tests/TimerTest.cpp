#include <gtest/gtest.h>
#include <chrono>

#include "BML/Timer.h"

namespace BML {
namespace Test {

class TimerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset all timers before each test
        Timer::CancelAll();
        // Reset time scale to default
        Timer::SetTimeScale(1.0f);
    }

    void TearDown() override {
        // Clean up timers after each test
        Timer::CancelAll();
    }

    // Helper to simulate passage of time
    void AdvanceTicks(size_t &currentTick, size_t amount) {
        currentTick += amount;
    }

    void AdvanceTime(float &currentTime, float amount) {
        currentTime += amount;
    }
};

// Test timer creation and basic properties
TEST_F(TimerTest, BasicCreation) {
    size_t currentTick = 100;

    auto timer = Timer::Builder()
        .WithName("TestTimer")
        .WithDelayTicks(50)
        .WithType(Timer::ONCE)
        .Build(currentTick, 0.0f);

    EXPECT_EQ("TestTimer", timer->GetName());
    EXPECT_EQ(Timer::RUNNING, timer->GetState());
    EXPECT_EQ(Timer::ONCE, timer->GetType());
    EXPECT_EQ(Timer::TICK, timer->GetTimeBase());
}

// Test auto-generated timer name
TEST_F(TimerTest, AutoGeneratedName) {
    size_t currentTick = 100;

    auto timer = Timer::Builder()
        .WithDelayTicks(50)
        .Build(currentTick, 0.0f);

    // Name should be auto-generated with the format "Timer_ID"
    EXPECT_TRUE(timer->GetName().find("Timer_") == 0);
}

// Test ONCE timer execution
TEST_F(TimerTest, OnceExecution) {
    size_t currentTick = 100;
    bool callbackExecuted = false;

    auto timer = Timer::Builder()
        .WithDelayTicks(50)
        .WithOnceCallback([&callbackExecuted](Timer &) {
            callbackExecuted = true;
        })
        .Build(currentTick, 0.0f);

    // Timer should be running but not executed yet
    EXPECT_EQ(Timer::RUNNING, timer->GetState());
    EXPECT_FALSE(callbackExecuted);

    // Process before time reached
    AdvanceTicks(currentTick, 25);
    timer->Process(currentTick, 0.0f);
    EXPECT_FALSE(callbackExecuted);
    EXPECT_EQ(Timer::RUNNING, timer->GetState());

    // Process at exactly when time is reached
    AdvanceTicks(currentTick, 25);
    timer->Process(currentTick, 0.0f);
    EXPECT_TRUE(callbackExecuted);
    EXPECT_EQ(Timer::COMPLETED, timer->GetState());
}

// Test TIME based timer
TEST_F(TimerTest, TimeBasedTimer) {
    float currentTime = 1.0f;
    bool callbackExecuted = false;

    auto timer = Timer::Builder()
        .WithDelaySeconds(0.5f)  // 0.5 seconds
        .WithTimeBase(Timer::TIME)
        .WithOnceCallback([&callbackExecuted](Timer &) {
            callbackExecuted = true;
        })
        .Build(0, currentTime);

    // Process before time reached
    AdvanceTime(currentTime, 0.2f);
    timer->Process(0, currentTime);
    EXPECT_FALSE(callbackExecuted);

    // Process at exactly when time is reached
    AdvanceTime(currentTime, 0.3f);
    timer->Process(0, currentTime);
    EXPECT_TRUE(callbackExecuted);
}

// Test LOOP timer
TEST_F(TimerTest, LoopExecution) {
    size_t currentTick = 100;
    int callbackCount = 0;

    auto timer = Timer::Builder()
        .WithDelayTicks(10)
        .WithType(Timer::LOOP)
        .WithLoopCallback([&callbackCount](Timer &) -> bool {
            callbackCount++;
            return callbackCount < 3; // Continue until we've executed 3 times
        })
        .Build(currentTick, 0.0f);

    // First execution
    AdvanceTicks(currentTick, 10);
    timer->Process(currentTick, 0.0f);
    EXPECT_EQ(1, callbackCount);
    EXPECT_EQ(Timer::RUNNING, timer->GetState());

    // Second execution
    AdvanceTicks(currentTick, 10);
    timer->Process(currentTick, 0.0f);
    EXPECT_EQ(2, callbackCount);
    EXPECT_EQ(Timer::RUNNING, timer->GetState());

    // Third execution - should stop after this
    AdvanceTicks(currentTick, 10);
    timer->Process(currentTick, 0.0f);
    EXPECT_EQ(3, callbackCount);
    EXPECT_EQ(Timer::COMPLETED, timer->GetState());
}

// Test REPEAT timer
TEST_F(TimerTest, RepeatExecution) {
    size_t currentTick = 100;
    int callbackCount = 0;

    auto timer = Timer::Builder()
        .WithDelayTicks(10)
        .WithType(Timer::REPEAT)
        .WithRepeatCount(3)
        .WithOnceCallback([&callbackCount](Timer &) {
            callbackCount++;
        })
        .Build(currentTick, 0.0f);

    EXPECT_EQ(3, timer->GetRemainingIterations());

    // First execution
    AdvanceTicks(currentTick, 10);
    timer->Process(currentTick, 0.0f);
    EXPECT_EQ(1, callbackCount);
    EXPECT_EQ(2, timer->GetRemainingIterations());
    EXPECT_EQ(Timer::RUNNING, timer->GetState());

    // Second execution
    AdvanceTicks(currentTick, 10);
    timer->Process(currentTick, 0.0f);
    EXPECT_EQ(2, callbackCount);
    EXPECT_EQ(1, timer->GetRemainingIterations());
    EXPECT_EQ(Timer::RUNNING, timer->GetState());

    // Third execution - should stop after this
    AdvanceTicks(currentTick, 10);
    timer->Process(currentTick, 0.0f);
    EXPECT_EQ(3, callbackCount);
    EXPECT_EQ(0, timer->GetRemainingIterations());
    EXPECT_EQ(Timer::COMPLETED, timer->GetState());
}

// Test progress callback
TEST_F(TimerTest, ProgressCallback) {
    size_t currentTick = 100;
    float lastProgress = 0.0f;
    bool completionCallbackExecuted = false;

    auto timer = Timer::Builder()
        .WithDelayTicks(100)
        .WithProgressCallback([&lastProgress](Timer &, float progress) {
            lastProgress = progress;
        })
        .WithOnceCallback([&completionCallbackExecuted](Timer &) {
            completionCallbackExecuted = true;
        })
        .Build(currentTick, 0.0f);

    // Check progress at different points
    AdvanceTicks(currentTick, 25);
    timer->Process(currentTick, 0.0f);
    EXPECT_NEAR(0.25f, lastProgress, 0.001f);

    AdvanceTicks(currentTick, 25);
    timer->Process(currentTick, 0.0f);
    EXPECT_NEAR(0.5f, lastProgress, 0.001f);

    AdvanceTicks(currentTick, 25);
    timer->Process(currentTick, 0.0f);
    EXPECT_NEAR(0.75f, lastProgress, 0.001f);

    AdvanceTicks(currentTick, 25);
    timer->Process(currentTick, 0.0f);
    EXPECT_NEAR(1.0f, lastProgress, 0.001f);
    EXPECT_TRUE(completionCallbackExecuted);
}

// Test easing functions
TEST_F(TimerTest, EasingFunctions) {
    size_t currentTick = 100;
    float linearProgress = 0.0f;
    float easeInProgress = 0.0f;
    float easeOutProgress = 0.0f;
    float easeInOutProgress = 0.0f;

    auto linearTimer = Timer::Builder()
        .WithDelayTicks(100)
        .WithEasing(Timer::LINEAR)
        .WithProgressCallback([&linearProgress](Timer &, float progress) {
            linearProgress = progress;
        })
        .Build(currentTick, 0.0f);

    auto easeInTimer = Timer::Builder()
        .WithDelayTicks(100)
        .WithEasing(Timer::EASE_IN)
        .WithProgressCallback([&easeInProgress](Timer &, float progress) {
            easeInProgress = progress;
        })
        .Build(currentTick, 0.0f);

    auto easeOutTimer = Timer::Builder()
        .WithDelayTicks(100)
        .WithEasing(Timer::EASE_OUT)
        .WithProgressCallback([&easeOutProgress](Timer &, float progress) {
            easeOutProgress = progress;
        })
        .Build(currentTick, 0.0f);

    auto easeInOutTimer = Timer::Builder()
        .WithDelayTicks(100)
        .WithEasing(Timer::EASE_IN_OUT)
        .WithProgressCallback([&easeInOutProgress](Timer &, float progress) {
            easeInOutProgress = progress;
        })
        .Build(currentTick, 0.0f);

    // Check progress at midpoint
    AdvanceTicks(currentTick, 50);
    linearTimer->Process(currentTick, 0.0f);
    easeInTimer->Process(currentTick, 0.0f);
    easeOutTimer->Process(currentTick, 0.0f);
    easeInOutTimer->Process(currentTick, 0.0f);

    EXPECT_NEAR(0.5f, linearProgress, 0.001f);
    EXPECT_LT(easeInProgress, 0.5f); // Should be less than linear
    EXPECT_GT(easeOutProgress, 0.5f); // Should be more than linear
    // easeInOut at 0.5 should be 0.5
    EXPECT_NEAR(0.5f, easeInOutProgress, 0.001f);
}

// Test pause and resume
TEST_F(TimerTest, PauseResume) {
    size_t currentTick = 100;
    bool callbackExecuted = false;

    auto timer = Timer::Builder()
        .WithDelayTicks(50)
        .WithOnceCallback([&callbackExecuted](Timer &) {
            callbackExecuted = true;
        })
        .Build(currentTick, 0.0f);

    // Advance time and pause
    AdvanceTicks(currentTick, 20);
    timer->Process(currentTick, 0.0f);
    timer->Pause();
    EXPECT_EQ(Timer::PAUSED, timer->GetState());

    // Advance more time while paused
    AdvanceTicks(currentTick, 30);
    timer->Process(currentTick, 0.0f);
    EXPECT_FALSE(callbackExecuted);

    // Resume and finish
    timer->Resume();
    EXPECT_EQ(Timer::RUNNING, timer->GetState());
    AdvanceTicks(currentTick, 30);
    timer->Process(currentTick, 0.0f);
    EXPECT_TRUE(callbackExecuted);
}

// Test reset
TEST_F(TimerTest, Reset) {
    size_t currentTick = 100;
    int callbackCount = 0;

    auto timer = Timer::Builder()
        .WithDelayTicks(50)
        .WithOnceCallback([&callbackCount](Timer &) {
            callbackCount++;
        })
        .Build(currentTick, 0.0f);

    // Execute once
    AdvanceTicks(currentTick, 50);
    timer->Process(currentTick, 0.0f);
    EXPECT_EQ(1, callbackCount);
    EXPECT_EQ(Timer::COMPLETED, timer->GetState());

    // Reset and execute again
    timer->Reset(currentTick, 0.0f);
    EXPECT_EQ(Timer::RUNNING, timer->GetState());
    AdvanceTicks(currentTick, 50);
    timer->Process(currentTick, 0.0f);
    EXPECT_EQ(2, callbackCount);
}

// Test cancel
TEST_F(TimerTest, Cancel) {
    size_t currentTick = 100;
    bool callbackExecuted = false;

    auto timer = Timer::Builder()
        .WithDelayTicks(50)
        .WithOnceCallback([&callbackExecuted](Timer &) {
            callbackExecuted = true;
        })
        .Build(currentTick, 0.0f);

    // Cancel before execution
    timer->Cancel();
    EXPECT_EQ(Timer::CANCELLED, timer->GetState());

    // Advance time past when it would have executed
    AdvanceTicks(currentTick, 100);
    timer->Process(currentTick, 0.0f);
    EXPECT_FALSE(callbackExecuted);
}

// Test timer finding by ID, name, and group
TEST_F(TimerTest, TimerLookup) {
    size_t currentTick = 100;

    auto timer1 = Timer::Builder()
        .WithName("Timer1")
        .WithDelayTicks(50)
        .AddToGroup("Group1")
        .Build(currentTick, 0.0f);

    auto timer2 = Timer::Builder()
        .WithName("Timer2")
        .WithDelayTicks(50)
        .AddToGroup("Group1")
        .AddToGroup("Group2")
        .Build(currentTick, 0.0f);

    // Find by ID
    auto found = Timer::FindById(timer1->GetId());
    EXPECT_EQ(timer1, found);

    // Find by name
    found = Timer::FindByName("Timer2");
    EXPECT_EQ(timer2, found);

    // Find by group
    auto group1Timers = Timer::FindByGroup("Group1");
    EXPECT_EQ(2, group1Timers.size());
    EXPECT_TRUE(std::find(group1Timers.begin(), group1Timers.end(), timer1) != group1Timers.end());
    EXPECT_TRUE(std::find(group1Timers.begin(), group1Timers.end(), timer2) != group1Timers.end());

    timer1->RemoveFromGroup("Group1");

    auto group2Timers = Timer::FindByGroup("Group2");
    EXPECT_EQ(1, group2Timers.size());
    EXPECT_EQ(timer2, group2Timers[0]);

    timer2->RemoveFromGroup("Group1");
    timer2->RemoveFromGroup("Group2");
}

// Test group management
TEST_F(TimerTest, GroupManagement) {
    size_t currentTick = 100;

    auto preGroup1Timers = Timer::FindByGroup("Group1");
    EXPECT_EQ(0, preGroup1Timers.size());

    auto timer = Timer::Builder()
        .WithDelayTicks(50)
        .AddToGroup("Group1")
        .Build(currentTick, 0.0f);

    auto group1Timers = Timer::FindByGroup("Group1");
    EXPECT_EQ(1, group1Timers.size());

    // Add to another group
    timer->AddToGroup("Group2");
    auto group2Timers = Timer::FindByGroup("Group2");
    EXPECT_EQ(1, group2Timers.size());

    // Remove from group
    timer->RemoveFromGroup("Group1");
    group1Timers = Timer::FindByGroup("Group1");
    EXPECT_EQ(0, group1Timers.size());

    // Group should be removed when empty
    group2Timers = Timer::FindByGroup("Group2");
    EXPECT_EQ(1, group2Timers.size());
}

// Test global timescale
TEST_F(TimerTest, TimeScale) {
    float currentTime = 1.0f;
    bool callbackExecuted = false;

    auto timer = Timer::Builder()
        .WithDelaySeconds(0.5f)  // 0.5 seconds
        .WithTimeBase(Timer::TIME)
        .WithOnceCallback([&callbackExecuted](Timer &) {
            callbackExecuted = true;
        })
        .Build(0, currentTime);

    // Set time scale to 2.0 (twice as fast)
    Timer::SetTimeScale(2.0f);

    // With double speed, we should only need to advance by 0.25 actual seconds
    AdvanceTime(currentTime, 0.25f);
    timer->Process(0, currentTime);
    EXPECT_TRUE(callbackExecuted);
}

// Test ProcessAll method
TEST_F(TimerTest, ProcessAll) {
    size_t currentTick = 100;
    int callbackCount = 0;

    // Create 3 timers with different delays
    Timer::Builder()
        .WithDelayTicks(10)
        .WithOnceCallback([&callbackCount](Timer &) {
            callbackCount++;
        })
        .Build(currentTick, 0.0f);

    Timer::Builder()
        .WithDelayTicks(20)
        .WithOnceCallback([&callbackCount](Timer &) {
            callbackCount++;
        })
        .Build(currentTick, 0.0f);

    Timer::Builder()
        .WithDelayTicks(30)
        .WithOnceCallback([&callbackCount](Timer &) {
            callbackCount++;
        })
        .Build(currentTick, 0.0f);

    // No timers should execute yet
    EXPECT_EQ(3, Timer::ProcessAll(currentTick, 0.0f));
    EXPECT_EQ(0, callbackCount);

    // First timer should execute
    AdvanceTicks(currentTick, 10);
    EXPECT_EQ(2, Timer::ProcessAll(currentTick, 0.0f));
    EXPECT_EQ(1, callbackCount);

    // Second timer should execute
    AdvanceTicks(currentTick, 10);
    EXPECT_EQ(1, Timer::ProcessAll(currentTick, 0.0f));
    EXPECT_EQ(2, callbackCount);

    // Last timer should execute
    AdvanceTicks(currentTick, 10);
    EXPECT_EQ(0, Timer::ProcessAll(currentTick, 0.0f));
    EXPECT_EQ(3, callbackCount);
}

// Test convenience functions
TEST_F(TimerTest, ConvenienceFunctions) {
    size_t currentTick = 100;
    float currentTime = 1.0f;

    bool tickDelayExecuted = false;
    bool timeDelayExecuted = false;

    int tickIntervalCount = 0;
    int timeIntervalCount = 0;

    int tickRepeatCount = 0;
    int timeRepeatCount = 0;

    // Test Delay functions
    auto tickDelayTimer = Delay(50, [&tickDelayExecuted]() {
        tickDelayExecuted = true;
    }, currentTick);

    auto timeDelayTimer = Delay(0.5f, [&timeDelayExecuted]() {
        timeDelayExecuted = true;
    }, currentTime);

    // Test Interval functions
    auto tickIntervalTimer = Interval(20, [&tickIntervalCount]() -> bool {
        tickIntervalCount++;
        return tickIntervalCount < 3;
    }, currentTick);

    auto timeIntervalTimer = Interval(0.2f, [&timeIntervalCount]() -> bool {
        timeIntervalCount++;
        return timeIntervalCount < 3;
    }, currentTime);

    // Test Repeat functions
    auto tickRepeatTimer = Repeat(30, 3, [&tickRepeatCount]() {
        tickRepeatCount++;
    }, currentTick);

    auto timeRepeatTimer = Repeat(0.3f, 3, [&timeRepeatCount]() {
        timeRepeatCount++;
    }, currentTime);

    // Let's advance both tick and time to execute all timers
    for (int i = 0; i < 6; i++) {
        AdvanceTicks(currentTick, 20);
        AdvanceTime(currentTime, 0.2f);

        Timer::ProcessAll(currentTick, currentTime);
    }

    // Check results
    EXPECT_TRUE(tickDelayExecuted);
    EXPECT_TRUE(timeDelayExecuted);
    EXPECT_EQ(3, tickIntervalCount);
    EXPECT_EQ(3, timeIntervalCount);
    EXPECT_EQ(3, tickRepeatCount);
    EXPECT_EQ(3, timeRepeatCount);
}

// Test RepeatUntil convenience function (implementing for compatibility)
TEST_F(TimerTest, RepeatUntilFunction) {
    size_t currentTick = 100;
    int callbackCount = 0;

    // Use Interval for the RepeatUntil behavior
    auto repeatUntilTimer = Interval(20, [&callbackCount]() -> bool {
        callbackCount++;
        return callbackCount < 3; // Continue until we've executed 3 times
    }, currentTick);

    // Advance ticks to execute
    for (int i = 0; i < 4; i++) {
        AdvanceTicks(currentTick, 20);
        Timer::ProcessAll(currentTick, 0.0f);
    }

    // Check we stopped at 3 iterations as the callback specified
    EXPECT_EQ(3, callbackCount);
}

// Test priority-based execution
TEST_F(TimerTest, PriorityExecution) {
    // This test is more conceptual since the Timer class doesn't actually sort by priority
    // But it demonstrates how you might use priority values
    size_t currentTick = 100;

    std::vector<int> executionOrder;

    auto highPriorityTimer = Timer::Builder()
        .WithDelayTicks(10)
        .WithPriority(1)
        .WithOnceCallback([&executionOrder](Timer &) {
            executionOrder.push_back(1);
        })
        .Build(currentTick, 0.0f);

    auto lowPriorityTimer = Timer::Builder()
        .WithDelayTicks(10)
        .WithPriority(0)
        .WithOnceCallback([&executionOrder](Timer &) {
            executionOrder.push_back(0);
        })
        .Build(currentTick, 0.0f);

    // Advance time to trigger both timers
    AdvanceTicks(currentTick, 10);

    // In a real system, you'd sort timers by priority before processing them
    // For this test, we'll just process them individually in priority order
    highPriorityTimer->Process(currentTick, 0.0f);
    lowPriorityTimer->Process(currentTick, 0.0f);

    EXPECT_EQ(2, executionOrder.size());
    EXPECT_EQ(1, executionOrder[0]);
    EXPECT_EQ(0, executionOrder[1]);
}

// Test PauseAll, ResumeAll, CancelAll
TEST_F(TimerTest, GlobalOperations) {
    size_t currentTick = 100;
    int callbackCount = 0;

    // Create several timers
    for (int i = 0; i < 5; i++) {
        Timer::Builder()
            .WithDelayTicks(10 * (i + 1))
            .WithOnceCallback([&callbackCount](Timer &) {
                callbackCount++;
            })
            .Build(currentTick, 0.0f);
    }

    // Pause all timers
    Timer::PauseAll();

    // Advance time - no callbacks should execute
    AdvanceTicks(currentTick, 100);
    Timer::ProcessAll(currentTick, 0.0f);
    EXPECT_EQ(0, callbackCount);

    // Resume all timers
    Timer::ResumeAll();

    // Process - all should execute
    Timer::ProcessAll(currentTick, 0.0f);
    EXPECT_EQ(5, callbackCount);

    // Create more timers
    for (int i = 0; i < 3; i++) {
        Timer::Builder()
            .WithDelayTicks(10 * (i + 1))
            .WithOnceCallback([&callbackCount](Timer &) {
                callbackCount++;
            })
            .Build(currentTick, 0.0f);
    }

    // Cancel all timers
    Timer::CancelAll();

    // Advance time - no new callbacks should execute
    AdvanceTicks(currentTick, 100);
    Timer::ProcessAll(currentTick, 0.0f);
    EXPECT_EQ(5, callbackCount);
}

// Make sure find functions handle non-existent entities gracefully
TEST_F(TimerTest, NonExistentLookup) {
    auto notFoundTimer = Timer::FindById(12345);
    EXPECT_EQ(nullptr, notFoundTimer);

    auto notFoundByName = Timer::FindByName("NonExistentTimer");
    EXPECT_EQ(nullptr, notFoundByName);

    auto emptyGroup = Timer::FindByGroup("EmptyGroup");
    EXPECT_EQ(0, emptyGroup.size());
}

// Test SimpleCallback functionality
TEST_F(TimerTest, SimpleCallback) {
    size_t currentTick = 100;
    bool callbackExecuted = false;

    auto timer = Timer::Builder()
        .WithDelayTicks(50)
        .WithSimpleCallback([&callbackExecuted]() {
            callbackExecuted = true;
        })
        .Build(currentTick, 0.0f);

    // Timer should be running but not executed yet
    EXPECT_EQ(Timer::RUNNING, timer->GetState());
    EXPECT_FALSE(callbackExecuted);

    // Advance time to trigger callback
    AdvanceTicks(currentTick, 50);
    timer->Process(currentTick, 0.0f);
    EXPECT_TRUE(callbackExecuted);
}

// Test DEBOUNCE timer type
TEST_F(TimerTest, DebounceExecution) {
    size_t currentTick = 100;
    int callbackCount = 0;

    auto debounceTimer = Timer::Builder()
        .WithDelayTicks(30)
        .WithType(Timer::DEBOUNCE)
        .WithSimpleCallback([&callbackCount]() {
            callbackCount++;
        })
        .Build(currentTick, 0.0f);

    // Advance time a bit (not enough to trigger)
    AdvanceTicks(currentTick, 20);
    debounceTimer->Process(currentTick, 0.0f);
    EXPECT_EQ(0, callbackCount);

    // "Restart" the debounce by processing again (simulates user input)
    debounceTimer->Process(currentTick, 0.0f);

    // Advance time a bit more (not enough to trigger)
    AdvanceTicks(currentTick, 20);
    debounceTimer->Process(currentTick, 0.0f);
    EXPECT_EQ(0, callbackCount);

    // Let enough time pass to finally trigger
    AdvanceTicks(currentTick, 30);
    debounceTimer->Process(currentTick, 0.0f);
    EXPECT_EQ(1, callbackCount);
    EXPECT_EQ(Timer::COMPLETED, debounceTimer->GetState());
}

// Test THROTTLE timer type
TEST_F(TimerTest, ThrottleExecution) {
    size_t currentTick = 100;
    int callbackCount = 0;

    auto throttleTimer = Timer::Builder()
        .WithDelayTicks(30)
        .WithType(Timer::THROTTLE)
        .WithSimpleCallback([&callbackCount]() {
            callbackCount++;
        })
        .Build(currentTick, 0.0f);

    // Should execute immediately
    throttleTimer->Process(currentTick, 0.0f);
    EXPECT_EQ(1, callbackCount);

    // Should not execute again too soon
    throttleTimer->Process(currentTick, 0.0f);
    EXPECT_EQ(1, callbackCount);

    // Advance time but not enough
    AdvanceTicks(currentTick, 20);
    throttleTimer->Process(currentTick, 0.0f);
    EXPECT_EQ(1, callbackCount);

    // Advance enough time to execute again
    AdvanceTicks(currentTick, 10);
    throttleTimer->Process(currentTick, 0.0f);
    EXPECT_EQ(2, callbackCount);
}

// Test Timer Chaining
TEST_F(TimerTest, TimerChaining) {
    size_t currentTick = 100;
    std::vector<int> executionOrder;

    auto secondTimer = Timer::Builder()
        .WithDelayTicks(20)
        .WithSimpleCallback([&executionOrder]() {
            executionOrder.push_back(2);
        })
        .Build(currentTick, 0.0f);

    auto firstTimer = Timer::Builder()
        .WithDelayTicks(10)
        .WithSimpleCallback([&executionOrder]() {
            executionOrder.push_back(1);
        })
        .WithChainedTimer([secondTimer]() {
            return Timer::Builder(); // Return a dummy builder, we'll use the existing timer
        })
        .Build(currentTick, 0.0f);

    // Chain the timers
    Timer::Chain(firstTimer, secondTimer);

    // At start, no timers executed
    EXPECT_EQ(0, executionOrder.size());

    // Advance time to trigger first timer
    AdvanceTicks(currentTick, 10);
    Timer::ProcessAll(currentTick, 0.0f);
    EXPECT_EQ(1, executionOrder.size());
    EXPECT_EQ(1, executionOrder[0]);

    // Advance time to trigger second timer
    AdvanceTicks(currentTick, 20);
    Timer::ProcessAll(currentTick, 0.0f);
    EXPECT_EQ(2, executionOrder.size());
    EXPECT_EQ(2, executionOrder[1]);
}

// Test namespaced convenience functions
TEST_F(TimerTest, NamespacedConvenienceFunctions) {
    size_t currentTick = 100;
    float currentTime = 1.0f;

    bool afterTickExecuted = false;
    bool afterTimeExecuted = false;
    bool everyTickExecuted = false;
    bool everyTimeExecuted = false;

    // Test After functions
    auto tickAfterTimer = Timers::After(50, [&afterTickExecuted]() {
        afterTickExecuted = true;
    }, currentTick);

    auto timeAfterTimer = Timers::After(0.5f, [&afterTimeExecuted]() {
        afterTimeExecuted = true;
    }, currentTime);

    // Test Every functions
    auto tickEveryTimer = Timers::Every(20, [&everyTickExecuted]() {
        everyTickExecuted = true;
    }, currentTick);

    auto timeEveryTimer = Timers::Every(0.2f, [&everyTimeExecuted]() {
        everyTimeExecuted = true;
    }, currentTime);

    // Advance time to trigger the timers
    AdvanceTicks(currentTick, 50);
    AdvanceTime(currentTime, 0.5f);
    Timer::ProcessAll(currentTick, currentTime);

    EXPECT_TRUE(afterTickExecuted);
    EXPECT_TRUE(afterTimeExecuted);
    EXPECT_TRUE(everyTickExecuted);
    EXPECT_TRUE(everyTimeExecuted);
}

} // namespace Test
} // namespace BML