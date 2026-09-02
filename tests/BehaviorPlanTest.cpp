#include "Behavior/Plan.h"

#include <map>
#include <memory>
#include <set>
#include <thread>
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
        if (BusyClose.contains(installation))
            return {Error::Busy, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR, "close is still in progress"};
        if (FailClose.contains(installation))
            return {Error::RevertConflict, CKERR_INVALIDPARAMETER,
                    CKBR_PARAMETERERROR, "close conflicted"};
        Live.erase(installation);
        return {};
    }

    Installation Next = 1;
    std::map<Installation, ObjectRef> Live;
    std::set<std::uint32_t> FailInstall;
    std::set<Installation> BusyClose;
    std::set<Installation> FailClose;
    std::vector<std::string> Calls;
};

TEST(BehaviorPlan, ReconcilesTheDesiredTargetSetDeterministically) {
    Plan plan({"mod", "patch"}, {"Gameplay_Events"});
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
    Plan plan({"mod", "patch"}, {"Gameplay_Events"});
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

TEST(BehaviorPlan, IsUnsatisfiedWhenNoScriptMatches) {
    Plan plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));

    ASSERT_TRUE(plan.Reconcile({}, 1, world));
    EXPECT_EQ(plan.State(), PlanState::Unsatisfied);
    EXPECT_EQ(plan.WorldEpoch(), 1u);
    EXPECT_EQ(plan.Size(), 0u);
    EXPECT_TRUE(world.Live.empty());
}

TEST(BehaviorPlan, EnforcesContinuousSingleTargetCardinality) {
    Plan plan({"mod", "single"}, {"Gameplay_Events", TargetSet::One});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));
    Status status = plan.Reconcile({Target(1), Target(2)}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TargetCardinality);
    EXPECT_EQ(plan.State(), PlanState::Unsatisfied);
    EXPECT_EQ(plan.Size(), 0u);
    EXPECT_TRUE(world.Live.empty());
}

TEST(BehaviorPlan, SingleTargetPlanWithoutTargetsIsUnsatisfiedNotFailed) {
    Plan plan({"mod", "single"}, {"Gameplay_Events", TargetSet::One});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));

    // Losing the only match is an ordinary wait, not a cardinality error.
    const Status status = plan.Reconcile({}, 1, world);
    EXPECT_TRUE(status);
    EXPECT_EQ(plan.State(), PlanState::Unsatisfied);
    EXPECT_EQ(plan.Size(), 0u);
    EXPECT_TRUE(world.Live.empty());

    ASSERT_TRUE(plan.Reconcile({Target(2)}, 1, world));
    EXPECT_EQ(plan.State(), PlanState::Active);
    EXPECT_TRUE(plan.Contains(Target(2)));
}

TEST(BehaviorPlan, RollsBackNewInstallationsAndKeepsTheCanonicalPlan) {
    Plan plan({"mod", "patch"}, {"Gameplay_Events"});
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
    Plan plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));
    world.FailClose.insert(world.Live.begin()->first);
    const Status status = plan.Reconcile({}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RevertConflict);
    EXPECT_EQ(plan.State(), PlanState::Conflicted);
    EXPECT_EQ(plan.Size(), 1u);
    EXPECT_EQ(world.Live.size(), 1u);
}

TEST(BehaviorPlan, ClosesEveryIndependentInstallationWhenOneNeedsRepair) {
    Plan plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1), Target(2), Target(3)}, 1, world));
    const Installation conflicted = world.Live.begin()->first;
    world.FailClose.insert(conflicted);

    const Status status = plan.Reconcile({}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RevertConflict);
    EXPECT_EQ(plan.State(), PlanState::Conflicted);
    EXPECT_EQ(plan.Size(), 1u);
    EXPECT_TRUE(world.Live.contains(conflicted));
    EXPECT_EQ(world.Live.size(), 1u);
}

TEST(BehaviorPlan, KeepsOnlyRollbackConflictsAfterAnInstallFailure) {
    Plan plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    world.FailInstall.insert(3);
    world.FailClose.insert(1);

    const Status status = plan.Reconcile(
        {Target(1), Target(2), Target(3)}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RevertConflict);
    EXPECT_EQ(plan.State(), PlanState::Conflicted);
    EXPECT_EQ(plan.Size(), 1u);
    EXPECT_EQ(world.Live.size(), 1u);
    EXPECT_TRUE(plan.Contains(Target(1)));
}

TEST(BehaviorPlan, RetirementIsIdempotentAndRejectsNewTargets) {
    Plan plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1), Target(2)}, 1, world));
    ASSERT_TRUE(plan.Retire(world));
    ASSERT_TRUE(plan.Retire(world));
    EXPECT_EQ(plan.State(), PlanState::Retiring);
    EXPECT_TRUE(world.Live.empty());
    EXPECT_FALSE(plan.Reconcile({Target(3)}, 1, world));
}

