#include "gtest/gtest.h"

#include <atomic>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "Core/ApiRegistrationMacros.h"
#include "Core/ApiRegistry.h"
#include "Core/CoreErrors.h"
#include "Core/DiagnosticManager.h"
#include "TestKernel.h"

using namespace BML::Core;

namespace {
using BML::Core::Testing::TestKernel;

void DummyFuncA() {}
void DummyFuncB() {}

BML_Result TestGuardedImpl(int value) {
    if (value < 0) {
        throw std::runtime_error("guarded failure");
    }
    if (value == 0) {
        return BML_RESULT_INVALID_ARGUMENT;
    }
    return BML_RESULT_OK;
}

void TestVoidImpl(int value) {
    if (value < 0) {
        throw std::logic_error("void failure");
    }
}

int TestSimpleImpl() {
    return 42;
}

std::vector<int> g_CoreRegistrationOrder;

void CoreNodeA() { g_CoreRegistrationOrder.push_back(1); }
void CoreNodeB() { g_CoreRegistrationOrder.push_back(2); }
void CoreNodeC() { g_CoreRegistrationOrder.push_back(3); }
}

class ApiRegistryTest : public ::testing::Test {
protected:
    TestKernel kernel_;

    void SetUp() override {
        kernel_->api_registry = std::make_unique<ApiRegistry>();
        kernel_->diagnostics = std::make_unique<DiagnosticManager>();
    }

    void TearDown() override {
    }
};

TEST_F(ApiRegistryTest, BasicRegistrationAndGetByName) {
    auto &registry = *kernel_->api_registry;
    registry.RegisterApi("test.log", reinterpret_cast<void *>(+DummyFuncA), "test");

    EXPECT_EQ(registry.Get("test.log"), reinterpret_cast<void *>(+DummyFuncA));
}

TEST_F(ApiRegistryTest, RegisterRejectsDuplicateNames) {
    auto &registry = *kernel_->api_registry;
    registry.RegisterApi("api.one", reinterpret_cast<void *>(+DummyFuncA), "test");
    registry.RegisterApi("api.one", reinterpret_cast<void *>(+DummyFuncB), "test");

    // First registration wins
    EXPECT_EQ(registry.Get("api.one"), reinterpret_cast<void *>(+DummyFuncA));
}

TEST_F(ApiRegistryTest, RegisterRejectsNullPointer) {
    auto &registry = *kernel_->api_registry;
    registry.RegisterApi("api.null", nullptr, "test");

    EXPECT_EQ(registry.Get("api.null"), nullptr);
    EXPECT_EQ(registry.GetApiCount(), 0u);

    BML_ApiId api_id = BML_API_INVALID_ID;
    EXPECT_FALSE(registry.GetApiId("api.null", &api_id));
}

TEST_F(ApiRegistryTest, UnregisterRemovesApi) {
    auto &registry = *kernel_->api_registry;
    registry.RegisterApi("api.cache", reinterpret_cast<void *>(+DummyFuncA), "test");
    ASSERT_NE(registry.Get("api.cache"), nullptr);

    ASSERT_TRUE(registry.Unregister("api.cache"));
    EXPECT_EQ(registry.Get("api.cache"), nullptr);
}

TEST_F(ApiRegistryTest, RegisterApiCopiesNameAndProviderStrings) {
    auto &registry = *kernel_->api_registry;
    std::string dynamicName = "dynamic.api";
    std::string dynamicProvider = "provider.dynamic";

    const char *name_ptr = dynamicName.c_str();
    const char *provider_ptr = dynamicProvider.c_str();

    registry.RegisterApi(name_ptr, reinterpret_cast<void *>(+DummyFuncA), provider_ptr);

    const std::string expected_name = "dynamic.api";
    const std::string expected_provider = "provider.dynamic";
    dynamicName.assign("mutated-name");
    dynamicProvider.assign("mutated-provider");

    // Verify API still accessible with original name
    EXPECT_NE(registry.Get(expected_name.c_str()), nullptr);

    // Verify unregister by provider still works with original provider name
    EXPECT_EQ(registry.UnregisterByProvider(expected_provider), 1u);
}

TEST_F(ApiRegistryTest, UnregisterByProviderRemovesAllFromProvider) {
    auto &registry = *kernel_->api_registry;
    registry.RegisterApi("api1", reinterpret_cast<void *>(+DummyFuncA), "provider.test");
    registry.RegisterApi("api2", reinterpret_cast<void *>(+DummyFuncB), "provider.test");
    registry.RegisterApi("api3", reinterpret_cast<void *>(+DummyFuncA), "other.provider");

    EXPECT_EQ(registry.UnregisterByProvider("provider.test"), 2u);
    EXPECT_EQ(registry.Get("api1"), nullptr);
    EXPECT_EQ(registry.Get("api2"), nullptr);
    EXPECT_NE(registry.Get("api3"), nullptr);
}

