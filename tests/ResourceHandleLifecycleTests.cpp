#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <cstdint>
#include <thread>
#include <vector>

#include "Core/ApiRegistry.h"
#include "Core/ResourceApi.h"

#include "bml_errors.h"
#include "bml_extension.h"
#include "bml_resource.h"

using BML::Core::ApiRegistry;

namespace {

using PFN_HandleCreate = BML_Result (*)(BML_HandleType, BML_HandleDesc *);
using PFN_HandleRetain = BML_Result (*)(const BML_HandleDesc *);
using PFN_HandleRelease = BML_Result (*)(const BML_HandleDesc *);
using PFN_HandleValidate = BML_Result (*)(const BML_HandleDesc *, BML_Bool *);
using PFN_HandleAttachUserData = BML_Result (*)(const BML_HandleDesc *, void *);
using PFN_HandleGetUserData = BML_Result (*)(const BML_HandleDesc *, void **);

class ResourceHandleLifecycleTests : public ::testing::Test {
protected:
    void SetUp() override {
        ApiRegistry::Instance().Clear();
        BML::Core::RegisterResourceApis();
    }

    template <typename Fn>
    Fn Lookup(const char *name) {
        auto fn = reinterpret_cast<Fn>(ApiRegistry::Instance().Get(name));
        if (!fn) {
            ADD_FAILURE() << "Missing API: " << name;
        }
        return fn;
    }

    BML_HandleType RegisterCountingType(std::atomic<int> &finalize_counter) {
        BML_ResourceTypeDesc desc{};
        desc.struct_size = sizeof(BML_ResourceTypeDesc);
        desc.name = "resource.handle.lifecycle";
        desc.user_data = &finalize_counter;
        desc.on_finalize = +[](BML_Context, const BML_HandleDesc *, void *user_data) {
            auto *counter = static_cast<std::atomic<int> *>(user_data);
            counter->fetch_add(1, std::memory_order_relaxed);
        };
        BML_HandleType type = 0;
        EXPECT_EQ(BML_RESULT_OK, BML::Core::RegisterResourceType(&desc, &type));
        return type;
    }
};

TEST_F(ResourceHandleLifecycleTests, FinalizeRunsExactlyOncePerHandleUnderConcurrentRelease) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto retain = Lookup<PFN_HandleRetain>("bmlHandleRetain");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(retain, nullptr);
    ASSERT_NE(release, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);
    ASSERT_NE(type, 0u);

    constexpr size_t kHandles = 64;
    constexpr int kExtraRetains = 2;

    std::vector<BML_HandleDesc> handles(kHandles);
    for (auto &desc : handles) {
        ASSERT_EQ(BML_RESULT_OK, create(type, &desc));
        for (int i = 0; i < kExtraRetains; ++i) {
            ASSERT_EQ(BML_RESULT_OK, retain(&desc));
        }
    }

    const size_t total_releases = kHandles * (static_cast<size_t>(kExtraRetains) + 1u);
    std::barrier start(static_cast<std::ptrdiff_t>(total_releases + 1));

    std::vector<std::thread> releasers;
    releasers.reserve(total_releases);
    for (const auto &desc : handles) {
        for (int i = 0; i < kExtraRetains + 1; ++i) {
            releasers.emplace_back([&, copy = desc] {
                start.arrive_and_wait();
                EXPECT_EQ(BML_RESULT_OK, release(&copy));
            });
        }
    }

    start.arrive_and_wait();
    for (auto &thread : releasers) {
        thread.join();
    }

    EXPECT_EQ(finalize_counter.load(std::memory_order_relaxed), static_cast<int>(kHandles));
}

TEST_F(ResourceHandleLifecycleTests, CreateRejectsDescriptorWithMismatchedStructSize) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    ASSERT_NE(create, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);

    BML_HandleDesc desc{};
    desc.struct_size = sizeof(BML_HandleDesc) - 4; // Intentionally too small
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, create(type, &desc));
    EXPECT_EQ(finalize_counter.load(), 0);
}

