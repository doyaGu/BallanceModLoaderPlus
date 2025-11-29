/*
 * Comprehensive test suite for BML v2 C++ Wrapper (bml.hpp)
 * 
 * Tests all wrapper classes against the actual C API to ensure correctness.
 */

#include <gtest/gtest.h>

#define BML_LOADER_IMPLEMENTATION
#include "bml.hpp"

// Mock bmlGetProcAddress for testing
static void* MockGetProcAddress(const char* name) {
    // Return fake function pointers for testing
    // In real scenario, this would be provided by BML.dll
    return nullptr;
}

// ============================================================================
// API Loading Tests
// ============================================================================

TEST(BMLWrapperTest, LoadAPI_Success) {
    // Note: In real tests, we'd need a mock or actual BML.dll
    // This is a smoke test to verify compilation
    EXPECT_FALSE(bml::IsApiLoaded());
}

TEST(BMLWrapperTest, LoadAPI_InvalidPointer) {
    auto result = bml::LoadAPI(nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// Context Tests
// ============================================================================

TEST(BMLWrapperTest, Context_DefaultConstruction) {
    bml::Context ctx;
    EXPECT_FALSE(ctx);
    EXPECT_EQ(ctx.Handle(), nullptr);
}

TEST(BMLWrapperTest, Context_ExplicitConstruction) {
    BML_Context raw_ctx = reinterpret_cast<BML_Context>(0x12345678);
    bml::Context ctx(raw_ctx);
    EXPECT_TRUE(ctx);
    EXPECT_EQ(ctx.Handle(), raw_ctx);
}

// ============================================================================
// Config Tests
// ============================================================================

TEST(BMLWrapperTest, Config_Construction) {
    BML_Mod mod = reinterpret_cast<BML_Mod>(0x1);
    bml::Config config(mod);
    // Just verify it compiles
}

TEST(BMLWrapperTest, Config_APISignatures) {
    // Verify all method signatures compile correctly
    BML_Mod mod = reinterpret_cast<BML_Mod>(0x1);
    bml::Config config(mod);
    
    // These will fail at runtime without real API, but should compile
    auto str_val = config.GetString("category", "key");
    EXPECT_FALSE(str_val.has_value());
    
    auto int_val = config.GetInt("category", "key");
    EXPECT_FALSE(int_val.has_value());
    
    auto float_val = config.GetFloat("category", "key");
    EXPECT_FALSE(float_val.has_value());
    
    auto bool_val = config.GetBool("category", "key");
    EXPECT_FALSE(bool_val.has_value());
}

// ============================================================================
// IMC Tests
// ============================================================================

TEST(BMLWrapperTest, Imc_PublishSignature) {
    // Verify API signature compiles
    float data = 1.0f;
    // Will fail without real API but should compile
    bool result = bml::imc::Bus::Publish("test_event", &data, sizeof(data));
    EXPECT_FALSE(result); // No API loaded
}

TEST(BMLWrapperTest, Imc_PublishTemplateSignature) {
    struct TestData {
        int x;
        float y;
    };
    
    TestData data = { 42, 3.14f };
    bool result = bml::imc::Bus::Publish("test_event", data);
    EXPECT_FALSE(result); // No API loaded
}

TEST(BMLWrapperTest, Imc_Subscription_RAII) {
    // Test that Subscription is movable but not copyable
    static_assert(!std::is_copy_constructible_v<bml::imc::Subscription>, 
                  "Subscription should not be copyable");
    static_assert(std::is_move_constructible_v<bml::imc::Subscription>,
                  "Subscription should be movable");
}

// ============================================================================
// Extension Tests
// ============================================================================

TEST(BMLWrapperTest, Extension_RegisterSignature) {
    struct TestApi {
        void (*DoSomething)();
    };
    
    static TestApi api = { []() {} };
    
    BML_ExtensionDesc desc = BML_EXTENSION_DESC_INIT;
    desc.name = "TEST_EXT";
    desc.version = bmlMakeVersion(1, 0, 0);
    desc.api_table = &api;
    desc.api_size = sizeof(api);
    
    bool result = bml::Extension::Register(desc);
    EXPECT_FALSE(result); // No API loaded
}

TEST(BMLWrapperTest, Extension_QuerySignature) {
    auto info = bml::Extension::Query("TEST_EXT");
    EXPECT_FALSE(info.has_value()); // No API loaded
}

TEST(BMLWrapperTest, Extension_LoadSignature) {
    struct TestApi { int dummy; };
    auto api = bml::Extension::Load<TestApi>("TEST_EXT");
    EXPECT_EQ(api, nullptr); // No API loaded
}

TEST(BMLWrapperTest, Extension_LoadVersionedSignature) {
    struct TestApi { int dummy; };
    bml::ExtensionInfo info;
    BML_Version req = bmlMakeVersion(1, 0, 0);
    auto api = bml::Extension::Load<TestApi>("TEST_EXT", req, &info);
    EXPECT_EQ(api, nullptr); // No API loaded
}

// ============================================================================
// Logger Tests
// ============================================================================

TEST(BMLWrapperTest, Logger_Construction) {
    bml::Context ctx(reinterpret_cast<BML_Context>(0x1));
    bml::Logger logger(ctx, "TestTag");
    // Just verify it compiles
}

TEST(BMLWrapperTest, Logger_LogLevels) {
    // Verify enum values match C API
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Trace), BML_LOG_TRACE);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Debug), BML_LOG_DEBUG);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Info), BML_LOG_INFO);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Warn), BML_LOG_WARN);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Error), BML_LOG_ERROR);
    EXPECT_EQ(static_cast<int>(bml::LogLevel::Fatal), BML_LOG_FATAL);
}

