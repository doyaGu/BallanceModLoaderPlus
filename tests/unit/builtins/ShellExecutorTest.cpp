#include <gtest/gtest.h>

#include <functional>
#include <map>

#include "Console/Shell/ShellExecutor.h"
#include "Console/Shell/ShellTypes.h"

using namespace BML::Shell;

namespace {
    struct Call {
        std::vector<std::string> args;
        std::string input;
        bool hadInput = false;
    };

    class FakeDispatcher final : public IDispatcher {
    public:
        std::vector<Call> calls;
        std::vector<std::string> errors;
        std::vector<std::string> logged;
        std::vector<std::string> board; // what reached the player
        std::map<std::string, int> statuses;
        std::function<void(FakeDispatcher &, const std::vector<std::string> &)> onInvoke;
        std::vector<OutputSink *> sinks;

        // Simulates a command printing through IBML::SendIngameMessage.
        void Print(std::string_view message) {
            if (!sinks.empty())
                sinks.back()->Write(message);
            else
                board.emplace_back(message);
        }

        int Invoke(const std::vector<std::string> &args, const std::string *input) override {
            Call call;
            call.args = args;
            if (input) {
                call.input = *input;
                call.hadInput = true;
            }
            calls.push_back(call);
            if (onInvoke)
                onInvoke(*this, args);
            if (args[0] == "echo") {
                std::string text;
                for (std::size_t i = 1; i < args.size(); ++i) {
                    if (i > 1)
                        text += ' ';
                    text += args[i];
                }
                Print(text);
                return Status::Ok;
            }
            if (args[0] == "cat") {
                if (input)
                    Print(*input);
                return Status::Ok;
            }
            if (args[0] == "true")
                return Status::Ok;
            if (args[0] == "false")
                return Status::Failure;
            const auto it = statuses.find(args[0]);
            if (it != statuses.end())
                return it->second;
            errors.push_back("unknown " + args[0]);
            return Status::Unknown;
        }

        void WriteError(std::string_view message) override {
            errors.emplace_back(message);
            board.emplace_back(message);
        }

        void PushSink(OutputSink *sink) override { sinks.push_back(sink); }
        void PopSink() override { sinks.pop_back(); }
        void LogExecute(std::string_view line) override { logged.emplace_back(line); }
    };

    class MapEnv final : public VariableResolver, public AliasResolver {
    public:
        std::map<std::string, std::string> variables;
        std::map<std::string, std::string> aliases;

        bool LookupVariable(std::string_view name, std::string &value) const override {
            const auto it = variables.find(std::string(name));
            if (it == variables.end())
                return false;
            value = it->second;
            return true;
        }

        bool LookupAlias(std::string_view name, std::string &body) const override {
            const auto it = aliases.find(std::string(name));
            if (it == aliases.end())
                return false;
            body = it->second;
            return true;
        }
    };

    class ShellExecutorTest : public ::testing::Test {
    protected:
        FakeDispatcher dispatcher;
        MapEnv env;
        Executor executor{dispatcher, &env, &env};
    };
}

TEST_F(ShellExecutorTest, RunsSingleCommandWithSplitWords) {
    EXPECT_EQ(Status::Ok, executor.Execute("echo hello \"big world\""));
    ASSERT_EQ(1u, dispatcher.calls.size());
    EXPECT_EQ((std::vector<std::string>{"echo", "hello", "big world"}), dispatcher.calls[0].args);
    EXPECT_FALSE(dispatcher.calls[0].hadInput);
    EXPECT_EQ((std::vector<std::string>{"hello big world"}), dispatcher.board);
    ASSERT_EQ(1u, dispatcher.logged.size());
    EXPECT_EQ("echo hello \"big world\"", dispatcher.logged[0]);
}

TEST_F(ShellExecutorTest, SemicolonRunsEverything) {
    EXPECT_EQ(Status::Ok, executor.Execute("false; echo a; echo b"));
    ASSERT_EQ(3u, dispatcher.calls.size());
    EXPECT_EQ((std::vector<std::string>{"a", "b"}), dispatcher.board);
}