TEST_F(ResourceHandleLifecycleTests, SlotReuseIncrementsGenerationAndInvalidatesOldDescriptors) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    auto validate = Lookup<PFN_HandleValidate>("bmlHandleValidate");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(release, nullptr);
    ASSERT_NE(validate, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);

    BML_HandleDesc first{};
    ASSERT_EQ(BML_RESULT_OK, create(type, &first));
    ASSERT_EQ(BML_RESULT_OK, release(&first));

    BML_HandleDesc second{};
    ASSERT_EQ(BML_RESULT_OK, create(type, &second));

    BML_Bool valid = BML_FALSE;
    ASSERT_EQ(BML_RESULT_OK, validate(&first, &valid));
    EXPECT_EQ(valid, BML_FALSE);

    ASSERT_EQ(BML_RESULT_OK, validate(&second, &valid));
    EXPECT_EQ(valid, BML_TRUE);

    // Slot reuse should bump generation when same slot is recycled.
    EXPECT_EQ(first.slot, second.slot);
    EXPECT_NE(first.generation, second.generation);

    ASSERT_EQ(BML_RESULT_OK, release(&second));
    EXPECT_EQ(finalize_counter.load(std::memory_order_relaxed), 2);
}

TEST_F(ResourceHandleLifecycleTests, UserDataAttachesAndReadsFromMultipleThreads) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto attach = Lookup<PFN_HandleAttachUserData>("bmlHandleAttachUserData");
    auto get_data = Lookup<PFN_HandleGetUserData>("bmlHandleGetUserData");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(attach, nullptr);
    ASSERT_NE(get_data, nullptr);
    ASSERT_NE(release, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);

    BML_HandleDesc desc{};
    ASSERT_EQ(BML_RESULT_OK, create(type, &desc));

    int payload = 12345;
    ASSERT_EQ(BML_RESULT_OK, attach(&desc, &payload));

    constexpr size_t kReaders = 16;
    std::barrier start(static_cast<std::ptrdiff_t>(kReaders + 1));
    std::atomic<size_t> matches{0};

    std::vector<std::thread> threads;
    threads.reserve(kReaders);
    for (size_t i = 0; i < kReaders; ++i) {
        threads.emplace_back([&, copy = desc] {
            start.arrive_and_wait();
            void *value = nullptr;
            ASSERT_EQ(BML_RESULT_OK, get_data(&copy, &value));
            if (value == &payload) {
                matches.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    start.arrive_and_wait();
    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(matches.load(std::memory_order_relaxed), kReaders);
    ASSERT_EQ(BML_RESULT_OK, release(&desc));
    EXPECT_EQ(finalize_counter.load(std::memory_order_relaxed), 1);
}

// ========================================================================
// Additional Tests for Edge Cases
// ========================================================================

TEST_F(ResourceHandleLifecycleTests, DoubleReleaseReturnsInvalidState) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(release, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);

    BML_HandleDesc desc{};
    ASSERT_EQ(BML_RESULT_OK, create(type, &desc));
    
    // First release should succeed
    ASSERT_EQ(BML_RESULT_OK, release(&desc));
    EXPECT_EQ(finalize_counter.load(), 1);
    
    // Second release with same descriptor should fail (slot reused or invalid)
    BML_Result result = release(&desc);
    // Could be INVALID_ARGUMENT (wrong generation) or INVALID_STATE (underflow)
    EXPECT_NE(result, BML_RESULT_OK);
}

TEST_F(ResourceHandleLifecycleTests, ReleaseOnNullDescriptorReturnsInvalidArgument) {
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    ASSERT_NE(release, nullptr);
    
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, release(nullptr));
}

TEST_F(ResourceHandleLifecycleTests, RetainOnReleasedHandleReturnsError) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto retain = Lookup<PFN_HandleRetain>("bmlHandleRetain");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(retain, nullptr);
    ASSERT_NE(release, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);

    BML_HandleDesc desc{};
    ASSERT_EQ(BML_RESULT_OK, create(type, &desc));
    ASSERT_EQ(BML_RESULT_OK, release(&desc));
    
    // Retain on released handle should fail
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, retain(&desc));
}

