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

TEST(ScriptApiReferenceTest, BuiltinCapabilityFacadesAreDocumented) {
    constexpr const char *kScriptApiStub = "docs/api/bml-script-mod-api.as";
    constexpr const char *kPredefinedApi = "docs/api/as.predefined";
    const std::vector<std::string> declarations = {
        "namespace Runtime",
        "class State",
        "State GetState()",
        "Clock GetClock()",
        "Score GetScore()",
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
    const std::vector<std::string> removedRuntimeReads = {
        "int ReadState(State &out state)",
        "int ReadClock(Clock &out state)",
        "int ReadScore(Score &out state)",
    };
    ExpectContainsNone(ReadTextFile(kScriptApiStub), removedRuntimeReads,
                       kScriptApiStub);
    ExpectContainsNone(ReadTextFile(kPredefinedApi), removedRuntimeReads,
                       kPredefinedApi);
    const std::string builtinFacade = ReadTextFile("src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsAll(builtinFacade,
                      {"RegisterScriptImcFacade", "RegisterRuntime", "RegisterGameplay", "RegisterEvents",
                       "ScriptImcClients", "SubscribeAll", "BML_IMC_EXECUTION_GAME_THREAD",
                       "if (capacity < 0)", "if (capacity == 0)", "capacity = 256;"},
                      "src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsNone(builtinFacade,
                       {"namespace BML::Interop", "InteropRegistry", "BML_RecordRef", "BML_StreamRef", "BML_CursorRef"},
                       "src/AngelScript/ScriptImcFacade.cpp");
}

TEST(ScriptApiReferenceTest, SceneFacadeDoesNotDuplicateCKAngelScript) {
    constexpr const char *kScriptApiStub = "docs/api/bml-script-mod-api.as";
    constexpr const char *kPredefinedApi = "docs/api/as.predefined";
    const std::vector<std::string> removed = {
        "class ObjectInfo",
        "class EntityTransform",
        "int Find(const string &in name, CKObject@ &out object)",
        "int ReadObject(CKObject@ object, ObjectInfo &out info)",
        "int ReadEntity(CKObject@ object, EntityTransform &out transform)",
    };
    ExpectContainsNone(ReadTextFile(kScriptApiStub), removed, kScriptApiStub);
    ExpectContainsNone(ReadTextFile(kPredefinedApi), removed, kPredefinedApi);

    const std::string builtinFacade = ReadTextFile("src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsNone(builtinFacade,
                       {"RegisterScene", "SceneImc::", "bml_scene_imc.hpp"},
                       "src/AngelScript/ScriptImcFacade.cpp");

    const std::string clients = ReadTextFile("src/AngelScript/ScriptImcClients.h") +
                                ReadTextFile("src/AngelScript/ScriptImcClients.cpp");
    ExpectContainsNone(clients,
                       {"bml_scene_imc.hpp", "ScriptImcClients::Scene", "m_Scene"},
                       "ScriptImcClients");
}

TEST(ScriptApiReferenceTest, RuntimeFacadeUsesDirectHostReads) {
    const std::string builtinFacade = ReadTextFile("src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsAll(builtinFacade,
                      {"context->ReadRuntimeState()", "context->GetTimeManager()",
                       "context->GetSRScore()", "context->GetHSScore()"},
                      "src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsNone(builtinFacade,
                       {"bml_runtime_imc.hpp", "RuntimeImc::", "clients->Runtime(",
                        "int ReadState(State &out state)",
                        "int ReadClock(Clock &out state)",
                        "int ReadScore(Score &out state)"},
                       "src/AngelScript/ScriptImcFacade.cpp");

    const std::string clients = ReadTextFile("src/AngelScript/ScriptImcClients.h") +
                                ReadTextFile("src/AngelScript/ScriptImcClients.cpp");
    ExpectContainsNone(clients,
                       {"bml_runtime_imc.hpp", "ScriptImcClients::Runtime", "m_Runtime"},
                       "ScriptImcClients");

    const std::string smoke = ReadTextFile(
        "tests/smoke/AngelScript/BMLAngelScriptSmoke/runtime.as");
    ExpectContainsAll(smoke,
                      {"BML::Runtime::GetState()", "BML::Runtime::GetClock()",
                       "BML::Runtime::GetScore()"},
                      "BMLAngelScriptSmoke/runtime.as");
    ExpectContainsNone(smoke,
                       {"BML::Runtime::ReadState(", "BML::Runtime::ReadClock(",
                        "BML::Runtime::ReadScore("},
                       "BMLAngelScriptSmoke/runtime.as");
}

TEST(ScriptApiReferenceTest, GameplayFacadeUsesDirectBuiltinReads) {
    const std::string builtinFacade = ReadTextFile("src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsAll(builtinFacade,
                      {"ReadBuiltinGameplayLevel", "ReadBuiltinGameplayEnergy",
                       "ReadBuiltinGameplayCatalog", "ReadBuiltinGameplayCheckpoints",
                       "ReadBuiltinGameplayResetpoints"},
                      "src/AngelScript/ScriptImcFacade.cpp");
    ExpectContainsNone(builtinFacade,
                       {"clients->Gameplay(", "client.CallLevel(", "client.CallEnergy(",
                        "client.CallCatalog(", "client.CallCheckpoints(",
                        "client.CallResetpoints("},
                       "src/AngelScript/ScriptImcFacade.cpp");

    const std::string clients = ReadTextFile("src/AngelScript/ScriptImcClients.h") +
                                ReadTextFile("src/AngelScript/ScriptImcClients.cpp");
    ExpectContainsNone(clients,
                       {"bml_gameplay_imc.hpp", "ScriptImcClients::Gameplay", "m_Gameplay"},
                       "ScriptImcClients");
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
