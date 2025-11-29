#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>

#include "Core/HotReloadCoordinator.h"
#include "Core/Context.h"
#include "Core/ModManifest.h"

using namespace std::chrono_literals;
using namespace BML::Core;

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
    void SetUp() override {
        m_TempDir = CreateTempDir();
        m_Context = &Context::Instance();
        m_Context->Initialize({0, 4, 0});
    }

    void TearDown() override {
        m_Context->Cleanup();
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
