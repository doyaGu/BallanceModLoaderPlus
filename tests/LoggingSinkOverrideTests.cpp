#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "Core/ApiRegistry.h"
#include "Core/Context.h"
#include "Core/Logging.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"

#include "bml_extension.h"
#include "bml_logging.h"

using BML::Core::ApiRegistry;
using BML::Core::Context;

namespace {

struct CapturedLog {
    BML_LogSeverity severity{BML_LOG_INFO};
    std::string tag;
    std::string message;
    std::string formatted;
    std::string mod_id;
};

class LoggingSinkOverrideTests : public ::testing::Test {
protected:
    void SetUp() override {
        ApiRegistry::Instance().Clear();
        Context::SetCurrentModule(nullptr);
        BML::Core::RegisterLoggingApis();
    }

    void TearDown() override {
        (void)BML::Core::ClearLogSinkOverride();
        Context::SetCurrentModule(nullptr);
        manifests_.clear();
        mods_.clear();
    }

    template <typename Fn>
    Fn Lookup(const char *name) {
        auto fn = reinterpret_cast<Fn>(ApiRegistry::Instance().Get(name));
        if (!fn) {
            ADD_FAILURE() << "Missing API: " << name;
        }
        return fn;
    }

    BML_Mod MakeMod(const std::string &id) {
        auto manifest = std::make_unique<BML::Core::ModManifest>();
        manifest->package.id = id;
        manifest->package.name = id;
        manifest->package.version = "1.0.0";
        manifest->package.parsed_version = {1, 0, 0};
        manifest->directory = L"";
        manifest->manifest_path = L"";

        auto handle = Context::Instance().CreateModHandle(*manifest);
        BML_Mod mod = handle.get();
        manifests_.push_back(std::move(manifest));
        mods_.push_back(std::move(handle));
        return mod;
    }

    std::vector<std::unique_ptr<BML::Core::ModManifest>> manifests_;
    std::vector<std::unique_ptr<BML_Mod_T>> mods_;
};

TEST_F(LoggingSinkOverrideTests, OverrideDispatchesAndShutdownFires) {
    auto log_fn = Lookup<PFN_BML_Log>("bmlLog");
    ASSERT_NE(log_fn, nullptr);

    struct CaptureState {
        std::vector<CapturedLog> logs;
        std::atomic<int> dispatch_count{0};
        bool shutdown_called{false};
    } state;

    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.flags = BML_LOG_SINK_OVERRIDE_SUPPRESS_DEFAULT;
    desc.user_data = &state;
    desc.dispatch = [](BML_Context, const BML_LogMessageInfo *info, void *user_data) {
        auto *capture = static_cast<CaptureState *>(user_data);
        CapturedLog log;
        if (info) {
            log.severity = info->severity;
            log.tag = info->tag ? info->tag : "";
            log.message = info->message ? info->message : "";
            log.formatted = info->formatted_line ? info->formatted_line : "";
            log.mod_id = info->mod_id ? info->mod_id : "";
        }
        capture->logs.push_back(std::move(log));
        capture->dispatch_count.fetch_add(1, std::memory_order_relaxed);
    };
    desc.on_shutdown = [](void *user_data) {
        auto *capture = static_cast<CaptureState *>(user_data);
        if (capture) {
            capture->shutdown_called = true;
        }
    };

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterLogSinkOverride(&desc));

    auto mod = MakeMod("log.mod");
    Context::SetCurrentModule(mod);
    log_fn(Context::Instance().GetHandle(), BML_LOG_WARN, "sink.override", "value=%d", 123);

    ASSERT_EQ(state.dispatch_count.load(), 1);
    ASSERT_EQ(state.logs.size(), 1u);
    const auto &entry = state.logs[0];
    EXPECT_EQ(entry.severity, BML_LOG_WARN);
    EXPECT_EQ(entry.tag, "sink.override");
    EXPECT_TRUE(entry.formatted.find("value=123") != std::string::npos);
    EXPECT_EQ(entry.mod_id, "log.mod");

