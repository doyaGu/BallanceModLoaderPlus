#include <gtest/gtest.h>

#include <algorithm>
#include <utility>

#include "AngelScript/ScriptLibraryRegistry.h"
#include "AngelScript/ScriptSourceSnapshotBuilder.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {
namespace Test {
namespace {

class ScriptSourceSnapshotBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        Root = utils::CombinePathW(utils::GetTempPathW(), L"BMLScriptSourceSnapshotBuilderTest");
        utils::DeleteDirectoryW(Root);
        utils::CreateFileTreeW(Root);
        ModsRoot = utils::CombinePathW(Root, L"Mods");
        LibRoot = utils::CombinePathW(Root, L"ScriptLibs");
        utils::CreateFileTreeW(ModsRoot);
        utils::CreateFileTreeW(LibRoot);
    }

    void TearDown() override {
        utils::DeleteDirectoryW(Root);
    }

    void Write(const std::wstring &path, const std::string &content) {
        ASSERT_TRUE(utils::WriteTextFileUtf8(utils::Utf16ToUtf8(path), content));
    }

    bool TryBuildDirectoryMod(const std::wstring &modName,
                              ScriptSourceSnapshot &snapshot,
                              ScriptDiagnostic &diagnostic,
                              ScriptLibrarySourceCache *sourceCache = nullptr) {
        const std::wstring modRoot = utils::CombinePathW(ModsRoot, modName);
        ScriptModLoadCandidate candidate = MakeDirectoryScriptModCandidate(
            modRoot,
            ScriptModEntrySourceKind::Directory,
            modRoot);
        ScriptModEntry entry = MakeScriptModEntry(candidate, candidate.EntryPaths.empty() ? std::wstring() : candidate.EntryPaths.front());

        ScriptLibraryRegistry registry(LibRoot);
        std::string registryDiagnostic;
        EXPECT_TRUE(registry.Scan(registryDiagnostic)) << registryDiagnostic;
        ScriptSourceSnapshotBuilder builder(std::move(registry));
        builder.SetLibrarySourceCache(sourceCache);
        return builder.Build(entry, snapshot, diagnostic);
    }

    ScriptSourceSnapshot BuildDirectoryMod(const std::wstring &modName) {
        ScriptSourceSnapshot snapshot;
        ScriptDiagnostic diagnostic;
        EXPECT_TRUE(TryBuildDirectoryMod(modName, snapshot, diagnostic)) << diagnostic.Message;
        return snapshot;
    }

    std::wstring Root;
    std::wstring ModsRoot;
    std::wstring LibRoot;
};

bool HasSection(const ScriptSourceSnapshot &snapshot, const std::string &name) {
    return std::any_of(snapshot.Sections.begin(), snapshot.Sections.end(), [&](const ScriptSourceSection &section) {
        return section.Name == name;
    });
}

const ScriptSourceSection *FindSection(const ScriptSourceSnapshot &snapshot, const std::string &name) {
    auto it = std::find_if(snapshot.Sections.begin(), snapshot.Sections.end(), [&](const ScriptSourceSection &section) {
        return section.Name == name;
    });
    return it == snapshot.Sections.end() ? nullptr : &*it;
}

const ScriptSourceDependency *FindDependency(const ScriptSourceSnapshot &snapshot, const std::string &name) {
    auto it = std::find_if(snapshot.Dependencies.begin(), snapshot.Dependencies.end(), [&](const ScriptSourceDependency &dependency) {
        return dependency.VirtualSection == name;
    });
    return it == snapshot.Dependencies.end() ? nullptr : &*it;
}

} // namespace

TEST_F(ScriptSourceSnapshotBuilderTest, DirectoryModKeepsEntryFirstAndLocalHelpers) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"Hello");
    Write(utils::CombinePathW(modRoot, L"Hello.mod.as"), "#include \"runtime.as\"\nclass Hello {}\n");
    Write(utils::CombinePathW(modRoot, L"runtime.as"), "void RuntimeHelper() {}\n");

    ScriptSourceSnapshot snapshot = BuildDirectoryMod(L"Hello");

    ASSERT_GE(snapshot.Sections.size(), 2u);
    EXPECT_EQ("Hello.mod.as", snapshot.EntrySectionName);
    EXPECT_EQ("Hello.mod.as", snapshot.Sections.front().Name);
    EXPECT_TRUE(HasSection(snapshot, "runtime.as"));
    const ScriptSourceDependency *entryDependency = FindDependency(snapshot, "Hello.mod.as");
    ASSERT_NE(nullptr, entryDependency);
    EXPECT_EQ(64u, entryDependency->ContentHash.size());
}

