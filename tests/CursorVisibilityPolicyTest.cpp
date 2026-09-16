#include <gtest/gtest.h>

#include "Hooks/CursorVisibilityPolicy.h"

TEST(CursorVisibilityPolicyTest, ReleasingOverlayPreservesMenuCursor) {
    CursorVisibilityPolicy policy;
    policy.Reset(false);

    EXPECT_TRUE(policy.SetOverlayVisible(true));
    EXPECT_TRUE(policy.SetGameVisible(true));
    EXPECT_TRUE(policy.SetOverlayVisible(false));
}

TEST(CursorVisibilityPolicyTest, ReleasingOverlayRestoresHiddenGameplayCursor) {
    CursorVisibilityPolicy policy;
    policy.Reset(false);

    EXPECT_TRUE(policy.SetOverlayVisible(true));
    EXPECT_FALSE(policy.SetOverlayVisible(false));
}

TEST(CursorVisibilityPolicyTest, GameMayHideCursorWhileOverlayStillNeedsIt) {
    CursorVisibilityPolicy policy;
    policy.Reset(true);

    EXPECT_TRUE(policy.SetOverlayVisible(true));
    EXPECT_TRUE(policy.SetGameVisible(false));
    EXPECT_FALSE(policy.SetOverlayVisible(false));
}

TEST(CursorVisibilityPolicyTest, ResetDiscardsPreviousOverlayRequest) {
    CursorVisibilityPolicy policy;
    policy.Reset(false);

    EXPECT_TRUE(policy.SetOverlayVisible(true));
    policy.Reset(false);
    EXPECT_FALSE(policy.IsVisible());
}
