#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/CrashDumpWriter.h"
#include "Core/FaultTracker.h"
#include "Core/Logging.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"
#include "TestKernel.h"

#include "bml_logging.h"

using BML::Core::ApiRegistry;
using BML::Core::ConfigStore;
using BML::Core::Context;
using BML::Core::CrashDumpWriter;
using BML::Core::FaultTracker;
using BML::Core::Testing::TestKernel;

namespace {

struct CapturedLog {
    BML_LogSeverity severity{BML_LOG_INFO};
    std::string tag;
    std::string message;
    std::string mod_id;
};

class LogListenerTests : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->api_registry  = std::make_unique<ApiRegistry>();
        kernel_->config        = std::make_unique<ConfigStore>();
        kernel_->crash_dump    = std::make_unique<CrashDumpWriter>();
        kernel_->fault_tracker = std::make_unique<FaultTracker>();
        kernel_->context       = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config, *kernel_->crash_dump, *kernel_->fault_tracker);
        kernel_->config->BindContext(*kernel_->context);
        kernel_->api_registry->Clear();
        Context::SetLifecycleModule(nullptr);
        BML::Core::RegisterLoggingApis(*kernel_->api_registry);
    }

    void TearDown() override {
        BML::Core::ClearAllLogListeners();
        Context::SetLifecycleModule(nullptr);
        manifests_.clear();
        mods_.clear();
    }

    template <typename Fn>
    Fn Lookup(const char *name) {
        auto fn = reinterpret_cast<Fn>(kernel_->api_registry->Get(name));
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

        auto handle = kernel_->context->CreateModHandle(*manifest);
        BML_Mod mod = handle.get();
        manifests_.push_back(std::move(manifest));
        mods_.push_back(std::move(handle));
        return mod;
    }

    std::vector<std::unique_ptr<BML::Core::ModManifest>> manifests_;
    std::vector<std::unique_ptr<BML_Mod_T>> mods_;
};

TEST_F(LogListenerTests, ListenerReceivesLogMessages) {
    auto log_fn = Lookup<PFN_BML_Log>("bmlLog");
    ASSERT_NE(log_fn, nullptr);

    struct CaptureState {
        std::vector<CapturedLog> logs;
        std::atomic<int> count{0};
    } state;

    auto listener = [](BML_Context, const BML_LogMessageInfo *info, void *user_data) {
        auto *capture = static_cast<CaptureState *>(user_data);
        CapturedLog log;
        if (info) {
            log.severity = info->severity;
            log.tag = info->tag ? info->tag : "";
            log.message = info->message ? info->message : "";
            log.mod_id = info->mod_id ? info->mod_id : "";
        }
        capture->logs.push_back(std::move(log));
        capture->count.fetch_add(1, std::memory_order_relaxed);
    };

    auto mod = MakeMod("test.mod");
    ASSERT_EQ(BML_RESULT_OK, BML::Core::AddLogListener(mod, listener, &state));

    log_fn(mod, BML_LOG_INFO, "test.tag", "test message %d", 42);

    ASSERT_EQ(state.count.load(), 1);
    ASSERT_EQ(state.logs.size(), 1u);
    const auto &entry = state.logs[0];
    EXPECT_EQ(entry.severity, BML_LOG_INFO);
    EXPECT_EQ(entry.tag, "test.tag");
    EXPECT_TRUE(entry.message.find("test message 42") != std::string::npos);
    EXPECT_EQ(entry.mod_id, "test.mod");

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RemoveLogListener(mod, listener));
}

