#include <gtest/gtest.h>

#include "UI/ColorCodec.h"

namespace {

using UiColor::Rgba8;

TEST(ColorCodec, ParsesRgbAndRgbaHexColours) {
    EXPECT_EQ((Rgba8{0x61, 0xAF, 0xEF, 0xFF}), UiColor::ParseHex("#61afEF"));
    EXPECT_EQ((Rgba8{0xE0, 0x6C, 0x75, 0x80}), UiColor::ParseHex("  E06C7580\t"));
}

TEST(ColorCodec, FormatsCanonicalUppercaseHex) {
    EXPECT_EQ("#61AFEF", UiColor::FormatHex(Rgba8{0x61, 0xAF, 0xEF, 0x80}));
    EXPECT_EQ("#61AFEF80", UiColor::FormatHex(Rgba8{0x61, 0xAF, 0xEF, 0x80}, true));
}

TEST(ColorCodec, RejectsMalformedColours) {
    EXPECT_FALSE(UiColor::ParseHex("#ABC"));
    EXPECT_FALSE(UiColor::ParseHex("#GG6C75"));
    EXPECT_FALSE(UiColor::ParseHex("#E06C75 trailing"));
    EXPECT_FALSE(UiColor::ParseHex(""));
}

} // namespace
