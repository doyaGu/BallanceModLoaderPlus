#include <gtest/gtest.h>

#include <map>

#include "Console/Shell/ShellParser.h"
#include "Console/Shell/ShellTypes.h"

using namespace BML::Shell;

namespace {
    class MapAliases final : public AliasResolver {
    public:
        std::map<std::string, std::string> aliases;

        bool LookupAlias(std::string_view name, std::string &body) const override {
            const auto it = aliases.find(std::string(name));
            if (it == aliases.end())
                return false;
            body = it->second;
            return true;
        }
    };

    std::vector<std::string> WordsOf(const SimpleCommand &command) {
        std::vector<std::string> words;
        for (const Word &word : command.words)
            words.push_back(word.Literal());
        return words;
    }
}

TEST(ShellParser, EmptyAndCommentOnlyLinesParseToNothing) {
    EXPECT_TRUE(Parse("").Ok());
    EXPECT_TRUE(Parse("").list.Empty());
    EXPECT_TRUE(Parse("   # just a comment").Ok());
    EXPECT_TRUE(Parse("   # just a comment").list.Empty());
    EXPECT_TRUE(Parse("\n\n").list.Empty());
}

TEST(ShellParser, SingleCommand) {
    const ParseResult result = Parse("echo a \"b c\"");
    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(1u, result.list.items.size());
    ASSERT_EQ(1u, result.list.items[0].pipelines.size());
    ASSERT_EQ(1u, result.list.items[0].pipelines[0].commands.size());
    EXPECT_EQ((std::vector<std::string>{"echo", "a", "b c"}), WordsOf(result.list.items[0].pipelines[0].commands[0]));
}

TEST(ShellParser, ListsAndOrPipelines) {
    const ParseResult result = Parse("a; b && c || d | e\nf;");
    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(3u, result.list.items.size());
    EXPECT_EQ(1u, result.list.items[0].pipelines.size());
    ASSERT_EQ(3u, result.list.items[1].pipelines.size());
    ASSERT_EQ(2u, result.list.items[1].ops.size());
    EXPECT_EQ(ListOp::And, result.list.items[1].ops[0]);
    EXPECT_EQ(ListOp::Or, result.list.items[1].ops[1]);
    ASSERT_EQ(2u, result.list.items[1].pipelines[2].commands.size());
    EXPECT_EQ("d", WordsOf(result.list.items[1].pipelines[2].commands[0])[0]);
    EXPECT_EQ("e", WordsOf(result.list.items[1].pipelines[2].commands[1])[0]);
    EXPECT_EQ("f", WordsOf(result.list.items[2].pipelines[0].commands[0])[0]);
}

TEST(ShellParser, NewlinesMayFollowOperators) {
    const ParseResult result = Parse("a &&\n b |\n c");
    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(1u, result.list.items.size());
    ASSERT_EQ(2u, result.list.items[0].pipelines.size());
    EXPECT_EQ(2u, result.list.items[0].pipelines[1].commands.size());
}

TEST(ShellParser, TrailingOperatorIsIncomplete) {
    EXPECT_TRUE(Parse("a &&").incomplete);
    EXPECT_TRUE(Parse("a ||").incomplete);
    EXPECT_TRUE(Parse("a |").incomplete);
    EXPECT_TRUE(Parse("echo 'x").incomplete);
    EXPECT_FALSE(Parse("a &&").error);
}

TEST(ShellParser, SyntaxErrorsWithPositions) {
    ParseResult result = Parse("a;;b");
    EXPECT_TRUE(result.error);
    EXPECT_EQ(2u, result.errorPos);

    result = Parse("| a");
    EXPECT_TRUE(result.error);
    EXPECT_EQ(0u, result.errorPos);

    result = Parse("a | | b");
    EXPECT_TRUE(result.error);
    EXPECT_EQ(4u, result.errorPos);

    result = Parse("a && ; b");
    EXPECT_TRUE(result.error);

    result = Parse("echo x & y");
    EXPECT_TRUE(result.error);
    EXPECT_EQ(7u, result.errorPos);

    result = Parse("; a");
    EXPECT_TRUE(result.error);
}

