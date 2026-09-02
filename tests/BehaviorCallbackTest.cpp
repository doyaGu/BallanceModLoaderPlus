#include "Behavior/HookBlock.h"

#include <stdexcept>
#include <thread>
#include <utility>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

struct ReferenceCounts {
    int Retains = 0;
    int Releases = 0;
};

void Retain(void *state) {
    ++static_cast<ReferenceCounts *>(state)->Retains;
}

void Release(void *state) {
    ++static_cast<ReferenceCounts *>(state)->Releases;
}

int Noop(const CKBehaviorContext *, void *) {
    return CKBR_OK;
}

TEST(BehaviorCallback, RetainsOnceAcrossLeasesAndReleasesAtSafePoint) {
    ReferenceCounts counts;
    PlanCallbackState plan =
        PlanCallbackState::Retained(&counts, Retain, Release);
    CallbackLease first = plan.OpenLease();
    CallbackLease second = plan.OpenLease();

    EXPECT_EQ(counts.Retains, 1);
    EXPECT_EQ(first.Close(), CallbackCloseResult::Ready);
    plan.Retire();
    EXPECT_FALSE(plan.Collect());
    EXPECT_EQ(counts.Releases, 0);
    EXPECT_EQ(second.Close(), CallbackCloseResult::Ready);
    EXPECT_TRUE(plan.Collect());
    EXPECT_TRUE(plan.Collect());
    EXPECT_EQ(counts.Releases, 1);
}

TEST(BehaviorCallback, RepeatedOccurrencesOwnIndependentReferences) {
    ReferenceCounts counts;
    PlanCallbackState first =
        PlanCallbackState::Retained(&counts, Retain, Release);
    PlanCallbackState second =
        PlanCallbackState::Retained(&counts, Retain, Release);
    CallbackLease firstLease = first.OpenLease();
    CallbackLease secondLease = second.OpenLease();
    EXPECT_EQ(counts.Retains, 2);

    firstLease.Close();
    secondLease.Close();
    first.Retire();
    second.Retire();
    EXPECT_TRUE(first.Collect());
    EXPECT_TRUE(second.Collect());
    EXPECT_EQ(counts.Releases, 2);
}

TEST(BehaviorCallback, StaticStateNeverCallsReferenceHooks) {
    ReferenceCounts counts;
    PlanCallbackState plan = PlanCallbackState::Static(&counts);
    CallbackLease lease = plan.OpenLease();
    EXPECT_TRUE(lease);
    lease.Close();
    plan.Retire();
    EXPECT_TRUE(plan.Collect());
    EXPECT_EQ(counts.Retains, 0);
    EXPECT_EQ(counts.Releases, 0);
    EXPECT_EQ(plan.State(), &counts);
}

TEST(BehaviorCallback, NestedAndReentrantInvocationsAreCounted) {
    ReferenceCounts counts;
    PlanCallbackState plan =
        PlanCallbackState::Retained(&counts, Retain, Release);
    CallbackLease lease = plan.OpenLease();

    CallbackInvocation outer = lease.Enter();
    ASSERT_TRUE(outer);
    EXPECT_TRUE(outer.IsCurrent());
    EXPECT_TRUE(lease.IsCurrentInvocation());
    {
        CallbackInvocation inner = lease.Enter();
        ASSERT_TRUE(inner);
        EXPECT_TRUE(inner.IsCurrent());
        EXPECT_EQ(lease.Close(), CallbackCloseResult::Queued);
        EXPECT_FALSE(lease.Enter());
    }
    EXPECT_EQ(lease.State(), CallbackLeaseState::Closing);
    plan.Retire();
    EXPECT_FALSE(plan.Collect());
    outer = {};
    EXPECT_EQ(lease.State(), CallbackLeaseState::Closed);
    EXPECT_TRUE(plan.Collect());
    EXPECT_EQ(counts.Releases, 1);
}

TEST(BehaviorCallback, SelfCloseNeverWaitsAndRejectsNewAdmission) {
    ReferenceCounts counts;
    PlanCallbackState plan =
        PlanCallbackState::Retained(&counts, Retain, Release);
    CallbackLease lease = plan.OpenLease();

    CallbackCall call = InvokeCallback(lease, -1, [&] {
        EXPECT_TRUE(lease.IsCurrentInvocation());
        EXPECT_EQ(lease.Close(), CallbackCloseResult::Queued);
        EXPECT_FALSE(lease.Enter());
        return 7;
    });
    EXPECT_TRUE(call.Invoked);
    EXPECT_FALSE(call.Fault);
    EXPECT_EQ(call.ReturnCode, 7);
    EXPECT_EQ(lease.State(), CallbackLeaseState::Closed);

    CallbackCall rejected = InvokeCallback(lease, -9, [] { return 0; });
    EXPECT_FALSE(rejected.Invoked);
    EXPECT_EQ(rejected.ReturnCode, -9);
    EXPECT_EQ(rejected.Fault.Code, CallbackError::AdmissionClosed);
    plan.Retire();
    EXPECT_TRUE(plan.Collect());
}

