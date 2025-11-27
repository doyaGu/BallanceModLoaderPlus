/**
 * @file ApiImprovementsTest.cpp
 * @brief Tests for API improvements (capability system, discovery, performance)
 * 
 * Tests the following improvements from docs/api-improvements/:
 * - 01: Performance optimization (direct index, TLS cache)
 * - 02: Usability (C++ wrappers, error handling)
 * - 03: Feature extensions (capability query, API discovery)
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Core/ApiRegistry.h"
#include "bml_capabilities.h"
#include "bml_api_ids.h"

using namespace BML::Core;

class ApiImprovementsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry for fresh state
        ApiRegistry::Instance().Clear();
    }
    
    void TearDown() override {
        ApiRegistry::Instance().Clear();
    }
};

// ============================================================================
// Performance Tests (01-performance-optimization.md)
// ============================================================================

TEST_F(ApiImprovementsTest, DirectIndexTableLookup) {
    // Register a test API with metadata
    ApiRegistry::ApiMetadata meta;
    meta.name = "TestApi";
    meta.id = 100;  // Within direct index range
    meta.pointer = (void*)0x12345678;
    meta.version_major = 0;
    meta.version_minor = 5;
    meta.version_patch = 0;
    meta.capabilities = BML_CAP_IMC_BASIC;
    meta.type = BML_API_TYPE_CORE;
    meta.threading = BML_THREADING_FREE;
    meta.provider_mod = "BML";
    
    ApiRegistry::Instance().RegisterApi(meta);
    
    // Test direct index lookup (fast path)
    void* ptr = ApiRegistry::Instance().GetByIdDirect(100);
    ASSERT_EQ(ptr, (void*)0x12345678);
    
    // Test out-of-range returns nullptr
    ptr = ApiRegistry::Instance().GetByIdDirect(0);
    ASSERT_EQ(ptr, nullptr);
}

TEST_F(ApiImprovementsTest, TlsCacheLookup) {
    // Register a test API
    ApiRegistry::ApiMetadata meta;
    meta.name = "CachedApi";
    meta.id = 200;
    meta.pointer = (void*)0xAABBCCDD;
    meta.type = BML_API_TYPE_CORE;
    
    ApiRegistry::Instance().RegisterApi(meta);
    
    // First lookup (cache miss)
    void* ptr1 = ApiRegistry::Instance().GetByIdCached(200);
    ASSERT_EQ(ptr1, (void*)0xAABBCCDD);
    
    // Second lookup (should hit cache)
    void* ptr2 = ApiRegistry::Instance().GetByIdCached(200);
    ASSERT_EQ(ptr2, (void*)0xAABBCCDD);
}

// ============================================================================
// Feature Extension Tests (03-feature-extensions.md)
// ============================================================================

TEST_F(ApiImprovementsTest, ApiMetadataStorage) {
    ApiRegistry::ApiMetadata meta;
    meta.name = "TestApiWithMetadata";
    meta.id = 1000;
    meta.pointer = (void*)0x11111111;
    meta.version_major = 1;
    meta.version_minor = 2;
    meta.version_patch = 3;
    meta.capabilities = BML_CAP_IMC_BASIC | BML_CAP_IMC_RPC;
    meta.type = BML_API_TYPE_CORE;
    meta.threading = BML_THREADING_FREE;
    meta.provider_mod = "BML";
    meta.description = "A test API";
    
    ApiRegistry::Instance().RegisterApi(meta);
    
    // Query by ID
    const auto* result = ApiRegistry::Instance().QueryApi((BML_ApiId)1000);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result->name, "TestApiWithMetadata");
    EXPECT_EQ(result->version_major, 1);
    EXPECT_EQ(result->version_minor, 2);
    EXPECT_EQ(result->version_patch, 3);
    EXPECT_EQ(result->capabilities, BML_CAP_IMC_BASIC | BML_CAP_IMC_RPC);
    EXPECT_EQ(result->type, BML_API_TYPE_CORE);
    EXPECT_EQ(result->threading, BML_THREADING_FREE);
}

TEST_F(ApiImprovementsTest, ApiDescriptorQuery) {
    ApiRegistry::ApiMetadata meta;
    meta.name = "DescriptorTest";
    meta.id = 2000;
    meta.pointer = (void*)0x22222222;
    meta.version_major = 0;
    meta.version_minor = 5;
    meta.version_patch = 0;
    meta.capabilities = BML_CAP_LOGGING;
    meta.type = BML_API_TYPE_EXTENSION;
    meta.threading = BML_THREADING_APARTMENT;
    meta.provider_mod = "TestMod";
    
    ApiRegistry::Instance().RegisterApi(meta);
    
    // Get descriptor
    BML_ApiDescriptor desc;
    bool found = ApiRegistry::Instance().GetDescriptor(2000, &desc);
    
    ASSERT_TRUE(found);
    EXPECT_EQ(desc.id, 2000u);
    EXPECT_STREQ(desc.name, "DescriptorTest");
    EXPECT_EQ(desc.type, BML_API_TYPE_EXTENSION);
    EXPECT_EQ(desc.version_major, 0);
    EXPECT_EQ(desc.version_minor, 5);
    EXPECT_EQ(desc.capabilities, BML_CAP_LOGGING);
    EXPECT_EQ(desc.threading, BML_THREADING_APARTMENT);
}

TEST_F(ApiImprovementsTest, CapabilityAggregation) {
    // Register multiple APIs with different capabilities
    ApiRegistry::ApiMetadata meta1;
    meta1.name = "Api1";
    meta1.id = 3001;
    meta1.pointer = (void*)1;
    meta1.capabilities = BML_CAP_IMC_BASIC;
    ApiRegistry::Instance().RegisterApi(meta1);
    
    ApiRegistry::ApiMetadata meta2;
    meta2.name = "Api2";
    meta2.id = 3002;
    meta2.pointer = (void*)2;
    meta2.capabilities = BML_CAP_SYNC_MUTEX;
    ApiRegistry::Instance().RegisterApi(meta2);
    
    ApiRegistry::ApiMetadata meta3;
    meta3.name = "Api3";
    meta3.id = 3003;
    meta3.pointer = (void*)3;
    meta3.capabilities = BML_CAP_LOGGING;
    ApiRegistry::Instance().RegisterApi(meta3);
    
    // Total capabilities should be OR of all
    uint64_t total = ApiRegistry::Instance().GetTotalCapabilities();
    EXPECT_TRUE(total & BML_CAP_IMC_BASIC);
    EXPECT_TRUE(total & BML_CAP_SYNC_MUTEX);
    EXPECT_TRUE(total & BML_CAP_LOGGING);
}

TEST_F(ApiImprovementsTest, ApiEnumeration) {
    // Register several APIs with unique names
    std::vector<std::string> names;
    for (int i = 0; i < 5; ++i) {
        names.push_back("EnumApi" + std::to_string(i));
    }
    
    for (int i = 0; i < 5; ++i) {
        ApiRegistry::ApiMetadata meta;
        meta.name = names[i].c_str();  // Unique names, kept alive by vector
        meta.id = 4000 + i;
        meta.pointer = (void*)(intptr_t)(i + 1);
        meta.type = (i % 2 == 0) ? BML_API_TYPE_CORE : BML_API_TYPE_EXTENSION;
        ApiRegistry::Instance().RegisterApi(meta);
    }
    
    // Count all APIs
    EXPECT_EQ(ApiRegistry::Instance().GetApiCount(-1), 5u);
    
    // Count by type
    EXPECT_EQ(ApiRegistry::Instance().GetApiCount(BML_API_TYPE_CORE), 3u);
    EXPECT_EQ(ApiRegistry::Instance().GetApiCount(BML_API_TYPE_EXTENSION), 2u);
}

TEST_F(ApiImprovementsTest, ExtensionRegistration) {
    // Test automatic extension ID allocation
    BML_ApiId id1 = ApiRegistry::Instance().RegisterExtension(
        "TestExt1", 1, 0, (void*)0x100, 16, "Mod1"
    );
    
    BML_ApiId id2 = ApiRegistry::Instance().RegisterExtension(
        "TestExt2", 2, 1, (void*)0x200, 32, "Mod2"
    );
    
    // IDs should be in extension range
    EXPECT_GE(id1, BML_EXTENSION_ID_START);
    EXPECT_GE(id2, BML_EXTENSION_ID_START);
    EXPECT_NE(id1, id2);
    
    // Should be queryable
    const auto* meta1 = ApiRegistry::Instance().QueryApi(id1);
    ASSERT_NE(meta1, nullptr);
    EXPECT_STREQ(meta1->name, "TestExt1");
    EXPECT_EQ(meta1->type, BML_API_TYPE_EXTENSION);
}

// ============================================================================
// Call Count Tests
// ============================================================================

TEST_F(ApiImprovementsTest, CallCountTracking) {
    ApiRegistry::ApiMetadata meta;
    meta.name = "CountedApi";
    meta.id = 5000;
    meta.pointer = (void*)0x55555555;
    
    ApiRegistry::Instance().RegisterApi(meta);
    
    // Initial count should be 0
    const auto* result = ApiRegistry::Instance().QueryApi((BML_ApiId)5000);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->call_count.load(), 0u);
    
    // Each GetById should increment
    ApiRegistry::Instance().GetById(5000);
    ApiRegistry::Instance().GetById(5000);
    ApiRegistry::Instance().GetById(5000);
    
    EXPECT_EQ(result->call_count.load(), 3u);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ApiImprovementsTest, InvalidApiIdHandling) {
    // Invalid ID should return nullptr
    EXPECT_EQ(ApiRegistry::Instance().GetByIdDirect(0), nullptr);
    EXPECT_EQ(ApiRegistry::Instance().GetByIdCached(0), nullptr);
    EXPECT_EQ(ApiRegistry::Instance().QueryApi((BML_ApiId)0), nullptr);
    
    // Non-existent ID should return nullptr
    EXPECT_EQ(ApiRegistry::Instance().GetByIdDirect(99999), nullptr);
    EXPECT_EQ(ApiRegistry::Instance().QueryApi((BML_ApiId)99999), nullptr);
}

TEST_F(ApiImprovementsTest, DuplicateRegistrationPrevented) {
    ApiRegistry::ApiMetadata meta;
    meta.name = "DuplicateTest";
    meta.id = 6000;
    meta.pointer = (void*)0x66666666;
    
    ApiRegistry::Instance().RegisterApi(meta);
    
    // Second registration with same ID should be rejected
    meta.pointer = (void*)0x77777777;
    ApiRegistry::Instance().RegisterApi(meta);
    
    // Original pointer should remain
    const auto* result = ApiRegistry::Instance().QueryApi((BML_ApiId)6000);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->pointer, (void*)0x66666666);
}

// ============================================================================
// Unified Registry Tests (03-feature-extensions.md)
// ============================================================================

TEST_F(ApiImprovementsTest, LoadVersionedCompatible) {
    // Register extension with version 1.5
    BML_ApiId id = ApiRegistry::Instance().RegisterExtension(
        "TestExt_Versioned", 1, 5, (void*)0xABCD0015, 32, "TestMod"
    );
    EXPECT_GE(id, BML_EXTENSION_ID_START);
    
    // Load with compatible requirements (1.3 - same major, lower minor)
    const void* api = nullptr;
    uint32_t actual_major = 0, actual_minor = 0;
    bool loaded = ApiRegistry::Instance().LoadVersioned(
        "TestExt_Versioned", 1, 3, &api, &actual_major, &actual_minor
    );
    
    EXPECT_TRUE(loaded);
    EXPECT_EQ(api, (void*)0xABCD0015);
    EXPECT_EQ(actual_major, 1u);
    EXPECT_EQ(actual_minor, 5u);
}

TEST_F(ApiImprovementsTest, LoadVersionedIncompatibleMinor) {
    // Register extension with version 1.2
    ApiRegistry::Instance().RegisterExtension(
        "TestExt_OldVersion", 1, 2, (void*)0x12, 32, "TestMod"
    );
    
    // Try to load with higher minor requirement (1.5)
    const void* api = nullptr;
    bool loaded = ApiRegistry::Instance().LoadVersioned(
        "TestExt_OldVersion", 1, 5, &api, nullptr, nullptr
    );
    
    EXPECT_FALSE(loaded);
    EXPECT_EQ(api, nullptr);
}

TEST_F(ApiImprovementsTest, LoadVersionedIncompatibleMajor) {
    // Register extension with version 2.0
    ApiRegistry::Instance().RegisterExtension(
        "TestExt_V2", 2, 0, (void*)0x20, 32, "TestMod"
    );
    
    // Try to load with different major version (1.0)
    const void* api = nullptr;
    bool loaded = ApiRegistry::Instance().LoadVersioned(
        "TestExt_V2", 1, 0, &api, nullptr, nullptr
    );
    
    EXPECT_FALSE(loaded);
}

TEST_F(ApiImprovementsTest, UnregisterByProvider) {
    // Register multiple extensions from same provider
    ApiRegistry::Instance().RegisterExtension(
        "Provider1_ExtA", 1, 0, (void*)0xA1, 16, "Provider1"
    );
    ApiRegistry::Instance().RegisterExtension(
        "Provider1_ExtB", 1, 0, (void*)0xB1, 16, "Provider1"
    );
    ApiRegistry::Instance().RegisterExtension(
        "Provider2_ExtA", 1, 0, (void*)0xA2, 16, "Provider2"
    );
    
    // Unregister all from Provider1
    size_t removed = ApiRegistry::Instance().UnregisterByProvider("Provider1");
    EXPECT_EQ(removed, 2u);
    
    // Provider1 extensions should be gone
    EXPECT_EQ(ApiRegistry::Instance().QueryApi("Provider1_ExtA"), nullptr);
    EXPECT_EQ(ApiRegistry::Instance().QueryApi("Provider1_ExtB"), nullptr);
    
    // Provider2 extension should still exist
    EXPECT_NE(ApiRegistry::Instance().QueryApi("Provider2_ExtA"), nullptr);
}

TEST_F(ApiImprovementsTest, UnregisterSingle) {
    ApiRegistry::Instance().RegisterExtension(
        "SingleUnregisterTest", 1, 0, (void*)0x999, 16, "TestMod"
    );
    
    // Verify it exists
    EXPECT_NE(ApiRegistry::Instance().QueryApi("SingleUnregisterTest"), nullptr);
    
    // Unregister it
    bool success = ApiRegistry::Instance().Unregister("SingleUnregisterTest");
    EXPECT_TRUE(success);
    
    // Verify it's gone
    EXPECT_EQ(ApiRegistry::Instance().QueryApi("SingleUnregisterTest"), nullptr);
    
    // Unregister again should fail
    success = ApiRegistry::Instance().Unregister("SingleUnregisterTest");
    EXPECT_FALSE(success);
}

TEST_F(ApiImprovementsTest, CoreAndExtensionCoexist) {
    // Register a core API
    ApiRegistry::ApiMetadata core_meta;
    core_meta.name = "CoreApi_Test";
    core_meta.id = 7000;
    core_meta.pointer = (void*)0xC0DE0001;
    core_meta.type = BML_API_TYPE_CORE;
    core_meta.capabilities = BML_CAP_IMC_BASIC;
    core_meta.provider_mod = "BML";
    ApiRegistry::Instance().RegisterApi(core_meta);
    
    // Register an extension API
    BML_ApiId ext_id = ApiRegistry::Instance().RegisterExtension(
        "Extension_Test", 1, 0, (void*)0xE001, 16, "TestMod"
    );
    
    // Both should be queryable
    auto* core = ApiRegistry::Instance().QueryApi((BML_ApiId)7000);
    auto* ext = ApiRegistry::Instance().QueryApi(ext_id);
    
    ASSERT_NE(core, nullptr);
    ASSERT_NE(ext, nullptr);
    
    EXPECT_EQ(core->type, BML_API_TYPE_CORE);
    EXPECT_EQ(ext->type, BML_API_TYPE_EXTENSION);
    
    // Type-filtered enumeration should work
    size_t core_count = ApiRegistry::Instance().GetApiCount(BML_API_TYPE_CORE);
    size_t ext_count = ApiRegistry::Instance().GetApiCount(BML_API_TYPE_EXTENSION);
    
    EXPECT_GE(core_count, 1u);  // At least our test core API
    EXPECT_GE(ext_count, 1u);   // At least our test extension
}

TEST_F(ApiImprovementsTest, ExtensionCountTracking) {
    size_t initial_count = ApiRegistry::Instance().GetExtensionCount();
    
    ApiRegistry::Instance().RegisterExtension(
        "CountTest_Ext1", 1, 0, (void*)0x1, 16, "CountMod"
    );
    ApiRegistry::Instance().RegisterExtension(
        "CountTest_Ext2", 1, 0, (void*)0x2, 16, "CountMod"
    );
    
    EXPECT_EQ(ApiRegistry::Instance().GetExtensionCount(), initial_count + 2);
    
    ApiRegistry::Instance().Unregister("CountTest_Ext1");
    EXPECT_EQ(ApiRegistry::Instance().GetExtensionCount(), initial_count + 1);
}
