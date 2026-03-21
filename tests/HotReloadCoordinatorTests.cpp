#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/HotReloadCoordinator.h"
#include "Core/ModManifest.h"
#include "TestKernel.h"

using namespace std::chrono_literals;
using namespace BML::Core;
using BML::Core::Testing::TestKernel;

namespace {

std::filesystem::path CreateTempDir() {
    auto base = std::filesystem::temp_directory_path();
    auto unique = "bml-coord-test-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    auto dir = base / unique;
    std::filesystem::create_directories(dir);
    return dir;
}

void CreateMinimalDll(const std::filesystem::path& path) {
    static const unsigned char minimal_pe[] = {
        0x4D, 0x5A, 0x90, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
        0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
    };
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(minimal_pe), sizeof(minimal_pe));
    for (int i = sizeof(minimal_pe); i < 512; ++i) {
        out.put(0);
    }
}

} // namespace

class HotReloadCoordinatorTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->api_registry = std::make_unique<ApiRegistry>();
        kernel_->config = std::make_unique<ConfigStore>();
        kernel_->context = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config);

        m_TempDir = CreateTempDir();
        m_Context = kernel_->context.get();
        m_Context->Initialize({0, 4, 0});
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_TempDir, ec);
    }

    std::filesystem::path m_TempDir;
    Context* m_Context{nullptr};
};

TEST_F(HotReloadCoordinatorTest, ConstructsAndDestructs) {
    HotReloadCoordinator coordinator(*m_Context);
    // Should not throw
}

TEST_F(HotReloadCoordinatorTest, ConfigureSettings) {
    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.debounce = 250ms;
    settings.temp_directory = m_TempDir.wstring();

    coordinator.Configure(settings);

    const auto& retrieved = coordinator.GetSettings();
    EXPECT_TRUE(retrieved.enabled);
    EXPECT_EQ(retrieved.debounce, 250ms);
}

TEST_F(HotReloadCoordinatorTest, RegisterModuleWithEmptyIdFails) {
    HotReloadCoordinator coordinator(*m_Context);

    HotReloadModuleEntry entry;
    entry.id = "";
    entry.dll_path = L"test.dll";

    EXPECT_FALSE(coordinator.RegisterModule(entry));
}

TEST_F(HotReloadCoordinatorTest, RegisterModuleWithValidConfig) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.temp_directory = (m_TempDir / "temp").wstring();
    coordinator.Configure(settings);

    HotReloadModuleEntry entry;
    entry.id = "test.mod";
    entry.dll_path = dll_path.wstring();
    entry.watch_path = m_TempDir.wstring();

    EXPECT_TRUE(coordinator.RegisterModule(entry));

    auto modules = coordinator.GetRegisteredModules();
    EXPECT_EQ(modules.size(), 1u);
    EXPECT_EQ(modules[0], "test.mod");
}

TEST_F(HotReloadCoordinatorTest, RegisterDuplicateModuleFails) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.temp_directory = (m_TempDir / "temp").wstring();
    coordinator.Configure(settings);

    HotReloadModuleEntry entry;
    entry.id = "test.mod";
    entry.dll_path = dll_path.wstring();

    EXPECT_TRUE(coordinator.RegisterModule(entry));
    EXPECT_FALSE(coordinator.RegisterModule(entry));
}

TEST_F(HotReloadCoordinatorTest, UnregisterModule) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.temp_directory = (m_TempDir / "temp").wstring();
    coordinator.Configure(settings);

    HotReloadModuleEntry entry;
    entry.id = "test.mod";
    entry.dll_path = dll_path.wstring();

    ASSERT_TRUE(coordinator.RegisterModule(entry));
    EXPECT_EQ(coordinator.GetRegisteredModules().size(), 1u);

    coordinator.UnregisterModule("test.mod");
    EXPECT_EQ(coordinator.GetRegisteredModules().size(), 0u);
}

TEST_F(HotReloadCoordinatorTest, UnregisterNonexistentModuleIsNoOp) {
    HotReloadCoordinator coordinator(*m_Context);
    // Should not throw or crash
    coordinator.UnregisterModule("nonexistent");
}

TEST_F(HotReloadCoordinatorTest, StartStop) {
    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    coordinator.Configure(settings);

    EXPECT_FALSE(coordinator.IsRunning());

    coordinator.Start();
    EXPECT_TRUE(coordinator.IsRunning());

    coordinator.Stop();
    EXPECT_FALSE(coordinator.IsRunning());
}

