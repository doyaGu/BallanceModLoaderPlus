/**
 * @file HookRegistryTests.cpp
 * @brief Unit tests for the HookRegistry
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "bml_hook.h"

#include "Core/HookRegistry.h"
#include "Core/Context.h"
#include "TestKernel.h"

using namespace BML::Core;
using BML::Core::Testing::TestKernel;

class HookRegistryTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->context = std::make_unique<Context>();
        kernel_->hooks   = std::make_unique<HookRegistry>();
        kernel_->context->Initialize(bmlMakeVersion(0, 4, 0));
        kernel_->hooks->Shutdown();
    }

    void TearDown() override {
    }
};

// Fake addresses for testing
static int g_FakeTarget1 = 0;
static int g_FakeTarget2 = 0;
static int g_FakeTarget3 = 0;

struct EnumResult {
    std::string owner;
    std::string name;
    void *address;
    int32_t priority;
    uint32_t flags;
};

static void CollectHooks(const BML_HookDesc *desc,
                          const char *owner_module_id,
                          void *user_data) {
    auto *results = static_cast<std::vector<EnumResult> *>(user_data);
    EnumResult r;
    r.owner = owner_module_id ? owner_module_id : "";
    r.name = desc->target_name ? desc->target_name : "";
    r.address = desc->target_address;
    r.priority = desc->priority;
    r.flags = desc->flags;
    results->push_back(r);
}

TEST_F(HookRegistryTest, RegisterAndEnumerate) {
    BML_HookDesc desc = BML_HOOK_DESC_INIT;
    desc.target_name = "CKRenderContext::Render";
    desc.target_address = &g_FakeTarget1;
    desc.priority = 0;

    ASSERT_EQ(kernel_->hooks->Register("bml.render", &desc), BML_RESULT_OK);

    std::vector<EnumResult> results;
    ASSERT_EQ(kernel_->hooks->Enumerate(CollectHooks, &results), BML_RESULT_OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].owner, "bml.render");
    EXPECT_EQ(results[0].name, "CKRenderContext::Render");
    EXPECT_EQ(results[0].address, &g_FakeTarget1);
    EXPECT_EQ(results[0].priority, 0);
    EXPECT_EQ(results[0].flags & BML_HOOK_FLAG_CONFLICT, 0u);
}

TEST_F(HookRegistryTest, ConflictDetection) {
    BML_HookDesc desc1 = BML_HOOK_DESC_INIT;
    desc1.target_name = "CKRenderContext::Render";
    desc1.target_address = &g_FakeTarget1;
    desc1.priority = 0;

    BML_HookDesc desc2 = BML_HOOK_DESC_INIT;
    desc2.target_name = "CKRenderContext::Render";
    desc2.target_address = &g_FakeTarget1;  // Same address
    desc2.priority = 10;

    ASSERT_EQ(kernel_->hooks->Register("bml.render", &desc1), BML_RESULT_OK);
    ASSERT_EQ(kernel_->hooks->Register("com.example.mymod", &desc2), BML_RESULT_OK);

    std::vector<EnumResult> results;
    ASSERT_EQ(kernel_->hooks->Enumerate(CollectHooks, &results), BML_RESULT_OK);
    ASSERT_EQ(results.size(), 2u);

    // Both entries should have conflict flag
    for (const auto &r : results) {
        EXPECT_NE(r.flags & BML_HOOK_FLAG_CONFLICT, 0u)
            << "Hook by " << r.owner << " should have CONFLICT flag";
    }
}

TEST_F(HookRegistryTest, UnregisterRemovesEntry) {
    BML_HookDesc desc = BML_HOOK_DESC_INIT;
    desc.target_name = "CKInputManager::IsKeyDown";
    desc.target_address = &g_FakeTarget2;

    ASSERT_EQ(kernel_->hooks->Register("bml.input", &desc), BML_RESULT_OK);
    ASSERT_EQ(kernel_->hooks->Unregister("bml.input", &g_FakeTarget2), BML_RESULT_OK);

    std::vector<EnumResult> results;
    ASSERT_EQ(kernel_->hooks->Enumerate(CollectHooks, &results), BML_RESULT_OK);
    EXPECT_EQ(results.size(), 0u);
}

TEST_F(HookRegistryTest, UnregisterNotFound) {
    EXPECT_EQ(kernel_->hooks->Unregister("bml.input", &g_FakeTarget2),
              BML_RESULT_NOT_FOUND);
}

TEST_F(HookRegistryTest, UnregisterWrongModule) {
    BML_HookDesc desc = BML_HOOK_DESC_INIT;
    desc.target_name = "SomeFunction";
    desc.target_address = &g_FakeTarget1;

    ASSERT_EQ(kernel_->hooks->Register("bml.render", &desc), BML_RESULT_OK);
    EXPECT_EQ(kernel_->hooks->Unregister("bml.input", &g_FakeTarget1),
              BML_RESULT_NOT_FOUND);
}

TEST_F(HookRegistryTest, ConflictClearedAfterUnregister) {
    BML_HookDesc desc = BML_HOOK_DESC_INIT;
    desc.target_name = "CKRenderContext::Render";
    desc.target_address = &g_FakeTarget1;

    ASSERT_EQ(kernel_->hooks->Register("bml.render", &desc), BML_RESULT_OK);
    ASSERT_EQ(kernel_->hooks->Register("com.example.mymod", &desc), BML_RESULT_OK);

    // Unregister one, conflict flag should be cleared on the remaining one
    ASSERT_EQ(kernel_->hooks->Unregister("com.example.mymod", &g_FakeTarget1), BML_RESULT_OK);

    std::vector<EnumResult> results;
    ASSERT_EQ(kernel_->hooks->Enumerate(CollectHooks, &results), BML_RESULT_OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].flags & BML_HOOK_FLAG_CONFLICT, 0u);
}

TEST_F(HookRegistryTest, CleanupOwnerRemovesAll) {
    BML_HookDesc desc1 = BML_HOOK_DESC_INIT;
    desc1.target_name = "Hook1";
    desc1.target_address = &g_FakeTarget1;

    BML_HookDesc desc2 = BML_HOOK_DESC_INIT;
    desc2.target_name = "Hook2";
    desc2.target_address = &g_FakeTarget2;

    BML_HookDesc desc3 = BML_HOOK_DESC_INIT;
    desc3.target_name = "Hook3";
    desc3.target_address = &g_FakeTarget3;

    ASSERT_EQ(kernel_->hooks->Register("bml.input", &desc1), BML_RESULT_OK);
    ASSERT_EQ(kernel_->hooks->Register("bml.input", &desc2), BML_RESULT_OK);
    ASSERT_EQ(kernel_->hooks->Register("bml.render", &desc3), BML_RESULT_OK);

    kernel_->hooks->CleanupOwner("bml.input");

    std::vector<EnumResult> results;
    ASSERT_EQ(kernel_->hooks->Enumerate(CollectHooks, &results), BML_RESULT_OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].owner, "bml.render");
}

TEST_F(HookRegistryTest, MultipleAddressesSameModule) {
    BML_HookDesc desc1 = BML_HOOK_DESC_INIT;
    desc1.target_name = "FuncA";
    desc1.target_address = &g_FakeTarget1;

    BML_HookDesc desc2 = BML_HOOK_DESC_INIT;
    desc2.target_name = "FuncB";
    desc2.target_address = &g_FakeTarget2;

    ASSERT_EQ(kernel_->hooks->Register("bml.physics", &desc1), BML_RESULT_OK);
    ASSERT_EQ(kernel_->hooks->Register("bml.physics", &desc2), BML_RESULT_OK);

    std::vector<EnumResult> results;
    ASSERT_EQ(kernel_->hooks->Enumerate(CollectHooks, &results), BML_RESULT_OK);
    EXPECT_EQ(results.size(), 2u);
}

TEST_F(HookRegistryTest, ReRegisterSameModuleSameAddress) {
    BML_HookDesc desc = BML_HOOK_DESC_INIT;
    desc.target_name = "OldName";
    desc.target_address = &g_FakeTarget1;
    desc.priority = 0;

    ASSERT_EQ(kernel_->hooks->Register("bml.render", &desc), BML_RESULT_OK);

    // Re-register with updated name and priority
    desc.target_name = "NewName";
    desc.priority = 5;
    ASSERT_EQ(kernel_->hooks->Register("bml.render", &desc), BML_RESULT_OK);

    std::vector<EnumResult> results;
    ASSERT_EQ(kernel_->hooks->Enumerate(CollectHooks, &results), BML_RESULT_OK);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].name, "NewName");
    EXPECT_EQ(results[0].priority, 5);
}

TEST_F(HookRegistryTest, InvalidArguments) {
    EXPECT_EQ(kernel_->hooks->Register("mod", nullptr), BML_RESULT_INVALID_ARGUMENT);

    BML_HookDesc desc = BML_HOOK_DESC_INIT;
    desc.target_address = nullptr;
    EXPECT_EQ(kernel_->hooks->Register("mod", &desc), BML_RESULT_INVALID_ARGUMENT);

    EXPECT_EQ(kernel_->hooks->Unregister("mod", nullptr), BML_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(kernel_->hooks->Enumerate(nullptr, nullptr), BML_RESULT_INVALID_ARGUMENT);
}
