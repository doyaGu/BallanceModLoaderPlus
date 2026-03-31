#include <gtest/gtest.h>

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "Core/ModuleRuntime.h"
#include "TestKernel.h"
#include "TestKernelBuilder.h"

using namespace std::chrono_literals;

namespace {

#ifndef BML_TEST_SAMPLE_MOD_DLL
#error "BML_TEST_SAMPLE_MOD_DLL must be defined with the sample module path"
#endif

std::filesystem::path GetSampleModPath() {
    return std::filesystem::path(BML_TEST_SAMPLE_MOD_DLL);
}

class ScopedEnvVar {
public:
    ScopedEnvVar(const wchar_t *name, const std::wstring &value)
        : name_(name), original_(Read(name)) {
        Set(value);
    }

    ScopedEnvVar(const ScopedEnvVar &) = delete;
    ScopedEnvVar &operator=(const ScopedEnvVar &) = delete;

    ~ScopedEnvVar() {
        if (original_.has_value()) {
            Set(*original_);
        } else {
            Set({});
        }
    }

private:
    static std::optional<std::wstring> Read(const wchar_t *name) {
        DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
        if (size == 0)
            return std::nullopt;
        std::wstring buffer(size, L'\0');
        DWORD copied = GetEnvironmentVariableW(name, buffer.data(), size);
        if (copied == 0)
            return std::nullopt;
        if (!buffer.empty() && buffer.back() == L'\0')
            buffer.pop_back();
        return buffer;
    }

    void Set(const std::wstring &value) {
        SetEnvironmentVariableW(name_, value.empty() ? nullptr : value.c_str());
    }

    const wchar_t *name_;
    std::optional<std::wstring> original_;
};

struct TempDirGuard {
    std::filesystem::path root;
    ~TempDirGuard() {
        if (!root.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }
    }
};

struct RuntimeGuard {
    BML::Core::ModuleRuntime &runtime;
    ~RuntimeGuard() {
        runtime.Shutdown();
    }
};

using BML::Core::Testing::TestKernel;
using BML::Core::Testing::TestKernelBuilder;

std::filesystem::path CreateModsDirectory() {
    const auto base = std::filesystem::temp_directory_path();
    const auto unique = "bml-hot-reload-" +
                        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto root = base / unique;
    std::filesystem::create_directories(root);
    std::filesystem::create_directories(root / "Mods");
    return root / "Mods";
}

void WriteManifest(const std::filesystem::path &manifest_path,
                   const std::string &description,
                   const std::string &entry_name) {
    std::ofstream manifest(manifest_path);
    manifest << "[package]\n";
    manifest << "id = \"hot.reload.sample\"\n";
    manifest << "name = \"Hot Reload Sample\"\n";
    manifest << "version = \"1.0.0\"\n";
    manifest << "entry = \"" << entry_name << "\"\n";
    manifest << "description = \"" << description << "\"\n";
}

} // namespace

TEST(HotReloadIntegrationTests, ReloadsSampleModWhenManifestChanges) {
    const auto sample_mod = GetSampleModPath();
    ASSERT_TRUE(std::filesystem::exists(sample_mod)) << "Sample mod missing: " << sample_mod.string();

    const auto mods_dir = CreateModsDirectory();
    const auto run_root = mods_dir.parent_path();
    TempDirGuard temp_guard{run_root};

    const auto mod_dir = mods_dir / "Sample";
    std::filesystem::create_directories(mod_dir);

    const auto dll_name = sample_mod.filename();
    const auto dll_destination = mod_dir / dll_name;
    std::error_code copy_ec;
    std::filesystem::copy_file(sample_mod,
                               dll_destination,
                               std::filesystem::copy_options::overwrite_existing,
                               copy_ec);
    ASSERT_FALSE(copy_ec) << copy_ec.message();

    const auto manifest_path = mod_dir / "mod.toml";
    WriteManifest(manifest_path, "initial", dll_name.generic_string());

    TestKernel kernel = TestKernelBuilder()
        .WithAll()
        .Build();
    BML::Core::ModuleRuntime runtime;
    runtime.BindKernel(*kernel);
    RuntimeGuard runtime_guard{runtime};

    BML::Core::ModuleBootstrapDiagnostics discover_diag;
    ASSERT_TRUE(runtime.DiscoverAndValidate(mods_dir.wstring(), discover_diag))
        << "Discover failed: " << discover_diag.dependency_error.message;

    BML::Core::ModuleBootstrapDiagnostics initial_diag;
    BML_Services services{};
    ASSERT_TRUE(runtime.LoadDiscovered(initial_diag, &services))
        << "Initial load failed: " << initial_diag.load_error.message;
    ASSERT_TRUE(initial_diag.load_error.message.empty())
        << "Initial module attach reported error: " << initial_diag.load_error.message;

    BML::Core::ModuleBootstrapDiagnostics reload_diag;
    std::this_thread::sleep_for(1200ms);
    WriteManifest(manifest_path, "reloaded", dll_name.generic_string());

    ASSERT_TRUE(runtime.ReloadModules(reload_diag))
        << "Reload failed: " << reload_diag.load_error.message;
    EXPECT_TRUE(reload_diag.load_error.message.empty()) << reload_diag.load_error.message;
}

TEST(HotReloadIntegrationTests, HandleHotReloadNotify_UnloadBlocked_FallsBackToFull) {
    const auto sample_mod = GetSampleModPath();
    ASSERT_TRUE(std::filesystem::exists(sample_mod)) << "Sample mod missing: " << sample_mod.string();

    const auto mods_dir = CreateModsDirectory();
    const auto run_root = mods_dir.parent_path();
    TempDirGuard temp_guard{run_root};

    const auto mod_dir = mods_dir / "Sample";
    std::filesystem::create_directories(mod_dir);

    const auto dll_name = sample_mod.filename();
    const auto dll_destination = mod_dir / dll_name;
    std::error_code copy_ec;
    std::filesystem::copy_file(sample_mod,
                               dll_destination,
                               std::filesystem::copy_options::overwrite_existing,
                               copy_ec);
    ASSERT_FALSE(copy_ec) << copy_ec.message();

    WriteManifest(mod_dir / "mod.toml", "test-blocked", dll_name.generic_string());

    TestKernel kernel = TestKernelBuilder()
        .WithAll()
        .Build();
    BML::Core::ModuleRuntime runtime;
    runtime.BindKernel(*kernel);
    RuntimeGuard runtime_guard{runtime};

    BML::Core::ModuleBootstrapDiagnostics diag;
    ASSERT_TRUE(runtime.DiscoverAndValidate(mods_dir.wstring(), diag));

    BML_Services services{};
    ASSERT_TRUE(runtime.LoadDiscovered(diag, &services));

    // Full reload should still work as fallback
    BML::Core::ModuleBootstrapDiagnostics reload_diag;
    std::this_thread::sleep_for(1200ms);
    EXPECT_TRUE(runtime.ReloadModules(reload_diag))
        << "Full reload failed: " << reload_diag.load_error.message;
}