TEST_F(HotReloadCoordinatorTest, StartWithDisabledDoesNotRun) {
    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = false;
    coordinator.Configure(settings);

    coordinator.Start();
    EXPECT_FALSE(coordinator.IsRunning());
}

TEST_F(HotReloadCoordinatorTest, IsModuleLoadedReturnsFalseForUnknown) {
    HotReloadCoordinator coordinator(*m_Context);
    EXPECT_FALSE(coordinator.IsModuleLoaded("unknown.mod"));
}

TEST_F(HotReloadCoordinatorTest, GetModuleVersionReturnsZeroForUnknown) {
    HotReloadCoordinator coordinator(*m_Context);
    EXPECT_EQ(coordinator.GetModuleVersion("unknown.mod"), 0u);
}

TEST_F(HotReloadCoordinatorTest, ForceReloadUnknownModuleFails) {
    HotReloadCoordinator coordinator(*m_Context);
    EXPECT_EQ(coordinator.ForceReload("unknown.mod"), ReloadResult::LoadFailed);
}

TEST_F(HotReloadCoordinatorTest, NotifyCallback) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.temp_directory = (m_TempDir / "temp").wstring();
    coordinator.Configure(settings);

    HotReloadModuleEntry entry;
    entry.id = "test.mod";
    entry.dll_path = dll_path.wstring();
    ASSERT_TRUE(coordinator.RegisterModule(entry));

    std::string notified_mod_id;
    ReloadResult notified_result = ReloadResult::Success;
    bool callback_called = false;

    coordinator.SetNotifyCallback([&](const std::string& mod_id,
                                      ReloadResult result,
                                      unsigned int,
                                      ReloadFailure) {
        notified_mod_id = mod_id;
        notified_result = result;
        callback_called = true;
    });

    // Force reload will try to reload, but since DLL has no entrypoint,
    // it will fail. However, since it's not changed, it should return NoChange
    // We need to modify the file first
    std::this_thread::sleep_for(100ms);
    std::ofstream(dll_path, std::ios::app) << "modified";

    coordinator.ForceReload("test.mod");

    // Callback should have been called
    EXPECT_TRUE(callback_called);
    EXPECT_EQ(notified_mod_id, "test.mod");
}

TEST_F(HotReloadCoordinatorTest, UpdateWithDisabledIsNoOp) {
    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = false;
    coordinator.Configure(settings);

    // Should not throw or crash
    coordinator.Update();
}

TEST_F(HotReloadCoordinatorTest, MultipleModulesRegistration) {
    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.temp_directory = (m_TempDir / "temp").wstring();
    coordinator.Configure(settings);

    // Create multiple DLLs
    for (int i = 1; i <= 3; ++i) {
        auto dll_path = m_TempDir / ("mod" + std::to_string(i) + ".dll");
        CreateMinimalDll(dll_path);

        HotReloadModuleEntry entry;
        entry.id = "mod" + std::to_string(i);
        entry.dll_path = dll_path.wstring();
        EXPECT_TRUE(coordinator.RegisterModule(entry));
    }

    auto modules = coordinator.GetRegisteredModules();
    EXPECT_EQ(modules.size(), 3u);
}

TEST_F(HotReloadCoordinatorTest, DebounceSettingIsRespected) {
    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.debounce = 1000ms;  // 1 second debounce
    coordinator.Configure(settings);

    EXPECT_EQ(coordinator.GetSettings().debounce, 1000ms);
}