TEST_F(ScriptSourceSnapshotBuilderTest, LibraryIncludeInjectsOnlyDiscoveredClosure) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"User");
    Write(utils::CombinePathW(modRoot, L"User.mod.as"),
          "#include \"/bml/libs/com.example.score@1.2.0/client.as\"\nclass User {}\n");

    const std::wstring packageRoot = utils::CombinePathW(LibRoot, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"client.as"), "#include \"api.as\"\nnamespace ScoreApi { int Add(int x) { return x; } }\n");
    Write(utils::CombinePathW(packageRoot, L"api.as"), "namespace ScoreApi { const int Version = 12; }\n");
    Write(utils::CombinePathW(packageRoot, L"unused.as"), "void Unused() {}\n");

    ScriptSourceSnapshot snapshot = BuildDirectoryMod(L"User");

    EXPECT_TRUE(HasSection(snapshot, "/bml/libs/com.example.score@1.2.0/client.as"));
    EXPECT_TRUE(HasSection(snapshot, "/bml/libs/com.example.score@1.2.0/api.as"));
    EXPECT_FALSE(HasSection(snapshot, "/bml/libs/com.example.score@1.2.0/unused.as"));
    ASSERT_EQ(1u, snapshot.Libraries.size());
    EXPECT_EQ("com.example.score", snapshot.Libraries.front().Id);
    EXPECT_EQ("1.2.0", snapshot.Libraries.front().Version);
    const ScriptSourceDependency *apiDependency =
        FindDependency(snapshot, "/bml/libs/com.example.score@1.2.0/api.as");
    ASSERT_NE(nullptr, apiDependency);
    EXPECT_TRUE(apiDependency->LibraryOwned);
    EXPECT_EQ(64u, apiDependency->ContentHash.size());
}

TEST_F(ScriptSourceSnapshotBuilderTest, SharedLibrarySourceCacheKeepsStableBytesAcrossBuilds) {
    const std::string include = "#include \"/bml/libs/com.example.score@1.2.0/api.as\"\nclass User {}\n";
    const std::wstring firstModRoot = utils::CombinePathW(ModsRoot, L"UserA");
    const std::wstring secondModRoot = utils::CombinePathW(ModsRoot, L"UserB");
    Write(utils::CombinePathW(firstModRoot, L"UserA.mod.as"), include);
    Write(utils::CombinePathW(secondModRoot, L"UserB.mod.as"), include);

    const std::wstring packageRoot = utils::CombinePathW(LibRoot, L"com.example.score\\1.2.0");
    const std::wstring apiPath = utils::CombinePathW(packageRoot, L"api.as");
    Write(apiPath, "namespace ScoreApi { const int Version = 1; }\n");

    ScriptLibraryRegistry registry(LibRoot);
    std::string registryDiagnostic;
    ASSERT_TRUE(registry.Scan(registryDiagnostic)) << registryDiagnostic;
    ScriptLibrarySourceCache sourceCache;
    ScriptDiagnostic diagnostic;
    ASSERT_TRUE(sourceCache.CapturePackage(registry, "com.example.score", "1.2.0", diagnostic)) << diagnostic.Message;
    ASSERT_EQ(1u, sourceCache.GetFileCount());

    Write(apiPath, "namespace ScoreApi { const int Version = 2; }\n");

    ScriptSourceSnapshot firstSnapshot;
    diagnostic = ScriptDiagnostic();
    ASSERT_TRUE(TryBuildDirectoryMod(L"UserA", firstSnapshot, diagnostic, &sourceCache)) << diagnostic.Message;
    ASSERT_EQ(1u, sourceCache.GetFileCount());

    ScriptSourceSnapshot secondSnapshot;
    diagnostic = ScriptDiagnostic();
    ASSERT_TRUE(TryBuildDirectoryMod(L"UserB", secondSnapshot, diagnostic, &sourceCache)) << diagnostic.Message;
    ASSERT_EQ(1u, sourceCache.GetFileCount());

    const ScriptSourceSection *firstApi =
        FindSection(firstSnapshot, "/bml/libs/com.example.score@1.2.0/api.as");
    const ScriptSourceSection *secondApi =
        FindSection(secondSnapshot, "/bml/libs/com.example.score@1.2.0/api.as");
    ASSERT_NE(nullptr, firstApi);
    ASSERT_NE(nullptr, secondApi);
    EXPECT_NE(std::string::npos, firstApi->Code.find("Version = 1"));
    EXPECT_NE(std::string::npos, secondApi->Code.find("Version = 1"));
    EXPECT_EQ(std::string::npos, secondApi->Code.find("Version = 2"));

    const ScriptSourceDependency *firstDependency =
        FindDependency(firstSnapshot, "/bml/libs/com.example.score@1.2.0/api.as");
    const ScriptSourceDependency *secondDependency =
        FindDependency(secondSnapshot, "/bml/libs/com.example.score@1.2.0/api.as");
    ASSERT_NE(nullptr, firstDependency);
    ASSERT_NE(nullptr, secondDependency);
    EXPECT_EQ(64u, firstDependency->ContentHash.size());
    EXPECT_EQ(firstDependency->ContentHash, secondDependency->ContentHash);
}

