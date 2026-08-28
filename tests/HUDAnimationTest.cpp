#include <gtest/gtest.h>

#include "HUD.h"

TEST(HUDAnimationTest, ColorEndpointsRemainExact) {
    const ImU32 start = IM_COL32(255, 0, 0, 255);
    const ImU32 end = IM_COL32(0, 64, 255, 17);
    HUDAnimation animation(start, end, 1.0f);

    EXPECT_EQ(start, animation.GetCurrentColor());
    animation.Update(1.0f);
    EXPECT_EQ(end, animation.GetCurrentColor());
    EXPECT_TRUE(animation.IsFinished());
}

TEST(HUDAnimationTest, ColorInterpolationIsChannelWise) {
    HUDAnimation animation(IM_COL32(0, 20, 200, 10),
                           IM_COL32(255, 100, 0, 250),
                           1.0f);

    animation.Update(0.5f);
    EXPECT_EQ(IM_COL32(128, 60, 100, 130), animation.GetCurrentColor());
}

TEST(HUDAnimationTest, ScalarConstructorRejectsColorProperty) {
    EXPECT_THROW((HUDAnimation(HUDAnimation::Color, 0.0f, 1.0f, 1.0f)), std::invalid_argument);
}
