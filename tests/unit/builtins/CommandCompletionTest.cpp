#include "Console/CommandCompletion.h"

#include <gtest/gtest.h>

TEST(CommandCompletionTest, CommonPrefixEndsAtUtf8CodepointBoundary) {
    const std::vector<std::string> candidates = {
        "\xe4\xbd\xa0\xe5\xa5\xbd",
        "\xe4\xbd\xa0\xe5\xae\x89",
    };

    const std::size_t prefixLength = CommandCompletion::CommonPrefixLength(candidates);
    EXPECT_EQ(prefixLength, 3u);
    EXPECT_EQ(candidates.front().substr(0, prefixLength), "\xe4\xbd\xa0");
}

TEST(CommandCompletionTest, CommonPrefixIsUnicodeCaseInsensitive) {
    const std::vector<std::string> candidates = {
        "\xc3\x84pfel",
        "\xc3\xa4PFELmus",
    };

    EXPECT_EQ(CommandCompletion::CommonPrefixLength(candidates), 6u);
}

TEST(CommandCompletionTest, InvalidUtf8HasNoCompletablePrefix) {
    const std::vector<std::string> candidates = {
        "valid",
        std::string("\xe5", 1),
    };

    EXPECT_EQ(CommandCompletion::CommonPrefixLength(candidates), 0u);
}

TEST(CommandCompletionTest, TokenRangePreservesExistingSeparator) {
    const std::string text = "map so tail";
    const CommandCompletion::TokenRange range =
        CommandCompletion::FindTokenRange(text, 6);

    EXPECT_EQ(range.begin, 4u);
    EXPECT_EQ(range.end, 6u);
    EXPECT_TRUE(range.followedByWhitespace);
}

TEST(CommandCompletionTest, TokenRangeCoversTextOnBothSidesOfCursor) {
    const std::string text = "map something tail";
    const CommandCompletion::TokenRange range =
        CommandCompletion::FindTokenRange(text, 8);

    EXPECT_EQ(text.substr(range.begin, range.end - range.begin), "something");
    EXPECT_TRUE(range.followedByWhitespace);
}

TEST(CommandCompletionTest, EmptyTrailingTokenNeedsSeparator) {
    const std::string text = "map ";
    const CommandCompletion::TokenRange range =
        CommandCompletion::FindTokenRange(text, text.size());

    EXPECT_EQ(range.begin, text.size());
    EXPECT_EQ(range.end, text.size());
    EXPECT_FALSE(range.followedByWhitespace);
}
