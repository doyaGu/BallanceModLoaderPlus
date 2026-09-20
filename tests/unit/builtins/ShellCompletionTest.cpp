#include <gtest/gtest.h>

#include <algorithm>
#include <map>

#include "Console/Shell/ShellCompletion.h"
#include "Console/Shell/ShellHighlighter.h"
#include "Console/Shell/ShellParser.h"

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

    struct Fixture {
        std::vector<std::vector<std::string>> argumentRequests;
        CompletionProviders providers;
        MapAliases aliases;

        Fixture() {
            providers.commandNames = [] { return std::vector<std::string>{"echo", "exit", "help", "map"}; };
            providers.aliasNames = [] { return std::vector<std::string>{"e", "hh"}; };
            providers.variableNames = [] { return std::vector<std::string>{"HOME", "HOST", "X"}; };
            providers.argumentCandidates = [this](const std::vector<std::string> &args) {
                argumentRequests.push_back(args);
                if (args[0] == "map" && args.size() == 2)
                    return std::vector<std::string>{"list", "load"};
                if (args[0] == "map" && args.size() == 3)
                    return std::vector<std::string>{"my map.nmo", "other.nmo"};
                return std::vector<std::string>{};
            };
            aliases.aliases["e"] = "echo -n";
        }

        CompletionPlan Build(const std::string &text) {
            return BuildCompletion(text, text.size(), providers, &aliases);
        }

        CompletionPlan BuildAt(const std::string &text, std::size_t cursor) {
            return BuildCompletion(text, cursor, providers, &aliases);
        }
    };

    std::string Apply(const std::string &text, const CompletionPlan &plan, const std::string &candidate, bool final) {
        std::string out = text;
        out.replace(plan.replaceBegin, plan.replaceEnd - plan.replaceBegin, RenderReplacement(plan, candidate, final));
        return out;
    }
}

TEST(ShellCompletion, CompletesCommandsAndAliasesAtLineStart) {
    Fixture fixture;
    const CompletionPlan plan = fixture.Build("e");
    EXPECT_EQ(CompletionPlan::Kind::Command, plan.kind);
    EXPECT_EQ((std::vector<std::string>{"echo", "exit", "e"}), plan.candidates);
    EXPECT_EQ(0u, plan.replaceBegin);
    EXPECT_EQ(1u, plan.replaceEnd);
    EXPECT_EQ("echo ", Apply("e", plan, "echo", true));
}

TEST(ShellCompletion, EmptyLineOffersEveryCommand) {
    Fixture fixture;
    const CompletionPlan plan = fixture.Build("");
    EXPECT_EQ(CompletionPlan::Kind::Command, plan.kind);
    EXPECT_EQ(6u, plan.candidates.size());
}

TEST(ShellCompletion, CommandPositionAfterOperators) {
    Fixture fixture;
    for (const char *text : {"echo a; he", "echo a && he", "echo a || he", "echo a | he", "echo a\nhe"}) {
        const CompletionPlan plan = fixture.Build(text);
        EXPECT_EQ(CompletionPlan::Kind::Command, plan.kind) << text;
        EXPECT_EQ((std::vector<std::string>{"help"}), plan.candidates) << text;
    }
    const CompletionPlan plan = fixture.Build("echo a |");
    EXPECT_EQ(CompletionPlan::Kind::Command, plan.kind);
    EXPECT_EQ(8u, plan.replaceBegin);
}

TEST(ShellCompletion, ArgumentsAskTheCommand) {
    Fixture fixture;
    CompletionPlan plan = fixture.Build("map l");
    EXPECT_EQ(CompletionPlan::Kind::Argument, plan.kind);
    ASSERT_EQ(1u, fixture.argumentRequests.size());
    EXPECT_EQ((std::vector<std::string>{"map", "l"}), fixture.argumentRequests[0]);
    EXPECT_EQ((std::vector<std::string>{"list", "load"}), plan.candidates);

    plan = fixture.Build("map load ");
    EXPECT_EQ((std::vector<std::string>{"map", "load", ""}), fixture.argumentRequests[1]);
    EXPECT_EQ(9u, plan.replaceBegin);
    EXPECT_EQ("map load my\\ map.nmo ", Apply("map load ", plan, "my map.nmo", true));
}

