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
