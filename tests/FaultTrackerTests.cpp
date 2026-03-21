/**
 * @file FaultTrackerTests.cpp
 * @brief Unit tests for the FaultTracker (JSON read/write, fault counting, disable)
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/FaultTracker.h"
#include "Core/Context.h"
#include "TestKernel.h"

using namespace BML::Core;
using BML::Core::Testing::TestKernel;

class FaultTrackerTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->context       = std::make_unique<Context>();
        kernel_->fault_tracker = std::make_unique<FaultTracker>();
        kernel_->context->Initialize(bmlMakeVersion(0, 4, 0));
        kernel_->fault_tracker->Shutdown();

        // Create a temp directory for the test
        m_TempDir = std::filesystem::temp_directory_path() / L"bml_fault_test";
        std::filesystem::remove_all(m_TempDir);
        std::filesystem::create_directories(m_TempDir);
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(m_TempDir, ec);
    }

    std::filesystem::path m_TempDir;
};

TEST_F(FaultTrackerTest, InitiallyZeroFaults) {
    kernel_->fault_tracker->Load(m_TempDir.wstring());
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.test"), 0);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.test"));
}

TEST_F(FaultTrackerTest, RecordFaultIncrements) {
    kernel_->fault_tracker->Load(m_TempDir.wstring());

    kernel_->fault_tracker->RecordFault("com.example.test", 0xC0000005);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.test"), 1);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.test"));

    kernel_->fault_tracker->RecordFault("com.example.test", 0xC0000005);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.test"), 2);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.test"));
}

TEST_F(FaultTrackerTest, DisabledAfterThreshold) {
    kernel_->fault_tracker->Load(m_TempDir.wstring());

    // Threshold is 3 faults
    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.badmod"));

    kernel_->fault_tracker->RecordFault("com.example.badmod", 0xC0000005);
    EXPECT_TRUE(kernel_->fault_tracker->IsDisabled("com.example.badmod"));
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.badmod"), 3);
}

TEST_F(FaultTrackerTest, EnableReenables) {
    kernel_->fault_tracker->Load(m_TempDir.wstring());

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
    kernel_->fault_tracker->Load(m_TempDir.wstring());
    kernel_->fault_tracker->RecordFault("com.example.mod1", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.mod1", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.mod1", 0xC0000005);
    kernel_->fault_tracker->RecordFault("com.example.mod2", 0xC000001D);
    kernel_->fault_tracker->Shutdown();

    // Verify the JSON file exists
    auto jsonPath = m_TempDir / L"ModLoader" / L"fault_log.json";
    ASSERT_TRUE(std::filesystem::exists(jsonPath));

    // Reload and verify state is preserved
    kernel_->fault_tracker->Load(m_TempDir.wstring());
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.mod1"), 3);
    EXPECT_TRUE(kernel_->fault_tracker->IsDisabled("com.example.mod1"));
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("com.example.mod2"), 1);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("com.example.mod2"));
}

TEST_F(FaultTrackerTest, MultipleModulesIndependent) {
    kernel_->fault_tracker->Load(m_TempDir.wstring());

    kernel_->fault_tracker->RecordFault("mod.a", 0xC0000005);
    kernel_->fault_tracker->RecordFault("mod.b", 0xC000001D);
    kernel_->fault_tracker->RecordFault("mod.b", 0xC000001D);

    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("mod.a"), 1);
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("mod.b"), 2);
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("mod.a"));
    EXPECT_FALSE(kernel_->fault_tracker->IsDisabled("mod.b"));
}

TEST_F(FaultTrackerTest, LoadNonExistentFile) {
    auto emptyDir = m_TempDir / L"nonexistent";
    std::filesystem::create_directories(emptyDir);

    // Should not crash, just load empty
    kernel_->fault_tracker->Load(emptyDir.wstring());
    EXPECT_EQ(kernel_->fault_tracker->GetFaultCount("any.mod"), 0);
}
