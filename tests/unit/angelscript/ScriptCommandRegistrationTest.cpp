#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <new>
#include <string>

#include "AngelScript/ScriptCommandService.h"
#include "BML/Detail/Api.hpp"

namespace {
thread_local std::ptrdiff_t allocationCountdown = -1;

class FailAllocation {
public:
    explicit FailAllocation(std::ptrdiff_t index) { allocationCountdown = index; }

    ~FailAllocation() { allocationCountdown = -1; }
};
} // namespace

// Failure injection is confined to this test executable and the calling thread.
void *operator new(std::size_t size) {
    if (allocationCountdown == 0) {
        allocationCountdown = -1;
        throw std::bad_alloc();
    }
    if (allocationCountdown > 0)
        --allocationCountdown;
    if (void *memory = std::malloc(size ? size : 1))
        return memory;
    throw std::bad_alloc();
}

void *operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete(void *memory) noexcept {
    std::free(memory);
}

void operator delete[](void *memory) noexcept {
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void *memory, std::size_t) noexcept {
    std::free(memory);
}

namespace BML::Test {
TEST(ScriptCommandRegistrationTest, AllocationFailuresReleaseStateAndPermitRetry) {
    const std::string name(160, 'x');
    unsigned failures = 0;
    bool succeeded = false;
    for (std::ptrdiff_t index = 0; index < 128; ++index) {
        ScriptCommandService service;
        auto lifetime = std::make_shared<int>(42);
        std::weak_ptr<int> weak = lifetime;
        ScriptCommandRef *ref = nullptr;
        int code;
        {
            FailAllocation failure(index);
            code = BML::Detail::Guard([&] {
                ref = service.AddTestCommandForRelease(name, "", [lifetime](const auto &) {});
                return ref ? BML_OK : BML_ERROR_OUT_OF_MEMORY;
            });
        }
        lifetime.reset();
        if (code == BML_OK) {
            ASSERT_NE(nullptr, ref);
            ref->Release();
            service.Release();
            EXPECT_TRUE(weak.expired());
            succeeded = true;
            break;
        }
        ++failures;
        EXPECT_EQ(BML_ERROR_OUT_OF_MEMORY, code) << index;
        EXPECT_EQ(nullptr, ref) << index;
        EXPECT_EQ(0u, service.GetActiveCount()) << index;
        EXPECT_TRUE(weak.expired()) << index;
        auto *retry = service.AddTestCommandForRelease(name);
        EXPECT_NE(nullptr, retry) << index;
        if (retry)
            retry->Release();
        service.Release();
    }
    EXPECT_GT(failures, 0u);
    EXPECT_TRUE(succeeded);
}

TEST(ScriptCommandRegistrationTest, HostExceptionsRollBackRetainedEntry) {
    ScriptCommandService service;
    auto lifetime = std::make_shared<int>(42);
    std::weak_ptr<int> weak = lifetime;
    EXPECT_THROW(
        service.AddTestCommandForRelease(
            "retry", "", [lifetime](const auto &) {}, []() -> bool { throw std::bad_alloc(); }),
        std::bad_alloc);
    lifetime.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(0u, service.GetActiveCount());
    auto *retry = service.AddTestCommandForRelease("retry");
    ASSERT_NE(nullptr, retry);
    retry->Release();
}

TEST(ScriptCommandRegistrationTest, HostRejectionRollsBackRetainedEntry) {
    ScriptCommandService service;
    auto lifetime = std::make_shared<int>(42);
    std::weak_ptr<int> weak = lifetime;
    EXPECT_EQ(nullptr, service.AddTestCommandForRelease(
                           "retry", "", [lifetime](const auto &) {}, [] { return false; }));
    lifetime.reset();
    EXPECT_TRUE(weak.expired());
    EXPECT_EQ(0u, service.GetActiveCount());
    auto *retry = service.AddTestCommandForRelease("retry");
    ASSERT_NE(nullptr, retry);
    retry->Release();
}
} // namespace BML::Test
