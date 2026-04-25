#include <gtest/gtest.h>

#include <string>

#include "AnsiText.h"

namespace {
    std::string SegmentText(const AnsiText::TextSegment &segment) {
        return std::string(segment.begin, segment.end);
    }
}

TEST(AnsiTextTest, MoveAssignmentRebindsShortStringSegmentsToDestinationBuffer) {
    AnsiText::AnsiString source("Connecting...");
    AnsiText::AnsiString dest("placeholder");

    dest = std::move(source);

    const std::string &stored = dest.GetOriginalText();
    const auto &segments = dest.GetSegments();

    ASSERT_EQ(stored, "Connecting...");
    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(SegmentText(segments[0]), stored);
    EXPECT_EQ(segments[0].begin, stored.c_str());
    EXPECT_EQ(segments[0].end, stored.c_str() + stored.size());
}
