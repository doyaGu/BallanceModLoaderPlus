#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include <windows.h>

#include "MapCatalog.h"

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

} // namespace
