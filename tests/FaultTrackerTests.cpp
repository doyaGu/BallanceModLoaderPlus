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

using namespace BML::Core;

class FaultTrackerTest : public ::testing::Test {
protected:
    void SetUp() override {
        Context::Instance().Initialize(bmlMakeVersion(0, 4, 0));
        FaultTracker::Instance().Shutdown();

        // Create a temp directory for the test
        m_TempDir = std::filesystem::temp_directory_path() / L"bml_fault_test";
        std::filesystem::remove_all(m_TempDir);
        std::filesystem::create_directories(m_TempDir);
    }

    void TearDown() override {
        FaultTracker::Instance().Shutdown();
        Context::Instance().Cleanup();

        std::error_code ec;
        std::filesystem::remove_all(m_TempDir, ec);
    }

    std::filesystem::path m_TempDir;
};

TEST_F(FaultTrackerTest, InitiallyZeroFaults) {
    FaultTracker::Instance().Load(m_TempDir.wstring());
    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("com.example.test"), 0);
    EXPECT_FALSE(FaultTracker::Instance().IsDisabled("com.example.test"));
}

TEST_F(FaultTrackerTest, RecordFaultIncrements) {
    FaultTracker::Instance().Load(m_TempDir.wstring());

    FaultTracker::Instance().RecordFault("com.example.test", 0xC0000005);
    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("com.example.test"), 1);
    EXPECT_FALSE(FaultTracker::Instance().IsDisabled("com.example.test"));

    FaultTracker::Instance().RecordFault("com.example.test", 0xC0000005);
    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("com.example.test"), 2);
    EXPECT_FALSE(FaultTracker::Instance().IsDisabled("com.example.test"));
}

TEST_F(FaultTrackerTest, DisabledAfterThreshold) {
    FaultTracker::Instance().Load(m_TempDir.wstring());

    // Threshold is 3 faults
    FaultTracker::Instance().RecordFault("com.example.badmod", 0xC0000005);
    FaultTracker::Instance().RecordFault("com.example.badmod", 0xC0000005);
    EXPECT_FALSE(FaultTracker::Instance().IsDisabled("com.example.badmod"));

    FaultTracker::Instance().RecordFault("com.example.badmod", 0xC0000005);
    EXPECT_TRUE(FaultTracker::Instance().IsDisabled("com.example.badmod"));
    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("com.example.badmod"), 3);
}

TEST_F(FaultTrackerTest, EnableReenables) {
    FaultTracker::Instance().Load(m_TempDir.wstring());

    FaultTracker::Instance().RecordFault("com.example.badmod", 0xC0000005);
    FaultTracker::Instance().RecordFault("com.example.badmod", 0xC0000005);
    FaultTracker::Instance().RecordFault("com.example.badmod", 0xC0000005);
    ASSERT_TRUE(FaultTracker::Instance().IsDisabled("com.example.badmod"));

    FaultTracker::Instance().Enable("com.example.badmod");
    EXPECT_FALSE(FaultTracker::Instance().IsDisabled("com.example.badmod"));
    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("com.example.badmod"), 0);
}

TEST_F(FaultTrackerTest, PersistenceRoundTrip) {
    // Record faults and write to JSON
    FaultTracker::Instance().Load(m_TempDir.wstring());
    FaultTracker::Instance().RecordFault("com.example.mod1", 0xC0000005);
    FaultTracker::Instance().RecordFault("com.example.mod1", 0xC0000005);
    FaultTracker::Instance().RecordFault("com.example.mod1", 0xC0000005);
    FaultTracker::Instance().RecordFault("com.example.mod2", 0xC000001D);
    FaultTracker::Instance().Shutdown();

    // Verify the JSON file exists
    auto jsonPath = m_TempDir / L"ModLoader" / L"fault_log.json";
    ASSERT_TRUE(std::filesystem::exists(jsonPath));

    // Reload and verify state is preserved
    FaultTracker::Instance().Load(m_TempDir.wstring());
    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("com.example.mod1"), 3);
    EXPECT_TRUE(FaultTracker::Instance().IsDisabled("com.example.mod1"));
    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("com.example.mod2"), 1);
    EXPECT_FALSE(FaultTracker::Instance().IsDisabled("com.example.mod2"));
}

TEST_F(FaultTrackerTest, MultipleModulesIndependent) {
    FaultTracker::Instance().Load(m_TempDir.wstring());

    FaultTracker::Instance().RecordFault("mod.a", 0xC0000005);
    FaultTracker::Instance().RecordFault("mod.b", 0xC000001D);
    FaultTracker::Instance().RecordFault("mod.b", 0xC000001D);

    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("mod.a"), 1);
    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("mod.b"), 2);
    EXPECT_FALSE(FaultTracker::Instance().IsDisabled("mod.a"));
    EXPECT_FALSE(FaultTracker::Instance().IsDisabled("mod.b"));
}

TEST_F(FaultTrackerTest, LoadNonExistentFile) {
    auto emptyDir = m_TempDir / L"nonexistent";
    std::filesystem::create_directories(emptyDir);

    // Should not crash, just load empty
    FaultTracker::Instance().Load(emptyDir.wstring());
    EXPECT_EQ(FaultTracker::Instance().GetFaultCount("any.mod"), 0);
}
