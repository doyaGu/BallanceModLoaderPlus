/**
 * @file FaultTrackerTests.cpp
 * @brief Unit tests for the FaultTracker (JSON read/write, fault counting, disable)
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/CrashDumpWriter.h"
#include "Core/FaultTracker.h"
#include "JsonUtils.h"
#include "PathUtils.h"
#include "TestKernel.h"

using namespace BML::Core;
using BML::Core::Testing::TestKernel;

class FaultTrackerTest : public ::testing::Test {
protected:
    TestKernel kernel_;
    utils::RuntimeLayoutNames m_RuntimeNames;
    utils::RuntimeLayout m_RuntimeLayout;

    void SetUp() override {
        kernel_->api_registry  = std::make_unique<ApiRegistry>();
        kernel_->config        = std::make_unique<ConfigStore>();
        kernel_->crash_dump    = std::make_unique<CrashDumpWriter>();
        kernel_->fault_tracker = std::make_unique<FaultTracker>();
        kernel_->context       = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config, *kernel_->crash_dump, *kernel_->fault_tracker);
        kernel_->config->BindContext(*kernel_->context);
        kernel_->context->Initialize(bmlMakeVersion(0, 4, 0));
        kernel_->fault_tracker->Shutdown();

        // Create a temp directory for the test
        m_TempDir = std::filesystem::temp_directory_path() / L"bml_fault_test";
        std::filesystem::remove_all(m_TempDir);
        std::filesystem::create_directories(m_TempDir);
        m_RuntimeLayout = utils::ResolveRuntimeLayoutFromRuntimeDirectory(
            m_TempDir / m_RuntimeNames.runtime_directory, m_RuntimeNames);
        std::filesystem::create_directories(m_RuntimeLayout.runtime_directory);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_TempDir, ec);
    }

    std::filesystem::path m_TempDir;
};

TEST_F(FaultTrackerTest, InitiallyZeroFaults) {
    kernel_->fault_tracker->LoadFromRuntimeDirectory(m_RuntimeLayout.runtime_directory);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.test"), 0);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.test"));
}

TEST_F(FaultTrackerTest, RecordFaultIncrements) {
    kernel_->fault_tracker->LoadFromRuntimeDirectory(m_RuntimeLayout.runtime_directory);

    kernel_->fault_tracker->RecordFault("com.example.test", 0xC0000005);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.test"), 1);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.test"));

    kernel_->fault_tracker->RecordFault("com.example.test", 0xC0000005);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.test"), 2);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.test"));
}

TEST_F(FaultTrackerTest, DisabledAfterThreshold) {
    kernel_->fault_tracker->LoadFromRuntimeDirectory(m_RuntimeLayout.runtime_directory);

    // Threshold is 3 faults
    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.badmod"));

    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    EXPECT_TRUE(kernel_->fault_tracker->IsDisabled("com.example.badmod"));
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.badmod"), 3);
}

TEST_F(FaultTrackerTest, EnableReenables) {
    kernel_->fault_tracker->LoadFromRuntimeDirectory(m_RuntimeLayout.runtime_directory);

    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    ASSERT_TRUE(kernel_->fault_tracker->IsDisabled("com.example.badmod"));

    kernel_->fault_tracker->Enable("com.example.badmod");
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.badmod"));
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.badmod"), 0);
}

TEST_F(FaultTrackerTest, PersistenceRoundTrip) {
    // Record faults and write to JSON
    kernel_->fault_tracker->LoadFromRuntimeDirectory(m_RuntimeLayout.runtime_directory);
    kernel_->fault_tracker->RecordFault("com.example.mod1", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.mod1", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.mod1", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.mod2", 0xC000001D);
    kernel_->fault_tracker->Shutdown();

    // Verify the JSON file exists
    auto jsonPath = m_RuntimeLayout.fault_log_path;
    ASSERT_TRUE(std::filesystem::exists(jsonPath));

    const std::string jsonContent = [&]() {
        std::ifstream ifs(jsonPath, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    }();
    std::string parseError;
    auto document = utils::JsonDocument::Parse(jsonContent, parseError);
    ASSERT_TRUE(document.IsValid()) << parseError;

    // Reload and verify state is preserved
    kernel_->fault_tracker->LoadFromRuntimeDirectory(m_RuntimeLayout.runtime_directory);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.mod1"), 3);
    EXPECT_TRUE(kernel_->fault_tracker->IsDisabled("com.example.mod1"));
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.mod2"), 1);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.mod2"));
}

TEST_F(FaultTrackerTest, MultipleModulesIndependent) {
    kernel_->fault_tracker->LoadFromRuntimeDirectory(m_RuntimeLayout.runtime_directory);

    kernel_->fault_tracker->RecordFault("mod.a", 0xC0000005);
    kernel_->fault_tracker->RecordFault("mod.b", 0xC000001D);
    kernel_->fault_tracker->RecordFault("mod.b", 0xC000001D);

    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("mod.a"), 1);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("mod.b"), 2);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("mod.a"));
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("mod.b"));
}

TEST_F(FaultTrackerTest, LoadNonExistentFile) {
    const auto emptyLayout = utils::ResolveRuntimeLayoutFromRuntimeDirectory(
        m_TempDir / L"nonexistent" / m_RuntimeNames.runtime_directory, m_RuntimeNames);
    std::filesystem::create_directories(emptyLayout.runtime_directory);

    // Should not crash, just load empty
    kernel_->fault_tracker->LoadFromRuntimeDirectory(emptyLayout.runtime_directory);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("any.mod"), 0);
}

TEST_F(FaultTrackerTest, InvalidJsonClearsPreviouslyLoadedFaultState) {
    kernel_->fault_tracker->LoadFromRuntimeDirectory(m_RuntimeLayout.runtime_directory);
    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    ASSERT_TRUE(kernel_->fault_tracker->IsDisabled("com.example.badmod"));

    auto jsonPath = m_RuntimeLayout.fault_log_path;
    {
        std::ofstream ofs(jsonPath, std::ios::binary | std::ios::trunc);
        ofs << "{ invalid json";
    }

    kernel_->fault_tracker->LoadFromRuntimeDirectory(m_RuntimeLayout.runtime_directory);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.badmod"), 0);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.badmod"));
}