TEST_F(ApiRegistryTest, ConcurrentRegisterAndLookupDoesNotCrash) {
    auto &registry = *kernel_->api_registry;
    std::atomic<bool> running{true};

    std::thread lookup([&]() {
        while (running.load(std::memory_order_relaxed)) {
            registry.Get("api.concurrent.test");
        }
    });

    for (int i = 0; i < 256; ++i) {
        std::string name = "api.concurrent." + std::to_string(i);
        registry.RegisterApi(name.c_str(), reinterpret_cast<void *>(+DummyFuncB), "test");
    }

    running.store(false, std::memory_order_relaxed);
    lookup.join();
}

TEST_F(ApiRegistryTest, GuardedRegistrationWrapsAndTranslatesErrors) {
    auto &registry = *kernel_->api_registry;
    BML_REGISTER_API_GUARDED(bmlTestGuarded, "tests.guard", TestGuardedImpl);

    auto fn = reinterpret_cast<BML_Result (*)(int)>(
        registry.Get("bmlTestGuarded"));
    ASSERT_NE(fn, nullptr);

    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);
    ClearLastErrorInfo();
    EXPECT_EQ(fn(1), BML_RESULT_OK);
    EXPECT_EQ(GetLastErrorInfo(&info), BML_RESULT_NOT_FOUND);

    ClearLastErrorInfo();
    EXPECT_EQ(fn(0), BML_RESULT_INVALID_ARGUMENT);
    EXPECT_EQ(GetLastErrorInfo(&info), BML_RESULT_NOT_FOUND);

    ClearLastErrorInfo();
    EXPECT_EQ(fn(-1), BML_RESULT_FAIL);
    info.struct_size = sizeof(BML_ErrorInfo);
    ASSERT_EQ(GetLastErrorInfo(&info), BML_RESULT_OK);
    ASSERT_NE(info.message, nullptr);
    EXPECT_STRNE(info.message, "");
}

TEST_F(ApiRegistryTest, VoidGuardedRegistrationSuppressesExceptions) {
    auto &registry = *kernel_->api_registry;
    BML_REGISTER_API_VOID_GUARDED(bmlTestVoid, "tests.void", TestVoidImpl);

    auto fn = reinterpret_cast<void (*)(int)>(registry.Get("bmlTestVoid"));
    ASSERT_NE(fn, nullptr);

    BML_ErrorInfo info{};
    info.struct_size = sizeof(BML_ErrorInfo);
    ClearLastErrorInfo();
    fn(5);
    EXPECT_EQ(GetLastErrorInfo(&info), BML_RESULT_NOT_FOUND);

    ClearLastErrorInfo();
    fn(-1);
    info.struct_size = sizeof(BML_ErrorInfo);
    EXPECT_EQ(GetLastErrorInfo(&info), BML_RESULT_OK);
}

TEST_F(ApiRegistryTest, SimpleRegistrationKeepsExactPointer) {
    auto &registry = *kernel_->api_registry;
    BML_REGISTER_API(bmlTestSimple, TestSimpleImpl);

    auto fn = reinterpret_cast<int (*)()>(registry.Get("bmlTestSimple"));
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn(), 42);
}

TEST(ApiRegistration, CoreApiSetFollowsDependencyOrder) {
    TestKernel kernel;
    kernel->api_registry = std::make_unique<ApiRegistry>();

    g_CoreRegistrationOrder.clear();
    ApiRegistry::CoreApiDescriptor descriptors[] = {
        {"NodeA", &CoreNodeA, 1u << 0, 0u},
        {"NodeB", &CoreNodeB, 1u << 1, 1u << 0},
        {"NodeC", &CoreNodeC, 1u << 2, (1u << 0) | (1u << 1)},
    };

    kernel->api_registry->RegisterCoreApiSet(descriptors, std::size(descriptors));
    ASSERT_EQ(g_CoreRegistrationOrder.size(), 3u);
    EXPECT_EQ(g_CoreRegistrationOrder[0], 1);
    EXPECT_EQ(g_CoreRegistrationOrder[1], 2);
    EXPECT_EQ(g_CoreRegistrationOrder[2], 3);
}

TEST(ApiRegistration, CoreApiSetDetectsCycles) {
    TestKernel kernel;
    kernel->api_registry = std::make_unique<ApiRegistry>();

    ApiRegistry::CoreApiDescriptor descriptors[] = {
        {"NodeA", &CoreNodeA, 1u << 0, 1u << 1},
        {"NodeB", &CoreNodeB, 1u << 1, 1u << 0},
    };

    EXPECT_THROW(kernel->api_registry->RegisterCoreApiSet(descriptors, 2),
                 std::runtime_error);
}
