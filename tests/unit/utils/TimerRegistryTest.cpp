#include "Timer/TimerRegistry.h"
#include <gtest/gtest.h>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <new>

namespace {
thread_local bool countAllocations = false;
std::atomic<size_t> allocations{0};
class TimerRegistryTest : public testing::Test {
    void SetUp() override {
        BML::TimerScheduler::CancelAll();
        BML::TimerScheduler::ProcessAll(0, 0);
    }
    void TearDown() override {
        BML::TimerScheduler::CancelAll();
        BML::TimerScheduler::ProcessAll(0, 0);
    }
};
BML_TimerDefinition Definition(uint32_t type = BML_TIMER_LOOP) {
    BML_TimerDefinition value = BML_TIMER_DEFINITION_INIT;
    value.Name = "long timer name that cannot rely on short string optimization";
    value.Type = type;
    value.TimeBase = BML_TIMER_TIMEBASE_TICK;
    value.DelayTicks = 1;
    return value;
}
TEST_F(TimerRegistryTest, SelfCancelAndNestedReadKeepBorrowAndReleaseExactlyOnce) {
    BML::TimerRegistry registry;
    auto definition = Definition();
    int owner = 0, releases = 0, calls = 0;
    BML_TimerHandle handle = 0;
    ASSERT_EQ(registry.Register(
                  &owner, &definition,
                  [&](const BML_TimerEvent &event, uint32_t &keep) {
                      ++calls;
                      EXPECT_EQ(registry.Read(&owner, handle,
                                              [&](const BML_TimerEvent &borrow) {
                                                  EXPECT_EQ(registry.Control(&owner, handle, 2),
                                                            BML_OK);
                                                  EXPECT_EQ(releases, 0);
                                                  EXPECT_STREQ(borrow.Name, definition.Name);
                                                  EXPECT_EQ(registry.Control(&owner, handle, 1),
                                                            BML_ERROR_INVALID_HANDLE);
                                                  return BML_OK;
                                              }),
                                BML_OK);
                      EXPECT_EQ(releases, 0);
                      EXPECT_STREQ(event.Name, definition.Name);
                      keep = 1; // Cannot resurrect the explicitly cancelled timer.
                      return BML_OK;
                  },
                  [&] { ++releases; }, 0, 0, &handle),
              BML_OK);
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(releases, 0);
    BML::TimerScheduler::ProcessAll(1, 0);
    BML::TimerScheduler::ProcessAll(2, 0);
    registry.FinishProcess();
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(releases, 1);
    EXPECT_EQ(registry.Size(), 0u);
}
TEST_F(TimerRegistryTest, FailedRegistrationDoesNotTransferReleaseOrDamageExistingTimer) {
    BML::TimerRegistry registry;
    int owner = 0, other = 0, releases = 0;
    auto definition = Definition();
    BML_TimerHandle original = 0, failed = 99;
    auto callback = [](const BML_TimerEvent &, uint32_t &) { return BML_OK; };
    ASSERT_EQ(registry.Register(
                  &owner, &definition, callback, [&] { ++releases; }, 0, 0, &original),
              BML_OK);
    definition.DelayTicks = UINT64_MAX;
    EXPECT_EQ(registry.Register(
                  &owner, &definition, callback, [&] { ++releases; }, 1, 0, &failed),
              BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(failed, 0u);
    EXPECT_EQ(releases, 0);
    EXPECT_EQ(registry.Control(&other, original, 2), BML_ERROR_INVALID_HANDLE);
    EXPECT_TRUE(registry.Find(&owner, original));
    EXPECT_EQ(registry.Control(&owner, original, 2), BML_OK);
    EXPECT_EQ(releases, 1);
}
TEST_F(TimerRegistryTest, FiniteRepeatAndCallbackFailureShareTerminalCleanup) {
    BML::TimerRegistry registry;
    int owner = 0, calls = 0, releases = 0;
    auto definition = Definition(BML_TIMER_REPEAT);
    definition.RepeatCount = 3;
    BML_TimerHandle handle = 0;
    ASSERT_EQ(registry.Register(
                  &owner, &definition,
                  [&](const BML_TimerEvent &event, uint32_t &) {
                      EXPECT_EQ(event.CompletedIterations, static_cast<uint64_t>(calls));
                      EXPECT_EQ(event.RemainingIterations, 3 - calls);
                      ++calls;
                      return BML_OK;
                  },
                  [&] { ++releases; }, 0, 0, &handle),
              BML_OK);
    for (int tick = 1; tick <= 5; ++tick)
        BML::TimerScheduler::ProcessAll(tick, 0);
    registry.FinishProcess();
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(releases, 1);
    EXPECT_FALSE(registry.Find(&owner, handle));
    definition = Definition();
    ASSERT_EQ(registry.Register(
                  &owner, &definition,
                  [](const BML_TimerEvent &, uint32_t &) { return BML_ERROR_FAIL; },
                  [&] { ++releases; }, 5, 0, &handle),
              BML_OK);
    BML::TimerScheduler::ProcessAll(6, 0);
    EXPECT_EQ(releases, 2);
    EXPECT_EQ(registry.Size(), 0u);
}
TEST_F(TimerRegistryTest, AcceptsFrozenPrefixAndUnknownTailButRejectsShortInput) {
    BML::TimerRegistry registry;
    int owner = 0;
    struct Extended {
        BML_TimerDefinition Value;
        uint64_t Future;
    } extended{Definition(), 0xabcdef};
    extended.Value.StructSize = sizeof(extended);
    BML_TimerHandle handle = 0;
    auto callback = [](const BML_TimerEvent &, uint32_t &) { return BML_OK; };
    EXPECT_EQ(registry.Register(&owner, &extended.Value, callback, {}, 0, 0, &handle), BML_OK);
    EXPECT_EQ(extended.Future, 0xabcdefu);
    EXPECT_EQ(registry.Control(&owner, handle, 2), BML_OK);
    extended.Value.StructSize = BML_TIMER_DEFINITION_1_0_SIZE - 1;
    EXPECT_EQ(registry.Register(&owner, &extended.Value, callback, {}, 0, 0, &handle),
              BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(handle, 0u);
}
TEST_F(TimerRegistryTest, ContinuousWarmTimersDoNotAllocateOrCopyCallbackState) {
    for (int count : {1, 64, 256}) {
        BML::TimerRegistry registry;
        int owner = 0, calls = 0;
        auto definition = Definition();
        for (int i = 0; i < count; ++i) {
            BML_TimerHandle handle = 0;
            ASSERT_EQ(registry.Register(
                          &owner, &definition,
                          [&](const BML_TimerEvent &, uint32_t &) {
                              ++calls;
                              return BML_OK;
                          },
                          {}, 0, 0, &handle),
                      BML_OK);
        }
        BML::TimerScheduler::ProcessAll(1, 0);
        allocations = 0;
        countAllocations = true;
        for (int tick = 2; tick < 102; ++tick) {
            BML::TimerScheduler::ProcessAll(tick, 0);
            registry.FinishProcess();
        }
        countAllocations = false;
        EXPECT_EQ(allocations.load(), 0u) << count;
        EXPECT_EQ(calls, count * 101);
    }
}
} // namespace
void *operator new(size_t size) {
    if (countAllocations)
        ++allocations;
    if (auto *memory = std::malloc(size ? size : 1))
        return memory;
    throw std::bad_alloc();
}
void *operator new[](size_t size) { return ::operator new(size); }
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete[](void *memory) noexcept { std::free(memory); }
