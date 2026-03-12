/**
 * @file LoaderTest.cpp
 * @brief Unit tests for the BML v2 Loader (header-only API loading mechanism)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstring>

#include "bml_export.h"

struct MockProcEntry {
    const char *name;
    BML_ApiId id;
    void *proc;
};

static MockProcEntry g_MockProcs[200] = {};
static size_t g_MockProcCount = 0;
static bool g_SimulatePartialFailure = false;
static BML_ApiId g_FailingApiId = 0;

static void* MockGetProcAddress(const char* proc_name) {
    if (!proc_name) {
        return nullptr;
    }

    for (size_t i = 0; i < g_MockProcCount; ++i) {
        if (g_SimulatePartialFailure && g_FailingApiId != 0 && g_MockProcs[i].id == g_FailingApiId) {
            continue;
        }
        if (g_MockProcs[i].name && std::strcmp(g_MockProcs[i].name, proc_name) == 0) {
            return g_MockProcs[i].proc;
        }
    }
    return nullptr;
}

static void* MockGetProcAddressById(BML_ApiId api_id) {
    if (g_SimulatePartialFailure && g_FailingApiId != 0 && api_id == g_FailingApiId) {
        return nullptr;
    }
    
    for (size_t i = 0; i < g_MockProcCount; ++i) {
        if (g_MockProcs[i].id == api_id) {
            return g_MockProcs[i].proc;
        }
    }
    return nullptr;
}

// Dummy function pointers to simulate real implementations
static void DummyFunc() {}

static void ResetMockProcs() {
    g_MockProcCount = 0;
    g_SimulatePartialFailure = false;
    g_FailingApiId = 0;
    for (size_t i = 0; i < 200; ++i) {
        g_MockProcs[i] = {};
    }
}

static void RegisterMockProc(const char* name, BML_ApiId api_id, void* func) {
    g_MockProcs[g_MockProcCount].name = name;
    g_MockProcs[g_MockProcCount].id = api_id;
    g_MockProcs[g_MockProcCount].proc = func;
    g_MockProcCount++;
}

// Register all required APIs by stable ID.
static void RegisterAllRequiredApis() {
    // Core APIs (required - 12 total)
    RegisterMockProc("bmlContextRetain", BML_API_ID_bmlContextRetain, (void*)DummyFunc);
    RegisterMockProc("bmlContextRelease", BML_API_ID_bmlContextRelease, (void*)DummyFunc);
    RegisterMockProc("bmlGetGlobalContext", BML_API_ID_bmlGetGlobalContext, (void*)DummyFunc);
    RegisterMockProc("bmlGetRuntimeVersion", BML_API_ID_bmlGetRuntimeVersion, (void*)DummyFunc);
    RegisterMockProc("bmlContextSetUserData", BML_API_ID_bmlContextSetUserData, (void*)DummyFunc);
    RegisterMockProc("bmlContextGetUserData", BML_API_ID_bmlContextGetUserData, (void*)DummyFunc);
    RegisterMockProc("bmlRequestCapability", BML_API_ID_bmlRequestCapability, (void*)DummyFunc);
    RegisterMockProc("bmlCheckCapability", BML_API_ID_bmlCheckCapability, (void*)DummyFunc);
    RegisterMockProc("bmlGetModId", BML_API_ID_bmlGetModId, (void*)DummyFunc);
    RegisterMockProc("bmlGetModVersion", BML_API_ID_bmlGetModVersion, (void*)DummyFunc);
    RegisterMockProc("bmlRegisterShutdownHook", BML_API_ID_bmlRegisterShutdownHook, (void*)DummyFunc);
    RegisterMockProc("bmlCoreGetCaps", BML_API_ID_bmlCoreGetCaps, (void*)DummyFunc);

    // Logging APIs (required - 2 total)
    RegisterMockProc("bmlLog", BML_API_ID_bmlLog, (void*)DummyFunc);
    RegisterMockProc("bmlLoggingGetCaps", BML_API_ID_bmlLoggingGetCaps, (void*)DummyFunc);

    // Config APIs (required - 3 total)
    RegisterMockProc("bmlConfigGet", BML_API_ID_bmlConfigGet, (void*)DummyFunc);
    RegisterMockProc("bmlConfigSet", BML_API_ID_bmlConfigSet, (void*)DummyFunc);
    RegisterMockProc("bmlConfigGetCaps", BML_API_ID_bmlConfigGetCaps, (void*)DummyFunc);

    // IMC APIs (required - 5 total)
    RegisterMockProc("bmlImcGetTopicId", BML_API_ID_bmlImcGetTopicId, (void*)DummyFunc);
    RegisterMockProc("bmlImcPublish", BML_API_ID_bmlImcPublish, (void*)DummyFunc);
    RegisterMockProc("bmlImcSubscribe", BML_API_ID_bmlImcSubscribe, (void*)DummyFunc);
    RegisterMockProc("bmlImcUnsubscribe", BML_API_ID_bmlImcUnsubscribe, (void*)DummyFunc);
    RegisterMockProc("bmlImcGetCaps", BML_API_ID_bmlImcGetCaps, (void*)DummyFunc);

    // Extension APIs (required - 4 total)
    RegisterMockProc("bmlExtensionRegister", BML_API_ID_bmlExtensionRegister, (void*)DummyFunc);
    RegisterMockProc("bmlExtensionQuery", BML_API_ID_bmlExtensionQuery, (void*)DummyFunc);
    RegisterMockProc("bmlExtensionLoad", BML_API_ID_bmlExtensionLoad, (void*)DummyFunc);
    RegisterMockProc("bmlExtensionGetCaps", BML_API_ID_bmlExtensionGetCaps, (void*)DummyFunc);
}

// Now include the loader implementation
#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

class LoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        ResetMockProcs();
        // Ensure loader is unloaded before each test
        bmlUnloadAPI();
    }

    void TearDown() override {
        bmlUnloadAPI();
        ResetMockProcs();
    }
};

/**
 * Test Case 1: Successful API Loading
 * Verify that when all required APIs are available, bmlLoadAPI returns OK
 * and all function pointers are populated correctly.
 */
