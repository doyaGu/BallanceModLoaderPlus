#include <gtest/gtest.h>

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "Core/ApiRegistry.h"
#include "Core/Context.h"
#include "Core/CoreErrors.h"
#include "Core/Logging.h"
#include "Core/ModHandle.h"

#include "bml_logging.h"
#include "bml_errors.h"

PFN_BML_GetLastError bmlGetLastError = nullptr;
PFN_BML_ClearLastError bmlClearLastError = nullptr;
PFN_BML_GetErrorString bmlGetErrorString = nullptr;

using BML::Core::ApiRegistry;
using BML::Core::Context;

namespace {

struct CapturedLog {
    BML_LogSeverity severity{BML_LOG_INFO};
    std::string tag;
    std::string message;
};

class CoreErrorsGuardTests : public ::testing::Test {
protected:
    void SetUp() override {
        ApiRegistry::Instance().Clear();
        Context::SetCurrentModule(nullptr);
        BML::Core::RegisterLoggingApis();
    }

    void TearDown() override {
        (void)BML::Core::ClearLogSinkOverride();
        Context::SetCurrentModule(nullptr);
    }

    void InstallCaptureSink() {
        BML_LogSinkOverrideDesc desc{};
        desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
        desc.flags = BML_LOG_SINK_OVERRIDE_SUPPRESS_DEFAULT;
        desc.user_data = &logs_;
        desc.dispatch = [](BML_Context, const BML_LogMessageInfo *info, void *user_data) {
            auto *logs = static_cast<std::vector<CapturedLog> *>(user_data);
            CapturedLog log;
            if (info) {
                log.severity = info->severity;
                log.tag = info->tag ? info->tag : "";
                log.message = info->message ? info->message : "";
            }
            logs->push_back(std::move(log));
        };
        ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterLogSinkOverride(&desc));
    }

    std::vector<CapturedLog> logs_;
};

[[noreturn]] void ThrowNestedLogicChain(int depth) {
    if (depth <= 0) {
        throw std::logic_error("logic-leaf");
    }

    try {
        ThrowNestedLogicChain(depth - 1);
    } catch (...) {
        std::string message = "logic-layer-" + std::to_string(depth);
        std::throw_with_nested(std::logic_error(message));
    }
}

[[noreturn]] void ThrowAlternatingNestedChain(int depth) {
    if (depth <= 0) {
        throw std::runtime_error("runtime-leaf");
    }

    try {
        ThrowAlternatingNestedChain(depth - 1);
    } catch (...) {
        std::string message = "depth-" + std::to_string(depth);
        if ((depth % 2) == 0) {
            std::throw_with_nested(std::logic_error(message));
        } else {
            std::throw_with_nested(std::runtime_error(message));
        }
    }
}

TEST_F(CoreErrorsGuardTests, RuntimeErrorTranslatesToFailAndLogs) {
    InstallCaptureSink();
    const char *subsystem = "guard.runtime";

    auto result = BML::Core::GuardResult(subsystem, [] {
        throw std::runtime_error("boom");
        return BML_RESULT_OK;
    });

    EXPECT_EQ(result, BML_RESULT_FAIL);
    ASSERT_FALSE(logs_.empty());
    const auto &log = logs_.back();
    EXPECT_EQ(log.severity, BML_LOG_ERROR);
    EXPECT_EQ(log.tag, subsystem);
    EXPECT_NE(log.message.find("boom"), std::string::npos);
}

TEST_F(CoreErrorsGuardTests, BadAllocTranslatesToOutOfMemory) {
    InstallCaptureSink();

    auto result = BML::Core::GuardResult("guard.bad_alloc", [] {
        throw std::bad_alloc();
        return BML_RESULT_OK;
    });

    EXPECT_EQ(result, BML_RESULT_OUT_OF_MEMORY);
    ASSERT_FALSE(logs_.empty());
    EXPECT_EQ(logs_.back().severity, BML_LOG_ERROR);
}

