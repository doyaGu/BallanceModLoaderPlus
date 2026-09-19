#include "UI/Ime/PresentationOwnership.h"

#include <gtest/gtest.h>

using Overlay::Ime::PresentationOwnership;

TEST(ImeOwnershipTest, FocusAloneDoesNotOwnPresentation) {
    PresentationOwnership ownership;
    ownership.Attach(true);

    EXPECT_TRUE(ownership.IsFocused());
    EXPECT_FALSE(ownership.IsVisible());
}

TEST(ImeOwnershipTest, ActiveTextPresentationRequiresFocus) {
    PresentationOwnership ownership;
    ownership.Attach(true);
    EXPECT_FALSE(ownership.ExchangeVisible(true));
    EXPECT_TRUE(ownership.IsVisible());

    EXPECT_TRUE(ownership.SetFocused(false));
    EXPECT_FALSE(ownership.IsFocused());
    EXPECT_FALSE(ownership.IsVisible());
}

TEST(ImeOwnershipTest, RefocusingDoesNotRestoreStalePresentation) {
    PresentationOwnership ownership;
    ownership.Attach(true);
    ownership.ExchangeVisible(true);
    ownership.SetFocused(false);

    EXPECT_TRUE(ownership.SetFocused(true));
    EXPECT_TRUE(ownership.IsFocused());
    EXPECT_FALSE(ownership.IsVisible());
}

TEST(ImeOwnershipTest, DetachClearsAllOwnership) {
    PresentationOwnership ownership;
    ownership.Attach(true);
    ownership.ExchangeVisible(true);
    ownership.Detach();

    EXPECT_FALSE(ownership.IsFocused());
    EXPECT_FALSE(ownership.IsVisible());
    EXPECT_FALSE(ownership.ExchangeVisible(true));
    EXPECT_FALSE(ownership.IsVisible());
}
