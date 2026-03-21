#include <gtest/gtest.h>

#include <string_view>

#include "Core/ApiRegistry.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "TestKernel.h"
#include "bml_module_runtime.h"

using BML::Core::ApiRegistry;
using BML::Core::ConfigStore;
using BML::Core::Testing::TestKernel;

namespace {

// Stub provider that handles ".as" files
static BML_Bool StubCanHandle(const char *entry_path) {
    if (!entry_path) return BML_FALSE;
    std::string_view path(entry_path);
    return path.ends_with(".as") ? BML_TRUE : BML_FALSE;
}

static BML_Result StubAttach(BML_Mod, PFN_BML_GetProcAddress,
                              const char *, const char *) {
    return BML_RESULT_OK;
}

static BML_Result StubPrepareDetach(BML_Mod) { return BML_RESULT_OK; }
static BML_Result StubDetach(BML_Mod) { return BML_RESULT_OK; }
static BML_Result StubReload(BML_Mod) { return BML_RESULT_OK; }

static const BML_ModuleRuntimeProvider kStubProvider = {
    BML_MODULE_RUNTIME_PROVIDER_INIT,
    StubCanHandle, StubAttach, StubPrepareDetach, StubDetach, StubReload
};

// Second stub that handles ".lua" files
static BML_Bool LuaCanHandle(const char *entry_path) {
    if (!entry_path) return BML_FALSE;
    std::string_view path(entry_path);
    return path.ends_with(".lua") ? BML_TRUE : BML_FALSE;
}

static const BML_ModuleRuntimeProvider kLuaProvider = {
    BML_MODULE_RUNTIME_PROVIDER_INIT,
    LuaCanHandle, StubAttach, StubPrepareDetach, StubDetach, StubReload
};

class RuntimeProviderTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->context = std::make_unique<BML::Core::Context>();
        kernel_->api_registry = std::make_unique<ApiRegistry>();
        kernel_->config = std::make_unique<ConfigStore>();

        auto &ctx = *kernel_->context;
        ctx.Initialize({0, 4, 0});
    }

    void TearDown() override {
        auto &ctx = *kernel_->context;
        // Ensure all providers are cleaned up
        ctx.UnregisterRuntimeProvider(&kStubProvider);
        ctx.UnregisterRuntimeProvider(&kLuaProvider);
    }
};

TEST_F(RuntimeProviderTest, RegisterAndFind) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_OK,
              ctx.RegisterRuntimeProvider(&kStubProvider, "com.bml.scripting"));

    auto *found = ctx.FindRuntimeProvider("C:/Mods/Test/main.as");
    EXPECT_EQ(found, &kStubProvider);
}

TEST_F(RuntimeProviderTest, FindReturnsNullForUnhandledExtension) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_OK,
              ctx.RegisterRuntimeProvider(&kStubProvider, "com.bml.scripting"));

    auto *found = ctx.FindRuntimeProvider("C:/Mods/Test/Test.dll");
    EXPECT_EQ(found, nullptr);
}

TEST_F(RuntimeProviderTest, FindReturnsNullWhenEmpty) {
    auto &ctx = *kernel_->context;

    auto *found = ctx.FindRuntimeProvider("C:/Mods/Test/main.as");
    EXPECT_EQ(found, nullptr);
}

TEST_F(RuntimeProviderTest, DuplicateRegistrationRejected) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_OK,
              ctx.RegisterRuntimeProvider(&kStubProvider, "com.bml.scripting"));
    EXPECT_EQ(BML_RESULT_ALREADY_EXISTS,
              ctx.RegisterRuntimeProvider(&kStubProvider, "com.bml.scripting"));
}

TEST_F(RuntimeProviderTest, MultipleProvidersCoexist) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_OK,
              ctx.RegisterRuntimeProvider(&kStubProvider, "com.bml.scripting"));
    EXPECT_EQ(BML_RESULT_OK,
              ctx.RegisterRuntimeProvider(&kLuaProvider, "com.bml.lua"));

    EXPECT_EQ(ctx.FindRuntimeProvider("main.as"), &kStubProvider);
    EXPECT_EQ(ctx.FindRuntimeProvider("main.lua"), &kLuaProvider);
    EXPECT_EQ(ctx.FindRuntimeProvider("mod.dll"), nullptr);
}

TEST_F(RuntimeProviderTest, Unregister) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_OK,
              ctx.RegisterRuntimeProvider(&kStubProvider, "com.bml.scripting"));
    EXPECT_NE(ctx.FindRuntimeProvider("main.as"), nullptr);

    EXPECT_EQ(BML_RESULT_OK,
              ctx.UnregisterRuntimeProvider(&kStubProvider));
    EXPECT_EQ(ctx.FindRuntimeProvider("main.as"), nullptr);
}

TEST_F(RuntimeProviderTest, UnregisterNotFoundReturnsError) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_NOT_FOUND,
              ctx.UnregisterRuntimeProvider(&kStubProvider));
}

TEST_F(RuntimeProviderTest, InvalidateByOwner) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_OK,
              ctx.RegisterRuntimeProvider(&kStubProvider, "com.bml.scripting"));
    EXPECT_EQ(BML_RESULT_OK,
              ctx.RegisterRuntimeProvider(&kLuaProvider, "com.bml.lua"));

    ctx.InvalidateRuntimeProvider("com.bml.scripting");

    EXPECT_EQ(ctx.FindRuntimeProvider("main.as"), nullptr);
    EXPECT_EQ(ctx.FindRuntimeProvider("main.lua"), &kLuaProvider);
}

TEST_F(RuntimeProviderTest, InvalidateNonExistentOwnerIsNoOp) {
    auto &ctx = *kernel_->context;

    ctx.InvalidateRuntimeProvider("nonexistent");
    // Should not crash or affect anything
}

TEST_F(RuntimeProviderTest, NullProviderRejected) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              ctx.RegisterRuntimeProvider(nullptr, "owner"));
}

TEST_F(RuntimeProviderTest, EmptyOwnerRejected) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              ctx.RegisterRuntimeProvider(&kStubProvider, ""));
}

TEST_F(RuntimeProviderTest, ProviderMissingRequiredFieldsRejected) {
    auto &ctx = *kernel_->context;

    BML_ModuleRuntimeProvider incomplete = {
        BML_MODULE_RUNTIME_PROVIDER_INIT,
        StubCanHandle,
        nullptr,       // AttachModule missing
        nullptr,
        nullptr,       // DetachModule missing
        nullptr
    };

    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT,
              ctx.RegisterRuntimeProvider(&incomplete, "owner"));
}

TEST_F(RuntimeProviderTest, FindWithEmptyPathReturnsNull) {
    auto &ctx = *kernel_->context;

    EXPECT_EQ(BML_RESULT_OK,
              ctx.RegisterRuntimeProvider(&kStubProvider, "com.bml.scripting"));

    EXPECT_EQ(ctx.FindRuntimeProvider(""), nullptr);
}

} // namespace
