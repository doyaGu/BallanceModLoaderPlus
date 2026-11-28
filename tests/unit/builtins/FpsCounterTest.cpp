#include <gtest/gtest.h>

#include "HUD/FpsCounter.h"

using namespace std::chrono_literals;

class FpsCounterTest : public ::testing::Test {
protected:
    void SetUp() override { counter.RecordFrame(now); }

    void RenderAfter(FpsCounter::Clock::duration interval, int frames = 1) {
        for (int frame = 0; frame < frames; ++frame) {
            now += interval;
            counter.RecordFrame(now);
        }
    }

    FpsCounter counter;
    FpsCounter::Clock::time_point now{};
};

TEST_F(FpsCounterTest, WaitsForACompleteInterval) {
    EXPECT_STREQ("FPS: --", counter.GetFormattedFps());
    counter.ClearDirty();
    counter.RecordFrame(now);
    counter.RecordFrame(now - 1ms);
    EXPECT_FALSE(counter.IsDirty());

    RenderAfter(16666667ns);
    EXPECT_STREQ("FPS: 60", counter.GetFormattedFps());
    EXPECT_NEAR(60.0f, counter.GetAverageFps(), 0.01f);
}

TEST_F(FpsCounterTest, PartialWindowCountsOnlyMeasuredFrames) {
    counter.SetUpdateFrequency(30);
    RenderAfter(50ms, 4);
    EXPECT_STREQ("FPS: --", counter.GetFormattedFps());
    RenderAfter(50ms);
    EXPECT_STREQ("FPS: 20", counter.GetFormattedFps());
    EXPECT_FLOAT_EQ(20.0f, counter.GetAverageFps());
}

TEST_F(FpsCounterTest, FrameFrequencyAppliesBeforeTheTimeLimit) {
    counter.SetUpdateFrequency(3);
    counter.ClearDirty();
    RenderAfter(10ms, 2);
    EXPECT_FALSE(counter.IsDirty());
    RenderAfter(10ms);
    EXPECT_TRUE(counter.IsDirty());
    EXPECT_STREQ("FPS: 100", counter.GetFormattedFps());
}

TEST_F(FpsCounterTest, UnchangedTextDoesNotRequireAnUpdate) {
    RenderAfter(10ms);
    ASSERT_TRUE(counter.IsDirty());
    counter.ClearDirty();
    RenderAfter(10ms);
    EXPECT_FALSE(counter.IsDirty());
    RenderAfter(30ms);
    EXPECT_TRUE(counter.IsDirty());
    EXPECT_STREQ("FPS: 60", counter.GetFormattedFps());
}

TEST_F(FpsCounterTest, OldFastFramesAgeOutAfterAFewSlowFrames) {
    counter.SetUpdateFrequency(30);
    RenderAfter(16666667ns, 60);
    EXPECT_STREQ("FPS: 60", counter.GetFormattedFps());
    RenderAfter(50ms, 10);
    EXPECT_STREQ("FPS: 20", counter.GetFormattedFps());
}

TEST_F(FpsCounterTest, UsesElapsedTimeBetweenRenderedFrames) {
    // Processing ticks between these renders never reach the counter.
    RenderAfter(100ms, 12);
    EXPECT_STREQ("FPS: 10", counter.GetFormattedFps());
}

TEST_F(FpsCounterTest, PreservesAFrameLongerThanTheWindow) {
    RenderAfter(10ms, 12);
    RenderAfter(1s);
    EXPECT_STREQ("FPS: 1", counter.GetFormattedFps());
    RenderAfter(10ms);
    EXPECT_STREQ("FPS: 100", counter.GetFormattedFps());
}

TEST_F(FpsCounterTest, ResetDiscardsSamplesAndKeepsConfiguredFrequency) {
    counter.SetUpdateFrequency(3);
    RenderAfter(1ms, 60);
    EXPECT_STREQ("FPS: 1000", counter.GetFormattedFps());
    counter.Reset();
    EXPECT_EQ(3u, counter.GetUpdateFrequency());
    EXPECT_STREQ("FPS: --", counter.GetFormattedFps());
    RenderAfter(5s);
    EXPECT_STREQ("FPS: --", counter.GetFormattedFps());
    RenderAfter(10ms, 3);
    EXPECT_STREQ("FPS: 100", counter.GetFormattedFps());
}

TEST_F(FpsCounterTest, ZeroSettingsAreClampedToOne) {
    FpsCounter small(0);
    small.SetUpdateFrequency(0);
    EXPECT_EQ(1u, small.GetUpdateFrequency());
    small.RecordFrame(now);
    small.RecordFrame(now + 10ms);
    small.RecordFrame(now + 30ms);
    EXPECT_STREQ("FPS: 50", small.GetFormattedFps());
}
