#include <gtest/gtest.h>

#include "Console/Shell/ShellEnvironment.h"

using namespace BML::Shell;

TEST(ShellEnvironment, SessionShadowsUniversal) {
    Environment env;
    env.SetVariable("X", "universal", Environment::Scope::Universal);
    std::string value;
    ASSERT_TRUE(env.LookupVariable("X", value));
    EXPECT_EQ("universal", value);

    env.SetVariable("X", "session", Environment::Scope::Session);
    ASSERT_TRUE(env.LookupVariable("X", value));
    EXPECT_EQ("session", value);

    // Setting the universal scope again drops the session shadow.
    env.SetVariable("X", "again", Environment::Scope::Universal);
    ASSERT_TRUE(env.LookupVariable("X", value));
    EXPECT_EQ("again", value);

    EXPECT_TRUE(env.EraseVariable("X"));
    EXPECT_FALSE(env.HasVariable("X"));
    EXPECT_FALSE(env.EraseVariable("X"));
}

TEST(ShellEnvironment, NamesAreValidated) {
    EXPECT_TRUE(Environment::IsValidName("abc_1"));
    EXPECT_TRUE(Environment::IsValidName("_"));
    EXPECT_FALSE(Environment::IsValidName(""));
    EXPECT_FALSE(Environment::IsValidName("1abc"));
    EXPECT_FALSE(Environment::IsValidName("a-b"));
    EXPECT_FALSE(Environment::IsValidName("a b"));
}

TEST(ShellEnvironment, AliasesAreTracked) {
    Environment env;
    EXPECT_FALSE(env.IsDirty());
    env.SetAlias("h", "help");
    env.SetAlias("e", "echo -n");
    EXPECT_TRUE(env.IsDirty());
    std::string body;
    ASSERT_TRUE(env.LookupAlias("e", body));
    EXPECT_EQ("echo -n", body);
    EXPECT_EQ((std::vector<std::string>{"e", "h"}), env.AliasNames());
    EXPECT_TRUE(env.RemoveAlias("h"));
    EXPECT_FALSE(env.RemoveAlias("h"));
    env.ClearAliases();
    EXPECT_TRUE(env.AliasNames().empty());
}

TEST(ShellEnvironment, JsonRoundTripKeepsUniversalStateOnly) {
    Environment env;
    env.SetVariable("U", "a;b # c", Environment::Scope::Universal);
    env.SetVariable("S", "session only", Environment::Scope::Session);
    env.SetAlias("ll", "echo 'it''s' && help");
    std::string error;
    const std::string json = env.ToJson(error);
    ASSERT_FALSE(json.empty()) << error;

    Environment loaded;
    ASSERT_TRUE(loaded.FromJson(json, error)) << error;
    std::string value;
    ASSERT_TRUE(loaded.LookupVariable("U", value));
    EXPECT_EQ("a;b # c", value);
    EXPECT_FALSE(loaded.HasVariable("S"));
    ASSERT_TRUE(loaded.LookupAlias("ll", value));
    EXPECT_EQ("echo 'it''s' && help", value);
    EXPECT_FALSE(loaded.IsDirty());
}

TEST(ShellEnvironment, JsonRejectsGarbageAndSkipsBadNames) {
    Environment env;
    std::string error;
    EXPECT_FALSE(env.FromJson("not json", error));
    EXPECT_FALSE(env.FromJson("[1,2]", error));
    ASSERT_TRUE(env.FromJson("{\"version\":1,\"variables\":{\"ok\":\"1\",\"1bad\":\"2\",\"num\":3},\"aliases\":{\"a\":\"b\"}}", error)) << error;
    EXPECT_TRUE(env.HasVariable("ok"));
    EXPECT_FALSE(env.HasVariable("1bad"));
    EXPECT_FALSE(env.HasVariable("num"));
    EXPECT_TRUE(env.HasAlias("a"));
    EXPECT_FALSE(env.FromJson("{\"variables\":[]}", error));
}

TEST(ShellEnvironment, VariableNamesUnionBothScopes) {
    Environment env;
    env.SetVariable("b", "1", Environment::Scope::Universal);
    env.SetVariable("a", "2", Environment::Scope::Session);
    env.SetVariable("b", "3", Environment::Scope::Session);
    EXPECT_EQ((std::vector<std::string>{"a", "b"}), env.VariableNames());
    EXPECT_EQ(1u, env.Variables(Environment::Scope::Universal).size());
    EXPECT_EQ(2u, env.Variables(Environment::Scope::Session).size());
}

TEST(ShellEnvironment, MissingFileLoadsEmpty) {
    Environment env;
    env.SetAlias("x", "y");
    std::string error;
    EXPECT_TRUE(env.Load(L"Z:\\definitely\\missing\\CommandBar.shell.json", error)) << error;
    EXPECT_FALSE(env.HasAlias("x"));
    EXPECT_FALSE(env.IsDirty());
}
