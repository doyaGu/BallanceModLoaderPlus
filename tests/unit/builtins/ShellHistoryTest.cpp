#include <gtest/gtest.h>

#include "Console/Shell/ShellHistory.h"
#include "Console/Shell/ShellTypes.h"

using namespace BML::Shell;

namespace {
    History MakeHistory(std::initializer_list<const char *> entries) {
        History history;
        for (const char *entry : entries)
            history.Add(entry);
        return history;
    }

    std::string ExpandOk(const History &history, const char *line, bool *changedOut = nullptr) {
        std::string out;
        std::string error;
        bool changed = false;
        EXPECT_TRUE(history.Expand(line, out, changed, error)) << error;
        if (changedOut)
            *changedOut = changed;
        return out;
    }
}

TEST(ShellHistory, AddMovesDuplicatesToNewest) {
    History history = MakeHistory({"a", "b", "a"});
    EXPECT_EQ((std::vector<std::string>{"b", "a"}), history.Entries());
    history.Add("");
    EXPECT_EQ(2u, history.Size());
    EXPECT_TRUE(history.Erase(1));
    EXPECT_EQ((std::vector<std::string>{"a"}), history.Entries());
    EXPECT_FALSE(history.Erase(5));
    history.Clear();
    EXPECT_TRUE(history.Empty());
}

TEST(ShellHistory, RevisionChangesOnlyWhenEntriesChange) {
    History history;
    const std::uint64_t emptyRevision = history.Revision();
    history.Add("");
    EXPECT_EQ(emptyRevision, history.Revision());

    history.Add("echo one");
    const std::uint64_t addedRevision = history.Revision();
    EXPECT_GT(addedRevision, emptyRevision);
    history.Add("echo one");
    EXPECT_EQ(addedRevision, history.Revision());

    history.Add("echo two");
    EXPECT_GT(history.Revision(), addedRevision);
    const std::uint64_t beforeErase = history.Revision();
    EXPECT_FALSE(history.Erase(99));
    EXPECT_EQ(beforeErase, history.Revision());
    EXPECT_TRUE(history.Erase(1));
    EXPECT_GT(history.Revision(), beforeErase);
}

TEST(ShellHistory, RejectsEntriesThatCannotBeExecuted) {
    History history;
    history.Add(std::string(Limits::MaxLineBytes + 1, 'x'));
    EXPECT_TRUE(history.Empty());
}

TEST(ShellHistory, TextRoundTrip) {
    History history = MakeHistory({"echo one", "echo \"two\nlines\"", "help"});
    const std::string encoded = history.ToText();
    EXPECT_EQ("BMLHIST2\n8:echo one\n16:echo \"two\nlines\"\n4:help\n", encoded);

    History loaded;
    loaded.FromText(encoded);
    EXPECT_EQ(history.Entries(), loaded.Entries());
}

TEST(ShellHistory, ReadsLegacyLineBasedFiles) {
    History loaded;
    loaded.FromText("x\r\ny\n\ny\n");
    EXPECT_EQ((std::vector<std::string>{"x", "y"}), loaded.Entries());
}

TEST(ShellHistory, RejectsMalformedLengthPrefixedFileWithoutPartialHistory) {
    History loaded = MakeHistory({"old"});
    loaded.FromText("BMLHIST2\n20:short\n");
    EXPECT_TRUE(loaded.Empty());
}

TEST(ShellHistory, ExpandsEventDesignators) {
    const History history = MakeHistory({"echo first", "help", "echo third"});
    bool changed = false;
    EXPECT_EQ("echo third", ExpandOk(history, "!!", &changed));
    EXPECT_TRUE(changed);
    EXPECT_EQ("help", ExpandOk(history, "!2"));
    EXPECT_EQ("help", ExpandOk(history, "!-2"));
    EXPECT_EQ("echo third", ExpandOk(history, "!echo"));
    EXPECT_EQ("help && echo third", ExpandOk(history, "!h && !!"));
    EXPECT_EQ("echo third | grep x", ExpandOk(history, "!! | grep x"));
}

TEST(ShellHistory, LeavesLiteralBangsAlone) {
    const History history = MakeHistory({"help"});
    bool changed = true;
    EXPECT_EQ("echo hi!", ExpandOk(history, "echo hi!", &changed));
    EXPECT_FALSE(changed);
    EXPECT_EQ("echo ! x", ExpandOk(history, "echo ! x"));
    EXPECT_EQ("echo a!=b", ExpandOk(history, "echo a!=b"));
    EXPECT_EQ("echo '!!'", ExpandOk(history, "echo '!!'"));
    EXPECT_EQ("echo \\!!", ExpandOk(history, "echo \\!!"));
    EXPECT_EQ("echo !-", ExpandOk(history, "echo !-"));
    EXPECT_EQ("echo \"help\"", ExpandOk(history, "echo \"!h\""));
}

