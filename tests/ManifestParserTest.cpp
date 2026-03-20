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

TEST_F(ManifestParserFixture, ParsesProvidesStringShorthand) {
    constexpr auto kManifest = R"TOML(
[package]
id = "provider.mod"
name = "Provider"
version = "1.0.0"

[provides]
"foo.bar" = "1.0.0"
"baz.qux" = "2.3.4"
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;
    ASSERT_EQ(manifest.provides.size(), 2u);

    // TOML table iteration order is alphabetical, so find by interface_id
    auto findIface = [&](const std::string &id) -> const BML::Core::ModProvidedInterface * {
        for (const auto &p : manifest.provides) {
            if (p.interface_id == id) return &p;
        }
        return nullptr;
    };

    auto *fooBar = findIface("foo.bar");
    ASSERT_NE(fooBar, nullptr);
    EXPECT_EQ(fooBar->version, "1.0.0");
    EXPECT_EQ(fooBar->parsed_version.major, 1);
    EXPECT_TRUE(fooBar->description.empty());

    auto *bazQux = findIface("baz.qux");
    ASSERT_NE(bazQux, nullptr);
    EXPECT_EQ(bazQux->version, "2.3.4");
    EXPECT_EQ(bazQux->parsed_version.major, 2);
    EXPECT_EQ(bazQux->parsed_version.minor, 3);
    EXPECT_EQ(bazQux->parsed_version.patch, 4);
}

TEST_F(ManifestParserFixture, ParsesProvidesTableForm) {
    constexpr auto kManifest = R"TOML(
[package]
id = "provider.mod"
name = "Provider"
version = "1.0.0"

[provides]
"foo.bar" = { version = "1.0.0", description = "Foo Bar API" }
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;
    ASSERT_EQ(manifest.provides.size(), 1u);
    EXPECT_EQ(manifest.provides[0].interface_id, "foo.bar");
    EXPECT_EQ(manifest.provides[0].version, "1.0.0");
    EXPECT_EQ(manifest.provides[0].description, "Foo Bar API");
}

TEST_F(ManifestParserFixture, RejectsProvidesTableMissingVersion) {
    constexpr auto kManifest = R"TOML(
[package]
id = "bad.mod"
name = "Bad"
version = "1.0.0"

[provides]
"foo.bar" = { description = "Missing version" }
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    EXPECT_FALSE(parser.ParseFile(path.wstring(), manifest, error));
    EXPECT_NE(error.message.find("version"), std::string::npos);
}

TEST_F(ManifestParserFixture, RejectsProvidesInvalidSemver) {
    constexpr auto kManifest = R"TOML(
[package]
id = "bad.mod"
name = "Bad"
version = "1.0.0"

[provides]
"foo.bar" = "not_a_version"
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    EXPECT_FALSE(parser.ParseFile(path.wstring(), manifest, error));
    EXPECT_NE(error.message.find("invalid version"), std::string::npos);
}

TEST_F(ManifestParserFixture, RejectsProvidesDuplicateInterfaceId) {
    constexpr auto kManifest = R"TOML(
[package]
id = "bad.mod"
name = "Bad"
version = "1.0.0"

[provides]
"foo.bar" = "1.0.0"
"foo.bar" = "2.0.0"
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    // TOML itself rejects duplicate keys as a parse error
    EXPECT_FALSE(parser.ParseFile(path.wstring(), manifest, error));
}

TEST_F(ManifestParserFixture, ParsesRequiresStringShorthand) {
    constexpr auto kManifest = R"TOML(
[package]
id = "consumer.mod"
name = "Consumer"
version = "1.0.0"

[requires]
"bml.input.capture" = ">=1.0.0"
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;
    ASSERT_EQ(manifest.requires_.size(), 1u);
    EXPECT_EQ(manifest.requires_[0].interface_id, "bml.input.capture");
    EXPECT_TRUE(manifest.requires_[0].requirement.parsed);
    EXPECT_FALSE(manifest.requires_[0].optional);
}

TEST_F(ManifestParserFixture, ParsesRequiresTableFormWithOptional) {
    constexpr auto kManifest = R"TOML(
[package]
id = "consumer.mod"
name = "Consumer"
version = "1.0.0"

[requires]
"bml.input.capture" = { version = ">=1.0.0", optional = true }
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;
    ASSERT_EQ(manifest.requires_.size(), 1u);
    EXPECT_EQ(manifest.requires_[0].interface_id, "bml.input.capture");
    EXPECT_TRUE(manifest.requires_[0].optional);
}

