#include <gtest/gtest.h>

#include <algorithm>
#include <map>

#include "Console/Shell/ShellEditing.h"
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

        bool HasAlias(std::string_view name) const override {
            return aliases.contains(std::string(name));
        }
    };
}

TEST(ShellEditing, ReportsSurfaceCommandAndArgumentContexts) {
    CursorAnalysis analysis = AnalyzeCursor("ma", 2);
    EXPECT_EQ(CursorTarget::Command, analysis.target);
    EXPECT_EQ("ma", analysis.prefix);
    EXPECT_EQ(0u, analysis.replaceBegin);
    EXPECT_EQ(2u, analysis.replaceEnd);

    analysis = AnalyzeCursor("map load ", 9);
    EXPECT_EQ(CursorTarget::Argument, analysis.target);
    EXPECT_EQ((std::vector<std::string>{"map", "load", ""}), analysis.arguments);
}

TEST(ShellEditing, ReportsQuoteAndVariableContexts) {
    CursorAnalysis analysis = AnalyzeCursor("map $'ot", 8);
    EXPECT_EQ(CursorTarget::Argument, analysis.target);
    EXPECT_EQ(QuoteContext::AnsiC, analysis.context);
    EXPECT_EQ("ot", analysis.prefix);

    analysis = AnalyzeCursor("echo \"$HO", 9);
    EXPECT_EQ(CursorTarget::Variable, analysis.target);
    EXPECT_EQ(QuoteContext::Double, analysis.context);
    EXPECT_EQ("HO", analysis.prefix);
    EXPECT_EQ(6u, analysis.replaceBegin);
}

TEST(ShellEditing, ResolvesAliasInjectedOperatorsAtTheCaret) {
    MapAliases aliases;
    aliases.aliases["pipe"] = "echo first |";
    aliases.aliases["next"] = "echo first;";

    CursorAnalysis analysis = AnalyzeCursor("pipe map l", 10, &aliases);
    EXPECT_EQ(CursorTarget::Argument, analysis.target);
    EXPECT_EQ((std::vector<std::string>{"map", "l"}), analysis.arguments);

    analysis = AnalyzeCursor("next ma", 7, &aliases);
    EXPECT_EQ(CursorTarget::Command, analysis.target);
    EXPECT_EQ("ma", analysis.prefix);
}

TEST(ShellEditing, NestedSubstitutionKeepsAbsoluteReplacementRange) {
    CursorAnalysis analysis = AnalyzeCursor("echo $(he", 9);
    EXPECT_EQ(CursorTarget::Command, analysis.target);
    EXPECT_EQ("he", analysis.prefix);
    EXPECT_EQ(7u, analysis.replaceBegin);
    EXPECT_EQ(9u, analysis.replaceEnd);

    analysis = AnalyzeCursor("echo $(ma tail)", 9);
    EXPECT_TRUE(analysis.followedByWhitespace);

    analysis = AnalyzeCursor("echo $(map load \"ot", 19);
    EXPECT_EQ(CursorTarget::Argument, analysis.target);
    EXPECT_EQ(QuoteContext::Double, analysis.context);
    EXPECT_EQ((std::vector<std::string>{"map", "load", "ot"}), analysis.arguments);
    EXPECT_EQ(16u, analysis.replaceBegin);
    EXPECT_EQ(19u, analysis.replaceEnd);

    analysis = AnalyzeCursor("echo $(echo $(ma", 16);
    EXPECT_EQ(CursorTarget::Command, analysis.target);
    EXPECT_EQ("ma", analysis.prefix);
    EXPECT_EQ(14u, analysis.replaceBegin);
}

TEST(ShellEditing, FindsTypedCommandHeadsAfterAliasExpansion) {
    MapAliases aliases;
    aliases.aliases["pipe"] = "echo first |";

    LineAnalysis line = AnalyzeLine("pipe map", &aliases);
    auto hasHead = [&](SourceRange source) {
        return std::find_if(line.commandHeads.begin(), line.commandHeads.end(),
                            [&](const CommandHead &head) { return head.source == source; }) !=
            line.commandHeads.end();
    };

    EXPECT_TRUE(hasHead(SourceRange{0, 4}));
    EXPECT_TRUE(hasHead(SourceRange{5, 8}));

    line = AnalyzeLine("pipe ma'", &aliases);
    EXPECT_NE(line.commandHeads.end(),
              std::find_if(line.commandHeads.begin(), line.commandHeads.end(),
                           [](const CommandHead &head) {
                               return head.source == SourceRange{5, 8} && head.literal == "ma";
                           }));
}

TEST(ShellEditing, LineAnalysisKeepsPresentationNeutral) {
    const LineAnalysis line = AnalyzeLine("echo \"$X\" | next # note");

    EXPECT_FALSE(line.errorBegin.has_value());
    EXPECT_NE(line.pieces.end(),
              std::find_if(line.pieces.begin(), line.pieces.end(), [](const SourcePiece &piece) {
                  return piece.kind == SourceKind::Expansion;
              }));
    EXPECT_NE(line.pieces.end(),
              std::find_if(line.pieces.begin(), line.pieces.end(), [](const SourcePiece &piece) {
                  return piece.kind == SourceKind::Operator;
              }));
    EXPECT_NE(line.pieces.end(),
              std::find_if(line.pieces.begin(), line.pieces.end(), [](const SourcePiece &piece) {
                  return piece.kind == SourceKind::Comment;
              }));

    const LineAnalysis error = AnalyzeLine("echo a & b");
    ASSERT_TRUE(error.errorBegin.has_value());
    EXPECT_EQ(7u, *error.errorBegin);
}
