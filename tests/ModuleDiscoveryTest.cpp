#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <chrono>
#include <set>
#include <string>

#include "Core/ModuleDiscovery.h"

namespace {

class ModuleDiscoveryFixture : public ::testing::Test {
protected:
    std::filesystem::path temp_dir;

    void SetUp() override {
        const auto unique = std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count());
        temp_dir = std::filesystem::temp_directory_path() /
            std::filesystem::path(L"bml_module_discovery_test_" + unique);
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

TEST_F(ModuleDiscoveryFixture, IgnoresRegularFilesWithoutManifest) {
    std::ofstream ofs(temp_dir / L"readme.txt", std::ios::trunc);
    ofs << "not a module";

    BML::Core::ManifestLoadResult result;
    ASSERT_TRUE(BML::Core::LoadManifestsFromDirectory(temp_dir.wstring(), result));
    EXPECT_TRUE(result.manifests.empty());
    EXPECT_TRUE(result.errors.empty());
}

TEST_F(ModuleDiscoveryFixture, IgnoresDirectoriesWithoutManifest) {
    std::filesystem::create_directories(temp_dir / L"EmptyModule");

    BML::Core::ManifestLoadResult result;
    ASSERT_TRUE(BML::Core::LoadManifestsFromDirectory(temp_dir.wstring(), result));
    EXPECT_TRUE(result.manifests.empty());
    EXPECT_TRUE(result.errors.empty());
}

} // namespace