TEST_F(ResourceHandleLifecycleTests, HandleOpsRejectShrunkStructs) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto retain = Lookup<PFN_HandleRetain>("bmlHandleRetain");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    auto attach = Lookup<PFN_HandleAttachUserData>("bmlHandleAttachUserData");
    auto get_user_data = Lookup<PFN_HandleGetUserData>("bmlHandleGetUserData");
    auto validate = Lookup<PFN_HandleValidate>("bmlHandleValidate");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(retain, nullptr);
    ASSERT_NE(release, nullptr);
    ASSERT_NE(attach, nullptr);
    ASSERT_NE(get_user_data, nullptr);
    ASSERT_NE(validate, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);

    BML_HandleDesc desc = BML_HANDLE_DESC_INIT;
    ASSERT_EQ(BML_RESULT_OK, create(type, &desc));

    auto shrink = [&](auto fn) {
        desc.struct_size = sizeof(BML_HandleDesc) - 1;
        EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, fn(&desc));
        desc.struct_size = sizeof(BML_HandleDesc);
    };

    shrink(retain);
    shrink(release);
    shrink([&](const BML_HandleDesc *d) { return attach(d, nullptr); });
    shrink([&](const BML_HandleDesc *d) {
        void *value = nullptr;
        return get_user_data(d, &value);
    });
    shrink([&](const BML_HandleDesc *d) {
        BML_Bool valid = BML_FALSE;
        return validate(d, &valid);
    });

    // Restore valid struct size and clean up
    desc.struct_size = sizeof(BML_HandleDesc);
    EXPECT_EQ(BML_RESULT_OK, release(&desc));
    EXPECT_EQ(finalize_counter.load(), 1);
}

TEST_F(ResourceHandleLifecycleTests, ValidateOnReleasedHandleReturnsFalse) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    auto validate = Lookup<PFN_HandleValidate>("bmlHandleValidate");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(release, nullptr);
    ASSERT_NE(validate, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);

    BML_HandleDesc desc{};
    ASSERT_EQ(BML_RESULT_OK, create(type, &desc));
    ASSERT_EQ(BML_RESULT_OK, release(&desc));
    
    BML_Bool valid = BML_TRUE;
    ASSERT_EQ(BML_RESULT_OK, validate(&desc, &valid));
    EXPECT_EQ(valid, BML_FALSE);
}

TEST_F(ResourceHandleLifecycleTests, HandleTypesAreIsolated) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    auto validate = Lookup<PFN_HandleValidate>("bmlHandleValidate");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(release, nullptr);
    ASSERT_NE(validate, nullptr);

    std::atomic<int> finalize_counter1{0};
    std::atomic<int> finalize_counter2{0};
    BML_HandleType type1 = RegisterCountingType(finalize_counter1);
    
    // Register a different type
    BML_ResourceTypeDesc desc2{};
    desc2.struct_size = sizeof(BML_ResourceTypeDesc);
    desc2.name = "resource.handle.type2";
    desc2.user_data = &finalize_counter2;
    desc2.on_finalize = +[](BML_Context, const BML_HandleDesc *, void *user_data) {
        auto *counter = static_cast<std::atomic<int> *>(user_data);
        counter->fetch_add(1, std::memory_order_relaxed);
    };
    BML_HandleType type2 = 0;
    ASSERT_EQ(BML_RESULT_OK, BML::Core::RegisterResourceType(&desc2, &type2));
    ASSERT_NE(type1, type2);

    // Create handles of both types
    BML_HandleDesc handle1{}, handle2{};
    ASSERT_EQ(BML_RESULT_OK, create(type1, &handle1));
    ASSERT_EQ(BML_RESULT_OK, create(type2, &handle2));

    // Validate both
    BML_Bool valid = BML_FALSE;
    ASSERT_EQ(BML_RESULT_OK, validate(&handle1, &valid));
    EXPECT_EQ(valid, BML_TRUE);
    ASSERT_EQ(BML_RESULT_OK, validate(&handle2, &valid));
    EXPECT_EQ(valid, BML_TRUE);

    // Release first type - should only call first finalizer
    ASSERT_EQ(BML_RESULT_OK, release(&handle1));
    EXPECT_EQ(finalize_counter1.load(), 1);
    EXPECT_EQ(finalize_counter2.load(), 0);

    // Release second type - should only call second finalizer
    ASSERT_EQ(BML_RESULT_OK, release(&handle2));
    EXPECT_EQ(finalize_counter1.load(), 1);
    EXPECT_EQ(finalize_counter2.load(), 1);
}

