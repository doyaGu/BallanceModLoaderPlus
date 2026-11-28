#include <gtest/gtest.h>

#include "HUD/SRTimer.h"

#include <limits>

class SRTimerTest : public ::testing::Test {
protected:
    SRTimer timer;
};

TEST_F(SRTimerTest, InitialState) {
    EXPECT_FLOAT_EQ(0.0f, timer.GetTime());
    EXPECT_FALSE(timer.IsRunning());
}

TEST_F(SRTimerTest, StartAndAccumulate) {
    timer.Start(1);
    EXPECT_TRUE(timer.IsRunning());

    timer.Update(1, 100.0f);
    EXPECT_EQ(0.0f, timer.GetTime());
    EXPECT_TRUE(timer.IsRunning());
    timer.Update(2, 100.0f);
    EXPECT_FLOAT_EQ(100.0f, timer.GetTime());

    timer.Update(3, 50.0f);
    EXPECT_FLOAT_EQ(150.0f, timer.GetTime());
}

TEST_F(SRTimerTest, PauseStopsAccumulation) {
    timer.Start(1);
    timer.Update(2, 100.0f);
    timer.Pause();
    EXPECT_FALSE(timer.IsRunning());

    timer.Update(3, 200.0f);
    EXPECT_FLOAT_EQ(100.0f, timer.GetTime());
}

TEST_F(SRTimerTest, ResetClearsTime) {
    timer.Start(1);
    timer.Update(2, 500.0f);
    timer.Reset();

    EXPECT_FLOAT_EQ(0.0f, timer.GetTime());
    EXPECT_FALSE(timer.IsRunning());
}

TEST_F(SRTimerTest, FormattedTimeBasic) {
    timer.Start(1);
    // 1h 23m 45s 678ms = 5025678ms
    timer.Update(2, 5025678.0f);

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

    timer.Start(1);
    timer.Update(2, 16.0f);
    EXPECT_TRUE(timer.IsDirty());
}

TEST_F(SRTimerTest, NotRunningDoesNotAccumulate) {
    timer.Update(1, 1000.0f);
    EXPECT_FLOAT_EQ(0.0f, timer.GetTime());
}

TEST_F(SRTimerTest, ResumeExcludesOnlyItsOwnTick) {
    timer.Start(1);
    timer.Update(2, 100.0f);
    timer.Pause();
    timer.Start(3);
    timer.Update(3, 200.0f);
    EXPECT_EQ(100.0f, timer.GetTime());
    timer.Update(4, 16.0f);
    EXPECT_EQ(116.0f, timer.GetTime());
}

TEST_F(SRTimerTest, ResumeAfterUpdateCountsTheNextTickEvenAcrossWrap) {
    const auto tick = (std::numeric_limits<uint32_t>::max)();
    timer.Update(tick, 50.0f);
    timer.Start(tick);
    timer.Update(0, 16.0f);
    EXPECT_EQ(16.0f, timer.GetTime());
}

TEST_F(SRTimerTest, RepeatedPlayHasTheSameFrameBoundaryAsRetail) {
    timer.Start(1);
    timer.Update(2, 100.0f);
    timer.Start(3);
    timer.Update(3, 200.0f);
    timer.Update(4, 16.0f);
    EXPECT_EQ(116.0f, timer.GetTime());
}

TEST_F(SRTimerTest, RoundsEveryFrameToFloatLikeRetail) {
    timer.Start(0);
    for (uint32_t tick = 1; tick <= 36000; ++tick)
        timer.Update(tick, 16.67f);
    EXPECT_EQ(600008.625f, timer.GetTime());
    EXPECT_STREQ("  00:10:00.008", timer.GetFormattedTime());
}

TEST_F(SRTimerTest, GameplayHonorsRetailInputOrderAndResume) {
    timer.UpdateGameplay(SRTimer::OnReset, 100.0f);
    EXPECT_EQ(timer.GetTime(), 100.0f);
    timer.UpdateGameplay(SRTimer::OnPause | SRTimer::OnPlay, 200.0f);
    timer.UpdateGameplay(0, 20.0f);
    EXPECT_EQ(timer.GetTime(), 100.0f);
    timer.UpdateGameplay(SRTimer::OnPlay, 300.0f);
    EXPECT_EQ(timer.GetTime(), 100.0f);
    timer.UpdateGameplay(0, 0.125f);
    EXPECT_EQ(timer.GetTime(), 100.125f);
    timer.UpdateGameplay(SRTimer::OnOff, 75.0f);
    EXPECT_EQ(timer.GetTime(), 0.0f);
    timer.UpdateGameplay(0, 0.25f);
    EXPECT_EQ(timer.GetTime(), 0.25f);
    timer.UpdateGameplay(SRTimer::OnReset | SRTimer::OnPause, 400.0f);
    timer.UpdateGameplay(0, 40.0f);
    EXPECT_EQ(timer.GetTime(), 0.0f);
    EXPECT_FALSE(timer.IsRunning());
}

TEST_F(SRTimerTest, ManualClockYieldsToTheNextGameplayCommand) {
    timer.UpdateGameplay(SRTimer::OnReset | SRTimer::OnPause, 100.0f);
    timer.Start(10);
    timer.UpdateGameplay(0, 200.0f);
    timer.Update(10, 200.0f);
    EXPECT_EQ(timer.GetTime(), 0.0f);
    timer.UpdateGameplay(0, 300.0f);
    timer.Update(11, 0.25f);
    EXPECT_EQ(timer.GetTime(), 0.25f);

    timer.UpdateGameplay(SRTimer::OnPlay, 400.0f);
    EXPECT_FALSE(timer.UsesManualClock());
    timer.UpdateGameplay(0, 0.5f);
    EXPECT_EQ(timer.GetTime(), 0.75f);
}

TEST_F(SRTimerTest, FormattedTimeUsesThePublishedFloatAtMillisecondBoundary) {
    timer.Start(1);
    timer.Update(2, 1024.0f);
    timer.Update(3, 0.99997f);
    EXPECT_EQ(1025.0f, timer.GetTime());
    EXPECT_STREQ("  00:00:01.025", timer.GetFormattedTime());
}

TEST_F(SRTimerTest, FormatsUpdatesDeferredWhileHiddenEvenAfterPauseOrReset) {
    timer.Start(1);
    timer.Update(2, 100.0f);
    EXPECT_STREQ("  00:00:00.100", timer.GetFormattedTime());

    timer.Update(3, 50.0f);
    timer.Pause();
    EXPECT_TRUE(timer.IsDirty());
    EXPECT_EQ(150.0f, timer.GetTime());
    EXPECT_STREQ("  00:00:00.150", timer.GetFormattedTime());

    timer.Reset();
    timer.Update(4, 20.0f);
    EXPECT_TRUE(timer.IsDirty());
    EXPECT_STREQ("  00:00:00.000", timer.GetFormattedTime());
}