TEST_F(ShellExecutorTest, AndOrShortCircuit) {
    EXPECT_EQ(Status::Failure, executor.Execute("false && echo no"));
    EXPECT_EQ(1u, dispatcher.calls.size());
    EXPECT_EQ(Status::Ok, executor.Execute("false || echo yes"));
    EXPECT_EQ(3u, dispatcher.calls.size());
    EXPECT_EQ(Status::Ok, executor.Execute("true && echo one || echo two"));
    EXPECT_EQ((std::vector<std::string>{"yes", "one"}), dispatcher.board);
    EXPECT_EQ(Status::Ok, executor.Execute("nope || echo fallback"));
    EXPECT_EQ("fallback", dispatcher.board.back());
}

TEST_F(ShellExecutorTest, StatusesPropagate) {
    dispatcher.statuses["cheat"] = Status::CheatRefused;
    EXPECT_EQ(Status::Unknown, executor.Execute("nope"));
    EXPECT_EQ(Status::Unknown, executor.LastStatus());
    EXPECT_EQ(Status::CheatRefused, executor.Execute("cheat"));
    EXPECT_EQ(Status::Ok, executor.Execute("echo $?"));
    EXPECT_EQ("126", dispatcher.board.back());
}

TEST_F(ShellExecutorTest, LastStatusVisibleThroughDollarQuestion) {
    executor.Execute("false; echo $?; echo $?");
    ASSERT_EQ(2u, dispatcher.board.size());
    EXPECT_EQ("1", dispatcher.board[0]);
    EXPECT_EQ("0", dispatcher.board[1]);
}

TEST_F(ShellExecutorTest, VariablesExpandWithoutSplitting) {
    env.variables["X"] = "a b";
    executor.Execute("echo $X \"$X\" '$X' ${X}c $UNSET \"$UNSET\" end");
    ASSERT_EQ(1u, dispatcher.calls.size());
    EXPECT_EQ((std::vector<std::string>{"echo", "a b", "a b", "$X", "a bc", "", "end"}), dispatcher.calls[0].args);
}

TEST_F(ShellExecutorTest, PipeFeedsCapturedOutputToNextStage) {
    EXPECT_EQ(Status::Ok, executor.Execute("echo one | cat"));
    ASSERT_EQ(2u, dispatcher.calls.size());
    EXPECT_TRUE(dispatcher.calls[1].hadInput);
    EXPECT_EQ("one\n", dispatcher.calls[1].input);
    EXPECT_EQ((std::vector<std::string>{"one\n"}), dispatcher.board);
}

TEST_F(ShellExecutorTest, PipelineStatusIsLastStage) {
    EXPECT_EQ(Status::Ok, executor.Execute("false | true"));
    EXPECT_EQ(Status::Failure, executor.Execute("true | false"));
}

TEST_F(ShellExecutorTest, CommandSubstitutionCapturesAndStripsNewlines) {
    executor.Execute("echo [$(echo inner)] `echo tick`");
    ASSERT_EQ(3u, dispatcher.calls.size());
    EXPECT_EQ((std::vector<std::string>{"echo", "[inner]", "tick"}), dispatcher.calls[2].args);
    EXPECT_EQ((std::vector<std::string>{"[inner] tick"}), dispatcher.board);
}

TEST_F(ShellExecutorTest, SubstitutionDoesNotDisturbLastStatus) {
    executor.Execute("false; echo $(true) $?");
    EXPECT_EQ("1", dispatcher.board.back());
}

TEST_F(ShellExecutorTest, NestedExecutionInsideCaptureInheritsSink) {
    // A command that itself prints while a capture is active writes into the pipe.
    dispatcher.onInvoke = [](FakeDispatcher &d, const std::vector<std::string> &args) {
        if (args[0] == "loud")
            d.Print("noise");
    };
    dispatcher.statuses["loud"] = Status::Ok;
    executor.Execute("loud | cat");
    EXPECT_EQ((std::vector<std::string>{"noise\n"}), dispatcher.board);
}

TEST_F(ShellExecutorTest, ErrorsBypassCapture) {
    executor.Execute("nope | cat");
    ASSERT_FALSE(dispatcher.errors.empty());
    // The unknown-command error reached the board even though a sink was active.
    EXPECT_EQ("unknown nope", dispatcher.errors[0]);
    ASSERT_EQ(2u, dispatcher.calls.size());
    EXPECT_EQ("", dispatcher.calls[1].input);
}

