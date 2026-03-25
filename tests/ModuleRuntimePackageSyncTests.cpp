#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <zip.h>

#include "Core/ModuleRuntime.h"
#include "PathUtils.h"
#include "TestKernel.h"
#include "TestKernelBuilder.h"
#include "StringUtils.h"

namespace {

namespace fs = std::filesystem;
using BML::Core::Testing::TestKernel;
using BML::Core::Testing::TestKernelBuilder;

class ModuleRuntimePackageSyncFixture : public ::testing::Test {
protected:
    utils::RuntimeLayoutNames runtime_names;
    utils::RuntimeLayout runtime_layout;
    fs::path temp_dir;
    fs::path mods_dir;
    fs::path packages_dir;

    void SetUp() override {
        const auto unique = std::to_wstring(
            std::chrono::steady_clock::now().time_since_epoch().count());
        temp_dir = fs::temp_directory_path() /
            fs::path(L"bml_runtime_package_sync_test_" + unique);
        runtime_layout = utils::ResolveRuntimeLayoutFromRuntimeDirectory(
            temp_dir / runtime_names.runtime_directory, runtime_names);
        mods_dir = runtime_layout.mods_directory;
        packages_dir = runtime_layout.packages_directory;
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
                      std::string_view entry_content = "binary") {
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

        AddArchiveEntry(zip, "mod.toml", manifest_content);
        AddArchiveEntry(zip, "Foo.dll", entry_content);
    }

    void WriteInstalledManifest(const std::wstring &directory_name, std::string_view manifest_content) {
        const fs::path mod_dir = mods_dir / directory_name;
        fs::create_directories(mod_dir);
        std::ofstream ofs(mod_dir / L"mod.toml", std::ios::trunc);
        ofs << manifest_content;
    }
};

TEST_F(ModuleRuntimePackageSyncFixture, DiscoverInstallsPendingPackagesBeforeScanningMods) {
    WritePackage(L"foo.bp", R"TOML(
[package]
id = "com.example.foo"
name = "Foo"
version = "1.0.0"
entry = "Foo.dll"
)TOML");

    TestKernel kernel = TestKernelBuilder()
        .WithAll()
        .Build();
    BML::Core::ModuleRuntime runtime;
    runtime.BindKernel(*kernel);

    BML::Core::ModuleBootstrapDiagnostics diagnostics;
    ASSERT_TRUE(runtime.DiscoverAndValidate(mods_dir.wstring(), diagnostics))
        << diagnostics.dependency_error.message;

    EXPECT_TRUE(fs::exists(mods_dir / L"com.example.foo" / L"mod.toml"));
    EXPECT_TRUE(fs::exists(packages_dir / L".bp-archive"));
    ASSERT_EQ(diagnostics.load_order.size(), 1u);
    EXPECT_EQ(diagnostics.load_order[0], "com.example.foo");

    runtime.Shutdown();
}

TEST_F(ModuleRuntimePackageSyncFixture, DiscoverQuarantinesRejectedPackageAndKeepsInstalledModulesVisible) {
    WriteInstalledManifest(L"InstalledFoo", R"TOML(
[package]
id = "com.example.installed"
name = "Installed"
version = "1.0.0"
)TOML");

    WritePackage(L"broken.bp", R"TOML(
[package]
id = "com.example.broken"
name = "Broken"
version = "1.0.0"
entry = "Missing.dll"
)TOML");

    TestKernel kernel = TestKernelBuilder()
        .WithAll()
        .Build();
    BML::Core::ModuleRuntime runtime;
    runtime.BindKernel(*kernel);

    BML::Core::ModuleBootstrapDiagnostics diagnostics;
    ASSERT_TRUE(runtime.DiscoverAndValidate(mods_dir.wstring(), diagnostics))
        << diagnostics.package_sync_error << diagnostics.dependency_error.message;

    ASSERT_EQ(diagnostics.load_order.size(), 1u);
    EXPECT_EQ(diagnostics.load_order[0], "com.example.installed");
    ASSERT_EQ(diagnostics.package_rejections.size(), 1u);
    EXPECT_EQ(diagnostics.package_rejections[0].stage, "validate");
    EXPECT_TRUE(fs::exists(packages_dir / L".bp-rejected"));
    EXPECT_TRUE(fs::exists(diagnostics.package_rejections[0].rejected_bp_path));
    EXPECT_TRUE(diagnostics.package_sync_error.empty());

    runtime.Shutdown();
}

} // namespace
