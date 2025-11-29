#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "Core/ReloadableModuleSlot.h"
#include "Core/Context.h"

using namespace std::chrono_literals;
using namespace BML::Core;

namespace {

std::filesystem::path CreateTempDir() {
    auto base = std::filesystem::temp_directory_path();
    auto unique = "bml-slot-test-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    auto dir = base / unique;
    std::filesystem::create_directories(dir);
    return dir;
}

// Create a minimal valid PE DLL file
void CreateMinimalDll(const std::filesystem::path& path) {
    // This is a minimal DOS/PE stub that Windows will recognize as a DLL
    // but it won't have any exports
    static const unsigned char minimal_pe[] = {
        // DOS Header
        0x4D, 0x5A, // MZ signature
        0x90, 0x00, // bytes on last page
        0x03, 0x00, // pages in file
        0x00, 0x00, // relocations
        0x04, 0x00, // size of header in paragraphs
        0x00, 0x00, // min extra paragraphs
        0xFF, 0xFF, // max extra paragraphs
        0x00, 0x00, // initial SS
        0xB8, 0x00, // initial SP
        0x00, 0x00, // checksum
        0x00, 0x00, // initial IP
        0x00, 0x00, // initial CS
        0x40, 0x00, // file address of relocation table
        0x00, 0x00, // overlay number
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // reserved
        0x00, 0x00, // OEM id
        0x00, 0x00, // OEM info
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // reserved
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, // reserved
        0x80, 0x00, 0x00, 0x00, // PE header offset

        // DOS stub
        0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
        0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
        0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72,
        0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
        0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E,
        0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
        0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A,
        0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        // PE signature
        0x50, 0x45, 0x00, 0x00,

        // COFF header
        0x4C, 0x01, // machine (i386)
        0x00, 0x00, // number of sections
        0x00, 0x00, 0x00, 0x00, // timestamp
        0x00, 0x00, 0x00, 0x00, // symbol table pointer
        0x00, 0x00, 0x00, 0x00, // number of symbols
        0xE0, 0x00, // size of optional header
        0x02, 0x21, // characteristics (DLL)

        // Optional header (PE32)
        0x0B, 0x01, // magic (PE32)
        0x0E, 0x00, // linker version
        0x00, 0x00, 0x00, 0x00, // size of code
        0x00, 0x00, 0x00, 0x00, // size of init data
        0x00, 0x00, 0x00, 0x00, // size of uninit data
        0x00, 0x00, 0x00, 0x00, // entry point
        0x00, 0x00, 0x00, 0x00, // base of code
        0x00, 0x00, 0x00, 0x00, // base of data
        0x00, 0x00, 0x40, 0x00, // image base
        0x00, 0x10, 0x00, 0x00, // section alignment
        0x00, 0x02, 0x00, 0x00, // file alignment
        0x06, 0x00, // OS version major
        0x00, 0x00, // OS version minor
        0x00, 0x00, // image version major
        0x00, 0x00, // image version minor
        0x06, 0x00, // subsystem version major
        0x00, 0x00, // subsystem version minor
        0x00, 0x00, 0x00, 0x00, // win32 version
        0x00, 0x10, 0x00, 0x00, // size of image
        0x00, 0x02, 0x00, 0x00, // size of headers
        0x00, 0x00, 0x00, 0x00, // checksum
        0x02, 0x00, // subsystem (GUI)
        0x40, 0x81, // DLL characteristics (dynamic base, NX, no SEH)
    };

    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(minimal_pe), sizeof(minimal_pe));
    // Pad to minimum size
    for (int i = sizeof(minimal_pe); i < 512; ++i) {
        out.put(0);
    }
}

} // namespace

class ReloadableModuleSlotTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_TempDir = CreateTempDir();
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_TempDir, ec);
    }

    std::filesystem::path m_TempDir;
};

TEST_F(ReloadableModuleSlotTest, ConstructsAndDestructs) {
    ReloadableModuleSlot slot;
    // Should not throw
}

TEST_F(ReloadableModuleSlotTest, InitializeWithEmptyPathFails) {
    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = L"";
    EXPECT_FALSE(slot.Initialize(config));
}

TEST_F(ReloadableModuleSlotTest, InitializeWithNonexistentPathFails) {
    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = L"C:\\nonexistent\\path\\to\\module.dll";
    EXPECT_FALSE(slot.Initialize(config));
}

TEST_F(ReloadableModuleSlotTest, InitializeWithNullContextFails) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = dll_path.wstring();
    config.context = nullptr;
    EXPECT_FALSE(slot.Initialize(config));
}

