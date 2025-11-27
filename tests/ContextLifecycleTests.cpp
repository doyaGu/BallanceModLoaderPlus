#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <initializer_list>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Core/Context.h"
#include "Core/ModuleLoader.h"

using namespace std::chrono_literals;

namespace BML::Core {
namespace {
std::mutex g_RecordMutex;
std::vector<std::string> g_ShutdownOrder;

void RecordingShutdownHook(BML_Context, void *user_data) {
    const char *label = static_cast<const char *>(user_data);
    std::lock_guard<std::mutex> lock(g_RecordMutex);
    g_ShutdownOrder.emplace_back(label ? label : "");
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
    void SetUp() override {
        auto &ctx = Context::Instance();
        ctx.Cleanup();
        ctx.Initialize({0, 4, 0});
        std::lock_guard<std::mutex> lock(g_RecordMutex);
        g_ShutdownOrder.clear();
    }

    void TearDown() override {
        Context::Instance().Cleanup();
        std::lock_guard<std::mutex> lock(g_RecordMutex);
        g_ShutdownOrder.clear();
    }
};

TEST_F(ContextLifecycleTests, CleanupWaitsForOutstandingRetain) {
    auto &ctx = Context::Instance();
    ASSERT_EQ(ctx.RetainHandle(), BML_RESULT_OK);

    std::atomic<bool> cleanup_finished{false};
    std::thread cleaner([&]() {
        ctx.Cleanup();
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
    auto &ctx = Context::Instance();

    ctx.AddLoadedModule(BuildLoadedModule("alpha", {"alpha-first", "alpha-second"}));
    ctx.AddLoadedModule(BuildLoadedModule("beta", {"beta-first", "beta-second"}));

    ctx.ShutdownModules();

    std::vector<std::string> expected = {
        "beta-second",
        "beta-first",
        "alpha-second",
        "alpha-first"
    };

    std::lock_guard<std::mutex> lock(g_RecordMutex);
    EXPECT_EQ(g_ShutdownOrder, expected);
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
