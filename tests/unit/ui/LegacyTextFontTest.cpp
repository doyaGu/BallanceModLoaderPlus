#include <gtest/gtest.h>

#include "UI/Gui/LegacyTextFont.h"

using BGui::ChooseLegacyTextDefaultFace;

TEST(LegacyTextFontTest, PrefersChineseUiFaceRegardlessOfEnumerationOrder) {
    const std::vector<std::string> faces = {
        "Segoe UI", "Microsoft YaHei", "Microsoft YaHei UI"};

    EXPECT_EQ(ChooseLegacyTextDefaultFace(faces), "Microsoft YaHei UI");
}

TEST(LegacyTextFontTest, UsesSegoeUiAsWindowsTenFallback) {
    EXPECT_EQ(ChooseLegacyTextDefaultFace({"Arial", "Segoe UI"}), "Segoe UI");
}

TEST(LegacyTextFontTest, UsesVirtoolsDefaultWhenNoPreferredFaceExists) {
    EXPECT_TRUE(ChooseLegacyTextDefaultFace({"Arial"}).empty());
}
