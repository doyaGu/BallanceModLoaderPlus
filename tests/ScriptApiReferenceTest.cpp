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

} // namespace

TEST(ScriptApiReferenceTest, ExportResolverBindingSurfaceIsDocumented) {
    const std::vector<std::string> resolverDeclarations = {
        "class ExportResolver",
        "bool get_IsBound() const",
        "int get_LastStatus() const",
        "string get_ModId() const",
        "string get_Name() const",
        "string get_Signature() const",
        "void Clear()",
        "int Rebind()",
        "int Resolve(ExportRef@ &out exportRef)",
        "int Call(CallFrame@ frame)",
        "int CallVoid()",
        "int CallString(const string &in argument, string &out result)",
        "int CallString(string &out result)",
        "int CallBool(bool argument, bool &out result)",
        "int CallBool(bool &out result)",
        "int CallInt(int argument, int &out result)",
        "int CallInt(int &out result)",
        "int CallFloat(float argument, float &out result)",
        "int CallFloat(float &out result)",
    };

    ExpectContainsAll(ReadTextFile("docs/bml-script-mod-api.as"),
                      resolverDeclarations,
                      "docs/bml-script-mod-api.as");
    ExpectContainsAll(ReadTextFile("docs/as.predefined"),
                      resolverDeclarations,
                      "docs/as.predefined");

    std::vector<std::string> bindingDeclarations = resolverDeclarations;
    bindingDeclarations.push_back("ExportResolver@ f(const string &in modId, const string &in name)");
    bindingDeclarations.push_back("ExportResolver@ f(const string &in modId, const string &in name, const string &in signature)");
    ExpectContainsAll(ReadTextFile("src/AngelScript/AngelScriptBindings.cpp"),
                      bindingDeclarations,
                      "src/AngelScript/AngelScriptBindings.cpp");
}
