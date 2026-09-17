#include <gtest/gtest.h>

#include "Console/CommandContext.h"
#include "Logging/Logger.h"

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
    bool IsCheat() override { return m_Cheat; }
    void SetCheat(bool cheat) { m_Cheat = cheat; }
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
    bool m_Cheat = false;
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

    bool Register(ICommand *command, const void *registrar = nullptr) {
        return ctx->RegisterCommand(registrar ? registrar : this, command);
    }

    BML::CommandContext::UnregisterResult Unregister(const char *name,
                                                     const void *registrar = nullptr) {
        return ctx->UnregisterCommand(registrar ? registrar : this, name);
    }

    BML::CommandContext *ctx = nullptr;
    std::vector<TestCommand *> ownedCommands;
};

// Registration
TEST_F(CommandContextTest, RegisterCommand) {
    auto *cmd = MakeCommand("test");
    EXPECT_TRUE(Register(cmd));
    EXPECT_EQ(1u, ctx->GetCommandCount());
}

TEST_F(CommandContextTest, CommandSnapshotCopiesSortedMetadata) {
    auto *second = MakeCommand("zeta", "z");
    auto *first = MakeCommand("alpha", "a");
    ASSERT_TRUE(Register(second));
    ASSERT_TRUE(Register(first));

    const auto snapshot = ctx->GetCommandSnapshot();
    ASSERT_EQ(2u, snapshot.size());
    EXPECT_EQ("alpha", snapshot[0].Name);
    EXPECT_EQ("a", snapshot[0].Alias);
    EXPECT_EQ("Test command", snapshot[0].Description);
    EXPECT_FALSE(snapshot[0].Cheat);
    EXPECT_EQ("zeta", snapshot[1].Name);
    EXPECT_EQ("z", snapshot[1].Alias);
}

TEST_F(CommandContextTest, CommandInfoLookupDoesNotExposeRegistryEntry) {
    auto *command = MakeCommand("teleport", "tp");
    ASSERT_TRUE(Register(command));

    BML::CommandContext::CommandInfo info;
    ASSERT_TRUE(ctx->GetCommandInfoByName("TP", info));
    EXPECT_EQ("teleport", info.Name);
    EXPECT_EQ("tp", info.Alias);
    ASSERT_TRUE(ctx->GetCommandInfoByIndex(0, info));
    EXPECT_EQ("teleport", info.Name);
    EXPECT_FALSE(ctx->GetCommandInfoByIndex(1, info));
}

TEST_F(CommandContextTest, InvocationUsesRegisteredMetadataSnapshot) {
    auto *command = MakeCommand("teleport", "tp");
    command->SetCheat(true);
    ASSERT_TRUE(Register(command));
    command->SetCheat(false);

    ICommand *resolved = nullptr;
    BML::CommandContext::CommandInfo info;
    ASSERT_TRUE(ctx->GetCommandInvocation("TP", resolved, info));
    EXPECT_EQ(command, resolved);
    EXPECT_TRUE(info.Cheat);
}

TEST_F(CommandContextTest, RegisterNullCommand) {
    EXPECT_FALSE(Register(nullptr));
    EXPECT_EQ(0u, ctx->GetCommandCount());
}

TEST_F(CommandContextTest, RegisterRequiresRegistrar) {
    EXPECT_FALSE(ctx->RegisterCommand(nullptr, MakeCommand("test")));
    EXPECT_EQ(0u, ctx->GetCommandCount());
}

TEST_F(CommandContextTest, RegisterDuplicateName) {
    auto *cmd1 = MakeCommand("test");
    auto *cmd2 = MakeCommand("test");
    EXPECT_TRUE(Register(cmd1));
    EXPECT_FALSE(Register(cmd2));
    EXPECT_EQ(1u, ctx->GetCommandCount());
}

TEST_F(CommandContextTest, RegisterWithAlias) {
    auto *cmd = MakeCommand("teleport", "tp");
    EXPECT_TRUE(Register(cmd));

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("teleport"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("tp"));
}

TEST_F(CommandContextTest, RegisterWithSymbolAlias) {
    auto *cmd = MakeCommand("help", "?");
    auto *punctuationCmd = MakeCommand("repeat", "!");
    EXPECT_TRUE(Register(cmd));
    EXPECT_TRUE(Register(punctuationCmd));

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("help"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("?"));
    EXPECT_EQ(static_cast<ICommand *>(punctuationCmd), ctx->GetCommandByName("repeat"));
    EXPECT_EQ(static_cast<ICommand *>(punctuationCmd), ctx->GetCommandByName("!"));
}

