#include "Hooks/RenderHook.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

TEST(RenderHookTest, KeepsFourByThreeFieldOfView) {
    const float cameraFov = 1.1f;
    float correctedFov = 0.0f;

    ASSERT_TRUE(RenderHook::CalculateWidescreenFov(cameraFov, 4.0f / 3.0f, &correctedFov));
    EXPECT_FLOAT_EQ(correctedFov, cameraFov);
}

TEST(RenderHookTest, ExpandsHorizontalFieldOfViewForWidescreen) {
    const float cameraFov = 1.1f;
    const float aspectRatio = 16.0f / 9.0f;
    float correctedFov = 0.0f;

    ASSERT_TRUE(RenderHook::CalculateWidescreenFov(cameraFov, aspectRatio, &correctedFov));
    const float expected = 2.0f * std::atan(std::tan(cameraFov * 0.5f) * 0.75f * aspectRatio);
    EXPECT_FLOAT_EQ(correctedFov, expected);
    EXPECT_GT(correctedFov, cameraFov);
}

TEST(RenderHookTest, LeavesNarrowerViewportsUnchanged) {
    const float cameraFov = 1.1f;
    float correctedFov = 0.0f;

    ASSERT_TRUE(RenderHook::CalculateWidescreenFov(cameraFov, 5.0f / 4.0f, &correctedFov));
    EXPECT_FLOAT_EQ(correctedFov, cameraFov);
}

TEST(RenderHookTest, RejectsInvalidProjectionInputs) {
    float correctedFov = 0.0f;

    EXPECT_FALSE(RenderHook::CalculateWidescreenFov(0.0f, 16.0f / 9.0f, &correctedFov));
    EXPECT_FALSE(RenderHook::CalculateWidescreenFov(1.0f, 0.0f, &correctedFov));
    EXPECT_FALSE(RenderHook::CalculateWidescreenFov(1.0f, 16.0f / 9.0f, nullptr));
    EXPECT_FALSE(RenderHook::CalculateWidescreenFov(
        std::numeric_limits<float>::quiet_NaN(), 16.0f / 9.0f, &correctedFov));
}
