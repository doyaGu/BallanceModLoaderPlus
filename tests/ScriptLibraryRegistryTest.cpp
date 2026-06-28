#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "AngelScript/ScriptLibraryRegistry.h"
#include "AngelScript/ScriptLibraryTools.h"
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

TEST_F(ScriptLibraryRegistryTest, ScanOrdersPackagesByIdAndNumericVersion) {
    Write(utils::CombinePathW(Root, L"com.example.z\\1.0.0\\api.as"), "namespace Z {}\n");
    Write(utils::CombinePathW(Root, L"com.example.a\\1.10.0\\api.as"), "namespace A10 {}\n");
    Write(utils::CombinePathW(Root, L"com.example.a\\1.2.0\\api.as"), "namespace A2 {}\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    ASSERT_TRUE(registry.Scan(diagnostic)) << diagnostic;

    const std::vector<ScriptLibraryPackage> &packages = registry.GetPackages();
    ASSERT_EQ(3u, packages.size());
    EXPECT_EQ("com.example.a", packages[0].Id);
    EXPECT_EQ("1.2.0", packages[0].Version);
    EXPECT_EQ("com.example.a", packages[1].Id);
    EXPECT_EQ("1.10.0", packages[1].Version);
    EXPECT_EQ("com.example.z", packages[2].Id);
    EXPECT_EQ("1.0.0", packages[2].Version);
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

TEST_F(ScriptLibraryRegistryTest, ToolReportRecordsHashesAndIncludeEdges) {
    const std::wstring packageRoot = utils::CombinePathW(Root, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"client.as"), "#include \"api.as\"\nnamespace ScoreApi {}\n");
    Write(utils::CombinePathW(packageRoot, L"api.as"), "namespace ScoreApi { const int Version = 12; }\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    ASSERT_TRUE(registry.Scan(diagnostic)) << diagnostic;

    ScriptLibraryPackage package;
    ASSERT_TRUE(registry.FindPackage("com.example.score", "1.2.0", package));

    ScriptLibraryPackageCheckReport report;
    ASSERT_TRUE(BuildScriptLibraryPackageCheckReport(registry, package, report));
    EXPECT_TRUE(report.Success);
    EXPECT_EQ(2u, report.Files.size());
    EXPECT_EQ(1u, report.IncludeCount);
    ASSERT_EQ(1u, report.IncludeEdges.size());
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/client.as", report.IncludeEdges.front().FromSection);
    EXPECT_EQ("api.as", report.IncludeEdges.front().Include);
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/api.as", report.IncludeEdges.front().ToSection);
    EXPECT_EQ(1u, report.IncludeEdges.front().Line);
    EXPECT_EQ("com.example.score", report.IncludeEdges.front().LibraryId);
    EXPECT_EQ("1.2.0", report.IncludeEdges.front().LibraryVersion);

    EXPECT_TRUE(std::all_of(report.Files.begin(), report.Files.end(), [](const ScriptLibraryToolFileReport &file) {
        return file.ContentHash.size() == 64;
    }));

    std::vector<std::string> lines;
    ScriptLibraryToolReportOptions options;
    options.IncludeFileHashes = true;
    options.IncludeIncludeGraph = true;
    AppendScriptLibraryPackageCheckLines(report, lines, options);
    EXPECT_TRUE(std::any_of(lines.begin(), lines.end(), [](const std::string &line) {
        return line.find("source hashes:") != std::string::npos;
    }));
    EXPECT_TRUE(std::any_of(lines.begin(), lines.end(), [](const std::string &line) {
        return line.find("include graph:") != std::string::npos;
    }));
}

TEST_F(ScriptLibraryRegistryTest, ParsesToolReportOptions) {
    const std::vector<std::string> args = {
        "script", "lib", "check", "com.example.score", "1.2.0", "--hashes", "--graph",
    };

    ScriptLibraryToolReportOptions options;
    std::string diagnostic;
    EXPECT_TRUE(ParseScriptLibraryToolReportOptions(args, 5, options, diagnostic)) << diagnostic;
    EXPECT_TRUE(options.IncludeFileHashes);
    EXPECT_TRUE(options.IncludeIncludeGraph);
    EXPECT_FALSE(options.Compile);
    EXPECT_TRUE(diagnostic.empty());

    const std::vector<std::string> compileArgs = {
        "script", "lib", "check", "com.example.score", "1.2.0", "--compile",
    };
    EXPECT_TRUE(ParseScriptLibraryToolReportOptions(compileArgs, 5, options, diagnostic)) << diagnostic;
    EXPECT_FALSE(options.IncludeFileHashes);
    EXPECT_FALSE(options.IncludeIncludeGraph);
    EXPECT_TRUE(options.Compile);

    const std::vector<std::string> badArgs = {
        "script", "lib", "check", "com.example.score", "1.2.0", "--unknown",
    };
    EXPECT_FALSE(ParseScriptLibraryToolReportOptions(badArgs, 5, options, diagnostic));
    EXPECT_FALSE(diagnostic.empty());
}

TEST_F(ScriptLibraryRegistryTest, ToolReportSelectsCompileRootsFromIncludeGraph) {
    const std::wstring packageRoot = utils::CombinePathW(Root, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"api.as"), "#include \"detail.as\"\nnamespace ScoreApi {}\n");
    Write(utils::CombinePathW(packageRoot, L"detail.as"), "namespace ScoreApi { void Detail() {} }\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    ASSERT_TRUE(registry.Scan(diagnostic)) << diagnostic;

    ScriptLibraryPackage package;
    ASSERT_TRUE(registry.FindPackage("com.example.score", "1.2.0", package));

    ScriptLibraryPackageCheckReport report;
    ASSERT_TRUE(BuildScriptLibraryPackageCheckReport(registry, package, report));
    const std::vector<std::string> roots = GetScriptLibraryCompileRoots(report);
    ASSERT_EQ(1u, roots.size());
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/api.as", roots.front());
}

TEST_F(ScriptLibraryRegistryTest, ToolReportOrdersFilesAndCompileRootsDeterministically) {
    const std::wstring packageRoot = utils::CombinePathW(Root, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"z.as"), "#include \"shared.as\"\nnamespace ScoreZ {}\n");
    Write(utils::CombinePathW(packageRoot, L"shared.as"), "namespace ScoreShared {}\n");
    Write(utils::CombinePathW(packageRoot, L"a.as"), "#include \"shared.as\"\nnamespace ScoreA {}\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    ASSERT_TRUE(registry.Scan(diagnostic)) << diagnostic;

    ScriptLibraryPackage package;
    ASSERT_TRUE(registry.FindPackage("com.example.score", "1.2.0", package));

    ScriptLibraryPackageCheckReport report;
    ASSERT_TRUE(BuildScriptLibraryPackageCheckReport(registry, package, report));
    ASSERT_EQ(3u, report.Files.size());
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/a.as", report.Files[0].VirtualSection);
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/shared.as", report.Files[1].VirtualSection);
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/z.as", report.Files[2].VirtualSection);

    ASSERT_EQ(2u, report.IncludeEdges.size());
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/a.as", report.IncludeEdges[0].FromSection);
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/z.as", report.IncludeEdges[1].FromSection);

    const std::vector<std::string> roots = GetScriptLibraryCompileRoots(report);
    ASSERT_EQ(2u, roots.size());
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/a.as", roots[0]);
    EXPECT_EQ("/bml/libs/com.example.score@1.2.0/z.as", roots[1]);
}

TEST_F(ScriptLibraryRegistryTest, ToolReportRejectsMissingIncludeDespiteCatalogFile) {
    const std::wstring packageRoot = utils::CombinePathW(Root, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"api.as"), "#include \"missing.as\"\nnamespace ScoreApi {}\n");
    Write(utils::CombinePathW(packageRoot, L"catalog.json"), "{\"files\":[\"missing.as\"]}\n");

    ScriptLibraryRegistry registry(Root);
    std::string diagnostic;
    ASSERT_TRUE(registry.Scan(diagnostic)) << diagnostic;

    ScriptLibraryPackage package;
    ASSERT_TRUE(registry.FindPackage("com.example.score", "1.2.0", package));

    ScriptLibraryPackageCheckReport report;
    EXPECT_FALSE(BuildScriptLibraryPackageCheckReport(registry, package, report));
    EXPECT_FALSE(report.Success);
    EXPECT_TRUE(std::any_of(report.Errors.begin(), report.Errors.end(), [](const std::string &line) {
        return line.find("unresolved relative include") != std::string::npos;
    }));
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
    EXPECT_NE(lines.end(),
              std::find(lines.begin(),
                        lines.end(),
                        "    [bml.mod id=\"bad\" name=\"Bad\" version=\"1.0.0\"]"));
    EXPECT_TRUE(std::none_of(lines.begin(), lines.end(), [](const std::string &line) {
        return line.rfind("  error [bml.", 0) == 0;
    }));
}

} // namespace Test
} // namespace BML
