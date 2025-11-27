#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/ModManifest.h"

namespace {

class ManifestParserFixture : public ::testing::Test {
protected:
    std::filesystem::path temp_dir;

    void SetUp() override {
        temp_dir = std::filesystem::temp_directory_path() / std::filesystem::path("bml_manifest_parser_test");
        std::filesystem::create_directories(temp_dir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }

    std::filesystem::path WriteManifest(std::string_view content) {
        auto path = temp_dir / "mod.toml";
        std::ofstream ofs(path, std::ios::trunc);
        ofs << content;
        ofs.close();
        return path;
    }
};

TEST_F(ManifestParserFixture, ParsesValidManifest) {
    constexpr auto kManifest = R"TOML(
capabilities = ["imc", "logging"]

[package]
id = "example.mod"
name = "Example Mod"
version = "1.2.3"
entry = "Example.dll"
description = "Sample"
authors = ["Alice", "Bob"]

[dependencies]
core = "^1.0"
"optional.mod" = { version = ">=2.0", optional = true }
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;
    EXPECT_EQ(manifest.package.id, "example.mod");
    EXPECT_EQ(manifest.package.name, "Example Mod");
    EXPECT_EQ(manifest.package.version, "1.2.3");
    EXPECT_EQ(manifest.package.entry, "Example.dll");
    ASSERT_EQ(manifest.package.authors.size(), 2u);
    EXPECT_EQ(manifest.package.authors[0], "Alice");
    EXPECT_EQ(manifest.package.authors[1], "Bob");

    ASSERT_EQ(manifest.dependencies.size(), 2u);
    EXPECT_EQ(manifest.dependencies[0].id, "core");
    EXPECT_FALSE(manifest.dependencies[0].optional);
    EXPECT_TRUE(manifest.dependencies[0].requirement.parsed);
    EXPECT_EQ(manifest.dependencies[1].id, "optional.mod");
    EXPECT_TRUE(manifest.dependencies[1].optional);

    ASSERT_EQ(manifest.capabilities.size(), 2u);
    EXPECT_EQ(manifest.capabilities[0], "imc");
    EXPECT_EQ(manifest.capabilities[1], "logging");

    EXPECT_EQ(std::filesystem::path(manifest.manifest_path).filename(), L"mod.toml");
    EXPECT_EQ(std::filesystem::path(manifest.directory), temp_dir);
}

TEST_F(ManifestParserFixture, FailsWithoutPackageTable) {
    auto path = WriteManifest("[not_package]\nid=\"broken\"");
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    EXPECT_FALSE(parser.ParseFile(path.wstring(), manifest, error));
    EXPECT_NE(error.message.find("Missing [package]"), std::string::npos);
    ASSERT_TRUE(error.file.has_value());
}

TEST_F(ManifestParserFixture, RejectsInvalidDependencyShape) {
    constexpr auto kManifest = R"TOML(
[package]
id = "bad.deps"
name = "Broken"
version = "0.1.0"

[dependencies]
weird = 42
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    EXPECT_FALSE(parser.ParseFile(path.wstring(), manifest, error));
    EXPECT_NE(error.message.find("Dependency 'weird'"), std::string::npos);
}

} // namespace