TEST_F(LoaderTest, LoadAPI_AllRequiredApis_Success) {
    RegisterAllRequiredApis();
    
    // Act
    BML_Result result = bmlLoadAPI(MockGetProcAddress, MockGetProcAddressById);
    
    // Assert
    EXPECT_EQ(result, BML_RESULT_OK);
    EXPECT_TRUE(bmlIsApiLoaded());
    
    // Verify critical function pointers are non-null
    EXPECT_NE(bmlContextRetain, nullptr);
    EXPECT_NE(bmlContextRelease, nullptr);
    EXPECT_NE(bmlGetGlobalContext, nullptr);
    EXPECT_NE(bmlLog, nullptr);
    EXPECT_NE(bmlConfigGet, nullptr);
    EXPECT_NE(bmlConfigSet, nullptr);
    EXPECT_NE(bmlImcPublish, nullptr);
    EXPECT_NE(bmlImcSubscribe, nullptr);
}

/**
 * Test Case 2: Null GetProcAddress
 * Verify that passing nullptr as the required name lookup callback returns INVALID_ARGUMENT.
 */
TEST_F(LoaderTest, LoadAPI_NullGetProcAddress_ReturnsInvalidArgument) {
    // Act
    BML_Result result = bmlLoadAPI(nullptr, nullptr);
    
    // Assert
    EXPECT_EQ(result, BML_RESULT_INVALID_ARGUMENT);
    EXPECT_FALSE(bmlIsApiLoaded());
}

/**
 * Test Case 3: Missing Required API
 * Verify that when a required API is missing, bmlLoadAPI fails and all
 * pointers are reset to null (graceful degradation).
 */
TEST_F(LoaderTest, LoadAPI_MissingRequiredApi_ReturnsNotFound) {
    RegisterAllRequiredApis();
    
    // Simulate missing required API
    g_SimulatePartialFailure = true;
    g_FailingApiId = BML_API_ID_bmlContextRetain;  // This is marked as required
    
    // Act
    BML_Result result = bmlLoadAPI(MockGetProcAddress, MockGetProcAddressById);
    
    // Assert
    EXPECT_EQ(result, BML_RESULT_NOT_FOUND);
    EXPECT_FALSE(bmlIsApiLoaded());
    
    // Verify all pointers are reset after failure
    EXPECT_EQ(bmlContextRetain, nullptr);
    EXPECT_EQ(bmlContextRelease, nullptr);
    EXPECT_EQ(bmlGetGlobalContext, nullptr);
}

/**
 * Test Case 4: Missing Optional API (Graceful Degradation)
 * Verify that when an optional API is missing, bmlLoadAPI still succeeds
 * and only the optional pointer remains null.
 */
