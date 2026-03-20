/**
 * @file ApiImprovementsTest.cpp
 * @brief Tests for simplified ApiRegistry (name-based lookup only)
 */

#include <gtest/gtest.h>

#include <string>

#include "Core/ApiRegistry.h"

using namespace BML::Core;

namespace {
void DummyFuncA() {}
void DummyFuncB() {}
void DummyFuncC() {}
}

class ApiImprovementsTest : public ::testing::Test {
protected:
    void SetUp() override {
        ApiRegistry::Instance().Clear();
    }

    void TearDown() override {
        ApiRegistry::Instance().Clear();
    }
};

// ============================================================================
// Basic Registration and Lookup Tests
// ============================================================================

TEST_F(ApiImprovementsTest, RegisterAndGetByName) {
    ApiRegistry::Instance().RegisterApi("test.api", reinterpret_cast<void*>(&DummyFuncA), "TestMod");

    void* ptr = ApiRegistry::Instance().Get("test.api");
    ASSERT_EQ(ptr, reinterpret_cast<void*>(&DummyFuncA));
}

TEST_F(ApiImprovementsTest, GetNonExistentReturnsNull) {
    void* ptr = ApiRegistry::Instance().Get("nonexistent.api");
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(ApiImprovementsTest, DuplicateRegistrationRejected) {
    ApiRegistry::Instance().RegisterApi("dup.test", reinterpret_cast<void*>(&DummyFuncA), "Mod1");
    ApiRegistry::Instance().RegisterApi("dup.test", reinterpret_cast<void*>(&DummyFuncB), "Mod2");

    // First registration should win
    void* ptr = ApiRegistry::Instance().Get("dup.test");
    EXPECT_EQ(ptr, reinterpret_cast<void*>(&DummyFuncA));
}

// ============================================================================
// Unregister Tests
// ============================================================================

TEST_F(ApiImprovementsTest, UnregisterRemovesApi) {
    ApiRegistry::Instance().RegisterApi("remove.test", reinterpret_cast<void*>(&DummyFuncA), "TestMod");

    ASSERT_NE(ApiRegistry::Instance().Get("remove.test"), nullptr);

    bool success = ApiRegistry::Instance().Unregister("remove.test");
    EXPECT_TRUE(success);

    EXPECT_EQ(ApiRegistry::Instance().Get("remove.test"), nullptr);
}

TEST_F(ApiImprovementsTest, UnregisterNonExistentReturnsFalse) {
    bool success = ApiRegistry::Instance().Unregister("nonexistent");
    EXPECT_FALSE(success);
}

TEST_F(ApiImprovementsTest, UnregisterByProvider) {
    ApiRegistry::Instance().RegisterApi("provider1.api_a", reinterpret_cast<void*>(&DummyFuncA), "Provider1");
    ApiRegistry::Instance().RegisterApi("provider1.api_b", reinterpret_cast<void*>(&DummyFuncB), "Provider1");
    ApiRegistry::Instance().RegisterApi("provider2.api_a", reinterpret_cast<void*>(&DummyFuncC), "Provider2");

    size_t removed = ApiRegistry::Instance().UnregisterByProvider("Provider1");
    EXPECT_EQ(removed, 2u);

    // Provider1 APIs should be gone
    EXPECT_EQ(ApiRegistry::Instance().Get("provider1.api_a"), nullptr);
    EXPECT_EQ(ApiRegistry::Instance().Get("provider1.api_b"), nullptr);

    // Provider2 API should still exist
    EXPECT_NE(ApiRegistry::Instance().Get("provider2.api_a"), nullptr);
}

TEST_F(ApiImprovementsTest, UnregisterByNonExistentProvider) {
    size_t removed = ApiRegistry::Instance().UnregisterByProvider("NonExistentProvider");
    EXPECT_EQ(removed, 0u);
}

// ============================================================================
// Provider String Storage Tests
// ============================================================================

TEST_F(ApiImprovementsTest, ProviderStringIsCopied) {
    std::string dynamic_provider = "DynamicProvider";
    const char* provider_ptr = dynamic_provider.c_str();

    ApiRegistry::Instance().RegisterApi("test.storage", reinterpret_cast<void*>(&DummyFuncA), provider_ptr);

    // Mutate the original string
    dynamic_provider = "Mutated";

    // Unregister by original provider name should still work
    size_t removed = ApiRegistry::Instance().UnregisterByProvider("DynamicProvider");
    EXPECT_EQ(removed, 1u);
}