TEST_F(CoreErrorsGuardTests, FilesystemErrorTranslatesToIoError) {
    InstallCaptureSink();

    auto result = BML::Core::GuardResult("guard.fs", [] {
        throw std::filesystem::filesystem_error(
            "io", std::filesystem::path("from"), std::filesystem::path("to"),
            std::make_error_code(std::errc::io_error));
        return BML_RESULT_OK;
    });

    EXPECT_EQ(result, BML_RESULT_IO_ERROR);
    ASSERT_FALSE(logs_.empty());
    const auto &log = logs_.back();
    EXPECT_EQ(log.severity, BML_LOG_ERROR);
    EXPECT_NE(log.message.find("io"), std::string::npos);
}

TEST_F(CoreErrorsGuardTests, SuccessfulLambdaReturnsOkWithoutLog) {
    InstallCaptureSink();

    auto result = BML::Core::GuardResult("guard.ok", [] {
        return BML_RESULT_OK;
    });

    EXPECT_EQ(result, BML_RESULT_OK);
    EXPECT_TRUE(logs_.empty());
}

TEST_F(CoreErrorsGuardTests, GuardVoidTranslatesExceptionsAndLogs) {
    InstallCaptureSink();

    EXPECT_NO_THROW(BML::Core::GuardVoid("guard.void", [] {
        throw std::logic_error("void failure");
    }));

    ASSERT_FALSE(logs_.empty());
    const auto &log = logs_.back();
    EXPECT_EQ(log.severity, BML_LOG_ERROR);
    EXPECT_EQ(log.tag, "guard.void");
    EXPECT_NE(log.message.find("void failure"), std::string::npos);

    BML_ErrorInfo info = BML_ERROR_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, BML::Core::GetLastErrorInfo(&info));
    EXPECT_EQ(info.result_code, BML_RESULT_INVALID_STATE);
    ASSERT_NE(info.message, nullptr);
    EXPECT_NE(std::string_view(info.message).find("void failure"), std::string::npos);
}

TEST_F(CoreErrorsGuardTests, GetLastErrorInfoRejectsInvalidStructSize) {
    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo) - 1;
    EXPECT_EQ(BML_RESULT_INVALID_SIZE, BML::Core::GetLastErrorInfo(&info));
}