TEST(ShellCompletion, BackslashContinuationKeepsArgumentContext) {
    Fixture fixture;
    const std::string text = "map \\\nlo";
    const CompletionPlan plan = fixture.Build(text);

    EXPECT_EQ(CompletionPlan::Kind::Argument, plan.kind);
    ASSERT_EQ(1u, fixture.argumentRequests.size());
    EXPECT_EQ((std::vector<std::string>{"map", "lo"}), fixture.argumentRequests[0]);
    EXPECT_EQ((std::vector<std::string>{"load"}), plan.candidates);
    EXPECT_EQ("map load ", Apply(text, plan, "load", true));
}

TEST(ShellCompletion, QuotedContextsRenderCandidatesCorrectly) {
    Fixture fixture;
    CompletionPlan plan = fixture.Build("map load \"my");
    EXPECT_EQ(CompletionPlan::Kind::Argument, plan.kind);
    EXPECT_EQ(QuoteContext::Double, plan.context);
    EXPECT_EQ("my", plan.prefix);
    EXPECT_EQ((std::vector<std::string>{"my map.nmo"}), plan.candidates);
    EXPECT_EQ("map load \"my map.nmo\" ", Apply("map load \"my", plan, "my map.nmo", true));
    EXPECT_EQ("map load \"my map", Apply("map load \"my", plan, "my map", false));

    plan = fixture.Build("map load 'ot");
    EXPECT_EQ(QuoteContext::Single, plan.context);
    EXPECT_EQ("map load 'other.nmo' ", Apply("map load 'ot", plan, "other.nmo", true));

    plan = fixture.Build("map load $'ot");
    EXPECT_EQ(QuoteContext::AnsiC, plan.context);
    EXPECT_EQ("map load $'other.nmo' ", Apply("map load $'ot", plan, "other.nmo", true));
}

TEST(ShellCompletion, IncompleteAnsiCQuoteFiltersByItsPartialText) {
    Fixture fixture;

    const CompletionPlan plan = fixture.Build("map load $'ot");

    EXPECT_EQ(QuoteContext::AnsiC, plan.context);
    EXPECT_EQ("ot", plan.prefix);
    EXPECT_EQ((std::vector<std::string>{"other.nmo"}), plan.candidates);
}

TEST(ShellCompletion, AliasExpandsForArgumentCompletion) {
    Fixture fixture;
    fixture.aliases.aliases["m"] = "map";
    const CompletionPlan plan = fixture.Build("m lo");
    EXPECT_EQ(CompletionPlan::Kind::Argument, plan.kind);
    ASSERT_EQ(1u, fixture.argumentRequests.size());
    EXPECT_EQ((std::vector<std::string>{"map", "lo"}), fixture.argumentRequests[0]);
    EXPECT_EQ((std::vector<std::string>{"load"}), plan.candidates);
}

TEST(ShellCompletion, AliasChainsUseParserRulesForArgumentCompletion) {
    Fixture fixture;
    fixture.aliases.aliases["m"] = "next";
    fixture.aliases.aliases["next"] = "map";

    const CompletionPlan plan = fixture.Build("m lo");

    ASSERT_EQ(1u, fixture.argumentRequests.size());
    EXPECT_EQ((std::vector<std::string>{"map", "lo"}), fixture.argumentRequests[0]);
    EXPECT_EQ((std::vector<std::string>{"load"}), plan.candidates);
}

TEST(ShellCompletion, AliasOperatorsCompleteTheFinalSimpleCommand) {
    Fixture fixture;
    fixture.aliases.aliases["both"] = "echo first && map";

    const CompletionPlan plan = fixture.Build("both lo");

    ASSERT_EQ(1u, fixture.argumentRequests.size());
    EXPECT_EQ((std::vector<std::string>{"map", "lo"}), fixture.argumentRequests[0]);
    EXPECT_EQ((std::vector<std::string>{"load"}), plan.candidates);
}

TEST(ShellCompletion, AliasTrailingPipeCanIntroduceTheCompletedCommand) {
    Fixture fixture;
    fixture.aliases.aliases["pipe"] = "echo first |";

    const CompletionPlan plan = fixture.Build("pipe map l");

    ASSERT_EQ(1u, fixture.argumentRequests.size());
    EXPECT_EQ((std::vector<std::string>{"map", "l"}), fixture.argumentRequests[0]);
    EXPECT_EQ((std::vector<std::string>{"list", "load"}), plan.candidates);
}