TEST_F(ShellExecutorTest, TruncatedPipelineStopsBeforeDownstreamCommand) {
    dispatcher.statuses["huge"] = Status::Ok;
    dispatcher.onInvoke = [](FakeDispatcher &d, const std::vector<std::string> &args) {
        if (args[0] == "huge")
            d.Print(std::string(Limits::MaxCaptureBytes, 'x'));
    };

    EXPECT_EQ(Status::Failure, executor.Execute("huge | cat"));
    ASSERT_EQ(1u, dispatcher.calls.size());
    EXPECT_EQ("huge", dispatcher.calls[0].args[0]);
    ASSERT_EQ(1u, dispatcher.errors.size());
    EXPECT_NE(std::string::npos, dispatcher.errors[0].find("capture limit"));
}

TEST_F(ShellExecutorTest, TruncatedCommandSubstitutionFailsExpansion) {
    dispatcher.statuses["huge"] = Status::Ok;
    dispatcher.onInvoke = [](FakeDispatcher &d, const std::vector<std::string> &args) {
        if (args[0] == "huge")
            d.Print(std::string(Limits::MaxCaptureBytes, 'x'));
    };

    EXPECT_EQ(Status::Failure, executor.Execute("echo $(huge)"));
    ASSERT_EQ(1u, dispatcher.calls.size());
    EXPECT_EQ("huge", dispatcher.calls[0].args[0]);
    ASSERT_EQ(1u, dispatcher.errors.size());
    EXPECT_NE(std::string::npos, dispatcher.errors[0].find("capture limit"));
}

TEST_F(ShellExecutorTest, SyntaxErrorsReportWithCaret) {
    EXPECT_EQ(Status::Syntax, executor.Execute("echo a & b"));
    ASSERT_EQ(1u, dispatcher.errors.size());
    EXPECT_NE(std::string::npos, dispatcher.errors[0].find("syntax error"));
    EXPECT_NE(std::string::npos, dispatcher.errors[0].find("\n  echo a & b\n         ^"));
    EXPECT_TRUE(dispatcher.calls.empty());
    EXPECT_TRUE(dispatcher.logged.empty());

    EXPECT_EQ(Status::Syntax, executor.Execute("echo 'open"));
    EXPECT_NE(std::string::npos, dispatcher.errors[1].find("unexpected end of input"));
}

TEST_F(ShellExecutorTest, BlankLineIsNoOpAndKeepsStatus) {
    executor.Execute("false");
    EXPECT_EQ(Status::Ok, executor.Execute("   # nothing"));
    EXPECT_EQ(Status::Failure, executor.LastStatus());
    EXPECT_TRUE(dispatcher.logged.size() == 1);
}

TEST_F(ShellExecutorTest, AliasesExpandThroughExecutor) {
    env.aliases["hi"] = "echo hello";
    executor.Execute("hi there");
    ASSERT_EQ(1u, dispatcher.calls.size());
    EXPECT_EQ((std::vector<std::string>{"echo", "hello", "there"}), dispatcher.calls[0].args);
}

TEST_F(ShellExecutorTest, EmptyExpansionOfBareVariableVanishes) {
    executor.Execute("$UNSET");
    EXPECT_TRUE(dispatcher.calls.empty());
    executor.Execute("$UNSET echo x");
    ASSERT_EQ(1u, dispatcher.calls.size());
    EXPECT_EQ("echo", dispatcher.calls[0].args[0]);
    executor.Execute("\"\"");
    ASSERT_EQ(2u, dispatcher.calls.size());
    EXPECT_EQ("", dispatcher.calls[1].args[0]);
}

TEST_F(ShellExecutorTest, SubstitutionDepthIsBounded) {
    std::string nested = "echo x";
    for (std::size_t i = 0; i <= Limits::MaxSubstitutionDepth; ++i)
        nested = "echo $(" + nested + ")";
    // The innermost substitution fails and reports; like other shells the outer
    // command still runs with the empty result.
    executor.Execute(nested);
    ASSERT_FALSE(dispatcher.errors.empty());
    EXPECT_NE(std::string::npos, dispatcher.errors.back().find("nested too deeply"));
    EXPECT_EQ(Limits::MaxSubstitutionDepth, dispatcher.calls.size());
}

TEST(ShellCaptureSink, AddsNewlineWhenMissing) {
    CaptureSink sink;
    sink.Write("a");
    sink.Write("b\n");
    sink.Write("");
    EXPECT_EQ("a\nb\n\n", sink.Text());
    EXPECT_FALSE(sink.Truncated());
}