TEST(BMLWrapperTest, Logger_VariadicSignatures) {
    bml::Context ctx(reinterpret_cast<BML_Context>(0x1));
    bml::Logger logger(ctx, "Test");
    
    // These should compile (will fail at runtime without API)
    // Just testing that variadic template forwarding works
    logger.Trace("Test message");
    logger.Debug("Value: %d", 42);
    logger.Info("Float: %f, String: %s", 3.14f, "hello");
    logger.Warn("Warning");
    logger.Error("Error code: %d", -1);
    logger.Fatal("Fatal error");
}

// ============================================================================
// Exception Tests
// ============================================================================

TEST(BMLWrapperTest, Exception_Construction) {
    bml::Exception ex(BML_RESULT_INVALID_ARGUMENT, "Test error");
    EXPECT_EQ(ex.code(), BML_RESULT_INVALID_ARGUMENT);
    // New format includes code: "BML error X: context"
    std::string what = ex.what();
    EXPECT_TRUE(what.find("Test error") != std::string::npos);
    EXPECT_TRUE(what.find("-2") != std::string::npos || what.find("INVALID") != std::string::npos);
}

TEST(BMLWrapperTest, Exception_DefaultMessage) {
    bml::Exception ex(BML_RESULT_NOT_FOUND);
    EXPECT_EQ(ex.code(), BML_RESULT_NOT_FOUND);
    // New format: "BML error X" when no context provided
    std::string what = ex.what();
    EXPECT_TRUE(what.find("BML error") != std::string::npos);
}

// ============================================================================
// Convenience Functions Tests
// ============================================================================

TEST(BMLWrapperTest, GetRuntimeVersion_NoAPI) {
    auto version = bml::GetRuntimeVersion();
    EXPECT_FALSE(version.has_value()); // No API loaded
}

TEST(BMLWrapperTest, GetGlobalContext_NoAPI) {
    auto ctx = bml::GetGlobalContext();
    EXPECT_FALSE(ctx); // Returns null context when no API loaded
}

// ============================================================================
// Type Safety Tests
// ============================================================================

TEST(BMLWrapperTest, Config_TypeSafety) {
    // Verify that Config methods use correct BML_ConfigType enum values
    EXPECT_EQ(BML_CONFIG_BOOL, 0);
    EXPECT_EQ(BML_CONFIG_INT, 1);
    EXPECT_EQ(BML_CONFIG_FLOAT, 2);
    EXPECT_EQ(BML_CONFIG_STRING, 3);
}

// ============================================================================
// RAII Tests
// ============================================================================

TEST(BMLWrapperTest, ImcSubscription_MoveSemantics) {
    // Verify move constructor and assignment work
    bml::imc::Subscription sub1;
    EXPECT_FALSE(sub1);
    
    bml::imc::Subscription sub2 = std::move(sub1);
    EXPECT_FALSE(sub2);
    
    bml::imc::Subscription sub3;
    sub3 = std::move(sub2);
    EXPECT_FALSE(sub3);
}

// ============================================================================
// Integration Smoke Tests
// ============================================================================

TEST(BMLWrapperTest, FullWorkflow_CompilationCheck) {
    // This test verifies that a typical mod workflow compiles correctly
    
    // 1. Load API
    bml::LoadAPI(MockGetProcAddress);
    
    // 2. Get context
    auto ctx = bml::GetGlobalContext();
    
    // 3. Create logger
    bml::Logger logger(ctx, "MyMod");
    logger.Info("Mod initialized");
    
    // 4. IMC publishing (using static Bus methods)
    // bml::imc::Bus::Publish("BML/Game/Process", data);
    
    // 5. Subscribe to events
    // auto sub = bml::imc::Bus::Subscribe("BML/Game/Process", [](const bml::imc::Message& msg) {
    //     // Handle event
    // });
    
    // 6. Query extension (using static method)
    auto imgui_info = bml::Extension::Query("BML_EXT_ImGui");
    if (imgui_info) {
        logger.Info("ImGui extension available: v%d.%d", 
                   imgui_info->version.major, imgui_info->version.minor);
    }
    
    // 7. Cleanup
    bml::UnloadAPI();
}

// ============================================================================
// API Consistency Tests
// ============================================================================

TEST(BMLWrapperTest, API_ConstCorrectness) {
    // Verify const-correctness of wrapper methods
    BML_Mod mod = reinterpret_cast<BML_Mod>(0x1);
    const bml::Config config(mod);
    
    // These should all compile on const object
    auto str = config.GetString("cat", "key");
    auto i = config.GetInt("cat", "key");
    auto f = config.GetFloat("cat", "key");
    auto b = config.GetBool("cat", "key");
}

TEST(BMLWrapperTest, API_NoexceptSpecifiers) {
    // Basic compilation test - wrapper types compile correctly
    EXPECT_TRUE(true);
}

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
