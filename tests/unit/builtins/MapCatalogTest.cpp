#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <windows.h>

#include "CustomMaps/MapCatalog.h"

namespace {

static_assert(!std::is_copy_constructible_v<MapEntry>);
static_assert(!std::is_copy_assignable_v<MapEntry>);
static_assert(!std::is_move_constructible_v<MapEntry>);
static_assert(!std::is_move_assignable_v<MapEntry>);

class TemporaryMapDirectory {
public:
    TemporaryMapDirectory() {
        m_Path = std::filesystem::temp_directory_path() /
                 (L"bml-map-catalog-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                  std::to_wstring(GetTickCount64()));
        std::filesystem::create_directories(m_Path);
    }

    ~TemporaryMapDirectory() {
        std::error_code error;
        std::filesystem::remove_all(m_Path, error);
    }

    const std::filesystem::path &Path() const { return m_Path; }

private:
    std::filesystem::path m_Path;
};

TEST(MapCatalogTest, EmptyRefreshReplacesPreviouslyPopulatedSnapshot) {
    TemporaryMapDirectory maps;
    const auto mapPath = maps.Path() / L"example.nmo";
    std::ofstream(mapPath, std::ios::binary).put('\0');

    MapCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(maps.Path().wstring(), 8, nullptr));
    ASSERT_EQ(catalog.GetRoot()->children.size(), 1u);

    ASSERT_TRUE(std::filesystem::remove(mapPath));
    EXPECT_TRUE(catalog.Refresh(maps.Path().wstring(), 8, nullptr));
    EXPECT_TRUE(catalog.GetRoot()->children.empty());
}

TEST(MapCatalogTest, MissingDirectoryProducesAnEmptySnapshot) {
    TemporaryMapDirectory maps;
    std::ofstream(maps.Path() / L"example.cmo", std::ios::binary).put('\0');

    MapCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(maps.Path().wstring(), 8, nullptr));
    ASSERT_FALSE(catalog.GetRoot()->children.empty());

    const std::filesystem::path missing = maps.Path() / L"missing";
    EXPECT_TRUE(catalog.Refresh(missing.wstring(), 8, nullptr));
    EXPECT_TRUE(catalog.GetRoot()->children.empty());
    EXPECT_EQ(catalog.GetRoot()->path, missing.wstring());
}

TEST(MapCatalogTest, RefreshAppliesChangedMaximumDepth) {
    TemporaryMapDirectory maps;
    const auto nested = maps.Path() / L"nested";
    std::filesystem::create_directory(nested);
    std::ofstream(nested / L"deep.nmo", std::ios::binary).put('\0');

    MapCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(maps.Path().wstring(), 1, nullptr));
    ASSERT_EQ(catalog.GetRoot()->children.size(), 1u);
    ASSERT_EQ(catalog.GetRoot()->children.front()->type, MAP_ENTRY_DIR);
    EXPECT_TRUE(catalog.GetRoot()->children.front()->children.empty());

    ASSERT_TRUE(catalog.Refresh(maps.Path().wstring(), 2, nullptr));
    ASSERT_EQ(catalog.GetRoot()->children.size(), 1u);
    ASSERT_EQ(catalog.GetRoot()->children.front()->children.size(), 1u);
    EXPECT_EQ(catalog.GetRoot()->children.front()->children.front()->name, "deep");
}

TEST(MapCatalogTest, FailedRefreshPreservesTheLastCompleteSnapshot) {
    TemporaryMapDirectory maps;
    std::ofstream(maps.Path() / L"example.nmo", std::ios::binary).put('\0');

    MapCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(maps.Path().wstring(), 8, nullptr));
    ASSERT_EQ(catalog.GetRoot()->children.size(), 1u);
    const std::wstring previousPath = catalog.GetRoot()->path;

    const auto regularFile = maps.Path() / L"not-a-directory";
    std::ofstream(regularFile, std::ios::binary).put('\0');
    EXPECT_FALSE(catalog.Refresh(regularFile.wstring(), 8, nullptr));
    EXPECT_EQ(catalog.GetRoot()->path, previousPath);
    EXPECT_EQ(catalog.GetRoot()->children.size(), 1u);
}

TEST(MapCatalogTest, ResolvesNestedUtf8MapPathWithSpaces) {
    TemporaryMapDirectory maps;
    const auto nested = maps.Path() / L"sub folder";
    std::filesystem::create_directory(nested);
    const auto mapPath = nested / L"\u5730\u56FE.cmo";
    std::ofstream(mapPath, std::ios::binary).put('\0');

    std::wstring resolved;
    std::string error;
    EXPECT_TRUE(MapCatalog::ResolveFile(maps.Path().wstring(),
                                        "sub folder/\xE5\x9C\xB0\xE5\x9B\xBE.cmo",
                                        resolved, error)) << error;
    EXPECT_EQ(resolved, std::filesystem::canonical(mapPath).wstring());
}