TEST_F(ScriptSourceSnapshotBuilderTest, IgnoresLibraryIncludesInUnreachableLocalSections) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"User");
    Write(utils::CombinePathW(modRoot, L"User.mod.as"), "class User {}\n");
    Write(utils::CombinePathW(modRoot, L"unused.as"),
          "#include \"/bml/libs/com.example.missing@1.0.0/api.as\"\n");

    ScriptSourceSnapshot snapshot = BuildDirectoryMod(L"User");

    EXPECT_TRUE(HasSection(snapshot, "unused.as"));
    EXPECT_TRUE(snapshot.Libraries.empty());
}

TEST_F(ScriptSourceSnapshotBuilderTest, DiscoversLibraryIncludesThroughReachableLocalHelpers) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"User");
    Write(utils::CombinePathW(modRoot, L"User.mod.as"), "#include \"scripts/helper.as\"\nclass User {}\n");
    Write(utils::CombinePathW(modRoot, L"scripts\\helper.as"),
          "#include \"/bml/libs/com.example.score@1.2.0/api.as\"\n");

    const std::wstring packageRoot = utils::CombinePathW(LibRoot, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"api.as"), "namespace ScoreApi { const int Version = 12; }\n");

    ScriptSourceSnapshot snapshot = BuildDirectoryMod(L"User");

    EXPECT_TRUE(HasSection(snapshot, "/bml/libs/com.example.score@1.2.0/api.as"));
    ASSERT_EQ(1u, snapshot.Libraries.size());
    EXPECT_EQ("com.example.score", snapshot.Libraries.front().Id);
}

TEST_F(ScriptSourceSnapshotBuilderTest, LibraryBmlMetadataIsRejectedButCommentsAndStringsAreIgnored) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"User");
    Write(utils::CombinePathW(modRoot, L"User.mod.as"),
          "#include \"/bml/libs/com.example.bad@1.0.0/api.as\"\nclass User {}\n");

    const std::wstring packageRoot = utils::CombinePathW(LibRoot, L"com.example.bad\\1.0.0");
    Write(utils::CombinePathW(packageRoot, L"api.as"),
          "// [bml.mod(id: \"comment\")]\n"
          "string text = \"[bml.export]\";\n"
          "[bml.export(name: \"Bad\")]\n"
          "void Bad() {}\n");

    ScriptSourceSnapshot snapshot;
    ScriptDiagnostic diagnostic;
    EXPECT_FALSE(TryBuildDirectoryMod(L"User", snapshot, diagnostic));

    EXPECT_EQ(ScriptDiagnosticPhase::Metadata, diagnostic.Phase);
    EXPECT_NE(std::string::npos, diagnostic.Message.find("BML metadata"));
}

