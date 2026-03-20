#include <gtest/gtest.h>

#include "../modules/BML_ModMenu/src/MenuInputState.h"

namespace {

using BML::ModMenu::MenuInputState;

TEST(MenuInputStateTests, IdleDevicesReleaseImmediately) {
    MenuInputState state;

    EXPECT_TRUE(state.BeginRelease());
}

TEST(MenuInputStateTests, EscapeReleaseWaitsForKeyUp) {
    MenuInputState state;
    state.OnKeyDown(0x01);

    EXPECT_FALSE(state.BeginRelease());
    EXPECT_FALSE(state.OnKeyUp(0x1C));
    EXPECT_TRUE(state.OnKeyUp(0x01));
}

TEST(MenuInputStateTests, EnterReleaseWaitsForKeyUp) {
    MenuInputState state;
    state.OnKeyDown(0x1C);

    EXPECT_FALSE(state.BeginRelease());
    EXPECT_TRUE(state.OnKeyUp(0x1C));
}

TEST(MenuInputStateTests, CancelPendingReleaseDropsStaleDeferredUnblock) {
    MenuInputState state;
    state.OnKeyDown(0x01);

    EXPECT_FALSE(state.BeginRelease());
    state.CancelPendingRelease();

    EXPECT_FALSE(state.OnKeyUp(0x01));
}

TEST(MenuInputStateTests, IrrelevantKeysDoNotDelayRelease) {
    MenuInputState state;
    state.OnKeyDown('A');

    EXPECT_TRUE(state.BeginRelease());
}

} // namespace
