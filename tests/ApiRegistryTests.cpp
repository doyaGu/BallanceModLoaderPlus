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

using namespace BML::Core;

namespace {
void DummyFuncA() {}
void DummyFuncB() {}

constexpr BML_ApiId BML_API_ID_bmlTestGuarded = 90000u;
constexpr BML_ApiId BML_API_ID_bmlTestVoid = 90001u;
constexpr BML_ApiId BML_API_ID_bmlTestSimple = 90002u;

// Helper to create a minimal ApiMetadata for tests
ApiRegistry::ApiMetadata MakeTestMetadata(const char* name, BML_ApiId id, void* ptr) {
    ApiRegistry::ApiMetadata meta{};
    meta.name = name;
    meta.id = id;
    meta.pointer = ptr;
    meta.version_major = 1;
    meta.version_minor = 0;
    meta.version_patch = 0;
    meta.capabilities = 0;
    meta.type = BML_API_TYPE_CORE;
    meta.threading = BML_THREADING_FREE;
    meta.provider_mod = "test";
    return meta;
}

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
    void SetUp() override {
        ApiRegistry::Instance().Clear();
    }

    void TearDown() override {
        ApiRegistry::Instance().Clear();
    }
};

TEST_F(ApiRegistryTest, RegistrationPopulatesDirectAndCachedLookups) {
    auto &registry = ApiRegistry::Instance();
    constexpr BML_ApiId kId = 42;
    registry.RegisterApi(MakeTestMetadata("test.log", kId, reinterpret_cast<void *>(+DummyFuncA)));

    EXPECT_EQ(registry.GetByIdDirect(kId), reinterpret_cast<void *>(+DummyFuncA));
    EXPECT_EQ(registry.GetByIdCached(kId), reinterpret_cast<void *>(+DummyFuncA));
}

TEST_F(ApiRegistryTest, CachedLookupFallsBackWhenDirectSlotMissing) {
    auto &registry = ApiRegistry::Instance();
    const BML_ApiId high_id = static_cast<BML_ApiId>(MAX_DIRECT_API_ID + 25);
    registry.RegisterApi(MakeTestMetadata("test.high", high_id, reinterpret_cast<void *>(+DummyFuncB)));

    EXPECT_EQ(registry.GetByIdCached(high_id), reinterpret_cast<void *>(+DummyFuncB));
}

TEST_F(ApiRegistryTest, RegisterRejectsDuplicateIds) {
    auto &registry = ApiRegistry::Instance();
    registry.RegisterApi(MakeTestMetadata("api.one", 55, reinterpret_cast<void *>(+DummyFuncA)));
    registry.RegisterApi(MakeTestMetadata("api.two", 55, reinterpret_cast<void *>(+DummyFuncB)));

    EXPECT_EQ(registry.Get("api.two"), nullptr);
    EXPECT_EQ(registry.GetById(55), reinterpret_cast<void *>(+DummyFuncA));
}

TEST_F(ApiRegistryTest, TlsCacheInvalidatesAfterUnregister) {
    auto &registry = ApiRegistry::Instance();
    constexpr BML_ApiId kId = 77;
    registry.RegisterApi(MakeTestMetadata("api.cache", kId, reinterpret_cast<void *>(+DummyFuncA)));
    ASSERT_NE(registry.GetByIdCached(kId), nullptr);

    ASSERT_TRUE(registry.Unregister("api.cache"));
    EXPECT_EQ(registry.GetByIdCached(kId), nullptr);
}

TEST_F(ApiRegistryTest, CapabilitiesRecomputedAfterProviderUnregister) {
    auto &registry = ApiRegistry::Instance();
    ApiRegistry::ApiMetadata meta{};
    meta.name = "capsApi";
    meta.id = 401;
    meta.pointer = reinterpret_cast<void *>(+DummyFuncA);
    meta.version_major = 1;
    meta.version_minor = 0;
    meta.capabilities = BML_CAP_EXTENSION_BASIC;
    meta.type = BML_API_TYPE_EXTENSION;
    meta.threading = BML_THREADING_FREE;
    meta.provider_mod = "provider.test";
    registry.RegisterApi(meta);

    EXPECT_EQ(registry.GetTotalCapabilities(), BML_CAP_EXTENSION_BASIC);
    EXPECT_EQ(registry.UnregisterByProvider("provider.test"), 1u);
    EXPECT_EQ(registry.GetTotalCapabilities(), 0u);
}

