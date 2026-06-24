#include "AngelScript/ScriptReloadStagingCleanup.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "Utils/PathUtils.h"

namespace BML {
namespace Test {

namespace {

std::filesystem::path MakeTempCleanupRoot() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path root = std::filesystem::temp_directory_path() /
                                 ("bml-script-reload-cleanup-" + std::to_string(now));
    std::filesystem::create_directories(root);
    return root;
}

class TempCleanupRoot {
public:
    TempCleanupRoot() : m_Path(MakeTempCleanupRoot()) {}
    ~TempCleanupRoot() { utils::DeleteDirectoryW(m_Path.wstring()); }

    const std::filesystem::path &Path() const { return m_Path; }

private:
    std::filesystem::path m_Path;
};

void TouchFile(const std::filesystem::path &path) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << "x";
}

} // namespace

TEST(ScriptReloadStagingCleanupTest, ParsesOnlyNumericReloadSuffixes) {
    std::wstring base;
    EXPECT_TRUE(TryParseScriptReloadArtifactName(L"Package.reload.12", L".reload.", &base));
    EXPECT_EQ(L"Package", base);

    EXPECT_FALSE(TryParseScriptReloadArtifactName(L".reload.12", L".reload.", &base));
    EXPECT_FALSE(TryParseScriptReloadArtifactName(L"Package.reload.", L".reload.", &base));
    EXPECT_FALSE(TryParseScriptReloadArtifactName(L"Package.reload.x", L".reload.", &base));
    EXPECT_FALSE(TryParseScriptReloadArtifactName(L"Package.reload.12.tmp", L".reload.", &base));
}

TEST(ScriptReloadStagingCleanupTest, RemovesSnapshotArtifactsWithoutSiblingArchive) {
    const TempCleanupRoot root;
    const std::filesystem::path stale = root.Path() / "Hello.snapshot.1";
    const std::filesystem::path nonNumeric = root.Path() / "Hello.snapshot.x";
    const std::filesystem::path normal = root.Path() / "Hello";
    std::filesystem::create_directories(stale);
    std::filesystem::create_directories(nonNumeric);
    std::filesystem::create_directories(normal);

    const ScriptReloadCleanupResult result =
        CleanupStaleScriptReloadDirectoryChildren(root.Path().wstring(), L".snapshot.", false);

    EXPECT_EQ(1u, result.Removed);
    EXPECT_EQ(0u, result.Failed);
    EXPECT_FALSE(std::filesystem::exists(stale));
    EXPECT_TRUE(std::filesystem::exists(nonNumeric));
    EXPECT_TRUE(std::filesystem::exists(normal));
}

TEST(ScriptReloadStagingCleanupTest, ReloadArtifactsRequireSiblingZipWhenRequested) {
    const TempCleanupRoot root;
    const std::filesystem::path stale = root.Path() / "Package.reload.1";
    const std::filesystem::path noArchive = root.Path() / "Loose.reload.2";
    const std::filesystem::path nonNumeric = root.Path() / "Package.reload.x";
    std::filesystem::create_directories(stale);
    std::filesystem::create_directories(noArchive);
    std::filesystem::create_directories(nonNumeric);
    TouchFile(root.Path() / "Package.zip");

    const ScriptReloadCleanupResult result =
        CleanupStaleScriptReloadDirectoryChildren(root.Path().wstring(), L".reload.", true);

    EXPECT_EQ(1u, result.Removed);
    EXPECT_EQ(0u, result.Failed);
    EXPECT_FALSE(std::filesystem::exists(stale));
    EXPECT_TRUE(std::filesystem::exists(noArchive));
    EXPECT_TRUE(std::filesystem::exists(nonNumeric));
}

} // namespace Test
} // namespace BML
