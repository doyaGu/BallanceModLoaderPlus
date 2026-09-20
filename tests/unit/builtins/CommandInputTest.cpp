#include <gtest/gtest.h>

#include "Console/CommandInput.h"

TEST(CommandInputTest, SplitAndJoinPreserveEveryPhysicalRow) {
    const std::string logical = "first\n\nthird\n";
    CommandInput::Continuations continuations;
    const std::string current = continuations.Replace(logical);

    ASSERT_EQ(3u, continuations.Size());
    EXPECT_EQ("first", continuations.Rows()[0]);
    EXPECT_EQ("", continuations.Rows()[1]);
    EXPECT_EQ("third", continuations.Rows()[2]);
    EXPECT_EQ("", current);
    EXPECT_EQ(logical.size(), continuations.Bytes());
    EXPECT_EQ(logical, continuations.Join(current));
}

TEST(CommandInputTest, PendingByteCountIncludesSeparatingNewlines) {
    CommandInput::Continuations continuations;
    continuations.Push("one");
    continuations.Push("two");
    EXPECT_EQ(8u, continuations.Bytes());
    EXPECT_EQ("one\ntwo\ncurrent", continuations.Join("current"));
    continuations.Clear();
    EXPECT_TRUE(continuations.Empty());
    EXPECT_EQ(0u, continuations.Bytes());
}

TEST(CommandInputTest, PreviewEscapesLineBreaksForOneRowRails) {
    EXPECT_EQ("one \\n two three", CommandInput::SingleLinePreview("one\ntwo\rthree"));
}
