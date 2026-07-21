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
            << label << " still exposes removed Interop surface: " << needle;
    }
}

} // namespace

TEST(ScriptApiReferenceTest, InteropApiFacadeIsDocumented) {
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
        "namespace Interop",
        "class Record",
        "int GetMat4Array(uint field, array<::BML::Mat4> &out values) const",
        "class Cursor",
        "int CreateInput(const string &in apiId, uint schema, Input@ &out input)",
        "int InvokeQuery(const string &in apiId, const string &in endpoint, Input@ input, Record@ &out record)",
        "class ApiBuilder",
        "int AddCompatibleApiHash(uint64 hash)",
        "class Provider",
        "int RegisterProvider(ApiBuilder@ api, Provider@ provider)",
        "int GetInputVec3Array(uint field, array<::BML::Vec3> &out values)",
        "int SetMat4Array(uint field, const array<::BML::Mat4> &in values)",
    };

    ExpectContainsAll(ReadTextFile("docs/bml-script-mod-api.as"), declarations, "docs/bml-script-mod-api.as");
    ExpectContainsAll(ReadTextFile("docs/as.predefined"), declarations, "docs/as.predefined");
    const std::string builtinFacade = ReadTextFile("src/AngelScript/ScriptInteropFacade.cpp");
    ExpectContainsAll(builtinFacade,
                      {"RegisterScriptInteropFacade", "RegisterRuntime", "RegisterScene", "RegisterGameplay", "RegisterEvents",
                       "ScriptInteropImcClients", "SubscribeAll", "BML_IMC_EXECUTION_GAME_THREAD",
                       "if (capacity < 0)", "if (capacity == 0)", "capacity = 256;"},
                      "src/AngelScript/ScriptInteropFacade.cpp");
    ExpectContainsNone(builtinFacade,
                       {"InteropRegistry", "BML_RecordRef", "BML_StreamRef", "BML_CursorRef"},
                       "src/AngelScript/ScriptInteropFacade.cpp");
    ExpectContainsAll(ReadTextFile("src/AngelScript/ScriptInteropProviderBridge.cpp"),
                      {"RegisterScriptInteropProviderBridge", "AddCompatibleApiHash", "RegisterProvider"},
                      "src/AngelScript/ScriptInteropProviderBridge.cpp");
    ExpectContainsAll(ReadTextFile("src/AngelScript/ScriptInteropConsumerBridge.cpp"),
                      {"RegisterScriptInteropConsumerBridge", "OpenCollection", "InvokeCommand"},
                      "src/AngelScript/ScriptInteropConsumerBridge.cpp");
}

TEST(ScriptApiReferenceTest, RemovedRawInteropSurfaceIsNotDocumented) {
    const std::vector<std::string> removed = {
        "namespace Observe",
        "class CallFrame",
        "class EventSubscription",
        "class EventRef",
        "class ExportResolver",
        "class ExportRef",
    };
    ExpectContainsNone(ReadTextFile("docs/bml-script-mod-api.as"), removed, "docs/bml-script-mod-api.as");
    ExpectContainsNone(ReadTextFile("docs/as.predefined"), removed, "docs/as.predefined");
}

TEST(ScriptApiReferenceTest, LegacyEventCompatibilityPathIsSubscriberGated) {
    const std::string provider = ReadTextFile("src/BuiltinInteropApis.cpp");
    ASSERT_FALSE(provider.empty());
    const std::size_t imcPublish = provider.find("PublishImcEvent(event)");
    const std::size_t legacyGate = provider.find("!registry.HasStreamConsumers(EventsApi::Descriptor.ApiId, \"all\")");
    const std::size_t legacyRecord = provider.find("registry.CreateStreamRecord");
    ASSERT_NE(std::string::npos, imcPublish);
    ASSERT_NE(std::string::npos, legacyGate);
    ASSERT_NE(std::string::npos, legacyRecord);
    EXPECT_LT(imcPublish, legacyGate);
    EXPECT_LT(legacyGate, legacyRecord);
}
