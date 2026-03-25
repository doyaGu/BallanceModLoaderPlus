#include <gtest/gtest.h>

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <zip.h>

#include "Core/PackageInstaller.h"
#include "StringUtils.h"

namespace {

namespace fs = std::filesystem;

class PackageInstallerFixture : public ::testing::Test {
protected:
    fs::path temp_dir;
    fs::path mods_dir;
    fs::path packages_dir;

    void SetUp() override {
        const auto unique = std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count());
        temp_dir = fs::temp_directory_path() /
            fs::path(L"bml_package_installer_test_" + unique);
        mods_dir = temp_dir / L"Mods";
        packages_dir = temp_dir / L"Packages";
        fs::create_directories(mods_dir);
        fs::create_directories(packages_dir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(temp_dir, ec);
    }

    static void AddArchiveEntry(zip_t *zip, const std::string &entryName, std::string_view content) {
        ASSERT_EQ(zip_entry_open(zip, entryName.c_str()), 0);
        ASSERT_GE(zip_entry_write(zip, content.data(), content.size()), 0);
        ASSERT_EQ(zip_entry_close(zip), 0);
    }

    void WritePackage(const std::wstring &archive_name,
                      std::string_view manifest_content,
                      std::string_view entry_content = "binary",
                      std::string_view entry_name = "Foo.dll") {
        auto archive_path = packages_dir / archive_name;
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

        AddArchiveEntry(zip, "mod.toml", manifest_content);
        if (!entry_name.empty()) {
            AddArchiveEntry(zip, std::string(entry_name), entry_content);
        }
    }