TEST(ShellCompletion, AliasTrailingSeparatorCanIntroduceCommandCompletion) {
    Fixture fixture;
    fixture.aliases.aliases["next"] = "echo first;";

    const CompletionPlan plan = fixture.Build("next ma");

    EXPECT_EQ(CompletionPlan::Kind::Command, plan.kind);
    EXPECT_TRUE(fixture.argumentRequests.empty());
    EXPECT_EQ((std::vector<std::string>{"map"}), plan.candidates);
}

TEST(ShellCompletion, QuotedAliasDoesNotExpandForArgumentCompletion) {
    Fixture fixture;
    fixture.aliases.aliases["m"] = "map";

    const CompletionPlan plan = fixture.Build("'m' lo");

    ASSERT_EQ(1u, fixture.argumentRequests.size());
    EXPECT_EQ((std::vector<std::string>{"m", "lo"}), fixture.argumentRequests[0]);
    EXPECT_TRUE(plan.candidates.empty());
}

TEST(ShellCompletion, VariablesCompleteAfterDollar) {
    Fixture fixture;
    CompletionPlan plan = fixture.Build("echo $HO");
    EXPECT_EQ(CompletionPlan::Kind::Variable, plan.kind);
    EXPECT_EQ("HO", plan.prefix);
    EXPECT_EQ((std::vector<std::string>{"HOME", "HOST"}), plan.candidates);
    EXPECT_EQ(5u, plan.replaceBegin);
    EXPECT_EQ("echo $HOME ", Apply("echo $HO", plan, "HOME", true));

    plan = fixture.Build("echo \"v=$X");
    EXPECT_EQ(CompletionPlan::Kind::Variable, plan.kind);
    EXPECT_EQ((std::vector<std::string>{"X"}), plan.candidates);
    EXPECT_EQ("echo \"v=$X", Apply("echo \"v=$X", plan, "X", true));

    plan = fixture.Build("echo $");
    EXPECT_NE(CompletionPlan::Kind::Variable, plan.kind);
}

TEST(ShellCompletion, InsideSubstitution) {
    Fixture fixture;
    CompletionPlan plan = fixture.Build("echo $(he");
    EXPECT_EQ(CompletionPlan::Kind::Command, plan.kind);
    EXPECT_EQ((std::vector<std::string>{"help"}), plan.candidates);
    EXPECT_EQ(7u, plan.replaceBegin);
    EXPECT_EQ("echo $(help ", Apply("echo $(he", plan, "help", true));

    const std::string text = "echo $(ma tail)";
    plan = fixture.BuildAt(text, 9);
    EXPECT_TRUE(plan.followedByWhitespace);
    EXPECT_EQ("echo $(map tail)", Apply(text, plan, "map", true));
}

TEST(ShellCompletion, IncompleteQuoteInsideSubstitutionUsesInnerCommand) {
    Fixture fixture;

    const CompletionPlan plan = fixture.Build("echo $(map load \"ot");

    EXPECT_EQ(CompletionPlan::Kind::Argument, plan.kind);
    EXPECT_EQ(QuoteContext::Double, plan.context);
    ASSERT_EQ(1u, fixture.argumentRequests.size());
    EXPECT_EQ((std::vector<std::string>{"map", "load", "ot"}), fixture.argumentRequests[0]);
    EXPECT_EQ((std::vector<std::string>{"other.nmo"}), plan.candidates);
}

TEST(ShellCompletion, CaretInTheMiddleUsesTextBeforeIt) {
    Fixture fixture;
    const std::string text = "ma tail";
    const CompletionPlan plan = fixture.BuildAt(text, 2);
    EXPECT_EQ(CompletionPlan::Kind::Command, plan.kind);
    EXPECT_TRUE(plan.followedByWhitespace);
    EXPECT_EQ("map tail", Apply(text, plan, "map", true));
}

