#include "AngelScript/ScriptAvailabilityLogLimiter.h"
#include "AngelScript/ScriptModRuntime.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <utility>

namespace BML {
namespace {

struct HostCallFilterSetProbe {
    int CallCount = 0;
    CKAngelScript *AngelScript = nullptr;
    CKAngelScriptHostCallFilterCallback Callback = nullptr;
    void *UserData = nullptr;
    CKAS_STATUS Status = CKAS_OK;
};

HostCallFilterSetProbe g_HostCallFilterSetProbe;

void __cdecl InitHostCallFilterResult(CKAngelScriptResult *result) {
    if (result)
        result->Size = sizeof(*result);
}

CKAS_STATUS __cdecl SetHostCallFilterProbe(CKAngelScript *angelScript,
                                           CKAngelScriptHostCallFilterCallback callback,
                                           void *userData,
                                           CKAngelScriptResult *) {
    ++g_HostCallFilterSetProbe.CallCount;
    g_HostCallFilterSetProbe.AngelScript = angelScript;
    g_HostCallFilterSetProbe.Callback = callback;
    g_HostCallFilterSetProbe.UserData = userData;
    return g_HostCallFilterSetProbe.Status;
}

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

TEST(ScriptModRuntimeTest, ConfiguresAndClearsCkasHostCallFilter) {
    CKAngelScriptAdapter::Api api;
    api.InitResult = InitHostCallFilterResult;
    api.SetHostCallFilter = SetHostCallFilterProbe;
    CKAngelScript *angelScript = reinterpret_cast<CKAngelScript *>(
        static_cast<std::uintptr_t>(1));
    ScriptDiagnostic diagnostic;

    g_HostCallFilterSetProbe = HostCallFilterSetProbe();
    ASSERT_TRUE(SetScriptModHostCallFilterEnabled(api, angelScript, true, diagnostic));
    EXPECT_EQ(1, g_HostCallFilterSetProbe.CallCount);
    EXPECT_EQ(angelScript, g_HostCallFilterSetProbe.AngelScript);
    EXPECT_NE(nullptr, g_HostCallFilterSetProbe.Callback);
    EXPECT_EQ(nullptr, g_HostCallFilterSetProbe.UserData);

    ASSERT_TRUE(SetScriptModHostCallFilterEnabled(api, angelScript, false, diagnostic));
    EXPECT_EQ(2, g_HostCallFilterSetProbe.CallCount);
    EXPECT_EQ(nullptr, g_HostCallFilterSetProbe.Callback);
    EXPECT_EQ(nullptr, g_HostCallFilterSetProbe.UserData);
}

TEST(ScriptModRuntimeTest, ReportsCkasHostCallFilterConfigurationFailure) {
    CKAngelScriptAdapter::Api api;
    api.InitResult = InitHostCallFilterResult;
    api.SetHostCallFilter = SetHostCallFilterProbe;
    CKAngelScript *angelScript = reinterpret_cast<CKAngelScript *>(
        static_cast<std::uintptr_t>(1));
    ScriptDiagnostic diagnostic;

    g_HostCallFilterSetProbe = HostCallFilterSetProbe();
    g_HostCallFilterSetProbe.Status = CKAS_INVALIDSTATE;
    EXPECT_FALSE(SetScriptModHostCallFilterEnabled(api, angelScript, true, diagnostic));
    EXPECT_EQ(CKAS_INVALIDSTATE, diagnostic.Status);
    EXPECT_EQ(ScriptDiagnosticPhase::CkasHost, diagnostic.Phase);
    EXPECT_NE(std::string::npos,
              diagnostic.Message.find("Failed to install CKAngelScript host-call filter"));
}

TEST(ScriptModRuntimeTest, RejectsCkasAsyncWorkOnlyWhileRunningScriptMods) {
    ScriptMod *owner = reinterpret_cast<ScriptMod *>(static_cast<std::uintptr_t>(1));

    EXPECT_EQ(CKAS_OK,
              ScriptModRuntime::TestFilterHostCall(
                  "Async::Schedule", CKAS_HOSTCALL_SCHEDULES_ASYNC_WORK));

    ScriptCurrentModScope scope(owner);
    EXPECT_EQ(CKAS_INVALIDSTATE,
              ScriptModRuntime::TestFilterHostCall(
                  "Async::Schedule", CKAS_HOSTCALL_SCHEDULES_ASYNC_WORK));
    EXPECT_EQ(CKAS_OK,
              ScriptModRuntime::TestFilterHostCall(
                  "Scene::Create", CKAS_HOSTCALL_MUTATES_HOST_STATE));
}

TEST(ScriptModRuntimeTest, RejectsMutationsAndAsyncWorkInRestrictedPhases) {
    ScriptMod *owner = reinterpret_cast<ScriptMod *>(static_cast<std::uintptr_t>(1));

    {
        ScriptObjectConstructionScope scope(owner);
        EXPECT_EQ(CKAS_INVALIDSTATE,
                  ScriptModRuntime::TestFilterHostCall(
                      "Scene::Create", CKAS_HOSTCALL_MUTATES_HOST_STATE));
    }
    {
        ScriptObjectConstructionScope scope(owner);
        EXPECT_EQ(CKAS_INVALIDSTATE,
                  ScriptModRuntime::TestFilterHostCall(
                      "Async::Schedule", CKAS_HOSTCALL_SCHEDULES_ASYNC_WORK));
    }

    ScriptModRuntime runtime("state-hook");
    ScriptStateHookScope scope(owner, &runtime, ScriptModReloadPhase::SaveState);
    EXPECT_EQ(CKAS_INVALIDSTATE,
              ScriptModRuntime::TestFilterHostCall(
                  "Scene::Create", CKAS_HOSTCALL_MUTATES_HOST_STATE));
    EXPECT_EQ(CKAS_INVALIDSTATE,
              ScriptModRuntime::TestFilterHostCall(
                  "Async::Schedule", CKAS_HOSTCALL_SCHEDULES_ASYNC_WORK));
    EXPECT_EQ(CKAS_OK,
              ScriptModRuntime::TestFilterHostCall(
                  "Scene::Current", CKAS_HOSTCALL_DEFAULT));
}

} // namespace
} // namespace BML