    void WriteArchiveEntries(const std::wstring &archive_name,
                             const std::vector<std::pair<std::string, std::string>> &entries) {
        auto archive_path = packages_dir / archive_name;
        auto archive_utf8 = utils::Utf16ToUtf8(archive_path.wstring());
        zip_t *zip = zip_open(archive_utf8.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
        ASSERT_NE(zip, nullptr);

        struct ZipGuard {
            zip_t *handle;
            ~ZipGuard() {
                if (handle) {
                    zip_close(handle);
                }
            }
        } guard{zip};

        for (const auto &[entryName, content] : entries) {
            AddArchiveEntry(zip, entryName, content);
        }
    }

    std::string ReadTextFile(const fs::path &path) const {
        std::ifstream ifs(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }

    fs::path FindSingleArchivedPackage() const {
        fs::path archive_dir = packages_dir / L".bp-archive";
        for (const auto &entry : fs::directory_iterator(archive_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == L".bp")
                return entry.path();
        }
        return {};
    }
};

TEST_F(PackageInstallerFixture, InstallsPackageIntoModsAndArchivesSource) {
    WritePackage(L"foo.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML");

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics diag;
    std::string error;
    ASSERT_TRUE(installer.SyncPackages(temp_dir.wstring(), diag, error)) << error;

    EXPECT_TRUE(fs::exists(mods_dir / L"com.example.foo" / L"mod.toml"));
    EXPECT_TRUE(fs::exists(mods_dir / L"com.example.foo" / L"Foo.dll"));
    EXPECT_TRUE(fs::exists(packages_dir / L".install-state" / L"packages.json"));
    EXPECT_TRUE(fs::exists(FindSingleArchivedPackage()));
    EXPECT_FALSE(fs::exists(packages_dir / L"foo.bp"));
    EXPECT_TRUE(diag.rejected.empty());
}

TEST_F(PackageInstallerFixture, ArchivesDuplicatePackageWithoutReinstall) {
    WritePackage(L"foo.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML", "first-version");

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics first_diag;
    std::string first_error;
    ASSERT_TRUE(installer.SyncPackages(temp_dir.wstring(), first_diag, first_error)) << first_error;

    WritePackage(L"foo-duplicate.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML", "first-version");

    BML::Core::PackageSyncDiagnostics second_diag;
    std::string second_error;
    ASSERT_TRUE(installer.SyncPackages(temp_dir.wstring(), second_diag, second_error)) << second_error;

    EXPECT_EQ(ReadTextFile(mods_dir / L"com.example.foo" / L"Foo.dll"), "first-version");
    EXPECT_EQ(std::count_if(fs::directory_iterator(packages_dir / L".bp-archive"), fs::directory_iterator(),
                            [](const fs::directory_entry &entry) {
                                return entry.is_regular_file() && entry.path().extension() == L".bp";
                            }),
              2);

    const std::string state = ReadTextFile(packages_dir / L".install-state" / L"packages.json");
    EXPECT_NE(state.find("\"already_installed\""), std::string::npos);
}

TEST_F(PackageInstallerFixture, RejectsPackageWithoutEntryAndWritesSidecarWithoutFailingSync) {
    WritePackage(L"broken.bp", R"TOML(
[package]
id = "com.example.broken"
name = "Broken"
version = "1.0.0"
entry = "Missing.dll"
)TOML", "", "");

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics diag;
    std::string error;
    EXPECT_TRUE(installer.SyncPackages(temp_dir.wstring(), diag, error)) << error;

    ASSERT_EQ(diag.rejected.size(), 1u);
    EXPECT_EQ(diag.rejected[0].stage, "validate");
    EXPECT_TRUE(fs::exists(packages_dir / L".bp-rejected"));
    EXPECT_TRUE(fs::exists(diag.rejected[0].rejected_bp_path));
    EXPECT_TRUE(fs::exists(diag.rejected[0].sidecar_json_path));
    EXPECT_FALSE(fs::exists(mods_dir / L"com.example.broken"));

    const std::string sidecar = ReadTextFile(diag.rejected[0].sidecar_json_path);
    EXPECT_NE(sidecar.find("\"stage\": \"validate\""), std::string::npos);
    EXPECT_NE(sidecar.find("Missing.dll"), std::string::npos);
}

TEST_F(PackageInstallerFixture, UpdatesInstalledPayloadWhenSameVersionHasDifferentContent) {
    WritePackage(L"foo-v1.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML", "first-version");

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics first_diag;
    std::string first_error;
    ASSERT_TRUE(installer.SyncPackages(temp_dir.wstring(), first_diag, first_error)) << first_error;

    WritePackage(L"foo-v1-hotfix.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML", "second-version");

    BML::Core::PackageSyncDiagnostics second_diag;
    std::string second_error;
    ASSERT_TRUE(installer.SyncPackages(temp_dir.wstring(), second_diag, second_error)) << second_error;

    EXPECT_EQ(ReadTextFile(mods_dir / L"com.example.foo" / L"Foo.dll"), "second-version");
    const std::string state = ReadTextFile(packages_dir / L".install-state" / L"packages.json");
    EXPECT_NE(state.find("\"updated\""), std::string::npos);
}

TEST_F(PackageInstallerFixture, RejectsCorruptPackageAndWritesSidecarWithoutFailingSync) {
    {
        std::ofstream ofs(packages_dir / L"corrupt.bp", std::ios::binary | std::ios::trunc);
        ofs << "not-a-zip";
    }

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics diag;
    std::string error;
    EXPECT_TRUE(installer.SyncPackages(temp_dir.wstring(), diag, error)) << error;

    ASSERT_EQ(diag.rejected.size(), 1u);
    EXPECT_EQ(diag.rejected[0].stage, "extract");
    EXPECT_TRUE(fs::exists(diag.rejected[0].rejected_bp_path));
    EXPECT_TRUE(fs::exists(diag.rejected[0].sidecar_json_path));
}

TEST_F(PackageInstallerFixture, RejectsArchiveWithUnsupportedRootLayoutWithoutFailingSync) {
    WriteArchiveEntries(L"nested.bp", {
        {"Wrapper/Inner/mod.toml", R"TOML(
[package]
id = "com.example.nested"
name = "Nested"
version = "1.0.0"
entry = "Foo.dll"
)TOML"},
        {"Wrapper/Inner/Foo.dll", "nested"},
    });

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics diag;
    std::string error;
    EXPECT_TRUE(installer.SyncPackages(temp_dir.wstring(), diag, error)) << error;

    ASSERT_EQ(diag.rejected.size(), 1u);
    EXPECT_EQ(diag.rejected[0].stage, "validate");
    EXPECT_FALSE(fs::exists(mods_dir / L"com.example.nested"));
}

TEST_F(PackageInstallerFixture, CleansStagingDirectoryBeforeProcessingPackages) {
    fs::create_directories(packages_dir / L".staging" / L"leftover");
    std::ofstream orphan(packages_dir / L".staging" / L"leftover" / L"orphan.txt", std::ios::trunc);
    orphan << "stale";
    orphan.close();

    WritePackage(L"foo.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML");

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics diag;
    std::string error;
    ASSERT_TRUE(installer.SyncPackages(temp_dir.wstring(), diag, error)) << error;

    EXPECT_FALSE(fs::exists(packages_dir / L".staging" / L"leftover"));
    EXPECT_TRUE(fs::exists(mods_dir / L"com.example.foo" / L"mod.toml"));
}

TEST_F(PackageInstallerFixture, RollsBackInstalledPayloadWhenStateWriteFails) {
    WritePackage(L"foo-v1.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML", "first-version");

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics first_diag;
    std::string first_error;
    ASSERT_TRUE(installer.SyncPackages(temp_dir.wstring(), first_diag, first_error)) << first_error;

    const fs::path state_path = packages_dir / L".install-state" / L"packages.json";
    const fs::path backup_blocker = packages_dir / L".install-state" / L"packages.json.bak";
    HANDLE backup_handle = ::CreateFileW(
        backup_blocker.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    ASSERT_NE(backup_handle, INVALID_HANDLE_VALUE);

    WritePackage(L"foo-v2.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.1"
entry = "Foo.dll"
)TOML", "second-version");

    BML::Core::PackageSyncDiagnostics second_diag;
    std::string second_error;
    EXPECT_TRUE(installer.SyncPackages(temp_dir.wstring(), second_diag, second_error)) << second_error;

    EXPECT_EQ(ReadTextFile(mods_dir / L"com.example.foo" / L"Foo.dll"), "first-version");
    ASSERT_EQ(second_diag.rejected.size(), 1u);
    EXPECT_EQ(second_diag.rejected[0].stage, "state_write");
    EXPECT_TRUE(fs::exists(second_diag.rejected[0].rejected_bp_path));

    ASSERT_TRUE(::CloseHandle(backup_handle));
}

TEST_F(PackageInstallerFixture, RecoversFromCorruptStateFileAndInstallsPendingPackage) {
    fs::create_directories(packages_dir / L".install-state");
    {
        std::ofstream ofs(packages_dir / L".install-state" / L"packages.json", std::ios::trunc);
        ofs << "{ not valid json";
    }

    WritePackage(L"foo.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML");

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics diag;
    std::string error;
    EXPECT_TRUE(installer.SyncPackages(temp_dir.wstring(), diag, error)) << error;

    EXPECT_TRUE(fs::exists(mods_dir / L"com.example.foo" / L"mod.toml"));
    EXPECT_TRUE(fs::exists(packages_dir / L".install-state" / L"packages.json"));
    EXPECT_TRUE(diag.rejected.empty());

    const std::string state = ReadTextFile(packages_dir / L".install-state" / L"packages.json");
    EXPECT_NE(state.find("\"installed\""), std::string::npos);
}

TEST_F(PackageInstallerFixture, ResetsStructurallyInvalidStateFileAndWarns) {
    fs::create_directories(packages_dir / L".install-state");
    {
        std::ofstream ofs(packages_dir / L".install-state" / L"packages.json", std::ios::trunc);
        ofs << "{\"installed\":[]}";
    }

    WritePackage(L"foo.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML");

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics diag;
    std::string error;
    ASSERT_TRUE(installer.SyncPackages(temp_dir.wstring(), diag, error)) << error;

    EXPECT_TRUE(fs::exists(mods_dir / L"com.example.foo" / L"mod.toml"));
    EXPECT_FALSE(diag.warnings.empty());
    EXPECT_NE(diag.warnings.front().find("corrupt"), std::string::npos);

    bool foundCorrupt = false;
    for (const auto &entry : fs::directory_iterator(packages_dir / L".install-state")) {
        if (entry.path().filename().wstring().find(L"packages.json.corrupt.") == 0) {
            foundCorrupt = true;
            break;
        }
    }
    EXPECT_TRUE(foundCorrupt);
}

TEST_F(PackageInstallerFixture, ResetsStateFileWhenPackageHistoryRecordIsNotAnObject) {
    fs::create_directories(packages_dir / L".install-state");
    {
        std::ofstream ofs(packages_dir / L".install-state" / L"packages.json", std::ios::trunc);
        ofs << R"JSON({"packages":{"deadbeef":[]}})JSON";
    }

    WritePackage(L"foo.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML");

    BML::Core::PackageInstaller installer;
    BML::Core::PackageSyncDiagnostics diag;
    std::string error;
    ASSERT_TRUE(installer.SyncPackages(temp_dir.wstring(), diag, error)) << error;

    EXPECT_TRUE(fs::exists(mods_dir / L"com.example.foo" / L"mod.toml"));
    EXPECT_NE(std::find_if(diag.warnings.begin(), diag.warnings.end(),
                           [](const std::string &warning) {
                               return warning.find("corrupt") != std::string::npos;
                           }),
              diag.warnings.end());
}

} // namespace
