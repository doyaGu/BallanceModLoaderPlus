#include <gtest/gtest.h>

#include <map>

#include "Console/Shell/ShellCompletion.h"
#include "Console/Shell/ShellHighlighter.h"

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

TEST(ShellCompletion, AliasExpandsForArgumentCompletion) {
    Fixture fixture;
    fixture.aliases.aliases["m"] = "map";
    const CompletionPlan plan = fixture.Build("m lo");
    EXPECT_EQ(CompletionPlan::Kind::Argument, plan.kind);
    ASSERT_EQ(1u, fixture.argumentRequests.size());
    EXPECT_EQ((std::vector<std::string>{"map", "lo"}), fixture.argumentRequests[0]);
    EXPECT_EQ((std::vector<std::string>{"load"}), plan.candidates);
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
    const CompletionPlan plan = fixture.Build("echo $(he");
    EXPECT_EQ(CompletionPlan::Kind::Command, plan.kind);
    EXPECT_EQ((std::vector<std::string>{"help"}), plan.candidates);
    EXPECT_EQ(7u, plan.replaceBegin);
    EXPECT_EQ("echo $(help ", Apply("echo $(he", plan, "help", true));
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
