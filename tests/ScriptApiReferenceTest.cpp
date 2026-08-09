#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef BML_TEST_SOURCE_ROOT
#define BML_TEST_SOURCE_ROOT "."
#endif

namespace {

std::string ReadTextFile(const char *relativePath) {
    const std::string path = std::string(BML_TEST_SOURCE_ROOT) + "/" + relativePath;
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return {};

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

void ExpectContainsAll(const std::string &text,
                       const std::vector<std::string> &needles,
                       const char *label) {
    ASSERT_FALSE(text.empty()) << label << " should be readable";
    for (const std::string &needle : needles) {
        EXPECT_NE(std::string::npos, text.find(needle))
            << label << " is missing: " << needle;
    }
}

void ExpectContainsNone(const std::string &text,
                        const std::vector<std::string> &needles,
                        const char *label) {
    ASSERT_FALSE(text.empty()) << label << " should be readable";
    for (const std::string &needle : needles) {
        EXPECT_EQ(std::string::npos, text.find(needle))
            << label << " still exposes removed surface: " << needle;
    }
}

} // namespace

TEST(ScriptApiReferenceTest, ImcBackedFacadesAreDocumented) {
    constexpr const char *kScriptApiStub = "docs/api/bml-script-mod-api.as";
    constexpr const char *kPredefinedApi = "docs/api/as.predefined";
    const std::vector<std::string> declarations = {
        "namespace Runtime",
        "class State",
        "int ReadState(State &out state)",
        "namespace Scene",
        "class ObjectInfo",
        "int ReadObject(CKObject@ object, ObjectInfo &out info)",
        "namespace Gameplay",
        "class LevelState",
        "int OpenCatalog(CatalogCursor@ &out cursor)",
        "namespace Events",
        "class Stream",
        "int Open(Stream@ &out stream, int capacity = 256)",
        "const int ERROR_IMC_ENDPOINT_NOT_FOUND",
        "const int ERROR_IMC_TARGET_EXECUTION_FAILED",
    };

    ExpectContainsAll(ReadTextFile(kScriptApiStub), declarations, kScriptApiStub);
    ExpectContainsAll(ReadTextFile(kPredefinedApi), declarations, kPredefinedApi);
    const std::string builtinFacade = ReadTextFile("src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsAll(builtinFacade,
                      {"RegisterScriptImcFacade", "RegisterRuntime", "RegisterScene", "RegisterGameplay", "RegisterEvents",
                       "ScriptImcClients", "SubscribeAll", "BML_IMC_EXECUTION_GAME_THREAD",
                       "if (capacity < 0)", "if (capacity == 0)", "capacity = 256;"},
                      "src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsNone(builtinFacade,
                       {"namespace BML::Interop", "InteropRegistry", "BML_RecordRef", "BML_StreamRef", "BML_CursorRef"},
                       "src/AngelScript/ScriptImcFacade.cpp");
}

TEST(ScriptApiReferenceTest, RemovedRawInteropSurfaceIsNotDocumented) {
    constexpr const char *kScriptApiStub = "docs/api/bml-script-mod-api.as";
    constexpr const char *kPredefinedApi = "docs/api/as.predefined";
    const std::vector<std::string> removed = {
        "namespace Interop",
        "ERROR_INTEROP_",
        "namespace Observe",
        "class CallFrame",
        "class EventSubscription",
        "class EventRef",
        "class ExportResolver",
        "class ExportRef",
    };
    ExpectContainsNone(ReadTextFile(kScriptApiStub), removed, kScriptApiStub);
    ExpectContainsNone(ReadTextFile(kPredefinedApi), removed, kPredefinedApi);
}

TEST(ScriptApiReferenceTest, ImcFacadeDeclarationsDoNotUseLeadingGlobalScope) {
    const std::string builtinFacade = ReadTextFile("src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsNone(builtinFacade,
                       {"\"::BML::"},
                       "src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsNone(ReadTextFile("docs/api/bml-script-mod-api.as"),
                       {"::BML::"},
                       "docs/api/bml-script-mod-api.as");
    ExpectContainsNone(ReadTextFile("docs/api/as.predefined"),
                       {"::BML::"},
                       "docs/api/as.predefined");
}

TEST(ScriptApiReferenceTest, UiAndSpeedrunHaveDistinctResponsibilities) {
    constexpr const char *kScriptApiStub = "docs/api/bml-script-mod-api.as";
    constexpr const char *kPredefinedApi = "docs/api/as.predefined";
    const std::vector<std::string> declarations = {
        "namespace UI",
        "void AddMessage(const string &in message)",
        "int GetHUDMode()",
        "void SetHUDMode(int mode)",
        "namespace Speedrun",
        "void SetTimerVisible(bool visible)",
        "float GetElapsedTime()",
        "enum ButtonType",
        "bool MainButton(const string &in label)",
    };
    const std::vector<std::string> removed = {
        "namespace Menu",
        "namespace HUD",
        "void SendMessage(const string &in message)",
        "void SendIngameMessage(const string &in message) const",
        "void ClearIngameMessages() const",
        "int GetHUD() const",
        "void SetHUD(int mode) const",
        "namespace Overlay",
        "void ShowSRTimer(bool show)",
        "void StartSRTimer()",
        "float GetSRTime()",
    };

    ExpectContainsAll(ReadTextFile(kScriptApiStub), declarations, kScriptApiStub);
    ExpectContainsAll(ReadTextFile(kPredefinedApi), declarations, kPredefinedApi);
    ExpectContainsNone(ReadTextFile(kScriptApiStub), removed, kScriptApiStub);
    ExpectContainsNone(ReadTextFile(kPredefinedApi), removed, kPredefinedApi);
}

TEST(ScriptApiReferenceTest, BuiltinEventsUseOnlyImcPublishing) {
    const std::string provider = ReadTextFile("src/BuiltinImcApis.cpp");
    ASSERT_FALSE(provider.empty());
    ExpectContainsAll(provider, {"PublishImcEvent(event)"}, "src/BuiltinImcApis.cpp");
    ExpectContainsNone(provider,
                       {"HasStreamConsumers", "CreateStreamRecord", "RecordBuilder", "InteropRegistry"},
                       "src/BuiltinImcApis.cpp");
}