TEST(BehaviorCallback, ExceptionsBecomeDiagnosticsAndDoNotEscape) {
    PlanCallbackState plan = PlanCallbackState::Static();
    CallbackLease lease = plan.OpenLease();

    CallbackCall known = InvokeCallback(lease, 99, []() -> int {
        throw std::runtime_error("author callback failed");
    });
    EXPECT_TRUE(known.Invoked);
    EXPECT_EQ(known.ReturnCode, 99);
    EXPECT_EQ(known.Fault.Code, CallbackError::Exception);
    EXPECT_EQ(known.Fault.Message, "author callback failed");

    CallbackCall unknown = InvokeCallback(lease, 101, []() -> int {
        throw 4;
    });
    EXPECT_EQ(unknown.ReturnCode, 101);
    EXPECT_EQ(unknown.Fault.Code, CallbackError::Exception);
    EXPECT_FALSE(unknown.Fault.Message.empty());
}

TEST(BehaviorCallback, OtherThreadCloseDoesNotWaitForInvocation) {
    ReferenceCounts counts;
    PlanCallbackState plan =
        PlanCallbackState::Retained(&counts, Retain, Release);
    CallbackLease lease = plan.OpenLease();
    CallbackInvocation invocation = lease.Enter();
    ASSERT_TRUE(invocation);

    CallbackCloseResult result = CallbackCloseResult::Closed;
    std::thread closer([&] { result = lease.Close(); });
    closer.join();
    EXPECT_EQ(result, CallbackCloseResult::Queued);
    EXPECT_EQ(lease.State(), CallbackLeaseState::Closing);

    plan.Retire();
    EXPECT_FALSE(plan.Collect());
    invocation = {};
    EXPECT_TRUE(plan.Collect());
    EXPECT_EQ(counts.Releases, 1);
}

TEST(BehaviorCallback, ReportsTheCurrentThreadInvocationExtent) {
    PlanCallbackState plan = PlanCallbackState::Static();
    CallbackLease outerLease = plan.OpenLease();
    CallbackLease innerLease = plan.OpenLease();
    EXPECT_FALSE(CallbackInvocation::Active());
    {
        CallbackInvocation outer = outerLease.Enter();
        ASSERT_TRUE(outer);
        EXPECT_TRUE(CallbackInvocation::Active());
        {
            CallbackInvocation inner = innerLease.Enter();
            ASSERT_TRUE(inner);
            EXPECT_TRUE(CallbackInvocation::Active());
        }
        EXPECT_TRUE(CallbackInvocation::Active());
    }
    EXPECT_FALSE(CallbackInvocation::Active());
}

TEST(BehaviorCallback, DurableHookOpensOneLeasePerInstallation) {
    ReferenceCounts counts;
    PlanCallbackState state =
        PlanCallbackState::Retained(&counts, Retain, Release);
    HookBlock::Hook hook(state, Noop, &counts);

    std::shared_ptr<HookBlock::Binding> first = hook.Bind();
    std::shared_ptr<HookBlock::Binding> second = hook.Bind();
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    EXPECT_NE(first.get(), second.get());
    EXPECT_EQ(counts.Retains, 1);

    first->CloseAdmission();
    EXPECT_TRUE(first->RetireAtSafePoint());
    first.reset();
    EXPECT_FALSE(state.Retired());
    EXPECT_EQ(second->State(), CallbackLeaseState::Open);

    std::shared_ptr<HookBlock::Binding> third = hook.Bind();
    ASSERT_TRUE(third);
    EXPECT_EQ(counts.Retains, 1);
    second->CloseAdmission();
    third->CloseAdmission();
    EXPECT_TRUE(second->RetireAtSafePoint());
    EXPECT_TRUE(third->RetireAtSafePoint());
    EXPECT_EQ(counts.Releases, 0);
}

TEST(BehaviorCallback, DurableHookReleasesAfterPlanAndEveryInstallation) {
    ReferenceCounts counts;
    PlanCallbackState state =
        PlanCallbackState::Retained(&counts, Retain, Release);
    std::shared_ptr<HookBlock::Binding> first;
    std::shared_ptr<HookBlock::Binding> second;
    {
        HookBlock::Hook hook(state, Noop, &counts);
        HookBlock::Hook copy = hook;
        first = hook.Bind();
        second = copy.Bind();
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        first->CloseAdmission();
        second->CloseAdmission();
    }

    EXPECT_TRUE(state.Retired());
    EXPECT_FALSE(first->RetireAtSafePoint());
    EXPECT_EQ(counts.Releases, 0);
    EXPECT_TRUE(second->RetireAtSafePoint());
    EXPECT_EQ(counts.Releases, 1);
    EXPECT_TRUE(first->RetireAtSafePoint());
    EXPECT_EQ(counts.Releases, 1);
}

} // namespace