TEST(ShellCompletion, CommentsOfferNothing) {
    Fixture fixture;
    const CompletionPlan plan = fixture.Build("echo a # he");
    EXPECT_EQ(CompletionPlan::Kind::None, plan.kind);
}

TEST(ShellCompletion, CaseInsensitivePrefix) {
    Fixture fixture;
    const CompletionPlan plan = fixture.Build("HE");
    EXPECT_EQ((std::vector<std::string>{"help"}), plan.candidates);
}

TEST(ShellCompletion, DeduplicatesCandidatesInProviderOrder) {
    Fixture fixture;
    fixture.providers.commandNames = [] {
        return std::vector<std::string>{"echo", "help", "echo"};
    };
    fixture.providers.aliasNames = [] {
        return std::vector<std::string>{"help", "e", "e"};
    };

    const CompletionPlan plan = fixture.Build("");
    EXPECT_EQ((std::vector<std::string>{"echo", "help", "e"}), plan.candidates);
}

TEST(ShellCompletion, CommonPrefixEndsAtUtf8CodepointBoundary) {
    Fixture fixture;
    fixture.providers.commandNames = [] {
        return std::vector<std::string>{
            "\xe4\xbd\xa0\xe5\xa5\xbd",
            "\xe4\xbd\xa0\xe5\xae\x89",
        };
    };
    fixture.providers.aliasNames = {};

    const CompletionPlan plan = fixture.Build("");
    EXPECT_EQ(3u, plan.commonPrefixLength);
    EXPECT_EQ(plan.candidates.front().substr(0, plan.commonPrefixLength), "\xe4\xbd\xa0");
}

TEST(ShellCompletion, CommonPrefixIsUnicodeCaseInsensitive) {
    Fixture fixture;
    fixture.providers.commandNames = [] {
        return std::vector<std::string>{
            "\xc3\x84pfel",
            "\xc3\xa4PFELmus",
        };
    };
    fixture.providers.aliasNames = {};

    EXPECT_EQ(6u, fixture.Build("").commonPrefixLength);
}

TEST(ShellCompletion, InvalidUtf8HasNoCompletablePrefix) {
    Fixture fixture;
    fixture.providers.commandNames = [] {
        return std::vector<std::string>{"valid", std::string("\xe5", 1)};
    };
    fixture.providers.aliasNames = {};

    EXPECT_EQ(0u, fixture.Build("").commonPrefixLength);
}

TEST(ShellHighlighter, PartitionsTextIntoSpans) {
    auto isCommand = [](std::string_view name) { return name == "echo" || name == "grep"; };
    const std::string text = "echo 'a b' \"c $X\" $Y | nope x # done";
    const std::vector<HighlightSpan> spans = Highlight(text, isCommand);
    ASSERT_FALSE(spans.empty());
    EXPECT_EQ(0u, spans.front().begin);
    EXPECT_EQ(text.size(), spans.back().end);
    for (std::size_t i = 1; i < spans.size(); ++i)
        EXPECT_EQ(spans[i - 1].end, spans[i].begin);

    auto kindAt = [&](std::size_t pos) {
        for (const HighlightSpan &span : spans) {
            if (pos >= span.begin && pos < span.end)
                return span.kind;
        }
        return HighlightSpan::Kind::Plain;
    };
    EXPECT_EQ(HighlightSpan::Kind::CommandValid, kindAt(0));
    EXPECT_EQ(HighlightSpan::Kind::String, kindAt(5));
    EXPECT_EQ(HighlightSpan::Kind::String, kindAt(text.find("\"c")));
    EXPECT_EQ(HighlightSpan::Kind::Variable, kindAt(text.find("$X")));
    EXPECT_EQ(HighlightSpan::Kind::Variable, kindAt(text.find("$Y")));
    EXPECT_EQ(HighlightSpan::Kind::Operator, kindAt(text.find('|')));
    EXPECT_EQ(HighlightSpan::Kind::CommandInvalid, kindAt(text.find("nope")));
    EXPECT_EQ(HighlightSpan::Kind::Plain, kindAt(text.find(" x") + 1));
    EXPECT_EQ(HighlightSpan::Kind::Comment, kindAt(text.find('#')));
}

