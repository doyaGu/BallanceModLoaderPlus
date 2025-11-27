/**
 * @file DiagnosticManagerTests.cpp
 * @brief Unit tests for DiagnosticManager thread-local error handling
 */

#include <gtest/gtest.h>

#include <atomic>
#include <future>
#include <thread>
#include <vector>

#include "Core/DiagnosticManager.h"

using BML::Core::DiagnosticManager;

namespace {

class DiagnosticManagerTests : public ::testing::Test {
protected:
    void SetUp() override {
        DiagnosticManager::Instance().ClearLastError();
    }

    void TearDown() override {
        DiagnosticManager::Instance().ClearLastError();
    }
};

// ============================================================================
// Basic Error Context Tests
// ============================================================================

TEST_F(DiagnosticManagerTests, InitiallyNoError) {
    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);

    BML_Result result = DiagnosticManager::Instance().GetLastError(&info);

    // When no error has been set, GetLastError returns NOT_FOUND
    EXPECT_EQ(result, BML_RESULT_NOT_FOUND);
}

TEST_F(DiagnosticManagerTests, SetAndGetError) {
    DiagnosticManager::Instance().SetError(
        BML_RESULT_INVALID_ARGUMENT,
        "Test error message",
        "TestApiFunction");

    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);

    BML_Result result = DiagnosticManager::Instance().GetLastError(&info);

    EXPECT_EQ(result, BML_RESULT_OK);
    EXPECT_EQ(info.result_code, BML_RESULT_INVALID_ARGUMENT);
    EXPECT_STREQ(info.message, "Test error message");
    EXPECT_STREQ(info.api_name, "TestApiFunction");
}

TEST_F(DiagnosticManagerTests, ClearLastError) {
    DiagnosticManager::Instance().SetError(
        BML_RESULT_NOT_FOUND,
        "Error to clear",
        "SomeApi");

    DiagnosticManager::Instance().ClearLastError();

    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);

    DiagnosticManager::Instance().GetLastError(&info);

    // After clear, result_code should be OK
    EXPECT_EQ(info.result_code, BML_RESULT_OK);
}

TEST_F(DiagnosticManagerTests, SetErrorWithSourceInfo) {
    DiagnosticManager::Instance().SetError(
        BML_RESULT_IO_ERROR,
        "IO failure",
        "ReadFile",
        "file.cpp",
        42);

    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);

    DiagnosticManager::Instance().GetLastError(&info);

    EXPECT_EQ(info.result_code, BML_RESULT_IO_ERROR);
    EXPECT_STREQ(info.message, "IO failure");
    EXPECT_STREQ(info.api_name, "ReadFile");
    EXPECT_STREQ(info.source_file, "file.cpp");
    EXPECT_EQ(info.source_line, 42);
}

TEST_F(DiagnosticManagerTests, LastErrorOverwritesPrevious) {
    DiagnosticManager::Instance().SetError(
        BML_RESULT_NOT_FOUND,
        "First error",
        "Api1");

    DiagnosticManager::Instance().SetError(
        BML_RESULT_INVALID_ARGUMENT,
        "Second error",
        "Api2");

    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);

    DiagnosticManager::Instance().GetLastError(&info);

    // Should contain the second error
    EXPECT_EQ(info.result_code, BML_RESULT_INVALID_ARGUMENT);
    EXPECT_STREQ(info.message, "Second error");
    EXPECT_STREQ(info.api_name, "Api2");
}

// ============================================================================
// Thread-Local Isolation Tests
// ============================================================================