TEST(MapCatalogTest, RejectsPathsOutsideMapsAndUnsupportedFiles) {
    TemporaryMapDirectory maps;
    const auto outside = std::filesystem::path(maps.Path().wstring() + L"-outside.nmo");
    std::ofstream(maps.Path() / L"notes.txt", std::ios::binary).put('\0');
    std::ofstream(outside, std::ios::binary).put('\0');

    std::wstring resolved;
    std::string error;
    EXPECT_FALSE(MapCatalog::ResolveFile(maps.Path().wstring(), "../outside.nmo", resolved, error));
    EXPECT_TRUE(resolved.empty());
    EXPECT_FALSE(MapCatalog::ResolveFile(maps.Path().wstring(), "notes.txt", resolved, error));
    EXPECT_FALSE(MapCatalog::ResolveFile(maps.Path().wstring(), "missing.nmo", resolved, error));
    EXPECT_FALSE(MapCatalog::ResolveFile(maps.Path().wstring(), "C:\\outside.nmo", resolved, error));
    EXPECT_FALSE(MapCatalog::ResolveFile(maps.Path().wstring(), "", resolved, error));
    EXPECT_FALSE(MapCatalog::ResolveFile(maps.Path().wstring(), "\xFF.nmo", resolved, error));
    EXPECT_FALSE(MapCatalog::ValidateFile(maps.Path().wstring(), outside.wstring(), resolved, error));
    std::filesystem::remove(outside);
}

TEST(MapCatalogTest, RejectsMapSymlinkEscapingDirectory) {
    TemporaryMapDirectory maps;
    const auto outside = std::filesystem::path(maps.Path().wstring() + L"-outside-link-target.nmo");
    std::ofstream(outside, std::ios::binary).put('\0');
    std::error_code linkError;
    std::filesystem::create_symlink(outside, maps.Path() / L"linked.nmo", linkError);
    if (linkError) {
        std::filesystem::remove(outside);
        GTEST_SKIP() << "symlinks are unavailable: " << linkError.message();
    }

    std::wstring resolved;
    std::string error;
    EXPECT_FALSE(MapCatalog::ResolveFile(maps.Path().wstring(), "linked.nmo", resolved, error));
    EXPECT_FALSE(MapCatalog::ValidateFile(maps.Path().wstring(),
                                          (maps.Path() / L"linked.nmo").wstring(), resolved, error));
    EXPECT_TRUE(resolved.empty());

    MapCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(maps.Path().wstring(), 8, nullptr));
    EXPECT_TRUE(catalog.GetRoot()->children.empty());
    std::filesystem::remove(outside);
}

TEST(MapCatalogTest, ListsOnlyLoadableRelativePathsAtConfiguredDepth) {
    TemporaryMapDirectory maps;
    const auto nested = maps.Path() / L"folder";
    const auto deeper = nested / L"deeper";
    std::filesystem::create_directories(deeper);
    std::ofstream(maps.Path() / L"first.nmo", std::ios::binary).put('\0');
    std::ofstream(nested / L"second.cmo", std::ios::binary).put('\0');
    std::ofstream(deeper / L"third.nmo", std::ios::binary).put('\0');
    std::ofstream(nested / L"notes.txt", std::ios::binary).put('\0');

    MapCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(maps.Path().wstring(), 2, nullptr));
    EXPECT_EQ(catalog.ListFiles({}, 10),
              (std::vector<std::string>{"folder\\second.cmo", "first.nmo"}));
    EXPECT_EQ(catalog.ListFiles("SECOND", 10),
              (std::vector<std::string>{"folder\\second.cmo"}));
    EXPECT_EQ(catalog.ListFiles({}, 1), (std::vector<std::string>{"folder\\second.cmo"}));
}

TEST(MapCatalogTest, ListFilterFoldsUtf8Case) {
    TemporaryMapDirectory maps;
    std::ofstream(maps.Path() / L"\u00c4pfel.nmo", std::ios::binary).put('\0');

    MapCatalog catalog;
    ASSERT_TRUE(catalog.Refresh(maps.Path().wstring(), 1, nullptr));
    EXPECT_EQ(catalog.ListFiles("\xc3\xa4PFEL", 10),
              (std::vector<std::string>{"\xc3\x84pfel.nmo"}));
}

} // namespace
