#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <zip.h>

#include "Core/ModuleDiscovery.h"
#include "StringUtils.h"

namespace {

class ModuleDiscoveryFixture : public ::testing::Test {
protected:
    std::filesystem::path temp_dir;

    void SetUp() override {
        temp_dir = std::filesystem::temp_directory_path() / std::filesystem::path("bml_module_discovery_test");
        std::filesystem::create_directories(temp_dir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    void WriteManifest(const std::wstring &mod_name, std::string_view content) {
        auto mod_dir = temp_dir / mod_name;
        std::filesystem::create_directories(mod_dir);
        std::ofstream ofs(mod_dir / L"mod.toml", std::ios::trunc);
        ofs << content;
    }

    void WriteArchive(const std::wstring &archive_name, std::string_view content, std::string_view root = {}) {
        auto archive_path = temp_dir / archive_name;
        auto archive_utf8 = utils::Utf16ToUtf8(archive_path.wstring());
        zip_t *zip = zip_open(archive_utf8.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
        ASSERT_NE(zip, nullptr);
        struct ZipGuard {
            zip_t *handle;
            ~ZipGuard() {
                if (handle)
                    zip_close(handle);
            }
        } guard{zip};

        std::string entry_name = root.empty() ? std::string("mod.toml") : std::string(root) + "/mod.toml";
        ASSERT_EQ(zip_entry_open(zip, entry_name.c_str()), 0);
        ASSERT_GE(zip_entry_write(zip, content.data(), content.size()), 0);
        ASSERT_EQ(zip_entry_close(zip), 0);
    }
};

TEST_F(ModuleDiscoveryFixture, DiscoversMultipleMods) {
    WriteManifest(L"Alpha", R"TOML(
[package]
id = "alpha"
name = "Alpha"
version = "1.0.0"
)TOML");

    WriteManifest(L"Beta", R"TOML(
[package]
id = "beta"
name = "Beta"
version = "2.0.0"
)TOML");

    BML::Core::ManifestLoadResult result;
    ASSERT_TRUE(BML::Core::LoadManifestsFromDirectory(temp_dir.wstring(), result));
    ASSERT_EQ(result.manifests.size(), 2u);
    std::set<std::string> ids;
    for (const auto &manifest : result.manifests) {
        ASSERT_NE(manifest, nullptr);
        ids.insert(manifest->package.id);
        EXPECT_FALSE(manifest->directory.empty());
    }
    EXPECT_EQ(ids.count("alpha"), 1u);
    EXPECT_EQ(ids.count("beta"), 1u);
}

TEST_F(ModuleDiscoveryFixture, ReportsParseErrors) {
    WriteManifest(L"Good", R"TOML(
[package]
id = "good"
name = "Good"
version = "1.0.0"
)TOML");

    WriteManifest(L"Broken", R"TOML(
[package]
name = "Broken"
version = "1.0.0"
)TOML");

    BML::Core::ManifestLoadResult result;
    EXPECT_FALSE(BML::Core::LoadManifestsFromDirectory(temp_dir.wstring(), result));
    ASSERT_EQ(result.errors.size(), 1u);
    EXPECT_NE(result.errors[0].message.find("id must be"), std::string::npos);
    EXPECT_EQ(result.manifests.size(), 1u);
}

TEST_F(ModuleDiscoveryFixture, BuildLoadOrderResolvesDependencies) {
    WriteManifest(L"Base", R"TOML(
[package]
id = "base"
name = "Base"
version = "1.0.0"
)TOML");

    WriteManifest(L"Addon", R"TOML(
[package]
id = "addon"
name = "Addon"
version = "1.0.0"

[dependencies]
base = "^1.0"
)TOML");

    BML::Core::ManifestLoadResult result;
    ASSERT_TRUE(BML::Core::LoadManifestsFromDirectory(temp_dir.wstring(), result));

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    ASSERT_TRUE(BML::Core::BuildLoadOrder(result, order, warnings, error)) << error.message;
    ASSERT_EQ(order.size(), 2u);

    auto find_index = [&](const std::string &id) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (order[i].id == id)
                return i;
        }
        return order.size();
    };

    EXPECT_LT(find_index("base"), find_index("addon"));
}

TEST_F(ModuleDiscoveryFixture, DiscoversBpArchives) {
    WriteArchive(L"Packed.bp", R"TOML(
[package]
id = "packed"
name = "Packed"
version = "1.0.0"
)TOML");

    BML::Core::ManifestLoadResult result;
    ASSERT_TRUE(BML::Core::LoadManifestsFromDirectory(temp_dir.wstring(), result));
    ASSERT_EQ(result.manifests.size(), 1u);
    EXPECT_EQ(result.manifests[0]->package.id, "packed");
}

TEST_F(ModuleDiscoveryFixture, ResolvesNestedFolderInsideArchive) {
    WriteArchive(L"Nested.bp", R"TOML(
[package]
id = "nested"
name = "Nested"
version = "1.0.0"
)TOML", "NestedMod");

    BML::Core::ManifestLoadResult result;
    ASSERT_TRUE(BML::Core::LoadManifestsFromDirectory(temp_dir.wstring(), result));
    ASSERT_EQ(result.manifests.size(), 1u);
    EXPECT_EQ(result.manifests[0]->package.id, "nested");
}

TEST_F(ModuleDiscoveryFixture, ReportsExtractionErrorsForInvalidBp) {
    std::ofstream ofs(temp_dir / L"Broken.bp", std::ios::binary);
    ofs << "notzip";

    BML::Core::ManifestLoadResult result;
    EXPECT_FALSE(BML::Core::LoadManifestsFromDirectory(temp_dir.wstring(), result));
    ASSERT_FALSE(result.errors.empty());
}

} // namespace