TEST_F(LogListenerTests, MultipleListenersAllReceiveMessages) {
    auto log_fn = Lookup<PFN_BML_Log>("bmlLog");
    ASSERT_NE(log_fn, nullptr);

    std::atomic<int> count1{0}, count2{0};

    auto listener1 = [](BML_Context, const BML_LogMessageInfo *, void *user_data) {
        auto *count = static_cast<std::atomic<int> *>(user_data);
        count->fetch_add(1, std::memory_order_relaxed);
    };

    auto listener2 = [](BML_Context, const BML_LogMessageInfo *, void *user_data) {
        auto *count = static_cast<std::atomic<int> *>(user_data);
        count->fetch_add(1, std::memory_order_relaxed);
    };

    auto mod = MakeMod("test.mod");
    ASSERT_EQ(BML_RESULT_OK, BML::Core::AddLogListener(mod, listener1, &count1));
    ASSERT_EQ(BML_RESULT_OK, BML::Core::AddLogListener(mod, listener2, &count2));

    log_fn(mod, BML_LOG_INFO, nullptr, "test");

    EXPECT_EQ(count1.load(), 1);
    EXPECT_EQ(count2.load(), 1);

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RemoveLogListener(mod, listener1));
    ASSERT_EQ(BML_RESULT_OK, BML::Core::RemoveLogListener(mod, listener2));
}

TEST_F(LogListenerTests, ListenersFireIndependentOfSinkOverride) {
    auto log_fn = Lookup<PFN_BML_Log>("bmlLog");
    ASSERT_NE(log_fn, nullptr);

    std::atomic<int> listener_count{0};
    std::atomic<int> sink_count{0};

    auto listener = [](BML_Context, const BML_LogMessageInfo *, void *user_data) {
        auto *count = static_cast<std::atomic<int> *>(user_data);
        count->fetch_add(1, std::memory_order_relaxed);
    };

    BML_LogSinkOverrideDesc desc{};
    desc.struct_size = sizeof(BML_LogSinkOverrideDesc);
    desc.flags = BML_LOG_SINK_OVERRIDE_SUPPRESS_DEFAULT;
    desc.user_data = &sink_count;
    desc.dispatch = [](BML_Context, const BML_LogMessageInfo *, void *user_data) {
        auto *count = static_cast<std::atomic<int> *>(user_data);
        count->fetch_add(1, std::memory_order_relaxed);
    };

    auto mod = MakeMod("test.mod");
    ASSERT_EQ(BML_RESULT_OK, BML::Core::AddLogListener(mod, listener, &listener_count));
    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterLogSinkOverride(&desc));

    log_fn(mod, BML_LOG_INFO, nullptr, "test");

    // Both listener and sink should fire
    EXPECT_EQ(listener_count.load(), 1);
    EXPECT_EQ(sink_count.load(), 1);

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RemoveLogListener(mod, listener));
    ASSERT_EQ(BML_RESULT_OK, BML::Core::ClearLogSinkOverride());
}

TEST_F(LogListenerTests, RemoveLogListenersForModuleCleansUpCorrectly) {
    auto log_fn = Lookup<PFN_BML_Log>("bmlLog");
    ASSERT_NE(log_fn, nullptr);

    std::atomic<int> count1{0}, count2{0};

    auto listener = [](BML_Context, const BML_LogMessageInfo *, void *user_data) {
        auto *count = static_cast<std::atomic<int> *>(user_data);
        count->fetch_add(1, std::memory_order_relaxed);
    };

    auto mod1 = MakeMod("mod1");
    auto mod2 = MakeMod("mod2");

    ASSERT_EQ(BML_RESULT_OK, BML::Core::AddLogListener(mod1, listener, &count1));
    ASSERT_EQ(BML_RESULT_OK, BML::Core::AddLogListener(mod2, listener, &count2));

    log_fn(mod1, BML_LOG_INFO, nullptr, "test");
    EXPECT_EQ(count1.load(), 1);
    EXPECT_EQ(count2.load(), 1);

    // Remove all listeners for mod1
    BML::Core::RemoveLogListenersForModule(mod1);

    count1 = 0;
    count2 = 0;
    log_fn(mod2, BML_LOG_INFO, nullptr, "test2");

    // mod1's listener should be gone, mod2's should still fire
    EXPECT_EQ(count1.load(), 0);
    EXPECT_EQ(count2.load(), 1);

    ASSERT_EQ(BML_RESULT_OK, BML::Core::RemoveLogListener(mod2, listener));
}

} // namespace
