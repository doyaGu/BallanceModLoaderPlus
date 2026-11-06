#include "Loader/ModInvocationGate.h"

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
        EXPECT_TRUE(outer.IsOutermost());
        EXPECT_TRUE(gate.IsCallActiveOnCurrentThread());
        auto inner = gate.LockCall();
        EXPECT_FALSE(inner.IsOutermost());
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

TEST(ModInvocationGateTest, NestedMutationsAreReentrant) {
    BML::ModInvocationGate gate;
    EXPECT_FALSE(gate.IsMutationActiveOnCurrentThread());
    {
        auto outer = gate.LockMutation();
        EXPECT_TRUE(gate.IsMutationActiveOnCurrentThread());
        auto inner = gate.LockMutation();
        EXPECT_TRUE(gate.IsMutationActiveOnCurrentThread());
        auto call = gate.LockCall();
        EXPECT_TRUE(call.IsOutermost());
        EXPECT_TRUE(gate.IsCallActiveOnCurrentThread());
    }
    EXPECT_FALSE(gate.IsMutationActiveOnCurrentThread());
    EXPECT_FALSE(gate.IsCallActiveOnCurrentThread());
}

TEST(ModInvocationGateTest, BorrowSurvivesNestedReadsAndEndsWithOuterCall) {
    BML::ModInvocationGate gate;
    std::weak_ptr<int> first,second;
    {
        auto outer=gate.LockCall();
        auto a=std::make_shared<int>(7);first=a;
        gate.HoldBorrow(a);gate.HoldBorrow(a);a.reset();
        {
            auto nested=gate.LockCall();
            auto b=std::make_shared<int>(9);second=b;
            gate.HoldBorrow(b);b.reset();
        }
        EXPECT_EQ(gate.CallDepth(),1u);
        EXPECT_EQ(*first.lock(),7);
        EXPECT_EQ(*second.lock(),9);
    }
    EXPECT_TRUE(first.expired());EXPECT_TRUE(second.expired());
    {auto next=gate.LockCall();EXPECT_EQ(gate.CallDepth(),1u);}
    EXPECT_EQ(gate.CallDepth(),0u);
}

#include "Api/HandleSequence.h"
#include <limits>
TEST(ModInvocationGateTest, ExhaustedSequenceNeverReusesPublishedHandles) {
    std::atomic<uint64_t> next{std::numeric_limits<uint64_t>::max()-1};
    EXPECT_EQ(BML::Api::NextHandle(next),UINT64_MAX-1);
    EXPECT_EQ(BML::Api::NextHandle(next),UINT64_MAX);
    EXPECT_EQ(BML::Api::NextHandle(next),0u);
    EXPECT_EQ(BML::Api::NextHandle(next),0u);
}
