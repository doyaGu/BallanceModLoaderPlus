#include <gtest/gtest.h>

#include <string>

#include "AnsiText.h"

namespace {
    std::string SegmentText(const AnsiText::TextSegment &segment) {
        return std::string(segment.begin, segment.end);
    }
}

TEST(AnsiTextTest, Utf8ContinuationByte9BIsNotTreatedAsCsi) {
    const std::string input = "\xE6\xB2\x9B\x6D"; // UTF-8 for U+6C9B followed by m

    AnsiText::AnsiString text(input);
    const auto &segments = text.GetSegments();

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(SegmentText(segments[0]), input);
    EXPECT_EQ(segments[0].color, AnsiText::ConsoleColor());
}

TEST(AnsiTextTest, EscSgrSequencesStillSplitSegments) {
    const std::string input = "\x1B[31mred\x1B[0mplain";

    AnsiText::AnsiString text(input);
    const auto &segments = text.GetSegments();

    ASSERT_EQ(segments.size(), 2u);
    EXPECT_EQ(SegmentText(segments[0]), "red");
    EXPECT_EQ(SegmentText(segments[1]), "plain");
    EXPECT_NE(segments[0].color, AnsiText::ConsoleColor());
    EXPECT_EQ(segments[1].color, AnsiText::ConsoleColor());
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
