#include <gtest/gtest.h>

#include "SRTimer.h"

#include <cstring>

class SRTimerTest : public ::testing::Test {
protected:
    SRTimer timer;
};

TEST_F(SRTimerTest, InitialState) {
    EXPECT_FLOAT_EQ(0.0f, timer.GetTime());
    EXPECT_FALSE(timer.IsRunning());
}

TEST_F(SRTimerTest, StartAndAccumulate) {
    timer.Start();
    EXPECT_TRUE(timer.IsRunning());

    timer.Update(100.0f); // 100ms
    EXPECT_FLOAT_EQ(100.0f, timer.GetTime());

    timer.Update(50.0f);
    EXPECT_FLOAT_EQ(150.0f, timer.GetTime());
}

TEST_F(SRTimerTest, PauseStopsAccumulation) {
    timer.Start();
    timer.Update(100.0f);
    timer.Pause();
    EXPECT_FALSE(timer.IsRunning());

    timer.Update(200.0f); // Should not accumulate
    EXPECT_FLOAT_EQ(100.0f, timer.GetTime());
}

TEST_F(SRTimerTest, ResetClearsTime) {
    timer.Start();
    timer.Update(500.0f);
    timer.Reset();

    EXPECT_FLOAT_EQ(0.0f, timer.GetTime());
    EXPECT_FALSE(timer.IsRunning());
}

TEST_F(SRTimerTest, FormattedTimeBasic) {
    timer.Start();
    // 1h 23m 45s 678ms = 5025678ms
    timer.Update(5025678.0f);

    const char *fmt = timer.GetFormattedTime();
    ASSERT_NE(nullptr, fmt);
    EXPECT_STREQ("  01:23:45.678", fmt);
}

TEST_F(SRTimerTest, FormattedTimeZero) {
    const char *fmt = timer.GetFormattedTime();
    ASSERT_NE(nullptr, fmt);
    EXPECT_STREQ("  00:00:00.000", fmt);
}

TEST_F(SRTimerTest, DirtyFlag) {
    EXPECT_TRUE(timer.IsDirty()); // Dirty on construction (via Reset-like init)

    timer.GetFormattedTime(); // Clears dirty
    EXPECT_FALSE(timer.IsDirty());

    timer.Start();
    timer.Update(16.0f);
    EXPECT_TRUE(timer.IsDirty());
}

TEST_F(SRTimerTest, NotRunningDoesNotAccumulate) {
    // Timer not started
    timer.Update(1000.0f);
    EXPECT_FLOAT_EQ(0.0f, timer.GetTime());
}

TEST_F(SRTimerTest, LongPlaySession) {
    timer.Start();
    // Simulate 10 hours at 60fps (600,000 frames * 16.67ms)
    for (int i = 0; i < 600000; ++i) {
        timer.Update(16.67f);
    }
    // Should be approximately 10,002,000ms = ~2h 46m
    // (600000 * 16.67 = 10,002,000ms)
    float time = timer.GetTime();
    EXPECT_GT(time, 9900000.0f);
    EXPECT_LT(time, 10100000.0f);
}