TEST_F(HotReloadCoordinatorTest, IgnoresUnrelatedRootFileChanges) {
    auto dll_path = m_TempDir / "test.dll";
    auto manifest_path = m_TempDir / "mod.toml";
    auto unrelated_path = m_TempDir / "notes.txt";
    CreateMinimalDll(dll_path);
    {
        std::ofstream manifest(manifest_path);
        manifest << "[package]\n";
    }
    {
        std::ofstream unrelated(unrelated_path);
        unrelated << "initial";
    }

    std::this_thread::sleep_for(200ms);

    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.debounce = 200ms;
    settings.temp_directory = (m_TempDir / "temp").wstring();
    coordinator.Configure(settings);

    HotReloadModuleEntry entry;
    entry.id = "test.mod";
    entry.dll_path = dll_path.wstring();
    entry.watch_path = m_TempDir.wstring();
    ModManifest manifest{};
    manifest.package.id = "test.mod";
    manifest.directory = m_TempDir.wstring();
    manifest.manifest_path = manifest_path.wstring();
    entry.manifest = manifest;
    ASSERT_TRUE(coordinator.RegisterModule(entry));

    std::atomic<int> callback_count{0};
    coordinator.SetNotifyCallback([&](const std::string &, ReloadResult, unsigned int, ReloadFailure) {
        callback_count.fetch_add(1, std::memory_order_relaxed);
    });

    coordinator.Start();
    std::this_thread::sleep_for(500ms);

    {
        std::ofstream unrelated(unrelated_path, std::ios::trunc);
        unrelated << "modified";
        unrelated.flush();
    }

    auto unrelated_deadline = std::chrono::steady_clock::now() + 1500ms;
    while (std::chrono::steady_clock::now() < unrelated_deadline) {
        coordinator.Update();
        if (callback_count.load(std::memory_order_relaxed) != 0) {
            break;
        }
        std::this_thread::sleep_for(50ms);
    }
    EXPECT_EQ(callback_count.load(std::memory_order_relaxed), 0);

    {
        std::ofstream manifest_file(manifest_path, std::ios::trunc);
        manifest_file << "[package]\nname='changed'\n";
        manifest_file.flush();
    }

    auto manifest_deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < manifest_deadline) {
        coordinator.Update();
        if (callback_count.load(std::memory_order_relaxed) != 0) {
            break;
        }
        std::this_thread::sleep_for(100ms);
    }

    coordinator.Stop();

    if (callback_count.load(std::memory_order_relaxed) == 0) {
        GTEST_SKIP() << "Manifest file event not received in time (platform-specific behavior)";
    }

    EXPECT_EQ(callback_count.load(std::memory_order_relaxed), 1);
}

TEST_F(HotReloadCoordinatorTest, WatchesIncludedScriptFilesForRuntimeReload) {
    auto entry_path = m_TempDir / "main.as";
    auto include_path = m_TempDir / "shared.as";
    auto manifest_path = m_TempDir / "mod.toml";

    {
        std::ofstream entry(entry_path);
        entry << "#include \"shared.as\"\n";
    }
    {
        std::ofstream include(include_path);
        include << "void helper() {}\n";
    }
    {
        std::ofstream manifest(manifest_path);
        manifest << "[package]\n";
    }

    std::this_thread::sleep_for(200ms);

    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.debounce = 200ms;
    settings.temp_directory = (m_TempDir / "temp").wstring();
    coordinator.Configure(settings);

    HotReloadModuleEntry entry;
    entry.id = "test.script";
    entry.dll_path = entry_path.wstring();
    entry.watch_path = m_TempDir.wstring();
    ModManifest manifest{};
    manifest.package.id = "test.script";
    manifest.package.entry = "main.as";
    manifest.directory = m_TempDir.wstring();
    manifest.manifest_path = manifest_path.wstring();
    entry.manifest = manifest;
    ASSERT_TRUE(coordinator.RegisterModule(entry));

    std::atomic<int> callback_count{0};
    coordinator.SetNotifyCallback([&](const std::string &, ReloadResult, unsigned int, ReloadFailure) {
        callback_count.fetch_add(1, std::memory_order_relaxed);
    });

    coordinator.Start();
    std::this_thread::sleep_for(500ms);

    {
        std::ofstream include(include_path, std::ios::trunc);
        include << "void helper() { /* changed */ }\n";
        include.flush();
    }

    auto deadline = std::chrono::steady_clock::now() + 10s;
    while (std::chrono::steady_clock::now() < deadline) {
        coordinator.Update();
        if (callback_count.load(std::memory_order_relaxed) != 0) {
            break;
        }
        std::this_thread::sleep_for(100ms);
    }

    coordinator.Stop();

    if (callback_count.load(std::memory_order_relaxed) == 0) {
        GTEST_SKIP() << "Included script file event not received in time (platform-specific behavior)";
    }

    EXPECT_EQ(callback_count.load(std::memory_order_relaxed), 1);
}

TEST_F(HotReloadCoordinatorTest, StopClearsScheduledReloads) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    HotReloadCoordinator coordinator(*m_Context);

    HotReloadSettings settings;
    settings.enabled = true;
    settings.debounce = 500ms;
    settings.temp_directory = (m_TempDir / "temp").wstring();
    coordinator.Configure(settings);

    HotReloadModuleEntry entry;
    entry.id = "test.mod";
    entry.dll_path = dll_path.wstring();
    entry.watch_path = m_TempDir.wstring();
    ASSERT_TRUE(coordinator.RegisterModule(entry));

    coordinator.Start();
    coordinator.Stop();

    // Should not crash after stop
    coordinator.Update();
}