TEST_F(CoreErrorsGuardTests, ThreadLocalErrorsAreIsolatedPerThread) {
    BML::Core::ClearLastErrorInfo();

    std::vector<std::thread> threads;
    std::vector<std::string> messages = {
        "thread-a", "thread-b", "thread-c", "thread-d"
    };

    for (const auto &msg : messages) {
        threads.emplace_back([msg] {
            BML::Core::SetLastError(BML_RESULT_FAIL, msg.c_str(), "thread.api");
            BML_ErrorInfo info = BML_ERROR_INFO_INIT;
            ASSERT_EQ(BML_RESULT_OK, BML::Core::GetLastErrorInfo(&info));
            ASSERT_NE(info.message, nullptr);
            EXPECT_STREQ(info.message, msg.c_str());
            ASSERT_NE(info.api_name, nullptr);
            EXPECT_STREQ(info.api_name, "thread.api");
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    BML_ErrorInfo info = BML_ERROR_INFO_INIT;
    EXPECT_EQ(BML_RESULT_NOT_FOUND, BML::Core::GetLastErrorInfo(&info));
}

TEST_F(CoreErrorsGuardTests, LargeErrorMessagesSurviveMultipleWrites) {
    // Note: message_buffer is 256 bytes, so test with sizes within that limit
    constexpr size_t kMaxBufferSize = 255; // Leave room for null terminator

    for (int i = 1; i <= 4; ++i) {
        // Use incrementing sizes up to the buffer limit
        const size_t current_size = std::min<size_t>(50 * static_cast<size_t>(i), kMaxBufferSize);
        std::string message(current_size, static_cast<char>('a' + i));
        message.back() = static_cast<char>('f' + i); // ensure non-uniform payload

        BML::Core::SetLastError(BML_RESULT_FAIL, message.c_str(), "large.test");
        BML_ErrorInfo info = BML_ERROR_INFO_INIT;
        ASSERT_EQ(BML_RESULT_OK, BML::Core::GetLastErrorInfo(&info));
        ASSERT_NE(info.message, nullptr);
        
        std::string retrieved(info.message);
        EXPECT_EQ(retrieved.length(), message.length()) 
            << "Message length mismatch at iteration " << i;
        EXPECT_EQ(retrieved, message)
            << "Message content mismatch at iteration " << i 
            << " (first 50 chars shown)";
        
        ASSERT_NE(info.api_name, nullptr);
        EXPECT_STREQ(info.api_name, "large.test");
    }

    BML::Core::ClearLastErrorInfo();
    BML_ErrorInfo cleared = BML_ERROR_INFO_INIT;
    EXPECT_EQ(BML_RESULT_NOT_FOUND, BML::Core::GetLastErrorInfo(&cleared));
}

TEST_F(CoreErrorsGuardTests, BmlGetLastErrorWorksAcrossThreadsAndModules) {
    auto previous_get = bmlGetLastError;
    auto previous_clear = bmlClearLastError;
    bmlGetLastError = &BML::Core::GetLastErrorInfo;
    bmlClearLastError = &BML::Core::ClearLastErrorInfo;

    BML_Mod_T module_a{};
    BML_Mod_T module_b{};
    module_a.id = "module_a";
    module_b.id = "module_b";
    BML_Mod modules[] = {&module_a, &module_b};

    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        const auto message = std::string("module-") + std::to_string(i);
        BML_Mod mod = modules[i % 2];
        threads.emplace_back([mod, message] {
            BML::Core::Context::SetCurrentModule(mod);
            BML::Core::SetLastError(BML_RESULT_FAIL, message.c_str(), "integration.api");

            BML_ErrorInfo info = BML_ERROR_INFO_INIT;
            ASSERT_EQ(BML_RESULT_OK, bmlGetLastError(&info));
            ASSERT_NE(info.message, nullptr);
            EXPECT_STREQ(info.message, message.c_str());
            ASSERT_NE(info.api_name, nullptr);
            EXPECT_STREQ(info.api_name, "integration.api");

            bmlClearLastError();
            BML_ErrorInfo cleared = BML_ERROR_INFO_INIT;
            EXPECT_EQ(BML_RESULT_NOT_FOUND, bmlGetLastError(&cleared));
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    bmlGetLastError = previous_get;
    bmlClearLastError = previous_clear;
}

TEST_F(CoreErrorsGuardTests, NestedLogicExceptionsPreserveEntireChain) {
    InstallCaptureSink();
    BML::Core::ClearLastErrorInfo();

    auto result = BML::Core::GuardResult("guard.nested.logic", [] {
        ThrowNestedLogicChain(6);
        return BML_RESULT_OK;
    });

    EXPECT_EQ(result, BML_RESULT_INVALID_STATE);
    ASSERT_FALSE(logs_.empty());
    EXPECT_EQ(logs_.back().tag, "guard.nested.logic");
    EXPECT_EQ(logs_.back().severity, BML_LOG_ERROR);

    BML_ErrorInfo info = BML_ERROR_INFO_INIT;
    ASSERT_EQ(BML_RESULT_OK, BML::Core::GetLastErrorInfo(&info));
    ASSERT_NE(info.message, nullptr);
    std::string message(info.message);
    for (int depth = 1; depth <= 6; ++depth) {
        const std::string needle = "logic-layer-" + std::to_string(depth);
        EXPECT_NE(message.find(needle), std::string::npos) << "Missing " << needle;
    }
}

TEST_F(CoreErrorsGuardTests, NestedExceptionFuzzingCoversAlternatingTypes) {
    BML::Core::ClearLastErrorInfo();

    for (int depth = 1; depth <= 16; ++depth) {
        auto result = BML::Core::GuardResult("guard.nested.fuzz", [depth] {
            ThrowAlternatingNestedChain(depth);
            return BML_RESULT_OK;
        });

        if ((depth % 2) == 0) {
            EXPECT_EQ(result, BML_RESULT_INVALID_STATE);
        } else {
            EXPECT_EQ(result, BML_RESULT_FAIL);
        }

        BML_ErrorInfo info = BML_ERROR_INFO_INIT;
        ASSERT_EQ(BML_RESULT_OK, BML::Core::GetLastErrorInfo(&info));
        ASSERT_NE(info.message, nullptr);
        const std::string needle = "depth-" + std::to_string(depth);
        EXPECT_NE(std::string(info.message).find(needle), std::string::npos)
            << "Expected nested chain to include " << needle;

        BML::Core::ClearLastErrorInfo();
    }
}

} // namespace