TEST_F(ScriptSourceSnapshotBuilderTest, IncludeScannerIgnoresCommentsAndStrings) {
    const std::string code =
        "// #include \"/bml/libs/nope@1.0.0/api.as\"\n"
        "string text = \"#include \\\"/bml/libs/nope@1.0.0/api.as\\\"\";\n"
        "#include \"/bml/libs/yes.lib@1.0.0/api.as\"\n";

    const std::vector<ScriptIncludeDirective> includes =
        ScriptSourceSnapshotBuilder::ScanIncludeDirectives(code);

    ASSERT_EQ(1u, includes.size());
    EXPECT_TRUE(includes.front().Quoted);
    EXPECT_EQ("/bml/libs/yes.lib@1.0.0/api.as", includes.front().Include);
}

TEST_F(ScriptSourceSnapshotBuilderTest, RejectsMacroGeneratedIncludeInReachableModSource) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"User");
    Write(utils::CombinePathW(modRoot, L"User.mod.as"),
          "#include SCORE_HELPER\nclass User {}\n");

    ScriptSourceSnapshot snapshot;
    ScriptDiagnostic diagnostic;
    EXPECT_FALSE(TryBuildDirectoryMod(L"User", snapshot, diagnostic));
    EXPECT_EQ(ScriptDiagnosticPhase::Entry, diagnostic.Phase);
    EXPECT_NE(std::string::npos, diagnostic.Message.find("direct quoted #include"));
}

TEST_F(ScriptSourceSnapshotBuilderTest, RejectsAbsoluteNonLibraryIncludeInReachableModSource) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"User");
    Write(utils::CombinePathW(modRoot, L"User.mod.as"),
          "#include \"/not-a-bml-library/helper.as\"\nclass User {}\n");

    ScriptSourceSnapshot snapshot;
    ScriptDiagnostic diagnostic;
    EXPECT_FALSE(TryBuildDirectoryMod(L"User", snapshot, diagnostic));
    EXPECT_EQ(ScriptDiagnosticPhase::Entry, diagnostic.Phase);
    EXPECT_NE(std::string::npos, diagnostic.Message.find("non-library absolute paths"));
}

TEST_F(ScriptSourceSnapshotBuilderTest, RejectsEscapingIncludeInReachableModSource) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"User");
    Write(utils::CombinePathW(modRoot, L"User.mod.as"),
          "#include \"../outside.as\"\nclass User {}\n");

    ScriptSourceSnapshot snapshot;
    ScriptDiagnostic diagnostic;
    EXPECT_FALSE(TryBuildDirectoryMod(L"User", snapshot, diagnostic));
    EXPECT_EQ(ScriptDiagnosticPhase::Entry, diagnostic.Phase);
    EXPECT_NE(std::string::npos, diagnostic.Message.find("escapes the script package"));
}

TEST_F(ScriptSourceSnapshotBuilderTest, RejectsMacroGeneratedIncludeInsideLibrary) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"User");
    Write(utils::CombinePathW(modRoot, L"User.mod.as"),
          "#include \"/bml/libs/com.example.score@1.2.0/api.as\"\nclass User {}\n");

    const std::wstring packageRoot = utils::CombinePathW(LibRoot, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"api.as"),
          "#include SCORE_HELPER\n"
          "namespace ScoreApi { const int Version = 12; }\n");

    ScriptSourceSnapshot snapshot;
    ScriptDiagnostic diagnostic;
    EXPECT_FALSE(TryBuildDirectoryMod(L"User", snapshot, diagnostic));
    EXPECT_NE(std::string::npos, diagnostic.Message.find("direct quoted #include"));
}

TEST_F(ScriptSourceSnapshotBuilderTest, RejectsAbsoluteNonLibraryIncludeInsideLibrary) {
    const std::wstring modRoot = utils::CombinePathW(ModsRoot, L"User");
    Write(utils::CombinePathW(modRoot, L"User.mod.as"),
          "#include \"/bml/libs/com.example.score@1.2.0/api.as\"\nclass User {}\n");

    const std::wstring packageRoot = utils::CombinePathW(LibRoot, L"com.example.score\\1.2.0");
    Write(utils::CombinePathW(packageRoot, L"api.as"),
          "#include \"/not-a-bml-library/helper.as\"\n"
          "namespace ScoreApi { const int Version = 12; }\n");

    ScriptSourceSnapshot snapshot;
    ScriptDiagnostic diagnostic;
    EXPECT_FALSE(TryBuildDirectoryMod(L"User", snapshot, diagnostic));
    EXPECT_NE(std::string::npos, diagnostic.Message.find("non-library absolute paths"));
}

} // namespace Test
} // namespace BML
