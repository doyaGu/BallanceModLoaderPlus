#include <gtest/gtest.h>

#include "../modules/BML_Console/src/CommandRegistry.h"

namespace {

BML_Result DummyExecute(const BML_ConsoleCommandInvocation *, void *) {
    return BML_RESULT_OK;
}

TEST(CommandRegistryTests, AllowsQuestionMarkAliasForHelpStyleCommands) {
    BML::Console::CommandRegistry registry;

    BML_ConsoleCommandDesc desc = BML_CONSOLE_COMMAND_DESC_INIT;
    desc.name_utf8 = "help";
    desc.alias_utf8 = "?";
    desc.execute = DummyExecute;

    ASSERT_EQ(BML_RESULT_OK, registry.Register(&desc));
    EXPECT_NE(nullptr, registry.Find("help"));
    EXPECT_NE(nullptr, registry.Find("?"));
    EXPECT_EQ(BML_RESULT_OK, registry.Execute("help"));
    EXPECT_EQ(BML_RESULT_OK, registry.Execute("?"));
}

} // namespace
