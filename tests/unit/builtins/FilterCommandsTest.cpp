#include <gtest/gtest.h>

#include <oniguruma.h>

#include "Console/Shell/FilterCommands.h"
#include "Console/Shell/ShellBuiltins.h"
#include "Logging/Logger.h"

// Stub Logger: CommandContext.cpp is linked for its name validation only.
Logger *Logger::m_DefaultLogger = nullptr;

Logger *Logger::GetDefault() {
    static Logger defaultLogger("Test");
    return &defaultLogger;
}

void Logger::SetDefault(Logger *logger) {
    m_DefaultLogger = logger;
}

Logger::Logger(const char *modName) : m_ModName(modName) {}

void Logger::Info(const char *fmt, ...) {}
void Logger::Warn(const char *fmt, ...) {}
void Logger::Error(const char *fmt, ...) {}
void Logger::Log(const char *, const char *, va_list) {}

using namespace BML::Shell;
using namespace BML::Shell::Filters;

namespace {
    class OnigEnvironment : public ::testing::Environment {
    public:
        void SetUp() override {
            OnigEncoding encodings[] = {ONIG_ENCODING_UTF8};
            onig_initialize(encodings, 1);
        }

        void TearDown() override { onig_end(); }
    };

    const ::testing::Environment *const g_Onig = ::testing::AddGlobalTestEnvironment(new OnigEnvironment());

    using Lines = std::vector<std::string>;
}

TEST(FilterSplitLines, HandlesTerminatorsAndCarriageReturns) {
    EXPECT_EQ((Lines{"a", "b"}), SplitLines("a\nb\n"));
    EXPECT_EQ((Lines{"a", "b"}), SplitLines("a\r\nb"));
    EXPECT_EQ((Lines{"a", "", "b"}), SplitLines("a\n\nb"));
    EXPECT_TRUE(SplitLines("").empty());
}

TEST(FilterGrep, MatchesRegexAndFlags) {
    const Lines lines{"apple", "Banana", "cherry pie", "\x1b[31mred apple\x1b[0m"};
    Lines out;
    std::string error;
    EXPECT_TRUE(Grep(lines, "app", GrepOptions{}, out, error));
    EXPECT_EQ((Lines{"apple", "\x1b[31mred apple\x1b[0m"}), out);

    GrepOptions ignoreCase;
    ignoreCase.ignoreCase = true;
    EXPECT_TRUE(Grep(lines, "^b", ignoreCase, out, error));
    EXPECT_EQ((Lines{"Banana"}), out);

    GrepOptions invertNumbered;
    invertNumbered.invert = true;
    invertNumbered.lineNumbers = true;
    EXPECT_TRUE(Grep(lines, "apple", invertNumbered, out, error));
    EXPECT_EQ((Lines{"2:Banana", "3:cherry pie"}), out);

    GrepOptions count;
    count.countOnly = true;
    EXPECT_TRUE(Grep(lines, "a", count, out, error));
    EXPECT_EQ((Lines{"3"}), out);

    EXPECT_FALSE(Grep(lines, "zzz", GrepOptions{}, out, error));
    EXPECT_TRUE(out.empty());
    EXPECT_TRUE(error.empty());
}

TEST(FilterGrep, FixedStringsAndBadPatterns) {
    const Lines lines{"a.b", "axb"};
    Lines out;
    std::string error;
    GrepOptions fixed;
    fixed.fixed = true;
    EXPECT_TRUE(Grep(lines, "a.b", fixed, out, error));
    EXPECT_EQ((Lines{"a.b"}), out);
    EXPECT_TRUE(Grep(lines, "a.b", GrepOptions{}, out, error));
    EXPECT_EQ(2u, out.size());

    EXPECT_FALSE(Grep(lines, "(unclosed", GrepOptions{}, out, error));
    EXPECT_FALSE(error.empty());
}

TEST(FilterHeadTail, ClampToAvailableLines) {
    const Lines lines{"1", "2", "3"};
    EXPECT_EQ((Lines{"1", "2"}), Head(lines, 2));
    EXPECT_EQ(lines, Head(lines, 10));
    EXPECT_EQ((Lines{"2", "3"}), Tail(lines, 2));
    EXPECT_EQ(lines, Tail(lines, 10));
    EXPECT_TRUE(Head(lines, 0).empty());
}

TEST(FilterWc, CountsLinesWordsBytes) {
    EXPECT_EQ("2 4 19", Wc("one two\nthree\tfour\n", WcOptions{}));
    WcOptions lines;
    lines.lines = true;
    EXPECT_EQ("2", Wc("a\nb\n", lines));
    WcOptions wordsBytes;
    wordsBytes.words = true;
    wordsBytes.bytes = true;
    EXPECT_EQ("1 3", Wc("abc", wordsBytes));
    EXPECT_EQ("0 0 0", Wc("", WcOptions{}));
}

TEST(FilterSort, TextNumericReverseUnique) {
    EXPECT_EQ((Lines{"apple", "Banana", "cherry"}), Sort(Lines{"cherry", "Banana", "apple"}, SortOptions{}));

    SortOptions numeric;
    numeric.numeric = true;
    EXPECT_EQ((Lines{"x", "2 b", "10 a"}), Sort(Lines{"10 a", "2 b", "x"}, numeric));

    SortOptions reverseUnique;
    reverseUnique.reverse = true;
    reverseUnique.unique = true;
    EXPECT_EQ((Lines{"b", "a"}), Sort(Lines{"a", "b", "a"}, reverseUnique));
}

TEST(FilterUniq, CollapsesAdjacentRuns) {
    EXPECT_EQ((Lines{"a", "b", "a"}), Uniq(Lines{"a", "a", "b", "a"}, false));
    EXPECT_EQ((Lines{"2 a", "1 b", "1 a"}), Uniq(Lines{"a", "a", "b", "a"}, true));
    EXPECT_TRUE(Uniq(Lines{}, false).empty());
}

TEST(ShellXargs, SplitsItems) {
    EXPECT_EQ((Lines{"a", "b", "c"}), CommandXargs::SplitItems(" a\tb\n\nc\n", nullptr));
    const std::string comma = ",";
    EXPECT_EQ((Lines{"a", "", "b c"}), CommandXargs::SplitItems("a,,b c\n", &comma));
    EXPECT_TRUE(CommandXargs::SplitItems("", nullptr).empty());
}
