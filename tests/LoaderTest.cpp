/**
 * @file LoaderTest.cpp
 * @brief Unit tests for the BML v2 Loader (header-only API loading mechanism)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Mock implementation of bmlGetProcAddress
static void* g_MockProcTable[200] = {nullptr};
static const char* g_MockProcNames[200] = {nullptr};
static size_t g_MockProcCount = 0;
static bool g_SimulatePartialFailure = false;
static const char* g_FailingApiName = nullptr;

static void* MockGetProcAddress(const char* name) {
    if (g_SimulatePartialFailure && g_FailingApiName && strcmp(name, g_FailingApiName) == 0) {
        return nullptr;
    }
    
    for (size_t i = 0; i < g_MockProcCount; ++i) {
        if (strcmp(g_MockProcNames[i], name) == 0) {
            return g_MockProcTable[i];
        }
    }
    return nullptr;
}

// Dummy function pointers to simulate real implementations
static void DummyFunc() {}

static void ResetMockProcs() {
    g_MockProcCount = 0;
    g_SimulatePartialFailure = false;
    g_FailingApiName = nullptr;
    for (size_t i = 0; i < 200; ++i) {
        g_MockProcTable[i] = nullptr;
        g_MockProcNames[i] = nullptr;
    }
}

static void RegisterMockProc(const char* name, void* func) {
    g_MockProcNames[g_MockProcCount] = name;
    g_MockProcTable[g_MockProcCount] = func;
    g_MockProcCount++;
}

// Register all required APIs (names must match kBmlApiEntries in bml_loader_autogen.h)
static void RegisterAllRequiredApis() {
    // Core APIs (required - 12 total)
    RegisterMockProc("bmlContextRetain", (void*)DummyFunc);
    RegisterMockProc("bmlContextRelease", (void*)DummyFunc);
    RegisterMockProc("bmlGetGlobalContext", (void*)DummyFunc);
    RegisterMockProc("bmlGetRuntimeVersion", (void*)DummyFunc);
    RegisterMockProc("bmlContextSetUserData", (void*)DummyFunc);
    RegisterMockProc("bmlContextGetUserData", (void*)DummyFunc);
    RegisterMockProc("bmlRequestCapability", (void*)DummyFunc);
    RegisterMockProc("bmlCheckCapability", (void*)DummyFunc);
    RegisterMockProc("bmlGetModId", (void*)DummyFunc);
    RegisterMockProc("bmlGetModVersion", (void*)DummyFunc);
    RegisterMockProc("bmlRegisterShutdownHook", (void*)DummyFunc);
    RegisterMockProc("bmlCoreGetCaps", (void*)DummyFunc);

    // Logging APIs (required - 2 total)
    RegisterMockProc("bmlLog", (void*)DummyFunc);
    RegisterMockProc("bmlLoggingGetCaps", (void*)DummyFunc);

    // Config APIs (required - 3 total)
    RegisterMockProc("bmlConfigGet", (void*)DummyFunc);
    RegisterMockProc("bmlConfigSet", (void*)DummyFunc);
    RegisterMockProc("bmlConfigGetCaps", (void*)DummyFunc);

    // IMC APIs (required - 5 total)
    RegisterMockProc("bmlImcGetTopicId", (void*)DummyFunc);
    RegisterMockProc("bmlImcPublish", (void*)DummyFunc);
    RegisterMockProc("bmlImcSubscribe", (void*)DummyFunc);
    RegisterMockProc("bmlImcUnsubscribe", (void*)DummyFunc);
    RegisterMockProc("bmlImcGetCaps", (void*)DummyFunc);

    // Extension APIs (required - 4 total)
    RegisterMockProc("bmlExtensionRegister", (void*)DummyFunc);
    RegisterMockProc("bmlExtensionQuery", (void*)DummyFunc);
    RegisterMockProc("bmlExtensionLoad", (void*)DummyFunc);
    RegisterMockProc("bmlExtensionGetCaps", (void*)DummyFunc);
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
    BML_Result result = bmlLoadAPI(MockGetProcAddress);
    
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
 * Verify that passing nullptr as getProcAddress returns INVALID_ARGUMENT.
 */
