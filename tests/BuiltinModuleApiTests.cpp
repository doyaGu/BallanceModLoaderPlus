#include <gtest/gtest.h>

#include "AnsiSegments.h"

TEST(BuiltinModuleAnsiSegmentsTest, ParseAndFreeAnsiSegments) {
    auto *segments = BML::UI::CreateParsedAnsiSegments("plain \x1b[31mred\x1b[0m text");
    ASSERT_NE(segments, nullptr);

    const auto &parsed = segments->GetText();
    EXPECT_FALSE(parsed.GetSegments().empty());
    EXPECT_GE(parsed.GetSegments().size(), 2u);

    BML::UI::DestroyParsedAnsiSegments(segments);
    EXPECT_EQ(nullptr, BML::UI::CreateParsedAnsiSegments(nullptr));
}
