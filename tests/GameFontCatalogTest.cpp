#include <gtest/gtest.h>

#include "UI/GameFontCatalog.h"

using BML::GameFont;
using BML::GameFontCatalog;

TEST(GameFontCatalogTest, DefaultsMatchLegacyFontValues) {
    GameFontCatalog fonts;

    EXPECT_EQ(fonts.Resolve(GameFont::None), 0);
    EXPECT_EQ(fonts.Resolve(GameFont::Normal), 1);
    EXPECT_EQ(fonts.Resolve(GameFont::CreditsBig), 7);
}

TEST(GameFontCatalogTest, BindsAndIdentifiesRuntimeFontIndices) {
    GameFontCatalog fonts;

    ASSERT_TRUE(fonts.Bind(GameFont::SmallGray, 37));
    EXPECT_EQ(fonts.Resolve(GameFont::SmallGray), 37);
    EXPECT_EQ(fonts.Identify(37), GameFont::SmallGray);
    EXPECT_EQ(fonts.Identify(999), GameFont::None);
}

TEST(GameFontCatalogTest, ResetRestoresStableLegacyFallbacks) {
    GameFontCatalog fonts;
    ASSERT_TRUE(fonts.Bind(GameFont::Huge, 61));

    fonts.Reset();

    EXPECT_EQ(fonts.Resolve(GameFont::Huge), 5);
    EXPECT_FALSE(fonts.Bind(static_cast<GameFont>(999), 1));
    EXPECT_EQ(fonts.Resolve(static_cast<GameFont>(999)), 0);
}
