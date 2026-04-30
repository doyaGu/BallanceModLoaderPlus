#include <gtest/gtest.h>

#include "CommandContext.h"
#include "Logger.h"

// Stub Logger for test builds
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

// Simple test command
class TestCommand : public ICommand {
public:
    explicit TestCommand(const char *name, const char *alias = "")
        : m_Name(name), m_Alias(alias) {}

    std::string GetName() override { return m_Name; }
    std::string GetAlias() override { return m_Alias; }
    std::string GetDescription() override { return "Test command"; }
    bool IsCheat() override { return false; }
    void Execute(IBML *bml, const std::vector<std::string> &args) override {
        m_LastArgs = args;
        m_ExecuteCount++;
    }
    const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &) override {
        return {};
    }

    std::vector<std::string> m_LastArgs;
    int m_ExecuteCount = 0;

private:
    std::string m_Name;
    std::string m_Alias;
};

class CommandContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = new BML::CommandContext();
    }

    void TearDown() override {
        delete ctx;
        for (auto *cmd : ownedCommands) {
            delete cmd;
        }
    }

    TestCommand *MakeCommand(const char *name, const char *alias = "") {
        auto *cmd = new TestCommand(name, alias);
        ownedCommands.push_back(cmd);
        return cmd;
    }

    BML::CommandContext *ctx = nullptr;
    std::vector<TestCommand *> ownedCommands;
};

// Registration
TEST_F(CommandContextTest, RegisterCommand) {
    auto *cmd = MakeCommand("test");
    EXPECT_TRUE(ctx->RegisterCommand(cmd));
    EXPECT_EQ(1u, ctx->GetCommandCount());
}

TEST_F(CommandContextTest, RegisterNullCommand) {
    EXPECT_FALSE(ctx->RegisterCommand(nullptr));
    EXPECT_EQ(0u, ctx->GetCommandCount());
}

TEST_F(CommandContextTest, RegisterDuplicateName) {
    auto *cmd1 = MakeCommand("test");
    auto *cmd2 = MakeCommand("test");
    EXPECT_TRUE(ctx->RegisterCommand(cmd1));
    EXPECT_FALSE(ctx->RegisterCommand(cmd2));
    EXPECT_EQ(1u, ctx->GetCommandCount());
}

TEST_F(CommandContextTest, RegisterWithAlias) {
    auto *cmd = MakeCommand("teleport", "tp");
    EXPECT_TRUE(ctx->RegisterCommand(cmd));

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("teleport"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("tp"));
}

TEST_F(CommandContextTest, RegisterWithSymbolAlias) {
    auto *cmd = MakeCommand("help", "?");
    auto *punctuationCmd = MakeCommand("repeat", "!");
    EXPECT_TRUE(ctx->RegisterCommand(cmd));
    EXPECT_TRUE(ctx->RegisterCommand(punctuationCmd));

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("help"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("?"));
    EXPECT_EQ(static_cast<ICommand *>(punctuationCmd), ctx->GetCommandByName("repeat"));
    EXPECT_EQ(static_cast<ICommand *>(punctuationCmd), ctx->GetCommandByName("!"));
}

TEST_F(CommandContextTest, RegisterRejectsCaseInsensitiveDuplicateName) {
    auto *cmd1 = MakeCommand("Teleport");
    auto *cmd2 = MakeCommand("teleport");

    EXPECT_TRUE(ctx->RegisterCommand(cmd1));
    EXPECT_FALSE(ctx->RegisterCommand(cmd2));
}

// Name validation
TEST_F(CommandContextTest, InvalidCommandNames) {
    // Starts with digit
    auto *cmd1 = MakeCommand("1test");
    EXPECT_FALSE(ctx->RegisterCommand(cmd1));

    // Starts with special char
    auto *cmd2 = MakeCommand("!test");
    EXPECT_FALSE(ctx->RegisterCommand(cmd2));

    // Empty name
    auto *cmd3 = MakeCommand("");
    EXPECT_FALSE(ctx->RegisterCommand(cmd3));
}