TEST(BehaviorPlans, AFreshlySubmittedPlanIsReconcilingNotUnsatisfied) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    // The script is already known, so the only thing keeping the Plan out of
    // the world is that no reconciliation pass has run yet.
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, world, plan));

    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Reconciling);
    EXPECT_EQ(info.Matches, 0u);
    EXPECT_EQ(info.Installations, 0u);
    EXPECT_EQ(info.World, 0u);
    EXPECT_TRUE(world->Live.empty());

    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Active);
    EXPECT_EQ(info.Installations, 1u);
}

TEST(BehaviorPlans, MatchesExactScriptNamesAtTheSafePoint) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, world, plan));
    ASSERT_NE(plan, 0u);

    ASSERT_TRUE(plans.LoadScript("Gameplay_Ingame", Target(1)));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(2)));
    EXPECT_TRUE(world->Live.empty());

    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    EXPECT_EQ(world->Live.begin()->second, Target(2));

    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Active);
    EXPECT_EQ(info.Matches, 1u);
    EXPECT_EQ(info.Installations, 1u);
    EXPECT_EQ(info.World, plans.WorldEpoch());
}

TEST(BehaviorPlans, ReconcilesContinuousSingleInstanceCardinality) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "single"}, 1, {"Gameplay_Events", TargetSet::One},
        world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);

    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(2)));
    Status status = plans.ProcessFrame();
    EXPECT_EQ(status.Code, Error::TargetCardinality);
    EXPECT_TRUE(world->Live.empty());

    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Unsatisfied);
    EXPECT_EQ(info.Matches, 2u);
    EXPECT_EQ(info.Installations, 0u);

    plans.Remove(Target(1));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    EXPECT_EQ(world->Live.begin()->second, Target(2));
}

TEST(BehaviorPlans, RemovingTheOnlyScriptLeavesASingleTargetPlanUnsatisfied) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "single"}, 1, {"Gameplay_Events", TargetSet::One},
        world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);

    // Deleting the host script is how a level ends, so the safe point must
    // report a clean teardown instead of a cardinality failure.
    plans.Remove(Target(1));
    EXPECT_TRUE(plans.ProcessFrame());
    EXPECT_TRUE(world->Live.empty());

    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Unsatisfied);
    EXPECT_EQ(info.Matches, 0u);
    EXPECT_EQ(info.Installations, 0u);
}

TEST(BehaviorPlans, LeavesTheOldWorldAndReinstallsAfterScriptLoad) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    const Epoch firstEpoch = plans.WorldEpoch();
    const Installation firstInstallation = world->Live.begin()->first;

    ASSERT_TRUE(plans.ResetWorld());
    EXPECT_GT(plans.WorldEpoch(), firstEpoch);
    EXPECT_TRUE(world->Live.empty());
    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Unsatisfied);
    EXPECT_EQ(info.World, 0u);
    EXPECT_EQ(info.Installations, 0u);

    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1, 2)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    EXPECT_FALSE(world->Live.contains(firstInstallation));
    EXPECT_EQ(world->Live.begin()->second, Target(1, 2));
    EXPECT_EQ(world->Calls,
              (std::vector<std::string>{"+1@1", "-1", "+1@2"}));
}

TEST(BehaviorPlans, TreatsReloadedObjectGenerationsAsNewInstances) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(5, 1)));
    ASSERT_TRUE(plans.ProcessFrame());

    plans.Remove(Target(5, 1));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(5, 2)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    EXPECT_EQ(world->Live.begin()->second, Target(5, 2));
    EXPECT_EQ(world->Calls,
              (std::vector<std::string>{"+5@1", "-1", "+5@1"}));
}

TEST(BehaviorPlans, ReplacesAKeyAndRetiresEveryPlanOwnedByAMod) {
    Plans plans;
    auto first = std::make_shared<FakeWorld>();
    auto replacement = std::make_shared<FakeWorld>();
    auto other = std::make_shared<FakeWorld>();
    PlanId oldId = 0;
    PlanId newId = 0;
    PlanId otherId = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, first, oldId));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());

    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Ingame"}, replacement, newId));
    EXPECT_NE(oldId, newId);
    EXPECT_TRUE(first->Live.empty());
    PlanInfo stale;
    EXPECT_EQ(plans.Read(oldId, stale).Code, Error::InvalidState);

    ASSERT_TRUE(plans.Submit(
        {"other", "events"}, 1, {"Gameplay_Events"}, other, otherId));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Ingame", Target(2)));
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_EQ(replacement->Live.size(), 1u);
    EXPECT_EQ(other->Live.size(), 1u);

    ASSERT_TRUE(plans.RetireOwner("mod"));
    EXPECT_TRUE(replacement->Live.empty());
    EXPECT_EQ(other->Live.size(), 1u);
    EXPECT_EQ(plans.Size(), 1u);
}

