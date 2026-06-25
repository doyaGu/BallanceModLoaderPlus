#include <gtest/gtest.h>

#include <algorithm>

#include "AngelScript/ScriptLibraryRegistry.h"
#include "AngelScript/ScriptLibraryValidator.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {
namespace Test {
namespace {

class ScriptLibraryRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        Root = utils::CombinePathW(utils::GetTempPathW(), L"BMLScriptLibraryRegistryTest");
        utils::DeleteDirectoryW(Root);
        utils::CreateFileTreeW(Root);
    }

    void TearDown() override {
        utils::DeleteDirectoryW(Root);
    }

    void Write(const std::wstring &path, const std::string &content) {
        ASSERT_TRUE(utils::WriteTextFileUtf8(utils::Utf16ToUtf8(path), content));
    }

    std::wstring Root;
};

} // namespace

TEST_F(ScriptLibraryRegistryTest, ValidatesIdAndVersion) {
    EXPECT_TRUE(ScriptLibraryRegistry::IsValidLibraryId("com.example.score"));
    EXPECT_TRUE(ScriptLibraryRegistry::IsValidLibraryId("com-example.score_lib"));
    EXPECT_FALSE(ScriptLibraryRegistry::IsValidLibraryId("Com.example"));
    EXPECT_FALSE(ScriptLibraryRegistry::IsValidLibraryId("com..example"));
    EXPECT_FALSE(ScriptLibraryRegistry::IsValidLibraryId(".example"));

    EXPECT_TRUE(ScriptLibraryRegistry::IsValidLibraryVersion("1.2.0"));
    EXPECT_FALSE(ScriptLibraryRegistry::IsValidLibraryVersion("1.2"));
    EXPECT_FALSE(ScriptLibraryRegistry::IsValidLibraryVersion("1.2.0-beta"));
    EXPECT_FALSE(ScriptLibraryRegistry::IsValidLibraryVersion("latest"));
}

TEST_F(ScriptLibraryRegistryTest, ScansAndResolvesExactPackage) {
    const std::wstring packageRoot = utils::CombinePathW(Root, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"api.as"), "namespace ScoreApi {}\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    ASSERT_TRUE(registry.Scan(diagnostic)) << diagnostic;
    ASSERT_EQ(1u, registry.GetPackages().size());

    ScriptLibraryInclude include;
    ASSERT_TRUE(ScriptLibraryRegistry::TryParseVirtualInclude(
        "/bml/libs/com.example.score@1.2.0/api.as",
        include,
        diagnostic)) << diagnostic;

    std::wstring physical;
    ASSERT_TRUE(registry.ResolveInclude(include, physical, diagnostic)) << diagnostic;
    EXPECT_TRUE(utils::IsPathInsideRootW(physical, packageRoot));
}

TEST_F(ScriptLibraryRegistryTest, RejectsInvalidPackageDirectories) {
    Write(utils::CombinePathW(Root, L"Com.Example\\1.2.0\\api.as"), "namespace Bad {}\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    EXPECT_FALSE(registry.Scan(diagnostic));
    EXPECT_NE(std::string::npos, diagnostic.find("Invalid script library id"));
}

TEST_F(ScriptLibraryRegistryTest, RejectsInvalidVersionDirectories) {
    Write(utils::CombinePathW(Root, L"com.example\\latest\\api.as"), "namespace Bad {}\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    EXPECT_FALSE(registry.Scan(diagnostic));
    EXPECT_NE(std::string::npos, diagnostic.find("Invalid script library version"));
}

TEST_F(ScriptLibraryRegistryTest, RejectsEscapingVirtualPath) {
    ScriptLibraryInclude include;
    std::string diagnostic;
    EXPECT_FALSE(ScriptLibraryRegistry::TryParseVirtualInclude(
        "/bml/libs/com.example.score@1.2.0/../api.as",
        include,
        diagnostic));
    EXPECT_FALSE(diagnostic.empty());
}

TEST_F(ScriptLibraryRegistryTest, ResolvesRelativeIncludeInsidePackage) {
    std::string resolved;
    std::string diagnostic;
    ASSERT_TRUE(ScriptLibraryRegistry::ResolveRelativeInclude(
        "/bml/libs/com.example.score@1.2.0/client/client.as",
        "../api.as",
        resolved,
        diagnostic)) << diagnostic;
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/api.as", resolved);

    EXPECT_FALSE(ScriptLibraryRegistry::ResolveRelativeInclude(
        "/bml/libs/com.example.score@1.2.0/client/client.as",
        "../../../escape.as",
        resolved,
        diagnostic));
}

TEST_F(ScriptLibraryRegistryTest, ValidatorAcceptsPackageWithRelativeInclude) {
    const std::wstring packageRoot = utils::CombinePathW(Root, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"client.as"), "#include \"api.as\"\nnamespace ScoreApi {}\n");
    Write(utils::CombinePathW(packageRoot, L"api.as"), "namespace ScoreApi { const int Version = 12; }\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    ASSERT_TRUE(registry.Scan(diagnostic)) << diagnostic;

    ScriptLibraryPackage package;
    ASSERT_TRUE(registry.FindPackage("com.example.score", "1.2.0", package));
    std::vector<std::string> lines;
    EXPECT_TRUE(ValidateScriptLibraryPackage(registry, package, lines));
    EXPECT_NE(lines.end(), std::find(lines.begin(), lines.end(), "  check=ok files=2 includes=1"));
}

TEST_F(ScriptLibraryRegistryTest, ValidatorRejectsBmlMetadata) {
    const std::wstring packageRoot = utils::CombinePathW(Root, L"com.example.bad\\1.0.0");
    Write(utils::CombinePathW(packageRoot, L"api.as"),
          "[bml.mod id=\"bad\" name=\"Bad\" version=\"1.0.0\"]\nclass Bad {}\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    ASSERT_TRUE(registry.Scan(diagnostic)) << diagnostic;

    ScriptLibraryPackage package;
    ASSERT_TRUE(registry.FindPackage("com.example.bad", "1.0.0", package));
    std::vector<std::string> lines;
    EXPECT_FALSE(ValidateScriptLibraryPackage(registry, package, lines));
    EXPECT_TRUE(std::any_of(lines.begin(), lines.end(), [](const std::string &line) {
        return line.find("metadata is not allowed") != std::string::npos;
    }));
}

} // namespace Test
} // namespace BML
