#include "ModInvocationGate.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include <gtest/gtest.h>

TEST(ModInvocationGateTest, MutationWaitsForActiveCall) {
    BML::ModInvocationGate gate;
    std::promise<void> entered;
    std::promise<void> release;
    std::shared_future<void> releaseFuture(release.get_future());

    std::thread caller([&] {
        auto lock = gate.LockCall();
        entered.set_value();
        releaseFuture.wait();
    });
    entered.get_future().wait();

    auto mutation = std::async(std::launch::async, [&] {
        auto lock = gate.LockMutation();
        return true;
    });
    EXPECT_EQ(mutation.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);

    release.set_value();
    caller.join();
    EXPECT_TRUE(mutation.get());
}

TEST(ModInvocationGateTest, NestedCallsAreReentrantAndDetectable) {
    BML::ModInvocationGate gate;
    EXPECT_FALSE(gate.IsCallActiveOnCurrentThread());
    {
        auto outer = gate.LockCall();
        EXPECT_TRUE(gate.IsCallActiveOnCurrentThread());
        auto inner = gate.LockCall();
        EXPECT_TRUE(gate.IsCallActiveOnCurrentThread());
    }
    EXPECT_FALSE(gate.IsCallActiveOnCurrentThread());
}

TEST(ModInvocationGateTest, InterleavedGateInstancesPreserveOuterCallState) {
    BML::ModInvocationGate first;
    BML::ModInvocationGate second;

    auto firstCall = first.LockCall();
    EXPECT_TRUE(first.IsCallActiveOnCurrentThread());
    {
        auto secondCall = second.LockCall();
        EXPECT_TRUE(first.IsCallActiveOnCurrentThread());
        EXPECT_TRUE(second.IsCallActiveOnCurrentThread());
    }
    EXPECT_TRUE(first.IsCallActiveOnCurrentThread());
    EXPECT_FALSE(second.IsCallActiveOnCurrentThread());
}
