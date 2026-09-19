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

TEST(GameFontCatalogTest, BindsKnownRuntimeNamesOnly) {
    GameFontCatalog fonts;
    GameFont boundRole = GameFont::None;

    EXPECT_TRUE(fonts.Bind("GameFont_03a", 37, &boundRole));
    EXPECT_EQ(boundRole, GameFont::SmallGray);
    EXPECT_EQ(fonts.Resolve(GameFont::SmallGray), 37);
    EXPECT_FALSE(fonts.Bind("GameFont_Unknown", 88));
    EXPECT_EQ(fonts.Identify(88), GameFont::None);
}

TEST(GameFontCatalogTest, ResetRestoresStableLegacyFallbacks) {
    GameFontCatalog fonts;
    ASSERT_TRUE(fonts.Bind(GameFont::Huge, 61));

    fonts.Reset();

    EXPECT_EQ(fonts.Resolve(GameFont::Huge), 5);
    EXPECT_FALSE(fonts.Bind(static_cast<GameFont>(999), 1));
    EXPECT_EQ(fonts.Resolve(static_cast<GameFont>(999)), 0);
}

TEST(GameFontCatalogTest, BoundRuntimeFontWinsOverLegacyFallbackCollision) {
    GameFontCatalog fonts;
    constexpr int LegacyNormalIndex = static_cast<int>(GameFont::Normal);

    ASSERT_TRUE(fonts.Bind(GameFont::SmallGray, LegacyNormalIndex));
    EXPECT_EQ(fonts.Identify(LegacyNormalIndex), GameFont::SmallGray);
}

TEST(GameFontCatalogTest, RejectsInvalidAndAmbiguousRuntimeHandles) {
    GameFontCatalog fonts;
    constexpr int RuntimeHandle = 12;
    constexpr int InvalidHandle = -1;

    EXPECT_FALSE(fonts.Bind(GameFont::None, RuntimeHandle));
    EXPECT_FALSE(fonts.Bind(GameFont::Normal, 0));
    EXPECT_FALSE(fonts.Bind(GameFont::Normal, InvalidHandle));
    ASSERT_TRUE(fonts.Bind(GameFont::Normal, RuntimeHandle));
    EXPECT_FALSE(fonts.Bind(GameFont::Large, RuntimeHandle));
    EXPECT_EQ(fonts.Identify(InvalidHandle), GameFont::None);
}