TEST_F(CommandContextTest, RegisterRejectsInvalidAliasWithoutAddingCommand) {
    auto *cmd = MakeCommand("teleport", "bad alias");

    EXPECT_FALSE(Register(cmd));
    EXPECT_EQ(0u, ctx->GetCommandCount());
    EXPECT_EQ(nullptr, ctx->GetCommandByName("teleport"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("bad alias"));
}

TEST_F(CommandContextTest, RegisterRejectsCaseInsensitiveDuplicateName) {
    auto *cmd1 = MakeCommand("Teleport");
    auto *cmd2 = MakeCommand("teleport");

    EXPECT_TRUE(Register(cmd1));
    EXPECT_FALSE(Register(cmd2));
}

TEST_F(CommandContextTest, AliasConflictKeepsCommandRegisteredByName) {
    auto *first = MakeCommand("teleport", "tp");
    auto *second = MakeCommand("teleport-home", "tp");

    ASSERT_TRUE(Register(first));
    ASSERT_TRUE(Register(second));

    EXPECT_EQ(static_cast<ICommand *>(first), ctx->GetCommandByName("tp"));
    EXPECT_EQ(static_cast<ICommand *>(second), ctx->GetCommandByName("teleport-home"));
}

// Name validation
TEST_F(CommandContextTest, InvalidCommandNames) {
    // Starts with digit
    auto *cmd1 = MakeCommand("1test");
    EXPECT_FALSE(Register(cmd1));

    // Starts with special char
    auto *cmd2 = MakeCommand("!test");
    EXPECT_FALSE(Register(cmd2));

    // Empty name
    auto *cmd3 = MakeCommand("");
    EXPECT_FALSE(Register(cmd3));
}

TEST_F(CommandContextTest, ValidCommandNames) {
    auto *cmd = MakeCommand("MyCommand123");
    EXPECT_TRUE(Register(cmd));

    auto *underscored = MakeCommand("_debug");
    EXPECT_TRUE(Register(underscored));

    auto *dashed = MakeCommand("mod-command");
    EXPECT_TRUE(Register(dashed));

    auto *dotted = MakeCommand("mod.command");
    EXPECT_TRUE(Register(dotted));

    auto *utf8Named = MakeCommand("\xE6\xB5\x8B\xE8\xAF\x95");
    EXPECT_TRUE(Register(utf8Named));
}

// Lookup
TEST_F(CommandContextTest, GetCommandByName) {
    auto *cmd = MakeCommand("hello");
    Register(cmd);

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("hello"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("nonexistent"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName(nullptr));
    EXPECT_EQ(nullptr, ctx->GetCommandByName(""));
}

TEST_F(CommandContextTest, GetCommandByNameIsCaseInsensitive) {
    auto *cmd = MakeCommand("Teleport", "Tp");
    ASSERT_TRUE(Register(cmd));

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("teleport"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("TELEPORT"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("tp"));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("TP"));
}

TEST_F(CommandContextTest, GetCommandByNameIsCaseInsensitiveForUtf8) {
    auto *cmd = MakeCommand("\xC3\x84pfel");
    ASSERT_TRUE(Register(cmd));

    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("\xC3\xA4PFEL"));
}

TEST_F(CommandContextTest, GetCommandByIndex) {
    auto *cmd1 = MakeCommand("aaa");
    auto *cmd2 = MakeCommand("bbb");
    Register(cmd1);
    Register(cmd2);

    EXPECT_EQ(static_cast<ICommand *>(cmd1), ctx->GetCommandByIndex(0));
    EXPECT_EQ(static_cast<ICommand *>(cmd2), ctx->GetCommandByIndex(1));
    EXPECT_EQ(nullptr, ctx->GetCommandByIndex(99));
}

// Unregistration
TEST_F(CommandContextTest, UnregisterCommand) {
    auto *cmd = MakeCommand("test");
    Register(cmd);

    EXPECT_EQ(BML::CommandContext::UnregisterResult::Success, Unregister("test"));
    EXPECT_EQ(0u, ctx->GetCommandCount());
    EXPECT_EQ(nullptr, ctx->GetCommandByName("test"));
}

TEST_F(CommandContextTest, UnregisterAllowsReplacementWithoutOldLookup) {
    auto *oldCmd = MakeCommand("reload-smoke", "rs");
    auto *newCmd = MakeCommand("reload-smoke", "rs2");
    ASSERT_TRUE(Register(oldCmd));

    EXPECT_EQ(BML::CommandContext::UnregisterResult::Success, Unregister("reload-smoke"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("reload-smoke"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("rs"));

    ASSERT_TRUE(Register(newCmd));
    EXPECT_EQ(static_cast<ICommand *>(newCmd), ctx->GetCommandByName("reload-smoke"));
    EXPECT_EQ(static_cast<ICommand *>(newCmd), ctx->GetCommandByName("rs2"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("rs"));
    EXPECT_EQ(1u, ctx->GetCommandCount());
}

TEST_F(CommandContextTest, UnregisterRemovesAlias) {
    auto *cmd = MakeCommand("teleport", "tp");
    Register(cmd);

    EXPECT_EQ(BML::CommandContext::UnregisterResult::Success, Unregister("teleport"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("tp"));
}

TEST_F(CommandContextTest, UnregisterIsCaseInsensitive) {
    auto *cmd = MakeCommand("Teleport", "Tp");
    ASSERT_TRUE(Register(cmd));

    EXPECT_EQ(BML::CommandContext::UnregisterResult::Success, Unregister("teleport"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("Teleport"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("tp"));
}

TEST_F(CommandContextTest, UnregisterNonexistent) {
    EXPECT_EQ(BML::CommandContext::UnregisterResult::NotFound, Unregister("nonexistent"));
    EXPECT_EQ(BML::CommandContext::UnregisterResult::InvalidName, Unregister(nullptr));
    EXPECT_EQ(BML::CommandContext::UnregisterResult::InvalidName, Unregister(""));
}

TEST_F(CommandContextTest, OnlyRegistrarCanUnregisterCommand) {
    int otherRegistrar = 0;
    auto *cmd = MakeCommand("teleport", "tp");
    ASSERT_TRUE(Register(cmd));

    EXPECT_EQ(BML::CommandContext::UnregisterResult::AccessDenied,
              Unregister("tp", &otherRegistrar));
    EXPECT_EQ(static_cast<ICommand *>(cmd), ctx->GetCommandByName("teleport"));
    EXPECT_EQ(BML::CommandContext::UnregisterResult::Success, Unregister("tp"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("teleport"));
}

TEST_F(CommandContextTest, UnregisterCommandsRemovesOnlyRegistrarCommands) {
    int otherRegistrar = 0;
    auto *first = MakeCommand("first", "f");
    auto *second = MakeCommand("second", "s");
    auto *other = MakeCommand("other", "o");
    ASSERT_TRUE(Register(first));
    ASSERT_TRUE(Register(second));
    ASSERT_TRUE(Register(other, &otherRegistrar));

    ctx->UnregisterCommands(this);

    EXPECT_EQ(nullptr, ctx->GetCommandByName("first"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("f"));
    EXPECT_EQ(nullptr, ctx->GetCommandByName("second"));
    EXPECT_EQ(static_cast<ICommand *>(other), ctx->GetCommandByName("o"));
    EXPECT_EQ(1u, ctx->GetCommandCount());
}

// Sort
TEST_F(CommandContextTest, CommandsStaySortedAfterRegistration) {
    auto *cmd1 = MakeCommand("zzz");
    auto *cmd2 = MakeCommand("aaa");
    auto *cmd3 = MakeCommand("mmm");
    Register(cmd1);
    Register(cmd2);
    Register(cmd3);

    EXPECT_EQ(static_cast<ICommand *>(cmd2), ctx->GetCommandByIndex(0)); // aaa
    EXPECT_EQ(static_cast<ICommand *>(cmd3), ctx->GetCommandByIndex(1)); // mmm
    EXPECT_EQ(static_cast<ICommand *>(cmd1), ctx->GetCommandByIndex(2)); // zzz
}

// Clear
TEST_F(CommandContextTest, ClearCommands) {
    Register(MakeCommand("aaa"));
    Register(MakeCommand("bbb"));

    ctx->ClearCommands();
    EXPECT_EQ(0u, ctx->GetCommandCount());
    EXPECT_EQ(nullptr, ctx->GetCommandByName("aaa"));
}

TEST_F(CommandContextTest, CheatPolicyChangesAreIdempotent) {
    EXPECT_FALSE(ctx->IsCheatEnabled());

    EXPECT_TRUE(ctx->SetCheatEnabled(true));
    EXPECT_TRUE(ctx->IsCheatEnabled());
    EXPECT_FALSE(ctx->SetCheatEnabled(true));

    EXPECT_TRUE(ctx->SetCheatEnabled(false));
    EXPECT_FALSE(ctx->IsCheatEnabled());
    EXPECT_FALSE(ctx->SetCheatEnabled(false));
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

TEST(ICommandParse, ParseFloatKeepsNegativeValuesByDefault) {
    EXPECT_FLOAT_EQ(-1.5f, ICommand::ParseFloat("-1.5"));
    EXPECT_FLOAT_EQ(-1000.0f, ICommand::ParseFloat("-1000"));
    EXPECT_FLOAT_EQ(0.0f, ICommand::ParseFloat("0"));
    EXPECT_FLOAT_EQ(1.5f, ICommand::ParseFloat("1.5"));
}

TEST(ICommandParse, ParseFloatClampsToExplicitBounds) {
    EXPECT_FLOAT_EQ(-1.0f, ICommand::ParseFloat("-5", -1.0f, 1.0f));
    EXPECT_FLOAT_EQ(1.0f, ICommand::ParseFloat("5", -1.0f, 1.0f));
    EXPECT_FLOAT_EQ(0.25f, ICommand::ParseFloat("0.25", -1.0f, 1.0f));
}

TEST(ICommandParse, ParseIntegerClampsToExplicitBounds) {
    EXPECT_EQ(-3, ICommand::ParseInteger("-7", -3, 3));
    EXPECT_EQ(3, ICommand::ParseInteger("7", -3, 3));
    EXPECT_EQ(-7, ICommand::ParseInteger("-7"));
}

TEST(ICommandParse, ParseBooleanAcceptsKnownTruthyTokens) {
    EXPECT_TRUE(ICommand::ParseBoolean("true"));
    EXPECT_TRUE(ICommand::ParseBoolean("on"));
    EXPECT_TRUE(ICommand::ParseBoolean("1"));
    EXPECT_FALSE(ICommand::ParseBoolean("false"));
    EXPECT_FALSE(ICommand::ParseBoolean("TRUE"));
}
