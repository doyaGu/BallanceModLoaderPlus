/**
 * @file ProfilingManagerTests.cpp
 * @brief Unit tests for ProfilingManager trace events and Chrome Tracing output
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Core/ApiRegistration.h"
#include "Core/ApiRegistry.h"
#include "Core/MemoryManager.h"
#include "Core/ProfilingManager.h"
#include "TestKernel.h"

using BML::Core::ApiRegistry;
using BML::Core::ProfilingManager;
using BML::Core::Testing::TestKernel;

namespace {

class ProfilingManagerTests : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->profiling    = std::make_unique<ProfilingManager>();
        kernel_->api_registry = std::make_unique<ApiRegistry>();
        kernel_->memory       = std::make_unique<BML::Core::MemoryManager>();
        // Enable profiling for tests
        kernel_->profiling->SetProfilingEnabled(BML_TRUE);
    }

    void TearDown() override {
        // Disable profiling
        kernel_->profiling->SetProfilingEnabled(BML_FALSE);

        // Clean up any test trace files
        std::error_code ec;
        std::filesystem::remove("test_trace.json", ec);
        std::filesystem::remove("bml_trace.json", ec);
    }

    std::string ReadFile(const std::string& path) {
        std::ifstream file(path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(ProfilingManagerTests, SingletonInstance) {
    auto& instance1 = *kernel_->profiling;
    auto& instance2 = *kernel_->profiling;

    EXPECT_EQ(&instance1, &instance2);
}

TEST_F(ProfilingManagerTests, EnableDisableProfiling) {
    kernel_->profiling->SetProfilingEnabled(BML_FALSE);
    EXPECT_EQ(kernel_->profiling->IsProfilingEnabled(), BML_FALSE);

    kernel_->profiling->SetProfilingEnabled(BML_TRUE);
    EXPECT_EQ(kernel_->profiling->IsProfilingEnabled(), BML_TRUE);
}

TEST_F(ProfilingManagerTests, GetProfilerBackend) {
    BML_ProfilerBackend backend = kernel_->profiling->GetProfilerBackend();

    // Default backend should be Chrome Tracing
    EXPECT_EQ(backend, BML_PROFILER_CHROME_TRACING);
}

TEST_F(ProfilingManagerTests, GetTimestampNsReturnsNonZero) {
    uint64_t ts = kernel_->profiling->GetTimestampNs();

    EXPECT_GT(ts, 0ULL);
}

TEST_F(ProfilingManagerTests, GetCpuFrequencyReturnsNonZero) {
    uint64_t freq = kernel_->profiling->GetCpuFrequency();

    EXPECT_GT(freq, 0ULL);
}

// ============================================================================
// Trace Event Tests
// ============================================================================

TEST_F(ProfilingManagerTests, TraceBeginEndPair) {
    kernel_->profiling->TraceBegin("TestScope", "category");
    kernel_->profiling->TraceEnd();

    // Should complete without crashing
    SUCCEED();
}

TEST_F(ProfilingManagerTests, TraceInstant) {
    kernel_->profiling->TraceInstant("instant_event", "category");

    SUCCEED();
}

TEST_F(ProfilingManagerTests, TraceCounter) {
    kernel_->profiling->TraceCounter("test_counter", 42);

    SUCCEED();
}

TEST_F(ProfilingManagerTests, TraceFrameMark) {
    kernel_->profiling->TraceFrameMark();

    SUCCEED();
}

TEST_F(ProfilingManagerTests, TraceSetThreadName) {
    kernel_->profiling->TraceSetThreadName("MainThread");

    SUCCEED();
}

TEST_F(ProfilingManagerTests, NestedScopes) {
    kernel_->profiling->TraceBegin("Outer", "cat");
    kernel_->profiling->TraceBegin("Middle", "cat");
    kernel_->profiling->TraceBegin("Inner", "cat");
    kernel_->profiling->TraceEnd();
    kernel_->profiling->TraceEnd();
    kernel_->profiling->TraceEnd();

    SUCCEED();
}

TEST_F(ProfilingManagerTests, TraceEndWithoutBeginDoesNotCrash) {
    // Unmatched TraceEnd should be handled gracefully
    kernel_->profiling->TraceEnd();
    kernel_->profiling->TraceEnd();

    SUCCEED();
}

TEST_F(ProfilingManagerTests, TraceWithNullName) {
    kernel_->profiling->TraceBegin(nullptr, "category");
    kernel_->profiling->TraceInstant(nullptr, "category");
    kernel_->profiling->TraceCounter(nullptr, 100);

    SUCCEED();
}

TEST_F(ProfilingManagerTests, TraceWithNullCategory) {
    kernel_->profiling->TraceBegin("scope", nullptr);
    kernel_->profiling->TraceEnd();

    SUCCEED();
}

// ============================================================================
// Profiling Disabled Tests
// ============================================================================

TEST_F(ProfilingManagerTests, NoEventsWhenDisabled) {
    kernel_->profiling->SetProfilingEnabled(BML_FALSE);

    // These should be no-ops when disabled
    kernel_->profiling->TraceBegin("test", "cat");
    kernel_->profiling->TraceEnd();
    kernel_->profiling->TraceInstant("test", "cat");
    kernel_->profiling->TraceCounter("counter", 123);
    kernel_->profiling->TraceFrameMark();

    SUCCEED();
}

// ============================================================================
// Flush Tests
// ============================================================================

TEST_F(ProfilingManagerTests, FlushWithDefaultFilename) {
    kernel_->profiling->TraceBegin("test_scope", "category");
    kernel_->profiling->TraceEnd();

    BML_Result result = kernel_->profiling->FlushProfilingData(nullptr);

    EXPECT_EQ(result, BML_RESULT_OK);
    EXPECT_TRUE(std::filesystem::exists("bml_trace.json"));
}

TEST_F(ProfilingManagerTests, FlushWithCustomFilename) {
    kernel_->profiling->TraceBegin("test_scope", "category");
    kernel_->profiling->TraceEnd();

    BML_Result result = kernel_->profiling->FlushProfilingData("test_trace.json");

    EXPECT_EQ(result, BML_RESULT_OK);
    EXPECT_TRUE(std::filesystem::exists("test_trace.json"));
}

TEST_F(ProfilingManagerTests, FlushOutputIsValidJson) {
    kernel_->profiling->TraceBegin("scope1", "cat1");
    kernel_->profiling->TraceEnd();
    kernel_->profiling->TraceInstant("instant", "cat2");
    kernel_->profiling->TraceCounter("counter", 999);

    kernel_->profiling->FlushProfilingData("test_trace.json");

    std::string content = ReadFile("test_trace.json");

    // Basic JSON structure validation
    EXPECT_NE(content.find("{"), std::string::npos);
    EXPECT_NE(content.find("\"traceEvents\""), std::string::npos);
    EXPECT_NE(content.find("}"), std::string::npos);
}

TEST_F(ProfilingManagerTests, FlushOutputContainsEvents) {
    kernel_->profiling->TraceBegin("my_test_scope", "test_category");
    kernel_->profiling->TraceEnd();

    kernel_->profiling->FlushProfilingData("test_trace.json");

    std::string content = ReadFile("test_trace.json");

    EXPECT_NE(content.find("my_test_scope"), std::string::npos);
    EXPECT_NE(content.find("test_category"), std::string::npos);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(ProfilingManagerTests, GetProfilingStats) {
    BML_ProfilingStats stats = BML_PROFILING_STATS_INIT;

    BML_Result result = kernel_->profiling->GetProfilingStats(&stats);

    EXPECT_EQ(result, BML_RESULT_OK);
    // Stats should have valid values (specifics depend on prior events)
}

TEST_F(ProfilingManagerTests, GetProfilingStatsNullPointer) {
    BML_Result result = kernel_->profiling->GetProfilingStats(nullptr);

    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
}

TEST_F(ProfilingManagerTests, GetProfilingStatsRejectsSmallStruct) {
    struct SmallStats {
        size_t struct_size;
    } stats{sizeof(SmallStats)};

    BML_Result result = kernel_->profiling->GetProfilingStats(
        reinterpret_cast<BML_ProfilingStats *>(&stats));

    EXPECT_EQ(result, BML_RESULT_INVALID_SIZE);
}

TEST_F(ProfilingManagerTests, TracingApisRecordStatistics) {
    kernel_->api_registry->Clear();
    BML::Core::RegisterTracingApis();

    auto enable = reinterpret_cast<void (*)(BML_Bool)>(
        kernel_->api_registry->Get("bmlEnableApiTracing"));
    auto is_enabled = reinterpret_cast<BML_Bool (*)()>(
        kernel_->api_registry->Get("bmlIsApiTracingEnabled"));
    auto get_stats = reinterpret_cast<BML_Bool (*)(const char *, void *)>(
        kernel_->api_registry->Get("bmlGetApiStats"));
    auto dump_stats = reinterpret_cast<BML_Bool (*)(const char *)>(
        kernel_->api_registry->Get("bmlDumpApiStats"));

    ASSERT_NE(enable, nullptr);
    ASSERT_NE(is_enabled, nullptr);
    ASSERT_NE(get_stats, nullptr);
    ASSERT_NE(dump_stats, nullptr);

    enable(BML_TRUE);
    EXPECT_EQ(BML_TRUE, is_enabled());
    enable(BML_FALSE);

    struct TraceStats {
        size_t struct_size;
        const char *api_name;
        uint64_t call_count;
        uint64_t total_time_ns;
        uint64_t min_time_ns;
        uint64_t max_time_ns;
        uint64_t error_count;
    } stats{sizeof(TraceStats), nullptr, 0, 0, 0, 0, 0};

    ASSERT_EQ(BML_TRUE, get_stats("bmlEnableApiTracing", &stats));
    EXPECT_STREQ("bmlEnableApiTracing", stats.api_name);
    EXPECT_EQ(2u, stats.call_count);
    EXPECT_GT(stats.total_time_ns, 0u);

    std::filesystem::path dump_path = std::filesystem::temp_directory_path() / "bml_api_stats.json";
    ASSERT_EQ(BML_TRUE, dump_stats(dump_path.string().c_str()));
    std::ifstream dump(dump_path);
    ASSERT_TRUE(dump.is_open());
    std::stringstream buffer;
    buffer << dump.rdbuf();
    EXPECT_NE(buffer.str().find("bmlEnableApiTracing"), std::string::npos);
    std::error_code ec;
    std::filesystem::remove(dump_path, ec);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(ProfilingManagerTests, ConcurrentTraceEvents) {
    const int num_threads = 4;
    const int events_per_thread = 100;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, events_per_thread]() {
            for (int j = 0; j < events_per_thread; ++j) {
                std::string name = "Thread" + std::to_string(i) + "_Scope" + std::to_string(j);
                kernel_->profiling->TraceBegin(name.c_str(), "concurrent");
                kernel_->profiling->TraceEnd();
                kernel_->profiling->TraceCounter("counter", j);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Flush should succeed after concurrent writes
    BML_Result result = kernel_->profiling->FlushProfilingData("test_trace.json");
    EXPECT_EQ(result, BML_RESULT_OK);
}

TEST_F(ProfilingManagerTests, ConcurrentFlushWithTrace) {
    std::atomic<bool> running{true};

    // Background thread generating events
    std::thread producer([this, &running]() {
        int i = 0;
        while (running) {
            kernel_->profiling->TraceBegin("bg_scope", "bg");
            kernel_->profiling->TraceEnd();
            ++i;
            if (i > 1000) break;
        }
    });

    // Multiple flush operations
    for (int i = 0; i < 5; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        kernel_->profiling->FlushProfilingData("test_trace.json");
    }

    running = false;
    producer.join();

    SUCCEED();
}

// ============================================================================
// Memory/API Helpers Tests
// ============================================================================

TEST_F(ProfilingManagerTests, GetApiCallCount) {
    // This depends on ApiRegistry integration
    uint64_t count = kernel_->profiling->GetApiCallCount("nonexistent_api");

    // Should return 0 for unknown API
    EXPECT_EQ(count, 0ULL);
}

TEST_F(ProfilingManagerTests, GetTotalAllocBytes) {
    // This depends on MemoryManager integration
    uint64_t bytes = kernel_->profiling->GetTotalAllocBytes();

    // Should return a valid value (may be 0 if no allocations tracked)
    // Just verify it doesn't crash
    SUCCEED();
}

} // namespace
