/**
 * @file ApiImprovementsTest.cpp
 * @brief Tests for simplified ApiRegistry (name-based lookup only)
 */

#include <gtest/gtest.h>

#include <string>

#include "Core/ApiRegistry.h"
#include "TestKernel.h"

using namespace BML::Core;

namespace {
using BML::Core::Testing::TestKernel;

void DummyFuncA() {}
void DummyFuncB() {}
void DummyFuncC() {}
}

class ApiImprovementsTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->api_registry = std::make_unique<ApiRegistry>();
    }

    void TearDown() override {
    }
};

// ============================================================================
// Basic Registration and Lookup Tests
// ============================================================================

TEST_F(ApiImprovementsTest, RegisterAndGetByName) {
    kernel_->api_registry->RegisterApi("test.api", reinterpret_cast<void*>(&DummyFuncA), "TestMod");

    void* ptr = kernel_->api_registry->Get("test.api");
    ASSERT_EQ(ptr, reinterpret_cast<void*>(&DummyFuncA));
}

TEST_F(ApiImprovementsTest, GetNonExistentReturnsNull) {
    void* ptr = kernel_->api_registry->Get("nonexistent.api");
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(ApiImprovementsTest, DuplicateRegistrationRejected) {
    kernel_->api_registry->RegisterApi("dup.test", reinterpret_cast<void*>(&DummyFuncA), "Mod1");
    kernel_->api_registry->RegisterApi("dup.test", reinterpret_cast<void*>(&DummyFuncB), "Mod2");

    // First registration should win
    void* ptr = kernel_->api_registry->Get("dup.test");
    EXPECT_EQ(ptr, reinterpret_cast<void*>(&DummyFuncA));
}

// ============================================================================
// Unregister Tests
// ============================================================================

TEST_F(ApiImprovementsTest, UnregisterRemovesApi) {
    kernel_->api_registry->RegisterApi("remove.test", reinterpret_cast<void*>(&DummyFuncA), "TestMod");

    ASSERT_NE(kernel_->api_registry->Get("remove.test"), nullptr);

    bool success = kernel_->api_registry->Unregister("remove.test");
    EXPECT_TRUE(success);

    EXPECT_EQ(kernel_->api_registry->Get("remove.test"), nullptr);
}

TEST_F(ApiImprovementsTest, UnregisterNonExistentReturnsFalse) {
    bool success = kernel_->api_registry->Unregister("nonexistent");
    EXPECT_FALSE(success);
}

TEST_F(ApiImprovementsTest, UnregisterByProvider) {
    kernel_->api_registry->RegisterApi("provider1.api_a", reinterpret_cast<void*>(&DummyFuncA), "Provider1");
    kernel_->api_registry->RegisterApi("provider1.api_b", reinterpret_cast<void*>(&DummyFuncB), "Provider1");
    kernel_->api_registry->RegisterApi("provider2.api_a", reinterpret_cast<void*>(&DummyFuncC), "Provider2");

    size_t removed = kernel_->api_registry->UnregisterByProvider("Provider1");
    EXPECT_EQ(removed, 2u);

    // Provider1 APIs should be gone
    EXPECT_EQ(kernel_->api_registry->Get("provider1.api_a"), nullptr);
    EXPECT_EQ(kernel_->api_registry->Get("provider1.api_b"), nullptr);

    // Provider2 API should still exist
    EXPECT_NE(kernel_->api_registry->Get("provider2.api_a"), nullptr);
}

TEST_F(ApiImprovementsTest, UnregisterByNonExistentProvider) {
    size_t removed = kernel_->api_registry->UnregisterByProvider("NonExistentProvider");
    EXPECT_EQ(removed, 0u);
}

// ============================================================================
// Provider String Storage Tests
// ============================================================================

TEST_F(ApiImprovementsTest, ProviderStringIsCopied) {
    std::string dynamic_provider = "DynamicProvider";
    const char* provider_ptr = dynamic_provider.c_str();

    kernel_->api_registry->RegisterApi("test.storage", reinterpret_cast<void*>(&DummyFuncA), provider_ptr);

    // Mutate the original string
    dynamic_provider = "Mutated";

    // Unregister by original provider name should still work
    size_t removed = kernel_->api_registry->UnregisterByProvider("DynamicProvider");
    EXPECT_EQ(removed, 1u);
}
