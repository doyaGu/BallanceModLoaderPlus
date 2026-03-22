#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <future>
#include <initializer_list>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/CrashDumpWriter.h"
#include "Core/FaultTracker.h"
#include "Core/ModuleLoader.h"
#include "TestKernel.h"

using namespace std::chrono_literals;

namespace BML::Core {
namespace {
using BML::Core::Testing::TestKernel;

std::mutex g_RecordMutex;
std::vector<std::string> g_ShutdownOrder;

struct ResolveDuringShutdownState {
    Context *context{nullptr};
    BML_Mod mod{nullptr};
    std::atomic<bool> invoked{false};
    std::atomic<bool> resolved{false};
};

void RecordingShutdownHook(BML_Context, void *user_data) {
    const char *label = static_cast<const char *>(user_data);
    std::lock_guard<std::mutex> lock(g_RecordMutex);
    g_ShutdownOrder.emplace_back(label ? label : "");
}

void ResolveDuringShutdownHook(BML_Context, void *user_data) {
    auto *state = static_cast<ResolveDuringShutdownState *>(user_data);
    state->invoked.store(true, std::memory_order_release);
    auto *resolved = state->context ? state->context->ResolveModHandle(state->mod) : nullptr;
    state->resolved.store(resolved == state->mod, std::memory_order_release);
}

LoadedModule BuildLoadedModule(const std::string &id, std::initializer_list<const char *> hooks) {
    LoadedModule module;
    module.id = id;
    module.mod_handle = std::make_unique<BML_Mod_T>();
    module.mod_handle->id = id;
    for (const char *hook_label : hooks) {
        module.mod_handle->shutdown_hooks.push_back({RecordingShutdownHook, const_cast<char *>(hook_label)});
    }
    return module;
}
} // namespace

class ContextLifecycleTests : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->api_registry  = std::make_unique<ApiRegistry>();
        kernel_->config        = std::make_unique<ConfigStore>();
        kernel_->crash_dump    = std::make_unique<CrashDumpWriter>();
        kernel_->fault_tracker = std::make_unique<FaultTracker>();
        kernel_->context = std::make_unique<Context>(*kernel_->api_registry, *kernel_->config, *kernel_->crash_dump, *kernel_->fault_tracker);
        kernel_->config->BindContext(*kernel_->context);

        auto &ctx = *kernel_->context;
        ctx.Initialize({0, 4, 0});
        std::lock_guard<std::mutex> lock(g_RecordMutex);
        g_ShutdownOrder.clear();
    }

    void TearDown() override {
        kernel_->context->Cleanup(*kernel_);
        std::lock_guard<std::mutex> lock(g_RecordMutex);
        g_ShutdownOrder.clear();
    }
};

TEST_F(ContextLifecycleTests, CleanupWaitsForOutstandingRetain) {
    auto &ctx = *kernel_->context;
    ASSERT_EQ(ctx.RetainHandle(), BML_RESULT_OK);

    std::atomic<bool> cleanup_finished{false};
    std::thread cleaner([&]() {
        ctx.Cleanup(*kernel_);
        cleanup_finished.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(cleanup_finished.load(std::memory_order_acquire));

    EXPECT_EQ(ctx.ReleaseHandle(), BML_RESULT_OK);
    cleaner.join();
    EXPECT_TRUE(cleanup_finished.load(std::memory_order_acquire));
    EXPECT_FALSE(ctx.IsInitialized());
}

TEST_F(ContextLifecycleTests, ShutdownHooksExecuteInReverseRegistrationOrder) {
    auto &ctx = *kernel_->context;

    ctx.AddLoadedModule(BuildLoadedModule("alpha", {"alpha-first", "alpha-second"}));
    ctx.AddLoadedModule(BuildLoadedModule("beta", {"beta-first", "beta-second"}));

    ctx.ShutdownModules(*kernel_);

    std::vector<std::string> expected = {
        "beta-second",
        "beta-first",
        "alpha-second",
        "alpha-first"
    };

    std::lock_guard<std::mutex> lock(g_RecordMutex);
    EXPECT_EQ(g_ShutdownOrder, expected);
}

TEST_F(ContextLifecycleTests, CleanupRunsShutdownHooksWithoutHoldingStateMutex) {
    auto &ctx = *kernel_->context;

    auto module = BuildLoadedModule("reentrant.lookup", {});
    ResolveDuringShutdownState state;
    state.context = &ctx;
    state.mod = module.mod_handle.get();

    ctx.AddLoadedModule(std::move(module));
    ctx.AppendShutdownHook(state.mod, ResolveDuringShutdownHook, &state);

    auto cleanup = std::async(std::launch::async, [&ctx, this] {
        ctx.Cleanup(*kernel_);
    });

    EXPECT_EQ(cleanup.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(state.invoked.load(std::memory_order_acquire));
    EXPECT_TRUE(state.resolved.load(std::memory_order_acquire));
}

TEST_F(ContextLifecycleTests, CurrentModuleIsThreadLocalPerThread) {
    auto primary = std::make_unique<BML_Mod_T>();
    primary->id = "context.primary";
    auto worker = std::make_unique<BML_Mod_T>();
    worker->id = "context.worker";

    Context::SetCurrentModule(primary.get());
    EXPECT_EQ(Context::GetCurrentModule(), primary.get());

    std::atomic<BML_Mod> worker_seen{nullptr};
    std::thread bg([&] {
        EXPECT_EQ(Context::GetCurrentModule(), nullptr);
        Context::SetCurrentModule(worker.get());
        worker_seen.store(Context::GetCurrentModule(), std::memory_order_release);
        Context::SetCurrentModule(nullptr);
    });
    bg.join();

    EXPECT_EQ(worker_seen.load(std::memory_order_acquire), worker.get());
    EXPECT_EQ(Context::GetCurrentModule(), primary.get());

    Context::SetCurrentModule(nullptr);
    EXPECT_EQ(Context::GetCurrentModule(), nullptr);
}

TEST(ContextSanitizerTests, PreservesSupplementaryCharacters) {
    std::string identifier = "module-\xF0\x9F\x9A\x80"; // rocket emoji
    auto sanitized = Context::SanitizeIdentifierForFilename(identifier);
    std::wstring expected_suffix = L"\xD83D\xDE80";
    auto position = sanitized.find(expected_suffix);
    EXPECT_NE(position, std::wstring::npos);
    EXPECT_EQ(sanitized.rfind(L"module-", 0), 0);
}

TEST(ContextSanitizerTests, FiltersReservedFilenameCharacters) {
    std::string identifier = "<bad>|name?.";
    auto sanitized = Context::SanitizeIdentifierForFilename(identifier);
    EXPECT_EQ(sanitized, L"_bad__name__");
}

} // namespace BML::Core
