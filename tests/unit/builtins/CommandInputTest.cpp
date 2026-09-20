#include <gtest/gtest.h>

#include "Console/CommandInput.h"

TEST(CommandInputTest, SplitAndJoinPreserveEveryPhysicalRow) {
    const std::string logical = "first\n\nthird\n";
    const CommandInput::Rows rows = CommandInput::Split(logical);

    ASSERT_EQ(3u, rows.pending.size());
    EXPECT_EQ("first", rows.pending[0]);
    EXPECT_EQ("", rows.pending[1]);
    EXPECT_EQ("third", rows.pending[2]);
    EXPECT_EQ("", rows.current);
    EXPECT_EQ(logical.size(), rows.currentOffset);
    EXPECT_EQ(logical, CommandInput::Join(rows.pending, rows.current));
}

TEST(CommandInputTest, PendingByteCountIncludesSeparatingNewlines) {
    const std::vector<std::string> pending{"one", "two"};
    EXPECT_EQ(8u, CommandInput::PendingBytes(pending));
    EXPECT_EQ("one\ntwo\ncurrent", CommandInput::Join(pending, "current"));
    EXPECT_TRUE(CommandInput::Equals(pending, "current", "one\ntwo\ncurrent"));
    EXPECT_FALSE(CommandInput::Equals(pending, "changed", "one\ntwo\ncurrent"));
    EXPECT_FALSE(CommandInput::Equals(pending, "current", "one\ntwo current"));
}

TEST(CommandInputTest, PreviewEscapesLineBreaksForOneRowRails) {
    EXPECT_EQ("one \\n two three", CommandInput::SingleLinePreview("one\ntwo\rthree"));
}