TEST(ShellHistory, MissingEventIsAnError) {
    const History history = MakeHistory({"help"});
    std::string out;
    std::string error;
    bool changed = false;
    EXPECT_FALSE(history.Expand("!zzz", out, changed, error));
    EXPECT_EQ("!zzz: event not found", error);
    EXPECT_FALSE(history.Expand("!9", out, changed, error));
    EXPECT_EQ("!9: event not found", error);
    const History empty;
    EXPECT_FALSE(empty.Expand("!!", out, changed, error));
}

TEST(ShellHistory, RejectsExpansionBeyondCommandLineLimit) {
    History history;
    history.Add(std::string(Limits::MaxLineBytes / 2 + 1, 'x'));

    std::string out;
    std::string error;
    bool changed = false;
    EXPECT_FALSE(history.Expand("!!!!", out, changed, error));
    EXPECT_TRUE(out.empty());
    EXPECT_FALSE(changed);
    EXPECT_NE(std::string::npos, error.find("command line limit"));
}

TEST(ShellHistory, AllowsExpansionAtCommandLineLimit) {
    History history;
    const std::size_t entrySize = (Limits::MaxLineBytes - 1) / 2;
    history.Add(std::string(entrySize, 'x'));

    std::string out;
    std::string error;
    bool changed = false;
    ASSERT_TRUE(history.Expand("!! !!", out, changed, error)) << error;
    EXPECT_EQ(Limits::MaxLineBytes, out.size());
    EXPECT_TRUE(changed);
}

TEST(ShellHistory, SearchAndSuggest) {
    const History history = MakeHistory({"echo alpha", "help", "echo beta"});
    EXPECT_EQ(2, history.SearchBackward("echo", 3));
    EXPECT_EQ(0, history.SearchBackward("echo", 2));
    EXPECT_EQ(-1, history.SearchBackward("echo", 0));
    EXPECT_EQ(-1, history.SearchBackward("zzz", 3));

    ASSERT_NE(nullptr, history.Suggest("ec"));
    EXPECT_EQ("echo beta", *history.Suggest("ec"));
    EXPECT_EQ(nullptr, history.Suggest("echo beta"));
    EXPECT_EQ(nullptr, history.Suggest(""));
    EXPECT_EQ(nullptr, history.Suggest("zzz"));
}

TEST(ShellHistoryNavigator, WalksAndRestoresDraft) {
    const History history = MakeHistory({"a1", "b1", "a2"});
    HistoryNavigator navigator;
    std::string text;
    EXPECT_TRUE(navigator.Up(history, "draft", text));
    EXPECT_EQ("a2", text);
    EXPECT_TRUE(navigator.Up(history, text, text));
    EXPECT_EQ("b1", text);
    EXPECT_TRUE(navigator.Up(history, text, text));
    EXPECT_EQ("a1", text);
    EXPECT_FALSE(navigator.Up(history, text, text));
    EXPECT_TRUE(navigator.Down(history, text, text));
    EXPECT_EQ("b1", text);
    EXPECT_TRUE(navigator.Down(history, text, text));
    EXPECT_TRUE(navigator.Down(history, text, text));
    EXPECT_EQ("draft", text);
    EXPECT_FALSE(navigator.Browsing());
    EXPECT_FALSE(navigator.Down(history, text, text));
}

TEST(ShellHistoryNavigator, PrefixFiltersWithNonEmptyDraft) {
    const History history = MakeHistory({"a1", "b1", "a2"});
    HistoryNavigator navigator;
    std::string text;
    EXPECT_TRUE(navigator.Up(history, "a", text));
    EXPECT_EQ("a2", text);
    EXPECT_TRUE(navigator.Up(history, text, text));
    EXPECT_EQ("a1", text);
    EXPECT_FALSE(navigator.Up(history, text, text));
    EXPECT_TRUE(navigator.Down(history, text, text));
    EXPECT_EQ("a2", text);
    EXPECT_TRUE(navigator.Down(history, text, text));
    EXPECT_EQ("a", text);
    navigator.Reset();
    EXPECT_FALSE(navigator.Browsing());
}

TEST(ShellHistoryNavigator, TreatsMultilineCommandAsOneEntry) {
    const History history = MakeHistory({"echo first", "echo one &&\necho two"});
    HistoryNavigator navigator;
    std::string text;

    EXPECT_TRUE(navigator.Up(history, "echo o", text));
    EXPECT_EQ("echo one &&\necho two", text);
    EXPECT_TRUE(navigator.Down(history, text, text));
    EXPECT_EQ("echo o", text);
}
