#include <gtest/gtest.h>

#include "FpsCounter.h"

class FpsCounterTest : public ::testing::Test {
protected:
    FpsCounter counter{60};
};

// C18: Division by zero when all frame times are zero
TEST_F(FpsCounterTest, DivisionByZeroOnZeroFrameTimes) {
    // First update triggers RecalculateAverage with all-zero frame times
    counter.Update(0.0f);

    float fps = counter.GetAverageFps();
    // Must not be inf, -inf, or NaN
    EXPECT_FALSE(std::isinf(fps)) << "FPS should not be infinity when frame times are zero";
    EXPECT_FALSE(std::isnan(fps)) << "FPS should not be NaN when frame times are zero";
}

TEST_F(FpsCounterTest, NormalFps60) {
    // Simulate 60 frames at ~16.67ms each
    for (int i = 0; i < 60; ++i) {
        counter.Update(1.0f / 60.0f);
    }
    float fps = counter.GetAverageFps();
    EXPECT_GT(fps, 50.0f);
    EXPECT_LT(fps, 70.0f);
}

TEST_F(FpsCounterTest, FormattedFpsNotEmpty) {
    counter.Update(1.0f / 60.0f);
    const char *formatted = counter.GetFormattedFps();
    ASSERT_NE(nullptr, formatted);
    EXPECT_GT(strlen(formatted), 0u);
}

TEST_F(FpsCounterTest, DirtyFlagAfterUpdate) {
    counter.ClearDirty();
    EXPECT_FALSE(counter.IsDirty());

    counter.Update(0.016f);
    EXPECT_TRUE(counter.IsDirty());

    counter.ClearDirty();
    EXPECT_FALSE(counter.IsDirty());
}

TEST_F(FpsCounterTest, UpdateFrequency) {
    counter.SetUpdateFrequency(5);
    EXPECT_EQ(5u, counter.GetUpdateFrequency());

    // Zero frequency should be clamped to 1
    counter.SetUpdateFrequency(0);
    EXPECT_EQ(1u, counter.GetUpdateFrequency());
}

TEST_F(FpsCounterTest, UpdateFrequencyDelaysDirty) {
    counter.SetUpdateFrequency(3);
    counter.ClearDirty();

    counter.Update(0.016f);
    EXPECT_FALSE(counter.IsDirty()) << "Should not be dirty after 1 of 3 frames";

    counter.Update(0.016f);
    EXPECT_FALSE(counter.IsDirty()) << "Should not be dirty after 2 of 3 frames";

    counter.Update(0.016f);
    EXPECT_TRUE(counter.IsDirty()) << "Should be dirty after 3 of 3 frames";
}

TEST_F(FpsCounterTest, VeryHighFps) {
    // 1000 FPS = 0.001s frame time
    for (int i = 0; i < 60; ++i) {
        counter.Update(0.001f);
    }
    float fps = counter.GetAverageFps();
    EXPECT_GT(fps, 900.0f);
    EXPECT_LT(fps, 1100.0f);
}

TEST_F(FpsCounterTest, VeryLowFps) {
    // 1 FPS = 1.0s frame time
    for (int i = 0; i < 60; ++i) {
        counter.Update(1.0f);
    }
    float fps = counter.GetAverageFps();
    EXPECT_GT(fps, 0.5f);
    EXPECT_LT(fps, 2.0f);
}

TEST_F(FpsCounterTest, SmallSampleCount) {
    FpsCounter small(1);
    small.Update(0.016f);
    float fps = small.GetAverageFps();
    EXPECT_GT(fps, 50.0f);
    EXPECT_LT(fps, 70.0f);
}