TEST_F(LoaderTest, LoadAPI_NullGetProcAddress_ReturnsInvalidArgument) {
    // Act
    BML_Result result = bmlLoadAPI(nullptr);
    
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
    g_FailingApiName = "bmlContextRetain";  // This is marked as required
    
    // Act
    BML_Result result = bmlLoadAPI(MockGetProcAddress);
    
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
    g_FailingApiName = "bmlLogVa";  // This is marked as optional
    
    // Act
    BML_Result result = bmlLoadAPI(MockGetProcAddress);
    
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
    RegisterMockProc("bmlContextRetain", (void*)DummyFunc);
    RegisterMockProc("bmlContextRelease", (void*)DummyFunc);
    RegisterMockProc("bmlGetGlobalContext", (void*)DummyFunc);
    RegisterMockProc("bmlGetRuntimeVersion", (void*)DummyFunc);
    RegisterMockProc("bmlContextSetUserData", (void*)DummyFunc);
    RegisterMockProc("bmlContextGetUserData", (void*)DummyFunc);
    RegisterMockProc("bmlRequestCapability", (void*)DummyFunc);
    RegisterMockProc("bmlCheckCapability", (void*)DummyFunc);
    RegisterMockProc("bmlGetModId", (void*)DummyFunc);
    RegisterMockProc("bmlGetModVersion", (void*)DummyFunc);
    RegisterMockProc("bmlRegisterShutdownHook", (void*)DummyFunc);
    RegisterMockProc("bmlCoreGetCaps", (void*)DummyFunc);
    RegisterMockProc("bmlLog", (void*)DummyFunc);
    RegisterMockProc("bmlConfigGet", (void*)DummyFunc);
    RegisterMockProc("bmlConfigSet", (void*)DummyFunc);
    RegisterMockProc("bmlLoggingGetCaps", (void*)DummyFunc);
    RegisterMockProc("bmlConfigGetCaps", (void*)DummyFunc);
    RegisterMockProc("bmlImcGetTopicId", (void*)DummyFunc);
    RegisterMockProc("bmlImcPublish", (void*)DummyFunc);
    RegisterMockProc("bmlImcSubscribe", (void*)DummyFunc);
    RegisterMockProc("bmlImcUnsubscribe", (void*)DummyFunc);
    RegisterMockProc("bmlImcGetCaps", (void*)DummyFunc);
    RegisterMockProc("bmlExtensionRegister", (void*)DummyFunc);
    RegisterMockProc("bmlExtensionQuery", (void*)DummyFunc);
    RegisterMockProc("bmlExtensionLoad", (void*)DummyFunc);
    RegisterMockProc("bmlExtensionGetCaps", (void*)DummyFunc);
    // Skip: bmlLogVa, bmlSetLogFilter, bmlConfigReset, bmlConfigEnumerate,
    //       bmlImcPump, bmlImcPublishBuffer, all RPC APIs, all Handle APIs
    
    // Act
    BML_Result result = bmlLoadAPI(MockGetProcAddress);
    
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
    BML_Result result = bmlLoadAPI(MockGetProcAddress);
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
    ASSERT_EQ(bmlLoadAPI(MockGetProcAddress), BML_RESULT_OK);
    bmlUnloadAPI();
    ASSERT_FALSE(bmlIsApiLoaded());
    
    // Act - Reload
    BML_Result result = bmlLoadAPI(MockGetProcAddress);
    
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
    
    ASSERT_EQ(bmlLoadAPI(MockGetProcAddress), BML_RESULT_OK);
    
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
 * Test Case 10: Empty GetProcAddress (Returns Null for Everything)
 * Verify proper handling when getProcAddress returns null for all queries.
 */
TEST_F(LoaderTest, LoadAPI_EmptyGetProcAddress_ReturnsNotFound) {
    // Don't register any procs - MockGetProcAddress will return null for everything
    
    // Act
    BML_Result result = bmlLoadAPI(MockGetProcAddress);
    
    // Assert
    EXPECT_EQ(result, BML_RESULT_NOT_FOUND);
    EXPECT_FALSE(bmlIsApiLoaded());
}