TEST_F(CommandContextTest, ValidCommandNames) {
    auto *cmd = MakeCommand("MyCommand123");
    EXPECT_TRUE(ctx->RegisterCommand(cmd));

    auto *underscored = MakeCommand("_debug");
    EXPECT_TRUE(ctx->RegisterCommand(underscored));

    auto *dashed = MakeCommand("mod-command");
    EXPECT_TRUE(ctx->RegisterCommand(dashed));

    auto *dotted = MakeCommand("mod.command");
    EXPECT_TRUE(ctx->RegisterCommand(dotted));

    auto *utf8Named = MakeCommand("\xE6\xB5\x8B\xE8\xAF\x95");
    EXPECT_TRUE(ctx->RegisterCommand(utf8Named));
}

// Lookup
TEST_F(CommandContextTest, GetCommandByName) {
    auto *cmd = MakeCommand("hello");
    ctx->RegisterCommand(cmd);

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("hello"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("nonexistent"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName(nullptr));
    EXPECT_EQ(nullptr, ctx->GetCommandByName(""));
}

TEST_F(CommandContextTest, GetCommandByNameIsCaseInsensitive) {
    auto *cmd = MakeCommand("Teleport", "Tp");
    ASSERT_TRUE(ctx->RegisterCommand(cmd));

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("teleport"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("TELEPORT"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("tp"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("TP"));
}

TEST_F(CommandContextTest, GetCommandByNameIsCaseInsensitiveForUtf8) {
    auto *cmd = MakeCommand("\xC3\x84pfel");
    ASSERT_TRUE(ctx->RegisterCommand(cmd));

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("\xC3\xA4PFEL"));
}

TEST_F(CommandContextTest, GetCommandByIndex) {
    auto *cmd1 = MakeCommand("aaa");
    auto *cmd2 = MakeCommand("bbb");
    ctx->RegisterCommand(cmd1);
    ctx->RegisterCommand(cmd2);

    EXPECT_EQ(static_cast<ICommand *>(cmd1), ctx->GetCommandByIndex(0));
    EXPECT_EQ(static_cast<ICommand *>(cmd2), ctx->GetCommandByIndex(1));
    EXPECT_EQ(nullptr, ctx->GetCommandByIndex(99));
}

// Unregistration
TEST_F(CommandContextTest, UnregisterCommand) {
    auto *cmd = MakeCommand("test");
    ctx->RegisterCommand(cmd);

    EXPECT_TRUE(ctx->UnregisterCommand("test"));
    EXPECT_EQ(0u, ctx->GetCommandCount());
    EXPECT_EQ(nullptr, ctx->GetCommandByName("test"));
}

TEST_F(CommandContextTest, UnregisterRemovesAlias) {
    auto *cmd = MakeCommand("teleport", "tp");
    ctx->RegisterCommand(cmd);

    ctx->UnregisterCommand("teleport");
    EXPECT_EQ(nullptr, ctx->GetCommandByName("tp"));
}

TEST_F(CommandContextTest, UnregisterIsCaseInsensitive) {
    auto *cmd = MakeCommand("Teleport", "Tp");
    ASSERT_TRUE(ctx->RegisterCommand(cmd));

    EXPECT_TRUE(ctx->UnregisterCommand("teleport"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("Teleport"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("tp"));
}

TEST_F(CommandContextTest, UnregisterNonexistent) {
    EXPECT_FALSE(ctx->UnregisterCommand("nonexistent"));
    EXPECT_FALSE(ctx->UnregisterCommand(nullptr));
    EXPECT_FALSE(ctx->UnregisterCommand(""));
}

// Sort
TEST_F(CommandContextTest, SortCommands) {
    auto *cmd1 = MakeCommand("zzz");
    auto *cmd2 = MakeCommand("aaa");
    auto *cmd3 = MakeCommand("mmm");
    ctx->RegisterCommand(cmd1);
    ctx->RegisterCommand(cmd2);
    ctx->RegisterCommand(cmd3);

    ctx->SortCommands();

    EXPECT_EQ(static_cast<ICommand *>(cmd2), ctx->GetCommandByIndex(0)); // aaa
    EXPECT_EQ(static_cast<ICommand *>(cmd3), ctx->GetCommandByIndex(1)); // mmm
    EXPECT_EQ(static_cast<ICommand *>(cmd1), ctx->GetCommandByIndex(2)); // zzz
}

// Clear
TEST_F(CommandContextTest, ClearCommands) {
    ctx->RegisterCommand(MakeCommand("aaa"));
    ctx->RegisterCommand(MakeCommand("bbb"));

    ctx->ClearCommands();
    EXPECT_EQ(0u, ctx->GetCommandCount());
    EXPECT_EQ(nullptr, ctx->GetCommandByName("aaa"));
}

// Variables
TEST_F(CommandContextTest, VariableLifecycle) {
    EXPECT_TRUE(ctx->AddVariable("key", "value"));
    EXPECT_STREQ("value", ctx->GetVariable("key"));

    // Duplicate insert fails
    EXPECT_FALSE(ctx->AddVariable("key", "other"));
    EXPECT_STREQ("value", ctx->GetVariable("key"));

    EXPECT_TRUE(ctx->RemoveVariable("key"));
    EXPECT_EQ(nullptr, ctx->GetVariable("key"));
}

TEST_F(CommandContextTest, VariableNullHandling) {
    EXPECT_FALSE(ctx->AddVariable(nullptr, "val"));
    EXPECT_FALSE(ctx->AddVariable("", "val"));
    EXPECT_EQ(nullptr, ctx->GetVariable(nullptr));
    EXPECT_EQ(nullptr, ctx->GetVariable(""));
    EXPECT_FALSE(ctx->RemoveVariable(nullptr));
}

TEST_F(CommandContextTest, VariableNullValue) {
    // Null value should be stored as empty string
    EXPECT_TRUE(ctx->AddVariable("key", nullptr));
    EXPECT_STREQ("", ctx->GetVariable("key"));
}

// Output callback
TEST_F(CommandContextTest, OutputCallback) {
    std::string captured;
    auto cb = [](const char *msg, void *ud) {
        *static_cast<std::string *>(ud) = msg;
    };

    EXPECT_TRUE(ctx->SetOutputCallback(cb, &captured));
    ctx->Output("hello world");
    EXPECT_EQ("hello world", captured);

    // Second set should fail (already set)
    EXPECT_FALSE(ctx->SetOutputCallback(cb, &captured));

    ctx->ClearOutputCallback();

    // After clear, can set again
    EXPECT_TRUE(ctx->SetOutputCallback(cb, &captured));
}

TEST_F(CommandContextTest, OutputCallbackNull) {
    EXPECT_FALSE(ctx->SetOutputCallback(nullptr, nullptr));
}

TEST_F(CommandContextTest, OutputFFormatting) {
    std::string captured;
    auto cb = [](const char *msg, void *ud) {
        *static_cast<std::string *>(ud) = msg;
    };
    ctx->SetOutputCallback(cb, &captured);

    ctx->OutputF("Hello %s, count=%d", "world", 42);
    EXPECT_EQ("Hello world, count=42", captured);
}

// ParseCommandLine
TEST_F(CommandContextTest, ParseCommandLineBasic) {
    auto args = BML::CommandContext::ParseCommandLine("hello world");
    ASSERT_EQ(2u, args.size());
    EXPECT_EQ("hello", args[0]);
    EXPECT_EQ("world", args[1]);
}

TEST_F(CommandContextTest, ParseCommandLineEmpty) {
    auto args = BML::CommandContext::ParseCommandLine("");
    EXPECT_TRUE(args.empty());

    args = BML::CommandContext::ParseCommandLine(nullptr);
    EXPECT_TRUE(args.empty());
}

TEST_F(CommandContextTest, ParseCommandLineMultipleSpaces) {
    auto args = BML::CommandContext::ParseCommandLine("  hello   world  ");
    ASSERT_EQ(2u, args.size());
    EXPECT_EQ("hello", args[0]);
    EXPECT_EQ("world", args[1]);
}

TEST_F(CommandContextTest, ParseCommandLineSingleArg) {
    auto args = BML::CommandContext::ParseCommandLine("test");
    ASSERT_EQ(1u, args.size());
    EXPECT_EQ("test", args[0]);
}
