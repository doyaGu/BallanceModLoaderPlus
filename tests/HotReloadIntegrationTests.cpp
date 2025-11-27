#include <gtest/gtest.h>

#include <Windows.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "Core/Context.h"
#include "Core/ModuleRuntime.h"

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

struct ContextGuard {
    BML::Core::Context &context;
    ~ContextGuard() {
        context.Cleanup();
    }
};

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

std::vector<std::string> ReadLogLines(const std::filesystem::path &log_path) {
    std::vector<std::string> lines;
    std::ifstream file(log_path);
    if (!file.is_open())
        return lines;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

bool ContainsOrdered(const std::vector<std::string> &lines, const std::vector<std::string> &sequence) {
    size_t index = 0;
    for (const auto &line : lines) {
        if (index < sequence.size() && line == sequence[index]) {
            ++index;
        }
    }
    return index == sequence.size();
}

bool WaitForLogSequence(const std::filesystem::path &log_path,
                        const std::vector<std::string> &sequence,
                        std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (ContainsOrdered(ReadLogLines(log_path), sequence))
            return true;
        std::this_thread::sleep_for(50ms);
    }
    return false;
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

    const auto log_path = run_root / "sample-log.txt";
    ScopedEnvVar hot_reload_env(L"BML_HOT_RELOAD", L"1");
    ScopedEnvVar log_env(L"BML_TEST_HOT_RELOAD_LOG", log_path.wstring());

    auto &context = BML::Core::Context::Instance();
    context.Initialize({0, 4, 0});
    ContextGuard context_guard{context};

    BML::Core::ModuleRuntime runtime;
    RuntimeGuard runtime_guard{runtime};

    BML::Core::ModuleBootstrapDiagnostics initial_diag;
    ASSERT_TRUE(runtime.Initialize(mods_dir.wstring(), initial_diag))
        << "Initial load failed: " << initial_diag.load_error.message;

    ASSERT_TRUE(WaitForLogSequence(log_path, {"init:1"}, 5s))
        << "Initial init entry missing";

    std::mutex diag_mutex;
    std::condition_variable diag_cv;
    BML::Core::ModuleBootstrapDiagnostics reload_diag;
    bool reload_received = false;

    runtime.SetDiagnosticsCallback([&](const BML::Core::ModuleBootstrapDiagnostics &diag) {
        std::lock_guard<std::mutex> lock(diag_mutex);
        reload_diag = diag;
        reload_received = true;
        diag_cv.notify_all();
    });

    std::this_thread::sleep_for(1200ms);
    WriteManifest(manifest_path, "reloaded", dll_name.generic_string());

    bool notified = false;
    {
        std::unique_lock<std::mutex> lock(diag_mutex);
        notified = diag_cv.wait_for(lock, 10s, [&] { return reload_received; });
    }
    ASSERT_TRUE(notified) << "Timed out waiting for reload diagnostics";
    EXPECT_TRUE(reload_diag.load_error.message.empty()) << reload_diag.load_error.message;

    EXPECT_TRUE(WaitForLogSequence(log_path, {"init:1", "shutdown:1", "init:2"}, 5s))
        << "Lifecycle log does not contain expected reload sequence";
}