    ASSERT_EQ(BML_RESULT_OK, BML::Core::ClearLogSinkOverride());
    EXPECT_TRUE(state.shutdown_called);
}

TEST_F(LoggingSinkOverrideTests, DuplicateOverrideRegistrationFailsUntilCleared) {
    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.dispatch = +[](BML_Context, const BML_LogMessageInfo *, void *) {};

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterLogSinkOverride(&desc));
    EXPECT_EQ(BML_RESULT_ALREADY_EXISTS, BML::Core::RegisterLogSinkOverride(&desc));

    ASSERT_EQ(BML_RESULT_OK, BML::Core::ClearLogSinkOverride());
    EXPECT_EQ(BML_RESULT_NOT_FOUND, BML::Core::ClearLogSinkOverride());
}

// ========================================================================
// Additional Logging Tests
// ========================================================================

TEST_F(LoggingSinkOverrideTests, LogFilterPreventsLowerSeverityMessages) {
    auto log_fn = Lookup<PFN_BML_Log>("bmlLog");
    auto set_filter = Lookup<PFN_BML_SetLogFilter>("bmlSetLogFilter");
    ASSERT_NE(log_fn, nullptr);
    ASSERT_NE(set_filter, nullptr);

    struct CaptureState {
        std::atomic<int> dispatch_count{0};
    } state;

    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.flags = BML_LOG_SINK_OVERRIDE_SUPPRESS_DEFAULT;
    desc.user_data = &state;
    desc.dispatch = [](BML_Context, const BML_LogMessageInfo *, void *user_data) {
        auto *capture = static_cast<CaptureState *>(user_data);
        capture->dispatch_count.fetch_add(1, std::memory_order_relaxed);
    };

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterLogSinkOverride(&desc));

    // Set filter to WARN - should block DEBUG and INFO
    set_filter(BML_LOG_WARN);
    
    log_fn(nullptr, BML_LOG_DEBUG, "test", "debug message");
    log_fn(nullptr, BML_LOG_INFO, "test", "info message");
    log_fn(nullptr, BML_LOG_WARN, "test", "warn message");
    log_fn(nullptr, BML_LOG_ERROR, "test", "error message");
    
    // Only WARN and ERROR should have been dispatched
    EXPECT_EQ(state.dispatch_count.load(), 2);
    
    // Reset filter to TRACE for cleanup
    set_filter(BML_LOG_TRACE);
}

TEST_F(LoggingSinkOverrideTests, NullFormatStringDoesNotCrash) {
    auto log_fn = Lookup<PFN_BML_Log>("bmlLog");
    ASSERT_NE(log_fn, nullptr);

    // Should not crash with null format string
    EXPECT_NO_THROW(log_fn(nullptr, BML_LOG_INFO, "test", nullptr));
}

TEST_F(LoggingSinkOverrideTests, OverrideExceptionDoesNotCrash) {
    auto log_fn = Lookup<PFN_BML_Log>("bmlLog");
    ASSERT_NE(log_fn, nullptr);

    struct CaptureState {
        std::atomic<int> dispatch_count{0};
        bool throw_exception{true};
    } state;

    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.flags = 0;  // Don't suppress default - should fall through after exception
    desc.user_data = &state;
    desc.dispatch = [](BML_Context, const BML_LogMessageInfo *, void *user_data) {
        auto *capture = static_cast<CaptureState *>(user_data);
        capture->dispatch_count.fetch_add(1, std::memory_order_relaxed);
        if (capture->throw_exception) {
            throw std::runtime_error("Test exception in dispatch");
        }
    };

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterLogSinkOverride(&desc));

    // Should not crash despite exception in dispatch
    EXPECT_NO_THROW(log_fn(nullptr, BML_LOG_INFO, "test", "message"));
    EXPECT_EQ(state.dispatch_count.load(), 1);
}