TEST_F(ReloadableModuleSlotTest, InitializeWithValidConfigSucceeds) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    auto& context = Context::Instance();
    context.Initialize({0, 4, 0});

    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = dll_path.wstring();
    config.temp_directory = (m_TempDir / "temp").wstring();
    config.context = &context;
    config.get_proc = &bmlGetProcAddress;

    EXPECT_TRUE(slot.Initialize(config));
    EXPECT_EQ(slot.GetVersion(), 0u);
    EXPECT_FALSE(slot.IsLoaded());

    context.Cleanup();
}

TEST_F(ReloadableModuleSlotTest, HasChangedReturnsFalseInitially) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    auto& context = Context::Instance();
    context.Initialize({0, 4, 0});

    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = dll_path.wstring();
    config.context = &context;
    config.get_proc = &bmlGetProcAddress;

    ASSERT_TRUE(slot.Initialize(config));
    EXPECT_FALSE(slot.HasChanged());

    context.Cleanup();
}

TEST_F(ReloadableModuleSlotTest, HasChangedDetectsFileModification) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    auto& context = Context::Instance();
    context.Initialize({0, 4, 0});

    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = dll_path.wstring();
    config.context = &context;
    config.get_proc = &bmlGetProcAddress;

    ASSERT_TRUE(slot.Initialize(config));
    EXPECT_FALSE(slot.HasChanged());

    // Wait a bit and modify the file
    std::this_thread::sleep_for(100ms);
    std::ofstream(dll_path, std::ios::app) << "modified";

    EXPECT_TRUE(slot.HasChanged());

    context.Cleanup();
}

TEST_F(ReloadableModuleSlotTest, ReloadWithNoChangeReturnsNoChange) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    auto& context = Context::Instance();
    context.Initialize({0, 4, 0});

    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = dll_path.wstring();
    config.context = &context;
    config.get_proc = &bmlGetProcAddress;

    ASSERT_TRUE(slot.Initialize(config));
    EXPECT_EQ(slot.Reload(), ReloadResult::NoChange);

    context.Cleanup();
}

TEST_F(ReloadableModuleSlotTest, GetPathReturnsConfiguredPath) {
    auto dll_path = m_TempDir / "mymodule.dll";
    CreateMinimalDll(dll_path);

    auto& context = Context::Instance();
    context.Initialize({0, 4, 0});

    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = dll_path.wstring();
    config.context = &context;
    config.get_proc = &bmlGetProcAddress;

    ASSERT_TRUE(slot.Initialize(config));
    EXPECT_EQ(slot.GetPath(), dll_path.wstring());

    context.Cleanup();
}

TEST_F(ReloadableModuleSlotTest, UserDataPersistence) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    auto& context = Context::Instance();
    context.Initialize({0, 4, 0});

    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = dll_path.wstring();
    config.context = &context;
    config.get_proc = &bmlGetProcAddress;

    ASSERT_TRUE(slot.Initialize(config));

    int test_data = 42;
    slot.SetUserData(&test_data);
    EXPECT_EQ(slot.GetUserData(), &test_data);

    context.Cleanup();
}

TEST_F(ReloadableModuleSlotTest, ShutdownCleansUp) {
    auto dll_path = m_TempDir / "test.dll";
    CreateMinimalDll(dll_path);

    auto& context = Context::Instance();
    context.Initialize({0, 4, 0});

    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = dll_path.wstring();
    config.temp_directory = (m_TempDir / "temp").wstring();
    config.context = &context;
    config.get_proc = &bmlGetProcAddress;

    ASSERT_TRUE(slot.Initialize(config));
    slot.Shutdown();

    EXPECT_FALSE(slot.IsLoaded());
    EXPECT_EQ(slot.GetVersion(), 0u);

    context.Cleanup();
}

TEST_F(ReloadableModuleSlotTest, GetLastFailureInitiallyNone) {
    ReloadableModuleSlot slot;
    EXPECT_EQ(slot.GetLastFailure(), ReloadFailure::None);
}

TEST_F(ReloadableModuleSlotTest, TempDirectoryCreatedOnInitialize) {
    auto dll_path = m_TempDir / "test.dll";
    auto temp_dir = m_TempDir / "reload_temp";
    CreateMinimalDll(dll_path);

    auto& context = Context::Instance();
    context.Initialize({0, 4, 0});

    EXPECT_FALSE(std::filesystem::exists(temp_dir));

    ReloadableModuleSlot slot;
    ReloadableSlotConfig config;
    config.dll_path = dll_path.wstring();
    config.temp_directory = temp_dir.wstring();
    config.context = &context;
    config.get_proc = &bmlGetProcAddress;

    ASSERT_TRUE(slot.Initialize(config));
    EXPECT_TRUE(std::filesystem::exists(temp_dir));

    context.Cleanup();
}