TEST_F(DiagnosticManagerTests, ErrorIsolatedPerThread) {
    std::atomic<bool> thread1_set{false};
    std::atomic<bool> thread2_set{false};
    std::atomic<bool> ready_to_check{false};

    BML_Result thread1_code = BML_RESULT_OK;
    BML_Result thread2_code = BML_RESULT_OK;

    std::thread t1([&]() {
        DiagnosticManager::Instance().SetError(
            BML_RESULT_NOT_FOUND,
            "Thread 1 error",
            "Api1");
        thread1_set = true;

        // Wait for thread 2 to set its error
        while (!ready_to_check) {
            std::this_thread::yield();
        }

        // Thread 1 should still see its own error
        BML_ErrorInfo info{};
        info.struct_size = sizeof(BML_ErrorInfo);
        DiagnosticManager::Instance().GetLastError(&info);
        thread1_code = info.result_code;
    });

    std::thread t2([&]() {
        // Wait for thread 1 to set its error
        while (!thread1_set) {
            std::this_thread::yield();
        }

        DiagnosticManager::Instance().SetError(
            BML_RESULT_INVALID_ARGUMENT,
            "Thread 2 error",
            "Api2");
        thread2_set = true;

        ready_to_check = true;

        // Thread 2 should see its own error
        BML_ErrorInfo info{};
        info.struct_size = sizeof(BML_ErrorInfo);
        DiagnosticManager::Instance().GetLastError(&info);
        thread2_code = info.result_code;
    });

    t1.join();
    t2.join();

    // Each thread should see its own error
    EXPECT_EQ(thread1_code, BML_RESULT_NOT_FOUND);
    EXPECT_EQ(thread2_code, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(DiagnosticManagerTests, ClearOnlyAffectsCurrentThread) {
    std::atomic<bool> thread1_cleared{false};
    std::atomic<bool> thread2_checked{false};

    BML_Result thread2_code_before = BML_RESULT_OK;
    BML_Result thread2_code_after = BML_RESULT_OK;

    std::thread t1([&]() {
        DiagnosticManager::Instance().SetError(
            BML_RESULT_NOT_FOUND,
            "Thread 1 error",
            "Api1");

        // Wait for thread 2 to set and check its error
        while (!thread2_checked) {
            std::this_thread::yield();
        }

        DiagnosticManager::Instance().ClearLastError();
        thread1_cleared = true;
    });

    std::thread t2([&]() {
        DiagnosticManager::Instance().SetError(
            BML_RESULT_INVALID_ARGUMENT,
            "Thread 2 error",
            "Api2");

        BML_ErrorInfo info{};
        info.struct_size = sizeof(BML_ErrorInfo);
        DiagnosticManager::Instance().GetLastError(&info);
        thread2_code_before = info.result_code;
        thread2_checked = true;

        // Wait for thread 1 to clear
        while (!thread1_cleared) {
            std::this_thread::yield();
        }

        // Thread 2's error should still be intact
        DiagnosticManager::Instance().GetLastError(&info);
        thread2_code_after = info.result_code;
    });

    t1.join();
    t2.join();

    EXPECT_EQ(thread2_code_before, BML_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(thread2_code_after, BML_RESULT_INVALID_ARGUMENT);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(DiagnosticManagerTests, NullMessageHandled) {
    DiagnosticManager::Instance().SetError(
        BML_RESULT_NOT_FOUND,
        nullptr,
        "Api");

    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);

    DiagnosticManager::Instance().GetLastError(&info);

    EXPECT_EQ(info.result_code, BML_RESULT_NOT_FOUND);
    EXPECT_EQ(info.message, nullptr);
}

TEST_F(DiagnosticManagerTests, NullApiNameHandled) {
    DiagnosticManager::Instance().SetError(
        BML_RESULT_NOT_FOUND,
        "Error message",
        nullptr);

    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);

    DiagnosticManager::Instance().GetLastError(&info);

    EXPECT_EQ(info.result_code, BML_RESULT_NOT_FOUND);
    EXPECT_STREQ(info.message, "Error message");
    EXPECT_EQ(info.api_name, nullptr);
}

TEST_F(DiagnosticManagerTests, GetLastErrorNullPointerReturnsError) {
    BML_Result result = DiagnosticManager::Instance().GetLastError(nullptr);

    EXPECT_NE(result, BML_RESULT_OK);
}

TEST_F(DiagnosticManagerTests, LongMessageTruncated) {
    // Create a very long message (> 256 chars)
    std::string long_message(300, 'x');

    DiagnosticManager::Instance().SetError(
        BML_RESULT_NOT_FOUND,
        long_message.c_str(),
        "Api");

    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);

    DiagnosticManager::Instance().GetLastError(&info);

    EXPECT_EQ(info.result_code, BML_RESULT_NOT_FOUND);
    // Message should be truncated but not crash
    EXPECT_NE(info.message, nullptr);
    EXPECT_LT(strlen(info.message), long_message.size());
}

} // namespace
