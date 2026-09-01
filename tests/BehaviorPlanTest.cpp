#include "Behavior/Plan.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

ObjectRef Target(std::uint32_t slot, std::uint32_t generation = 1) {
    return {3, slot, generation};
}

class FakeWorld final : public Plan::World {
public:
    Status Install(const PatchKey &, const ObjectRef &target, Epoch epoch,
                   Installation &out) override {
        Calls.push_back("+" + std::to_string(target.Slot) + "@" +
                        std::to_string(epoch));
        if (FailInstall.contains(target.Slot))
            return {Error::CreateFailed, CKERR_OUTOFMEMORY, CKBR_PARAMETERERROR,
                    "install failed"};
        out = Next++;
        Live[out] = target;
        return {};
    }

    Status Close(Installation installation) override {
        Calls.push_back("-" + std::to_string(installation));
        if (FailClose.contains(installation))
            return {Error::RevertConflict, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR, "close conflicted"};
        Live.erase(installation);
        return {};
    }

    Installation Next = 1;
    std::map<Installation, ObjectRef> Live;
    std::set<std::uint32_t> FailInstall;
    std::set<Installation> FailClose;
    std::vector<std::string> Calls;
};

TEST(BehaviorPlan, ReconcilesTheDesiredTargetSetDeterministically) {
    Plan plan({"mod", "patch"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(2), Target(1), Target(2)}, 4, world));
    EXPECT_EQ(plan.State(), PlanState::Active);
    EXPECT_EQ(plan.Size(), 2u);
    EXPECT_EQ(world.Calls,
              (std::vector<std::string>{"+1@4", "+2@4"}));

    ASSERT_TRUE(plan.Reconcile({Target(3), Target(2)}, 4, world));
    EXPECT_FALSE(plan.Contains(Target(1)));
    EXPECT_TRUE(plan.Contains(Target(2)));
    EXPECT_TRUE(plan.Contains(Target(3)));
    EXPECT_EQ(world.Calls.back(), "+3@4");
}

TEST(BehaviorPlan, ReplacesInstallationsAcrossWorldEpochs) {
    Plan plan({"mod", "patch"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 7, world));
    const Installation first = world.Live.begin()->first;
    ASSERT_TRUE(plan.Reconcile({Target(1, 2)}, 8, world));
    EXPECT_EQ(plan.WorldEpoch(), 8u);
    EXPECT_FALSE(world.Live.contains(first));
    EXPECT_TRUE(plan.Contains(Target(1, 2)));
    EXPECT_EQ(world.Calls,
              (std::vector<std::string>{"+1@7", "-1", "+1@8"}));
}

TEST(BehaviorPlan, EnforcesContinuousSingleTargetCardinality) {
    Plan plan({"mod", "single"}, TargetSet::One);
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));
    Status status = plan.Reconcile({Target(1), Target(2)}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TargetCardinality);
    EXPECT_EQ(plan.State(), PlanState::Unsatisfied);
    EXPECT_EQ(plan.Size(), 0u);
    EXPECT_TRUE(world.Live.empty());
}

TEST(BehaviorPlan, RollsBackNewInstallationsAndKeepsTheCanonicalPlan) {
    Plan plan({"mod", "patch"});
    FakeWorld world;
    world.FailInstall.insert(2);
    const Status status = plan.Reconcile({Target(1), Target(2)}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::CreateFailed);
    EXPECT_EQ(plan.State(), PlanState::Unsatisfied);
    EXPECT_EQ(plan.Size(), 0u);
    EXPECT_TRUE(world.Live.empty());

    world.FailInstall.clear();
    ASSERT_TRUE(plan.Reconcile({Target(1), Target(2)}, 1, world));
    EXPECT_EQ(plan.State(), PlanState::Active);
}

TEST(BehaviorPlan, PreservesAConflictedInstallationForRepair) {
    Plan plan({"mod", "patch"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));
    world.FailClose.insert(world.Live.begin()->first);
    const Status status = plan.Reconcile({}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RevertConflict);
    EXPECT_EQ(plan.State(), PlanState::RepairRequired);
    EXPECT_EQ(plan.Size(), 1u);
    EXPECT_EQ(world.Live.size(), 1u);
}

TEST(BehaviorPlan, ClosesEveryIndependentInstallationWhenOneNeedsRepair) {
    Plan plan({"mod", "patch"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1), Target(2), Target(3)}, 1, world));
    const Installation conflicted = world.Live.begin()->first;
    world.FailClose.insert(conflicted);

    const Status status = plan.Reconcile({}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RevertConflict);
    EXPECT_EQ(plan.State(), PlanState::RepairRequired);
    EXPECT_EQ(plan.Size(), 1u);
    EXPECT_TRUE(world.Live.contains(conflicted));
    EXPECT_EQ(world.Live.size(), 1u);
}

TEST(BehaviorPlan, KeepsOnlyRollbackConflictsAfterAnInstallFailure) {
    Plan plan({"mod", "patch"});
    FakeWorld world;
    world.FailInstall.insert(3);
    world.FailClose.insert(1);

    const Status status = plan.Reconcile(
        {Target(1), Target(2), Target(3)}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RevertConflict);
    EXPECT_EQ(plan.State(), PlanState::RepairRequired);
    EXPECT_EQ(plan.Size(), 1u);
    EXPECT_EQ(world.Live.size(), 1u);
    EXPECT_TRUE(plan.Contains(Target(1)));
}

TEST(BehaviorPlan, RetirementIsIdempotentAndRejectsNewTargets) {
    Plan plan({"mod", "patch"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1), Target(2)}, 1, world));
    ASSERT_TRUE(plan.Retire(world));
    ASSERT_TRUE(plan.Retire(world));
    EXPECT_EQ(plan.State(), PlanState::Retiring);
    EXPECT_TRUE(world.Live.empty());
    EXPECT_FALSE(plan.Reconcile({Target(3)}, 1, world));
}

} // namespace
