#include "AngelScript/ScriptAvailabilityLogLimiter.h"
#include "AngelScript/ScriptModRuntime.h"

#include <gtest/gtest.h>

#include <cstdint>
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

TEST(ScriptAvailabilityLogLimiterTest, LogsMissingModuleOnceUntilReset) {
    ScriptAvailabilityLogLimiter limiter;
    const std::string diagnostic = "AngelScript.dll is not loaded; script mods are unavailable.";

    EXPECT_TRUE(limiter.ShouldLog(CKAngelScriptAdapter::State::MissingModule, diagnostic));
    EXPECT_FALSE(limiter.ShouldLog(CKAngelScriptAdapter::State::MissingModule, diagnostic));

    limiter.Reset();

    EXPECT_TRUE(limiter.ShouldLog(CKAngelScriptAdapter::State::MissingModule, diagnostic));
}

TEST(ScriptAvailabilityLogLimiterTest, LogsChangedUnavailableDiagnostic) {
    ScriptAvailabilityLogLimiter limiter;

    EXPECT_TRUE(limiter.ShouldLog(CKAngelScriptAdapter::State::MissingModule, "missing"));
    EXPECT_TRUE(limiter.ShouldLog(CKAngelScriptAdapter::State::MissingRuntime, "missing"));
    EXPECT_TRUE(limiter.ShouldLog(CKAngelScriptAdapter::State::MissingRuntime, "runtime unavailable"));
    EXPECT_FALSE(limiter.ShouldLog(CKAngelScriptAdapter::State::MissingRuntime, "runtime unavailable"));
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

TEST(ScriptModRuntimeTest, ReleaseMethodKeepsHandleWhenAdapterRefreshFails) {
    ScriptModRuntime runtime("source");
    CKAngelScriptMethod *method = reinterpret_cast<CKAngelScriptMethod *>(static_cast<std::uintptr_t>(0x1234));
    CKAngelScriptMethod *original = method;
    ScriptDiagnostic diagnostic;

    EXPECT_FALSE(runtime.ReleaseMethod(nullptr, method, &diagnostic));

    EXPECT_EQ(original, method);
    EXPECT_FALSE(diagnostic.Message.empty());
}

} // namespace
} // namespace BML