TEST_F(LoaderTest, LoadAPI_MissingOptionalApi_Success) {
    RegisterAllRequiredApis();
    
    // Simulate missing optional API
    g_SimulatePartialFailure = true;
    g_FailingApiId = BML_API_ID_bmlLogVa;  // This is marked as optional
    
    // Act
    BML_Result result = bmlLoadAPI(MockGetProcAddress, MockGetProcAddressById);
    
    // Assert
    EXPECT_EQ(result, BML_RESULT_OK);
    EXPECT_TRUE(bmlIsApiLoaded());
    
    // Verify required pointers are still populated
    EXPECT_NE(bmlLog, nullptr);
    
    // Verify the optional pointer is null
    EXPECT_EQ(bmlLogVa, nullptr);
}

/**
 * Test Case 5: Multiple Optional APIs Missing
 * Verify loader can handle multiple missing optional APIs gracefully.
 */
TEST_F(LoaderTest, LoadAPI_MultipleOptionalApisMissing_Success) {
    // Only register required APIs, skip several optional ones
    RegisterMockProc("bmlContextRetain", BML_API_ID_bmlContextRetain, (void*)DummyFunc);
    RegisterMockProc("bmlContextRelease", BML_API_ID_bmlContextRelease, (void*)DummyFunc);
    RegisterMockProc("bmlGetGlobalContext", BML_API_ID_bmlGetGlobalContext, (void*)DummyFunc);
    RegisterMockProc("bmlGetRuntimeVersion", BML_API_ID_bmlGetRuntimeVersion, (void*)DummyFunc);
    RegisterMockProc("bmlContextSetUserData", BML_API_ID_bmlContextSetUserData, (void*)DummyFunc);
    RegisterMockProc("bmlContextGetUserData", BML_API_ID_bmlContextGetUserData, (void*)DummyFunc);
    RegisterMockProc("bmlRequestCapability", BML_API_ID_bmlRequestCapability, (void*)DummyFunc);
    RegisterMockProc("bmlCheckCapability", BML_API_ID_bmlCheckCapability, (void*)DummyFunc);
    RegisterMockProc("bmlGetModId", BML_API_ID_bmlGetModId, (void*)DummyFunc);
    RegisterMockProc("bmlGetModVersion", BML_API_ID_bmlGetModVersion, (void*)DummyFunc);
    RegisterMockProc("bmlRegisterShutdownHook", BML_API_ID_bmlRegisterShutdownHook, (void*)DummyFunc);
    RegisterMockProc("bmlCoreGetCaps", BML_API_ID_bmlCoreGetCaps, (void*)DummyFunc);
    RegisterMockProc("bmlLog", BML_API_ID_bmlLog, (void*)DummyFunc);
    RegisterMockProc("bmlConfigGet", BML_API_ID_bmlConfigGet, (void*)DummyFunc);
    RegisterMockProc("bmlConfigSet", BML_API_ID_bmlConfigSet, (void*)DummyFunc);
    RegisterMockProc("bmlLoggingGetCaps", BML_API_ID_bmlLoggingGetCaps, (void*)DummyFunc);
    RegisterMockProc("bmlConfigGetCaps", BML_API_ID_bmlConfigGetCaps, (void*)DummyFunc);
    RegisterMockProc("bmlImcGetTopicId", BML_API_ID_bmlImcGetTopicId, (void*)DummyFunc);
    RegisterMockProc("bmlImcPublish", BML_API_ID_bmlImcPublish, (void*)DummyFunc);
    RegisterMockProc("bmlImcSubscribe", BML_API_ID_bmlImcSubscribe, (void*)DummyFunc);
    RegisterMockProc("bmlImcUnsubscribe", BML_API_ID_bmlImcUnsubscribe, (void*)DummyFunc);
    RegisterMockProc("bmlImcGetCaps", BML_API_ID_bmlImcGetCaps, (void*)DummyFunc);
    RegisterMockProc("bmlExtensionRegister", BML_API_ID_bmlExtensionRegister, (void*)DummyFunc);
    RegisterMockProc("bmlExtensionQuery", BML_API_ID_bmlExtensionQuery, (void*)DummyFunc);
    RegisterMockProc("bmlExtensionLoad", BML_API_ID_bmlExtensionLoad, (void*)DummyFunc);
    RegisterMockProc("bmlExtensionGetCaps", BML_API_ID_bmlExtensionGetCaps, (void*)DummyFunc);
    // Skip: bmlLogVa, bmlSetLogFilter, bmlConfigReset, bmlConfigEnumerate,
    //       bmlImcPump, bmlImcPublishBuffer, all RPC APIs, all Handle APIs
    
    // Act
    BML_Result result = bmlLoadAPI(MockGetProcAddress, MockGetProcAddressById);
    
    // Assert
    EXPECT_EQ(result, BML_RESULT_OK);
    EXPECT_TRUE(bmlIsApiLoaded());
    
    // Verify optional pointers are null
    EXPECT_EQ(bmlLogVa, nullptr);
    EXPECT_EQ(bmlSetLogFilter, nullptr);
    EXPECT_EQ(bmlConfigReset, nullptr);
    EXPECT_EQ(bmlImcPump, nullptr);
    EXPECT_EQ(bmlImcRegisterRpc, nullptr);
    EXPECT_EQ(bmlHandleCreate, nullptr);
}

