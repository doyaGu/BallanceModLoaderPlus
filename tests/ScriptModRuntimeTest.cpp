#include "AngelScript/ScriptModRuntime.h"

#include <gtest/gtest.h>

#include <utility>

namespace BML {
namespace {

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