TEST_F(LoggingSinkOverrideTests, ShutdownExceptionDoesNotCrash) {
    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.dispatch = +[](BML_Context, const BML_LogMessageInfo *, void *) {};
    desc.on_shutdown = +[](void *) {
        throw std::runtime_error("Test exception in shutdown");
    };

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterLogSinkOverride(&desc));

    // Should not crash despite exception in shutdown
    EXPECT_NO_THROW(BML::Core::ClearLogSinkOverride());
}

TEST_F(LoggingSinkOverrideTests, GetLoggingCapsReturnsValidCaps) {
    auto get_caps = Lookup<PFN_BML_LoggingGetCaps>("bmlLoggingGetCaps");
    ASSERT_NE(get_caps, nullptr);

    BML_LogCaps caps = BML_LOG_CAPS_INIT;
    ASSERT_EQ(BML_RESULT_OK, get_caps(&caps));

    EXPECT_EQ(caps.struct_size, sizeof(BML_LogCaps));
    EXPECT_TRUE(caps.capability_flags & BML_LOG_CAP_STRUCTURED_TAGS);
    EXPECT_TRUE(caps.capability_flags & BML_LOG_CAP_VARIADIC);
    EXPECT_TRUE(caps.capability_flags & BML_LOG_CAP_FILTER_OVERRIDE);
    EXPECT_TRUE(caps.supported_severities_mask & BML_LOG_SEVERITY_MASK(BML_LOG_TRACE));
    EXPECT_TRUE(caps.supported_severities_mask & BML_LOG_SEVERITY_MASK(BML_LOG_FATAL));
    EXPECT_EQ(caps.threading_model, BML_THREADING_FREE);
}

TEST_F(LoggingSinkOverrideTests, GetLoggingCapsRejectsNullPointer) {
    auto get_caps = Lookup<PFN_BML_LoggingGetCaps>("bmlLoggingGetCaps");
    ASSERT_NE(get_caps, nullptr);

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, get_caps(nullptr));
}

TEST_F(LoggingSinkOverrideTests, RegisterOverrideRejectsInvalidDesc) {
    // Null descriptor
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, BML::Core::RegisterLogSinkOverride(nullptr));

    // Invalid struct_size
    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = 0;
    desc.dispatch = +[](BML_Context, const BML_LogMessageInfo *, void *) {};
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, BML::Core::RegisterLogSinkOverride(&desc));

    // Null dispatch
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.dispatch = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, BML::Core::RegisterLogSinkOverride(&desc));
}

TEST_F(LoggingSinkOverrideTests, LogMessageFormattingIncludesAllParts) {
    auto log_fn = Lookup<PFN_BML_Log>("bmlLog");
    ASSERT_NE(log_fn, nullptr);

    CapturedLog captured{};

    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.flags = BML_LOG_SINK_OVERRIDE_SUPPRESS_DEFAULT;
    desc.user_data = &captured;
    desc.dispatch = [](BML_Context, const BML_LogMessageInfo *info, void *user_data) {
        auto *cap = static_cast<CapturedLog *>(user_data);
        if (info) {
            cap->severity = info->severity;
            cap->tag = info->tag ? info->tag : "";
            cap->message = info->message ? info->message : "";
            cap->formatted = info->formatted_line ? info->formatted_line : "";
        }
    };

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterLogSinkOverride(&desc));

    log_fn(nullptr, BML_LOG_ERROR, "mytag", "value=%d, str=%s", 42, "hello");

    EXPECT_EQ(captured.severity, BML_LOG_ERROR);
    EXPECT_EQ(captured.tag, "mytag");
    EXPECT_EQ(captured.message, "value=42, str=hello");
    EXPECT_TRUE(captured.formatted.find("[ERROR]") != std::string::npos);
    EXPECT_TRUE(captured.formatted.find("[mytag]") != std::string::npos);
    EXPECT_TRUE(captured.formatted.find("value=42, str=hello") != std::string::npos);
}

} // namespace