TEST_F(ResourceHandleLifecycleTests, GetUserDataOnNullDescriptorReturnsInvalidArgument) {
    auto get_data = Lookup<PFN_HandleGetUserData>("bmlHandleGetUserData");
    ASSERT_NE(get_data, nullptr);
    
    void* user_data = nullptr;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, get_data(nullptr, &user_data));
}

TEST_F(ResourceHandleLifecycleTests, GetUserDataOnNullOutputReturnsInvalidArgument) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto get_data = Lookup<PFN_HandleGetUserData>("bmlHandleGetUserData");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(get_data, nullptr);
    ASSERT_NE(release, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);

    BML_HandleDesc desc{};
    ASSERT_EQ(BML_RESULT_OK, create(type, &desc));
    
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, get_data(&desc, nullptr));
    
    ASSERT_EQ(BML_RESULT_OK, release(&desc));
}

TEST_F(ResourceHandleLifecycleTests, AttachUserDataOnReleasedHandleReturnsError) {
    auto create = Lookup<PFN_HandleCreate>("bmlHandleCreate");
    auto attach = Lookup<PFN_HandleAttachUserData>("bmlHandleAttachUserData");
    auto release = Lookup<PFN_HandleRelease>("bmlHandleRelease");
    ASSERT_NE(create, nullptr);
    ASSERT_NE(attach, nullptr);
    ASSERT_NE(release, nullptr);

    std::atomic<int> finalize_counter{0};
    BML_HandleType type = RegisterCountingType(finalize_counter);

    BML_HandleDesc desc{};
    ASSERT_EQ(BML_RESULT_OK, create(type, &desc));
    ASSERT_EQ(BML_RESULT_OK, release(&desc));
    
    int payload = 42;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, attach(&desc, &payload));
}

TEST_F(ResourceHandleLifecycleTests, ResourceCapsReportsCorrectCapabilities) {
    using PFN_ResourceGetCaps = BML_Result (*)(BML_ResourceCaps *);
    auto getCaps = Lookup<PFN_ResourceGetCaps>("bmlResourceGetCaps");
    ASSERT_NE(getCaps, nullptr);
    
    BML_ResourceCaps caps = BML_RESOURCE_CAPS_INIT;
    ASSERT_EQ(BML_RESULT_OK, getCaps(&caps));
    
    EXPECT_EQ(caps.struct_size, sizeof(BML_ResourceCaps));
    EXPECT_TRUE(caps.capability_flags & BML_RESOURCE_CAP_STRONG_REFERENCES);
    EXPECT_TRUE(caps.capability_flags & BML_RESOURCE_CAP_USER_DATA);
    EXPECT_TRUE(caps.capability_flags & BML_RESOURCE_CAP_THREAD_SAFE);
    EXPECT_TRUE(caps.capability_flags & BML_RESOURCE_CAP_TYPE_ISOLATION);
}

TEST_F(ResourceHandleLifecycleTests, ResourceCapsRejectsMismatchedStructSize) {
    using PFN_ResourceGetCaps = BML_Result (*)(BML_ResourceCaps *);
    auto getCaps = Lookup<PFN_ResourceGetCaps>("bmlResourceGetCaps");
    ASSERT_NE(getCaps, nullptr);

    BML_ResourceCaps caps{};
    caps.struct_size = sizeof(BML_ResourceCaps) - 1;
    EXPECT_EQ(BML_RESULT_INVALID_ARGUMENT, getCaps(&caps));
}

} // namespace
