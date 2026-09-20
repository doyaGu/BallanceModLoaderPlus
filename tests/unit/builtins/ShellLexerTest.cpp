#include <gtest/gtest.h>

#include "Console/Shell/ShellCompletion.h"
#include "Console/Shell/ShellLexer.h"

using namespace BML::Shell;

namespace {
    std::vector<std::string> Words(const LexResult &result) {
        std::vector<std::string> words;
        for (const Token &token : result.tokens) {
            if (token.kind == Token::Kind::Word)
                words.push_back(LiteralText(token.parts));
        }
        return words;
    }

    std::vector<Token::Kind> Kinds(const LexResult &result) {
        std::vector<Token::Kind> kinds;
        for (const Token &token : result.tokens)
            kinds.push_back(token.kind);
        return kinds;
    }
}

TEST(ShellLexer, SplitsOnUnquotedWhitespace) {
    const LexResult result = Lex("  hello   world  ");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"hello", "world"}), Words(result));
}

TEST(ShellLexer, EmptyInputYieldsNoTokens) {
    const LexResult result = Lex("");
    EXPECT_TRUE(result.Ok());
    EXPECT_TRUE(result.tokens.empty());
}

TEST(ShellLexer, DoubleQuotesKeepWhitespaceAndDropDelimiters) {
    const LexResult result = Lex("map load \"folder/my  map.nmo\"");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"map", "load", "folder/my  map.nmo"}), Words(result));
}

TEST(ShellLexer, QuotesMayAppearMidWord) {
    const LexResult result = Lex("a\"b c\"d 'x y'z");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"ab cd", "x yz"}), Words(result));
    EXPECT_EQ(3u, result.tokens[0].parts.size());
}

TEST(ShellLexer, SingleQuotesAreLiteral) {
    const LexResult result = Lex("echo '$HOME \\n \"q\" # not a comment'");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"echo", "$HOME \\n \"q\" # not a comment"}), Words(result));
}

TEST(ShellLexer, EmptyQuotesYieldEmptyWord) {
    const LexResult result = Lex("echo \"\" ''");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"echo", "", ""}), Words(result));
}

TEST(ShellLexer, BackslashEscapesOnlyMetacharactersOutsideQuotes) {
    const LexResult result = Lex("map load C:\\Maps\\x.nmo a\\ b \\\"q\\\" \\$x \\\\ \\!");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"map", "load", "C:\\Maps\\x.nmo", "a b", "\"q\"", "$x", "\\", "!"}),
              Words(result));
}

TEST(ShellLexer, DoubleQuoteEscapesAreLimited) {
    const LexResult result = Lex("echo \"a\\\"b\\\\c\\$d\\ne\\`f\"");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"echo", "a\"b\\c$d\\ne`f"}), Words(result));
}

TEST(ShellLexer, AnsiCQuotingResolvesEscapes) {
    const LexResult result = Lex("echo $'a\\tb\\n' $'it\\'s' $'\\x41\\u00e9'");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"echo", "a\tb\n", "it's", "A\xC3\xA9"}), Words(result));
}

TEST(ShellLexer, VariablesAreTypedParts) {
    const LexResult result = Lex("echo $NAME ${NAME}x $? $ $1 \"v=$V\"");
    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(7u, result.tokens.size());
    ASSERT_EQ(1u, result.tokens[1].parts.size());
    EXPECT_EQ(WordPart::Kind::Variable, result.tokens[1].parts[0].kind);
    EXPECT_EQ("NAME", result.tokens[1].parts[0].text);
    ASSERT_EQ(2u, result.tokens[2].parts.size());
    EXPECT_TRUE(result.tokens[2].parts[0].braced);
    EXPECT_EQ("x", result.tokens[2].parts[1].text);
    EXPECT_EQ("?", result.tokens[3].parts[0].text);
    EXPECT_EQ("$", LiteralText(result.tokens[4].parts));
    EXPECT_EQ("$1", LiteralText(result.tokens[5].parts));
    ASSERT_EQ(1u, result.tokens[6].parts.size());
    EXPECT_EQ(WordPart::Kind::DoubleQuoted, result.tokens[6].parts[0].kind);
    ASSERT_EQ(2u, result.tokens[6].parts[0].children.size());
    EXPECT_EQ(WordPart::Kind::Variable, result.tokens[6].parts[0].children[1].kind);
}

TEST(ShellLexer, SubstitutionsCaptureInnerSource) {
    const LexResult result = Lex("echo $(a \"(x)\" $(b)) `c d`");
    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(3u, result.tokens.size());
    ASSERT_EQ(1u, result.tokens[1].parts.size());
    EXPECT_EQ(WordPart::Kind::Substitution, result.tokens[1].parts[0].kind);
    EXPECT_EQ("a \"(x)\" $(b)", result.tokens[1].parts[0].text);
    EXPECT_TRUE(result.tokens[2].parts[0].backtick);
    EXPECT_EQ("c d", result.tokens[2].parts[0].text);
}