TEST_F(ManifestParserFixture, RejectsRequiresTableMissingVersion) {
    constexpr auto kManifest = R"TOML(
[package]
id = "bad.mod"
name = "Bad"
version = "1.0.0"

[requires]
"foo.bar" = { optional = true }
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    EXPECT_FALSE(parser.ParseFile(path.wstring(), manifest, error));
    EXPECT_NE(error.message.find("version"), std::string::npos);
}

TEST_F(ManifestParserFixture, NoRequiresIsOk) {
    constexpr auto kManifest = R"TOML(
[package]
id = "consumer.mod"
name = "Consumer"
version = "1.0.0"
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;
    EXPECT_TRUE(manifest.requires_.empty());
}

TEST_F(ManifestParserFixture, NoProvidesIsOk) {
    constexpr auto kManifest = R"TOML(
[package]
id = "consumer.mod"
name = "Consumer"
version = "1.0.0"
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;
    EXPECT_TRUE(manifest.provides.empty());
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

TEST_F(ManifestParserFixture, RequiresEmptyVersionShowsInterfaceContext) {
    constexpr auto kManifest = R"TOML(
[package]
id = "bad.mod"
name = "Bad"
version = "1.0.0"

[requires]
"foo" = ""
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    EXPECT_FALSE(parser.ParseFile(path.wstring(), manifest, error));
    EXPECT_NE(error.message.find("foo"), std::string::npos)
        << "Error should mention the interface id 'foo': " << error.message;
    EXPECT_NE(error.message.find("[requires]"), std::string::npos)
        << "Error should mention [requires]: " << error.message;
}

TEST_F(ManifestParserFixture, ParsesCustomFields) {
    constexpr auto kManifest = R"TOML(
[package]
id = "custom.mod"
name = "Custom"
version = "1.0.0"

[mymod]
greeting = "Hello World"
max_speed = 42
scale = 1.5
debug = true

[mymod.nested]
deep_value = "found it"
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;

    // String field
    auto it = manifest.custom_fields.find("mymod.greeting");
    ASSERT_NE(it, manifest.custom_fields.end());
    ASSERT_TRUE(std::holds_alternative<std::string>(it->second));
    EXPECT_EQ(std::get<std::string>(it->second), "Hello World");

    // Integer field
    it = manifest.custom_fields.find("mymod.max_speed");
    ASSERT_NE(it, manifest.custom_fields.end());
    ASSERT_TRUE(std::holds_alternative<int64_t>(it->second));
    EXPECT_EQ(std::get<int64_t>(it->second), 42);

    // Float field
    it = manifest.custom_fields.find("mymod.scale");
    ASSERT_NE(it, manifest.custom_fields.end());
    ASSERT_TRUE(std::holds_alternative<double>(it->second));
    EXPECT_NEAR(std::get<double>(it->second), 1.5, 1e-9);

    // Bool field
    it = manifest.custom_fields.find("mymod.debug");
    ASSERT_NE(it, manifest.custom_fields.end());
    ASSERT_TRUE(std::holds_alternative<bool>(it->second));
    EXPECT_EQ(std::get<bool>(it->second), true);

    // Nested table field
    it = manifest.custom_fields.find("mymod.nested.deep_value");
    ASSERT_NE(it, manifest.custom_fields.end());
    ASSERT_TRUE(std::holds_alternative<std::string>(it->second));
    EXPECT_EQ(std::get<std::string>(it->second), "found it");
}

TEST_F(ManifestParserFixture, CustomFieldsIgnoreKnownSections) {
    constexpr auto kManifest = R"TOML(
[package]
id = "custom.mod"
name = "Custom"
version = "1.0.0"

[dependencies]
"other.mod" = ">=1.0"

[assets]
mount = "assets"

[extra]
key = "value"
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;

    // "dependencies" and "assets" should NOT appear in custom_fields
    EXPECT_EQ(manifest.custom_fields.count("dependencies.other.mod"), 0u);
    EXPECT_EQ(manifest.custom_fields.count("assets.mount"), 0u);

    // "extra" SHOULD appear
    auto it = manifest.custom_fields.find("extra.key");
    ASSERT_NE(it, manifest.custom_fields.end());
    EXPECT_EQ(std::get<std::string>(it->second), "value");
}

TEST_F(ManifestParserFixture, NoCustomFieldsIsOk) {
    constexpr auto kManifest = R"TOML(
[package]
id = "plain.mod"
name = "Plain"
version = "1.0.0"
)TOML";

    auto path = WriteManifest(kManifest);
    BML::Core::ManifestParser parser;
    BML::Core::ModManifest manifest;
    BML::Core::ManifestParseError error;

    ASSERT_TRUE(parser.ParseFile(path.wstring(), manifest, error)) << error.message;
    EXPECT_TRUE(manifest.custom_fields.empty());
}

} // namespace
