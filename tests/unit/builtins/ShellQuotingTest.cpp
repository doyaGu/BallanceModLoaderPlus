#include <gtest/gtest.h>

#include "Console/Shell/ShellLexer.h"
#include "Console/Shell/ShellQuoting.h"

using namespace BML::Shell;

namespace {
    const char *const kCorpus[] = {
        "plain",
        "with space",
        "it's",
        "say \"hi\"",
        "$HOME and ${X}",
        "a;b&&c||d|e",
        "#hash",
        "back\\slash",
        "C:\\Maps\\x.nmo",
        "paren(s)",
        "bang!",
        "`tick`",
        "tab\there",
        "\xE4\xBD\xA0\xE5\xA5\xBD \xC3\xA9",
        "",
    };

    // Lexes text and returns the single word it holds.
    bool LexOneWord(const std::string &text, std::string &word) {
        const LexResult result = Lex(text);
        if (!result.Ok() || result.tokens.size() != 1 || result.tokens[0].kind != Token::Kind::Word)
            return false;
        word = LiteralText(result.tokens[0].parts);
        return true;
    }
}

TEST(ShellQuoting, BareRoundTrip) {
    for (const char *value : kCorpus) {
        const std::string quoted = QuoteForContext(value, QuoteContext::Bare, false);
        std::string word;
        ASSERT_TRUE(LexOneWord(quoted, word)) << quoted;
        EXPECT_EQ(value, word) << quoted;
    }
}

TEST(ShellQuoting, SingleRoundTrip) {
    for (const char *value : kCorpus) {
        const std::string quoted = "'" + QuoteForContext(value, QuoteContext::Single, true);
        std::string word;
        ASSERT_TRUE(LexOneWord(quoted, word)) << quoted;
        EXPECT_EQ(value, word) << quoted;
    }
}

TEST(ShellQuoting, DoubleRoundTrip) {
    for (const char *value : kCorpus) {
        const std::string quoted = "\"" + QuoteForContext(value, QuoteContext::Double, true);
        std::string word;
        ASSERT_TRUE(LexOneWord(quoted, word)) << quoted;
        EXPECT_EQ(value, word) << quoted;
    }
}

TEST(ShellQuoting, AnsiCRoundTrip) {
    for (const char *value : kCorpus) {
        const std::string quoted = "$'" + QuoteForContext(value, QuoteContext::AnsiC, true);
        std::string word;
        ASSERT_TRUE(LexOneWord(quoted, word)) << quoted;
        EXPECT_EQ(value, word) << quoted;
    }
    const std::string quoted = "$'" + QuoteForContext("new\nline\x1b[0m", QuoteContext::AnsiC, true);
    std::string word;
    ASSERT_TRUE(LexOneWord(quoted, word));
    EXPECT_EQ("new\nline\x1b[0m", word);
}

TEST(ShellQuoting, BareLeavesPlainTextAlone) {
    EXPECT_EQ("C:\\Maps\\x.nmo", QuoteForContext("C:\\Maps\\x.nmo", QuoteContext::Bare, false));
    EXPECT_EQ("a\\ b", QuoteForContext("a b", QuoteContext::Bare, false));
    EXPECT_EQ("''", QuoteForContext("", QuoteContext::Bare, false));
}

TEST(ShellQuoting, SingleQuotedListingForm) {
    EXPECT_EQ("'echo hi'", SingleQuoted("echo hi"));
    EXPECT_EQ("'it'\\''s'", SingleQuoted("it's"));
}

TEST(ShellQuoting, NoClosingQuoteWhenAsked) {
    EXPECT_EQ("a b", QuoteForContext("a b", QuoteContext::Single, false));
    EXPECT_EQ("a\\$b", QuoteForContext("a$b", QuoteContext::Double, false));
}