TEST(ShellLexer, OperatorsAreTokens) {
    const LexResult result = Lex("a;b && c || d | e\nf");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<Token::Kind>{
                  Token::Kind::Word, Token::Kind::Semicolon, Token::Kind::Word, Token::Kind::AndAnd,
                  Token::Kind::Word, Token::Kind::OrOr, Token::Kind::Word, Token::Kind::Pipe,
                  Token::Kind::Word, Token::Kind::Newline, Token::Kind::Word}),
              Kinds(result));
}

TEST(ShellLexer, CommentOnlyAtWordStart) {
    const LexResult result = Lex("echo a#b # trailing ; not | ops");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"echo", "a#b"}), Words(result));
    EXPECT_EQ(Token::Kind::Comment, result.tokens.back().kind);
}

TEST(ShellLexer, LoneAmpersandAndParensAreErrors) {
    EXPECT_TRUE(Lex("a & b").error);
    EXPECT_TRUE(Lex("echo (x)").error);
    EXPECT_TRUE(Lex("echo x)").error);
    EXPECT_TRUE(Lex("echo ${1x}").error);
    const LexResult result = Lex("echo a & b");
    EXPECT_EQ(7u, result.errorPos);
}

TEST(ShellLexer, UnterminatedConstructsAreIncomplete) {
    EXPECT_TRUE(Lex("echo 'abc").incomplete);
    EXPECT_TRUE(Lex("echo \"abc").incomplete);
    EXPECT_TRUE(Lex("echo $'abc").incomplete);
    EXPECT_TRUE(Lex("echo $(abc").incomplete);
    EXPECT_TRUE(Lex("echo `abc").incomplete);
    EXPECT_TRUE(Lex("echo ${abc").incomplete);
    EXPECT_TRUE(Lex("echo abc \\").incomplete);
    EXPECT_FALSE(Lex("echo 'abc").error);
}

TEST(ShellLexer, LineContinuationJoinsRows) {
    const LexResult result = Lex("echo a\\\nb \"c\\\nd\"");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"echo", "ab", "cd"}), Words(result));
}

TEST(ShellLexer, IncompleteInputKeepsPartialWord) {
    const LexResult result = Lex("map load \"folder/my  ");
    ASSERT_TRUE(result.incomplete);
    EXPECT_EQ((std::vector<std::string>{"map", "load", "folder/my  "}), Words(result));
}

TEST(ShellLexer, Utf8PassesThrough) {
    const LexResult result = Lex("echo \xE4\xBD\xA0\xE5\xA5\xBD \"\xC3\xA9 t\xC3\xA9\"");
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"echo", "\xE4\xBD\xA0\xE5\xA5\xBD", "\xC3\xA9 t\xC3\xA9"}), Words(result));
}

TEST(ShellLexer, TokenRangesCoverSource) {
    const std::string text = "echo \"a b\" c";
    const LexResult result = Lex(text);
    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(3u, result.tokens.size());
    EXPECT_EQ(0u, result.tokens[0].begin);
    EXPECT_EQ(4u, result.tokens[0].end);
    EXPECT_EQ(5u, result.tokens[1].begin);
    EXPECT_EQ(10u, result.tokens[1].end);
    EXPECT_EQ(11u, result.tokens[2].begin);
    EXPECT_EQ(12u, result.tokens[2].end);
}

// SplitArguments feeds ICommand::GetTabCompletion.
TEST(ShellSplitArguments, BasicSplitting) {
    const ArgumentSplit split = SplitArguments("hello world");
    EXPECT_EQ((std::vector<std::string>{"hello", "world"}), split.args);
    EXPECT_FALSE(split.trailingSeparator);
}

TEST(ShellSplitArguments, ReportsTrailingSeparatorOnlyOutsideQuotes) {
    ArgumentSplit split = SplitArguments("map load ");
    EXPECT_EQ((std::vector<std::string>{"map", "load"}), split.args);
    EXPECT_TRUE(split.trailingSeparator);

    split = SplitArguments("map load \"folder/my  ");
    EXPECT_EQ((std::vector<std::string>{"map", "load", "folder/my  "}), split.args);
    EXPECT_FALSE(split.trailingSeparator);
}

TEST(ShellSplitArguments, UsesCommandAfterLastOperator) {
    ArgumentSplit split = SplitArguments("echo a && map lo");
    EXPECT_EQ((std::vector<std::string>{"map", "lo"}), split.args);

    split = SplitArguments("echo a | ");
    EXPECT_TRUE(split.args.empty());
    EXPECT_TRUE(split.trailingSeparator);

    split = SplitArguments("echo a;");
    EXPECT_TRUE(split.args.empty());
    EXPECT_FALSE(split.trailingSeparator);
}

TEST(ShellSplitArguments, EmptyAndNullLikeInput) {
    EXPECT_TRUE(SplitArguments("").args.empty());
    EXPECT_FALSE(SplitArguments("").trailingSeparator);
    const ArgumentSplit split = SplitArguments("test");
    EXPECT_EQ((std::vector<std::string>{"test"}), split.args);
}
