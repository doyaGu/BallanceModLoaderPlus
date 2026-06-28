#include "AngelScript/ScriptModRuntime.h"

#include <gtest/gtest.h>

#include <utility>

namespace BML {
namespace {

TEST(CKAngelScriptAdapterTest, NamesModuleFoundationFeatures) {
    EXPECT_STREQ("CKAS_FEATURE_MODULE_IMPORTS",
                 CKAngelScriptAdapter::FeatureName(CKAS_FEATURE_MODULE_IMPORTS));
    EXPECT_STREQ("CKAS_FEATURE_MODULE_BYTECODE",
                 CKAngelScriptAdapter::FeatureName(CKAS_FEATURE_MODULE_BYTECODE));
    EXPECT_STREQ("CKAS_FEATURE_MODULE_REPLACE_TRANSACTION",
                 CKAngelScriptAdapter::FeatureName(CKAS_FEATURE_MODULE_REPLACE_TRANSACTION));
    EXPECT_STREQ("CKAS_FEATURE_MODULE_GRAPH",
                 CKAngelScriptAdapter::FeatureName(CKAS_FEATURE_MODULE_GRAPH));
    EXPECT_STREQ("CKAS_FEATURE_MODULE_FINGERPRINT",
                 CKAngelScriptAdapter::FeatureName(CKAS_FEATURE_MODULE_FINGERPRINT));
}

TEST(ScriptModRuntimeTest, MoveConstructorRebindsCachedApiToDestinationAdapter) {
    ScriptModRuntime source("source");
    source.TestSetActiveCachedApi();

    const CKAngelScriptAdapter::Api *sourceApi = source.TestAdapterApi();
    ASSERT_EQ(sourceApi, source.TestCachedApi());

    ScriptModRuntime moved(std::move(source));

    EXPECT_EQ(moved.TestAdapterApi(), moved.TestCachedApi());
    EXPECT_NE(sourceApi, moved.TestCachedApi());
    EXPECT_EQ(nullptr, source.TestCachedApi());
    EXPECT_EQ(nullptr, source.TestAngelScript());
}

TEST(ScriptModRuntimeTest, MoveAssignmentRebindsCachedApiToDestinationAdapter) {
    ScriptModRuntime source("source");
    source.TestSetActiveCachedApi();

    const CKAngelScriptAdapter::Api *sourceApi = source.TestAdapterApi();
    ASSERT_EQ(sourceApi, source.TestCachedApi());

    ScriptModRuntime target("target");
    target = std::move(source);

    EXPECT_EQ(target.TestAdapterApi(), target.TestCachedApi());
    EXPECT_NE(sourceApi, target.TestCachedApi());
    EXPECT_EQ(nullptr, source.TestCachedApi());
    EXPECT_EQ(nullptr, source.TestAngelScript());
}

} // namespace
} // namespace BML
