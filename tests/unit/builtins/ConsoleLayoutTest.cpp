#include "Console/ConsoleLayout.h"

#include <gtest/gtest.h>

TEST(ConsoleLayoutTest, PlacesTransientSurfaceBelowCommandBar) {
    const ConsoleLayout::Stack stack = ConsoleLayout::Calculate(
        {100.0f, 50.0f, 1200.0f, 700.0f}, 24.0f, 26.0f);

    EXPECT_GT(stack.commandBar.width, 0.0f);
    EXPECT_GE(stack.commandBar.x, 100.0f);
    EXPECT_LT(stack.messageBottom, stack.commandBar.y);
    EXPECT_LT(stack.commandBar.y + stack.commandBar.height,
              stack.transientSurface.y);
    EXPECT_LE(stack.transientSurface.y + stack.transientSurface.height,
              750.0f);
    EXPECT_EQ(stack.transientSurface.x, stack.commandBar.x);
    EXPECT_EQ(stack.transientSurface.width, stack.commandBar.width);
}

TEST(ConsoleLayoutTest, ClampsDegenerateViewportWithoutNegativeRects) {
    const ConsoleLayout::Stack stack = ConsoleLayout::Calculate(
        {7.0f, 11.0f, 10.0f, 8.0f}, 30.0f, 30.0f);

    EXPECT_GE(stack.commandBar.width, 0.0f);
    EXPECT_GE(stack.commandBar.height, 0.0f);
    EXPECT_GE(stack.transientSurface.width, 0.0f);
    EXPECT_GE(stack.transientSurface.height, 0.0f);
    EXPECT_GE(stack.commandBar.y, 11.0f);
    EXPECT_GE(stack.transientSurface.y, 11.0f);
    EXPECT_LE(stack.messageBottom, 19.0f);
}