TEST(ShellHighlighter, ErrorsPaintTheTail) {
    auto isCommand = [](std::string_view) { return true; };
    const std::string text = "echo a & b";
    const std::vector<HighlightSpan> spans = Highlight(text, isCommand);
    ASSERT_FALSE(spans.empty());
    EXPECT_EQ(HighlightSpan::Kind::Error, spans.back().kind);
    EXPECT_EQ(7u, spans.back().begin);
    EXPECT_EQ(text.size(), spans.back().end);
    EXPECT_TRUE(Highlight("", isCommand).empty());
}

TEST(ShellHighlighter, IncompleteInputStillHighlights) {
    auto isCommand = [](std::string_view name) { return name == "echo"; };
    const std::vector<HighlightSpan> spans = Highlight("echo 'open", isCommand);
    ASSERT_GE(spans.size(), 2u);
    EXPECT_EQ(HighlightSpan::Kind::CommandValid, spans[0].kind);
    EXPECT_EQ(HighlightSpan::Kind::String, spans.back().kind);
    EXPECT_EQ(10u, spans.back().end);
}

TEST(ShellHighlighter, IncompleteAnsiCQuoteIsAString) {
    auto isCommand = [](std::string_view name) { return name == "map"; };
    const std::string text = "map load $'ot";
    const std::vector<HighlightSpan> spans = Highlight(text, isCommand);

    ASSERT_FALSE(spans.empty());
    EXPECT_EQ(HighlightSpan::Kind::String, spans.back().kind);
    EXPECT_EQ(text.find("$'"), spans.back().begin);
    EXPECT_EQ(text.size(), spans.back().end);
}

TEST(ShellHighlighter, IncompleteSubstitutionIsAnExpansion) {
    auto isCommand = [](std::string_view name) { return name == "echo"; };
    const std::string text = "echo $(map load \"ot";
    const std::vector<HighlightSpan> spans = Highlight(text, isCommand);

    ASSERT_FALSE(spans.empty());
    EXPECT_EQ(HighlightSpan::Kind::Variable, spans.back().kind);
    EXPECT_EQ(text.find("$("), spans.back().begin);
    EXPECT_EQ(text.size(), spans.back().end);
}

TEST(ShellHighlighter, AliasValidityRequiresOneBareLiteral) {
    MapAliases aliases;
    aliases.aliases["e"] = "echo";
    auto isCommand = [](std::string_view) { return false; };
    const std::string text = "e''; e";
    const std::vector<HighlightSpan> spans = Highlight(text, isCommand, &aliases);

    ASSERT_GE(spans.size(), 4u);
    EXPECT_EQ(HighlightSpan::Kind::CommandInvalid, spans.front().kind);
    const auto bare = std::find_if(spans.begin(), spans.end(), [&](const HighlightSpan &span) {
        return span.begin == text.rfind('e');
    });
    ASSERT_NE(spans.end(), bare);
    EXPECT_EQ(HighlightSpan::Kind::CommandValid, bare->kind);
}

TEST(ShellHighlighter, AliasOperatorExposesFollowingCommandHead) {
    MapAliases aliases;
    aliases.aliases["pipe"] = "echo first |";
    auto isCommand = [](std::string_view name) { return name == "echo" || name == "map"; };
    const std::string text = "pipe map";
    const std::vector<HighlightSpan> spans = Highlight(text, isCommand, &aliases);

    const auto map = std::find_if(spans.begin(), spans.end(), [&](const HighlightSpan &span) {
        return span.begin == text.find("map");
    });
    ASSERT_NE(spans.end(), map);
    EXPECT_EQ(HighlightSpan::Kind::CommandValid, map->kind);
}

TEST(ShellHighlighter, AliasOperatorExposesIncompleteCommandHead) {
    MapAliases aliases;
    aliases.aliases["pipe"] = "echo first |";
    auto isCommand = [](std::string_view name) { return name == "echo" || name == "map"; };
    const std::string text = "pipe ma'";
    const std::vector<HighlightSpan> spans = Highlight(text, isCommand, &aliases);

    const auto partial = std::find_if(spans.begin(), spans.end(), [&](const HighlightSpan &span) {
        return span.begin == text.find("ma");
    });
    ASSERT_NE(spans.end(), partial);
    EXPECT_EQ(HighlightSpan::Kind::CommandInvalid, partial->kind);
}