TEST(BehaviorPlans, RejectsHandlesFromAnotherOwnerGeneration) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));

    PlanInfo info;
    EXPECT_EQ(plans.Read("mod", 3, plan, info).Code, Error::OwnerInvalid);
    EXPECT_EQ(plans.Read("other", 4, plan, info).Code, Error::OwnerInvalid);
    EXPECT_EQ(plans.Close("mod", 3, plan).Code, Error::OwnerInvalid);
    EXPECT_EQ(plans.Size(), 1u);

    ASSERT_TRUE(plans.Read("mod", 4, plan, info));
    ASSERT_TRUE(plans.Close("mod", 4, plan));
    EXPECT_EQ(plans.Size(), 0u);
}

TEST(BehaviorPlans, FinishesAClosingPlanAtALaterSafePoint) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    const Installation installation = world->Live.begin()->first;
    world->BusyClose.insert(installation);

    const Status closing = plans.Close("mod", 4, plan);
    EXPECT_EQ(closing.Code, Error::Busy);
    PlanInfo info;
    ASSERT_TRUE(plans.Read("mod", 4, plan, info));
    EXPECT_EQ(info.State, PlanState::Retiring);
    EXPECT_EQ(info.Installations, 1u);

    world->BusyClose.clear();
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_TRUE(world->Live.empty());
    EXPECT_EQ(plans.Size(), 0u);
    EXPECT_EQ(plans.Read(plan, info).Code, Error::InvalidState);
}

TEST(BehaviorPlans, AcceptsRetirementFromAnotherThread) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    const Installation installation = world->Live.begin()->first;
    world->BusyClose.insert(installation);

    Status closing;
    std::thread closer([&] {
        closing = plans.Close("mod", 4, plan);
    });
    closer.join();

    EXPECT_EQ(closing.Code, Error::Busy);
    PlanInfo info;
    ASSERT_TRUE(plans.Read("mod", 4, plan, info));
    EXPECT_EQ(info.State, PlanState::Retiring);
    EXPECT_EQ(info.Installations, 1u);

    world->BusyClose.clear();
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_TRUE(world->Live.empty());
    EXPECT_EQ(plans.Size(), 0u);
}

TEST(BehaviorPlans, RetriesAConflictedRetirementWithoutTheCallerHandle) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    const Installation installation = world->Live.begin()->first;
    world->FailClose.insert(installation);

    const Status conflicted = plans.Close("mod", 4, plan);
    EXPECT_EQ(conflicted.Code, Error::RevertConflict);
    PlanInfo info;
    ASSERT_TRUE(plans.Read("mod", 4, plan, info));
    EXPECT_EQ(info.State, PlanState::Conflicted);

    world->FailClose.clear();
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_TRUE(world->Live.empty());
    EXPECT_EQ(plans.Size(), 0u);
}

TEST(BehaviorPlans, KeepsThePreviousPlanWhenReplacementCannotRetireIt) {
    Plans plans;
    auto first = std::make_shared<FakeWorld>();
    auto replacement = std::make_shared<FakeWorld>();
    PlanId oldId = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, first, oldId));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    first->FailClose.insert(first->Live.begin()->first);

    PlanId newId = 0;
    const Status status = plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Ingame"}, replacement, newId);
    EXPECT_EQ(status.Code, Error::RevertConflict);
    EXPECT_EQ(newId, 0u);
    EXPECT_EQ(plans.Size(), 1u);
    PlanInfo old;
    ASSERT_TRUE(plans.Read(oldId, old));
    EXPECT_EQ(old.State, PlanState::Conflicted);
    EXPECT_EQ(old.Installations, 1u);
    EXPECT_TRUE(replacement->Live.empty());
}

TEST(BehaviorPlans, ObjectDeletionRemovesEveryGenerationForThatIdentity) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(7, 1)));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(7, 2)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 2u);

    plans.RemoveObject(3, 7);
    EXPECT_EQ(world->Live.size(), 2u);
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_TRUE(world->Live.empty());
    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.Matches, 0u);
    EXPECT_EQ(info.Installations, 0u);
}

TEST(BehaviorPlans, WorldResetDropsOldInstallationIdentityAfterAConflict) {
    Plans plans;
    auto world = std::make_shared<FakeWorld>();
    PlanId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    world->FailClose.insert(world->Live.begin()->first);

    const Status status = plans.ResetWorld();
    EXPECT_EQ(status.Code, Error::RevertConflict);
    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Unsatisfied);
    EXPECT_EQ(info.World, 0u);
    EXPECT_EQ(info.Installations, 0u);
    EXPECT_EQ(info.Diagnostic.Code, Error::RevertConflict);
}

} // namespace