TEST(ShellParser, AliasReplacesFirstWordOnly) {
    MapAliases aliases;
    aliases.aliases["h"] = "help";
    aliases.aliases["e"] = "echo -n";
    const ParseResult result = Parse("e h x; h", &aliases);
    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(2u, result.list.items.size());
    const SimpleCommand &first = result.list.items[0].pipelines[0].commands[0];
    EXPECT_EQ((std::vector<std::string>{"echo", "-n", "h", "x"}), WordsOf(first));
    EXPECT_EQ("e", first.aliasName);
    const SimpleCommand &second = result.list.items[1].pipelines[0].commands[0];
    EXPECT_EQ((std::vector<std::string>{"help"}), WordsOf(second));
    EXPECT_EQ("h", second.aliasName);
}

TEST(ShellParser, AliasBodyMayHoldOperators) {
    MapAliases aliases;
    aliases.aliases["both"] = "echo a && echo b";
    const ParseResult result = Parse("both c", &aliases);
    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(1u, result.list.items.size());
    ASSERT_EQ(2u, result.list.items[0].pipelines.size());
    EXPECT_EQ((std::vector<std::string>{"echo", "b", "c"}),
              WordsOf(result.list.items[0].pipelines[1].commands[0]));
}

TEST(ShellParser, AliasChainsButNeverRecurses) {
    MapAliases aliases;
    aliases.aliases["a"] = "b";
    aliases.aliases["b"] = "c";
    aliases.aliases["c"] = "a x";
    const ParseResult result = Parse("a", &aliases);
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ((std::vector<std::string>{"a", "x"}), WordsOf(result.list.items[0].pipelines[0].commands[0]));

    aliases.aliases["ls"] = "ls -l";
    const ParseResult self = Parse("ls", &aliases);
    ASSERT_TRUE(self.Ok());
    EXPECT_EQ((std::vector<std::string>{"ls", "-l"}), WordsOf(self.list.items[0].pipelines[0].commands[0]));
}

TEST(ShellParser, QuotedWordIsNotAnAlias) {
    MapAliases aliases;
    aliases.aliases["h"] = "help";
    // Quoting bypasses an alias. A backslash before an ordinary letter is kept
    // literally by this shell, so \h names a different word altogether.
    const ParseResult result = Parse("'h'; \"h\"; \\h; h", &aliases);
    ASSERT_TRUE(result.Ok());
    EXPECT_EQ("h", WordsOf(result.list.items[0].pipelines[0].commands[0])[0]);
    EXPECT_EQ("h", WordsOf(result.list.items[1].pipelines[0].commands[0])[0]);
    EXPECT_EQ("\\h", WordsOf(result.list.items[2].pipelines[0].commands[0])[0]);
    EXPECT_EQ("help", WordsOf(result.list.items[3].pipelines[0].commands[0])[0]);
    EXPECT_TRUE(result.list.items[0].pipelines[0].commands[0].aliasName.empty());
}

TEST(ShellParser, BrokenAliasBodyIsAnError) {
    MapAliases aliases;
    aliases.aliases["bad"] = "echo 'unterminated";
    aliases.aliases["empty"] = "   ";
    EXPECT_TRUE(Parse("bad", &aliases).error);
    EXPECT_TRUE(Parse("empty", &aliases).error);
}

TEST(ShellParser, EnforcesLimits) {
    std::string many;
    for (std::size_t i = 0; i <= Limits::MaxPipelineStages; ++i) {
        if (i)
            many += " | ";
        many += "x";
    }
    EXPECT_TRUE(Parse(many).error);

    std::string commands;
    for (std::size_t i = 0; i <= Limits::MaxCommandsPerLine; ++i) {
        if (i)
            commands += "; ";
        commands += "x";
    }
    EXPECT_TRUE(Parse(commands).error);
}