TEST_F(ApiRegistryTest, RegisterExtensionRejectsConcurrentDuplicates) {
    auto &registry = ApiRegistry::Instance();
    std::atomic<int> successes{0};

    auto worker = [&]() {
        auto id = registry.RegisterExtension(
            "dup.ext",
            1,
            0,
            reinterpret_cast<void *>(+DummyFuncA),
            sizeof(void *),
            "provider.concurrent");
        if (id != BML_API_INVALID_ID) {
            successes.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::thread t1(worker);
    std::thread t2(worker);
    t1.join();
    t2.join();

    EXPECT_EQ(successes.load(std::memory_order_relaxed), 1);
}

TEST_F(ApiRegistryTest, ConcurrentRegisterAndLookupDoesNotCrash) {
    auto &registry = ApiRegistry::Instance();
    std::atomic<bool> running{true};

    std::thread lookup([&]() {
        while (running.load(std::memory_order_relaxed)) {
            registry.GetByIdCached(12345);
        }
    });

    for (int i = 0; i < 256; ++i) {
        std::string name = "api.concurrent." + std::to_string(i);
        registry.RegisterApi(MakeTestMetadata(
            name.c_str(),
            static_cast<BML_ApiId>(1000 + i),
            reinterpret_cast<void *>(+DummyFuncB)));
    }

    running.store(false, std::memory_order_relaxed);
    lookup.join();
}

TEST_F(ApiRegistryTest, GuardedRegistrationWrapsAndTranslatesErrors) {
    auto &registry = ApiRegistry::Instance();
    BML_REGISTER_API_GUARDED(bmlTestGuarded, "tests.guard", TestGuardedImpl);

    auto fn = reinterpret_cast<BML_Result (*)(int)>(
        registry.GetById(BML_API_ID_bmlTestGuarded));
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
    auto &registry = ApiRegistry::Instance();
    BML_REGISTER_API_VOID_GUARDED(bmlTestVoid, "tests.void", TestVoidImpl);

    auto fn = reinterpret_cast<void (*)(int)>(registry.GetById(BML_API_ID_bmlTestVoid));
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
    auto &registry = ApiRegistry::Instance();
    BML_REGISTER_API(bmlTestSimple, TestSimpleImpl);

    auto fn = reinterpret_cast<int (*)()>(registry.GetById(BML_API_ID_bmlTestSimple));
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn(), 42);
}

TEST(ApiRegistration, CoreApiSetFollowsDependencyOrder) {
    g_CoreRegistrationOrder.clear();
    ApiRegistry::CoreApiDescriptor descriptors[] = {
        {"NodeA", &CoreNodeA, 1u << 0, 0u},
        {"NodeB", &CoreNodeB, 1u << 1, 1u << 0},
        {"NodeC", &CoreNodeC, 1u << 2, (1u << 0) | (1u << 1)},
    };

    ApiRegistry::Instance().RegisterCoreApiSet(descriptors, std::size(descriptors));
    ASSERT_EQ(g_CoreRegistrationOrder.size(), 3u);
    EXPECT_EQ(g_CoreRegistrationOrder[0], 1);
    EXPECT_EQ(g_CoreRegistrationOrder[1], 2);
    EXPECT_EQ(g_CoreRegistrationOrder[2], 3);
}

TEST(ApiRegistration, CoreApiSetDetectsCycles) {
    ApiRegistry::CoreApiDescriptor descriptors[] = {
        {"NodeA", &CoreNodeA, 1u << 0, 1u << 1},
        {"NodeB", &CoreNodeB, 1u << 1, 1u << 0},
    };

    EXPECT_THROW(ApiRegistry::Instance().RegisterCoreApiSet(descriptors, 2),
                 std::runtime_error);
}