/**
 * Test Case 6: UnloadAPI Clears All Pointers
 * Verify that bmlUnloadAPI resets all function pointers to null.
 */
TEST_F(LoaderTest, UnloadAPI_ClearsAllPointers) {
    RegisterAllRequiredApis();
    
    // Load first
    BML_Result result = bmlLoadAPI(MockGetProcAddress, MockGetProcAddressById);
    ASSERT_EQ(result, BML_RESULT_OK);
    ASSERT_TRUE(bmlIsApiLoaded());
    
    // Act - Unload
    bmlUnloadAPI();
    
    // Assert
    EXPECT_FALSE(bmlIsApiLoaded());
    EXPECT_EQ(bmlContextRetain, nullptr);
    EXPECT_EQ(bmlContextRelease, nullptr);
    EXPECT_EQ(bmlLog, nullptr);
    EXPECT_EQ(bmlConfigGet, nullptr);
    EXPECT_EQ(bmlImcPublish, nullptr);
}

/**
 * Test Case 7: Reload After Unload
 * Verify that the loader can be reloaded after unloading.
 */
TEST_F(LoaderTest, ReloadAPI_AfterUnload_Success) {
    RegisterAllRequiredApis();
    
    // Load, unload, then reload
    ASSERT_EQ(bmlLoadAPI(MockGetProcAddress, MockGetProcAddressById), BML_RESULT_OK);
    bmlUnloadAPI();
    ASSERT_FALSE(bmlIsApiLoaded());
    
    // Act - Reload
    BML_Result result = bmlLoadAPI(MockGetProcAddress, MockGetProcAddressById);
    
    // Assert
    EXPECT_EQ(result, BML_RESULT_OK);
    EXPECT_TRUE(bmlIsApiLoaded());
    EXPECT_NE(bmlContextRetain, nullptr);
}

/**
 * Test Case 8: IsApiLoaded Before Loading
 * Verify that bmlIsApiLoaded returns false before any loading.
 */
TEST_F(LoaderTest, IsApiLoaded_BeforeLoading_ReturnsFalse) {
    EXPECT_FALSE(bmlIsApiLoaded());
}

/**
 * Test Case 9: Pointer Stability After Load
 * Verify that once loaded, the function pointers remain stable until unload.
 */
TEST_F(LoaderTest, LoadAPI_PointerStability_Maintained) {
    RegisterAllRequiredApis();
    
    ASSERT_EQ(bmlLoadAPI(MockGetProcAddress, MockGetProcAddressById), BML_RESULT_OK);
    
    // Capture pointers
    auto ptr1 = bmlContextRetain;
    auto ptr2 = bmlLog;
    auto ptr3 = bmlImcPublish;
    
    // Verify they don't change during loaded state
    EXPECT_EQ(bmlContextRetain, ptr1);
    EXPECT_EQ(bmlLog, ptr2);
    EXPECT_EQ(bmlImcPublish, ptr3);
}

/**
 * Test Case 10: Empty GetProcAddressById (Returns Null for Everything)
 * Verify proper handling when getProcAddressById returns null for all queries.
 */
TEST_F(LoaderTest, LoadAPI_EmptyGetProcAddressById_ReturnsNotFound) {
    // Don't register any procs - MockGetProcAddressById will return null for everything
    
    // Act
    BML_Result result = bmlLoadAPI(MockGetProcAddress, MockGetProcAddressById);

    // Assert
    EXPECT_EQ(result, BML_RESULT_NOT_FOUND);
    EXPECT_FALSE(bmlIsApiLoaded());
}

/**
 * Test Case 11: Name Fallback Without ID Lookup
 * Verify that the loader still succeeds when only the mandatory name-based
 * callback is available.
 */
TEST_F(LoaderTest, LoadAPI_NameFallbackOnly_Success) {
    RegisterAllRequiredApis();

    BML_Result result = bmlLoadAPI(MockGetProcAddress, nullptr);
    
    // Assert
    EXPECT_EQ(result, BML_RESULT_OK);
    EXPECT_TRUE(bmlIsApiLoaded());
    EXPECT_NE(bmlContextRetain, nullptr);
}
