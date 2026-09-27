#include "Behavior/Install/Selection.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior::Internal;

ObjectRef Target(std::uint32_t slot, std::uint32_t generation = 1) {
    return {3, slot, generation};
}

class FakeWorld final : public Selection::World {
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
        if (OnInstall)
            OnInstall();
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
        if (OnClose)
            OnClose();
        return {};
    }

    Installation Next = 1;
    std::map<Installation, ObjectRef> Live;
    std::set<std::uint32_t> FailInstall;
    std::set<Installation> BusyClose;
    std::set<Installation> FailClose;
    std::vector<std::string> Calls;
    std::function<void()> OnInstall;
    std::function<void()> OnClose;
};

TEST(BehaviorSelection, ReconcilesTheDesiredTargetSetDeterministically) {
    Selection plan({"mod", "patch"}, {"Gameplay_Events"});
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

TEST(BehaviorSelection, ReplacesInstallationsAcrossWorldEpochs) {
    Selection plan({"mod", "patch"}, {"Gameplay_Events"});
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

TEST(BehaviorSelection, IsUnsatisfiedWhenNoScriptMatches) {
    Selection plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));

    ASSERT_TRUE(plan.Reconcile({}, 1, world));
    EXPECT_EQ(plan.State(), PlanState::Unsatisfied);
    EXPECT_EQ(plan.WorldEpoch(), 1u);
    EXPECT_EQ(plan.Size(), 0u);
    EXPECT_TRUE(world.Live.empty());
}

TEST(BehaviorSelection, EnforcesContinuousSingleTargetCardinality) {
    Selection plan({"mod", "single"}, {"Gameplay_Events", TargetSet::One});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));
    Status status = plan.Reconcile({Target(1), Target(2)}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::TargetCardinality);
    EXPECT_EQ(plan.State(), PlanState::Unsatisfied);
    EXPECT_EQ(plan.Size(), 0u);
    EXPECT_TRUE(world.Live.empty());
}

TEST(BehaviorSelection, SingleTargetPlanWithoutTargetsIsUnsatisfiedNotFailed) {
    Selection plan({"mod", "single"}, {"Gameplay_Events", TargetSet::One});
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

TEST(BehaviorSelection, RollsBackNewInstallationsAndKeepsTheCanonicalPlan) {
    Selection plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    world.FailInstall.insert(2);
    const Status status = plan.Reconcile({Target(1), Target(2)}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::CreateFailed);
    EXPECT_EQ(plan.State(), PlanState::Unsatisfied);
    EXPECT_EQ(plan.Size(), 0u);
    EXPECT_TRUE(world.Live.empty());
    EXPECT_EQ(plan.LastStatus().Code, Error::CreateFailed);
    EXPECT_EQ(plan.ApplyFailure().Code, Error::CreateFailed);
    EXPECT_TRUE(plan.RestoreFailure());

    world.FailInstall.clear();
    ASSERT_TRUE(plan.Reconcile({Target(1), Target(2)}, 1, world));
    EXPECT_EQ(plan.State(), PlanState::Active);
    EXPECT_TRUE(plan.LastStatus());
    EXPECT_TRUE(plan.ApplyFailure());
    EXPECT_TRUE(plan.RestoreFailure());
}

TEST(BehaviorSelection, IsPartialWhenAnExistingTargetSurvivesAnInstallFailure) {
    Selection plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));
    world.FailInstall.insert(2);

    const Status status = plan.Reconcile(
        {Target(1), Target(2)}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::CreateFailed);
    EXPECT_EQ(plan.State(), PlanState::Partial);
    EXPECT_EQ(plan.Size(), 1u);
    EXPECT_TRUE(plan.Contains(Target(1)));
    EXPECT_EQ(plan.ApplyFailure().Code, Error::CreateFailed);
    EXPECT_TRUE(plan.RestoreFailure());
}

TEST(BehaviorSelection, PreservesAConflictedInstallationForRepair) {
    Selection plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1)}, 1, world));
    world.FailClose.insert(world.Live.begin()->first);
    const Status status = plan.Reconcile({}, 1, world);
    EXPECT_FALSE(status);
    EXPECT_EQ(status.Code, Error::RevertConflict);
    EXPECT_EQ(plan.State(), PlanState::Conflicted);
    EXPECT_EQ(plan.Size(), 1u);
    EXPECT_EQ(world.Live.size(), 1u);
    EXPECT_EQ(plan.LastStatus().Code, Error::RevertConflict);
    EXPECT_TRUE(plan.ApplyFailure());
    EXPECT_EQ(plan.RestoreFailure().Code, Error::RevertConflict);
}

TEST(BehaviorSelection, ClosesEveryIndependentInstallationWhenOneNeedsRepair) {
    Selection plan({"mod", "patch"}, {"Gameplay_Events"});
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

TEST(BehaviorSelection, KeepsOnlyRollbackConflictsAfterAnInstallFailure) {
    Selection plan({"mod", "patch"}, {"Gameplay_Events"});
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
    EXPECT_EQ(plan.LastStatus().Code, Error::RevertConflict);
    EXPECT_EQ(plan.ApplyFailure().Code, Error::CreateFailed);
    EXPECT_EQ(plan.RestoreFailure().Code, Error::RevertConflict);
}

TEST(BehaviorSelection, RetirementIsIdempotentAndRejectsNewTargets) {
    Selection plan({"mod", "patch"}, {"Gameplay_Events"});
    FakeWorld world;
    ASSERT_TRUE(plan.Reconcile({Target(1), Target(2)}, 1, world));
    ASSERT_TRUE(plan.Retire(world));
    ASSERT_TRUE(plan.Retire(world));
    EXPECT_EQ(plan.State(), PlanState::Retiring);
    EXPECT_TRUE(world.Live.empty());
    EXPECT_FALSE(plan.Reconcile({Target(3)}, 1, world));
}

TEST(BehaviorSelections, AFreshlySubmittedPlanIsReconcilingNotUnsatisfied) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
    // The script is already known, so the only thing keeping the Selection out of
    // the world is that no reconciliation pass has run yet.
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, world, plan));

    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Reconciling);
    EXPECT_EQ(info.Matches, 0u);
    EXPECT_EQ(info.Instances, 0u);
    EXPECT_EQ(info.World, 0u);
    EXPECT_TRUE(world->Live.empty());

    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Active);
    EXPECT_EQ(info.Instances, 1u);
}

TEST(BehaviorSelections, MatchesExactScriptNamesAtTheSafePoint) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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
    EXPECT_EQ(info.Instances, 1u);
    EXPECT_EQ(info.World, plans.WorldEpoch());
}

TEST(BehaviorSelections, InstallationSnapshotsChangeWhenTheirWorldBindingChanges) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(2)));
    ASSERT_TRUE(plans.ProcessFrame());

    std::vector<InstallationInfo> first;
    ASSERT_TRUE(plans.ReadInstallations("mod", 1, plan, first));
    ASSERT_EQ(first.size(), 1u);
    EXPECT_EQ(first[0].Target, Target(2));
    EXPECT_EQ(first[0].World, plans.WorldEpoch());
    EXPECT_NE(first[0].Id, 0u);
    EXPECT_NE(first[0].Revision, 0u);

    plans.Remove(Target(2));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(3)));
    ASSERT_TRUE(plans.ProcessFrame());

    std::vector<InstallationInfo> second;
    ASSERT_TRUE(plans.ReadInstallations("mod", 1, plan, second));
    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(second[0].Target, Target(3));
    EXPECT_NE(second[0].Id, first[0].Id);
    EXPECT_NE(second[0].Revision, first[0].Revision);
}

TEST(BehaviorSelections, ReconcilesContinuousSingleInstanceCardinality) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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
    EXPECT_EQ(info.Instances, 0u);
    EXPECT_EQ(info.LastStatus.Code, Error::TargetCardinality);
    EXPECT_EQ(info.ApplyFailure.Code, Error::TargetCardinality);
    EXPECT_TRUE(info.RestoreFailure);

    plans.Remove(Target(1));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    EXPECT_EQ(world->Live.begin()->second, Target(2));
}

TEST(BehaviorSelections, RemovingTheOnlyScriptLeavesASingleTargetPlanUnsatisfied) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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
    EXPECT_EQ(info.Instances, 0u);
}

TEST(BehaviorSelections, LeavesTheOldWorldAndReinstallsAfterScriptLoad) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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
    EXPECT_EQ(info.Instances, 0u);

    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1, 2)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    EXPECT_FALSE(world->Live.contains(firstInstallation));
    EXPECT_EQ(world->Live.begin()->second, Target(1, 2));
    EXPECT_EQ(world->Calls,
              (std::vector<std::string>{"+1@1", "-1", "+1@2"}));
}

TEST(BehaviorSelections, TreatsReloadedObjectGenerationsAsNewInstances) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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

TEST(BehaviorSelections, ReplacesAKeyAndRetiresEveryPlanOwnedByAMod) {
    Selections plans;
    auto first = std::make_shared<FakeWorld>();
    auto replacement = std::make_shared<FakeWorld>();
    auto other = std::make_shared<FakeWorld>();
    SelectionId oldId = 0;
    SelectionId newId = 0;
    SelectionId otherId = 0;
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

TEST(BehaviorSelections, RejectsHandlesFromAnotherOwnerGeneration) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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

TEST(BehaviorSelections, FinishesAClosingPlanAtALaterSafePoint) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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
    EXPECT_EQ(info.Instances, 1u);

    world->BusyClose.clear();
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_TRUE(world->Live.empty());
    EXPECT_EQ(plans.Size(), 0u);
    EXPECT_EQ(plans.Read(plan, info).Code, Error::InvalidState);
}

TEST(BehaviorSelections, RetriesABusyTargetRemovalAtTheNextSafePoint) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    const Installation installation = world->Live.begin()->first;
    world->BusyClose.insert(installation);

    plans.Remove(Target(1));
    EXPECT_EQ(plans.ProcessFrame().Code, Error::Busy);
    world->BusyClose.clear();
    ASSERT_TRUE(plans.ProcessFrame());

    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Unsatisfied);
    EXPECT_EQ(info.Instances, 0u);
    EXPECT_TRUE(world->Live.empty());
}

TEST(BehaviorSelections, RetriesAConflictedTargetSetOnlyWhenRequested) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    const Installation installation = world->Live.begin()->first;
    world->FailClose.insert(installation);

    plans.Remove(Target(1));
    EXPECT_EQ(plans.ProcessFrame().Code, Error::RevertConflict);
    world->FailClose.clear();
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_EQ(world->Live.size(), 1u);

    ASSERT_TRUE(plans.Retry(plan));
    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Reconciling);
    EXPECT_TRUE(info.LastStatus);
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_TRUE(world->Live.empty());
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.State, PlanState::Unsatisfied);
}

TEST(BehaviorSelections, AsksWhetherARetryIsAcceptedWithoutRetrying) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    const Installation installation = world->Live.begin()->first;
    world->FailClose.insert(installation);

    plans.Remove(Target(1));
    EXPECT_EQ(plans.ProcessFrame().Code, Error::RevertConflict);
    PlanInfo before;
    ASSERT_TRUE(plans.Read(plan, before));
    ASSERT_TRUE(plans.CanRetry(plan));
    PlanInfo after;
    ASSERT_TRUE(plans.Read(plan, after));
    EXPECT_EQ(after.State, before.State);
    EXPECT_EQ(after.LastStatus.Code, before.LastStatus.Code);
    EXPECT_EQ(plans.CanRetry(plan + 1).Code, Error::InvalidState);

    world->FailClose.clear();
    world->BusyClose.insert(installation);
    EXPECT_EQ(plans.Close("mod", 4, plan).Code, Error::Busy);
    EXPECT_EQ(plans.CanRetry(plan).Code, Error::InvalidState);
    EXPECT_EQ(plans.Retry(plan).Code, Error::InvalidState);

    world->BusyClose.clear();
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_EQ(plans.Size(), 0u);
}

TEST(BehaviorSelections, KeepsItsWorldOnTheGameThread) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    ASSERT_EQ(world->Live.size(), 1u);
    Status closing;
    std::thread closer([&] {
        closing = plans.Close("mod", 4, plan);
    });
    closer.join();

    EXPECT_EQ(closing.Code, Error::WrongThread);
    PlanInfo info;
    ASSERT_TRUE(plans.Read("mod", 4, plan, info));
    EXPECT_EQ(info.State, PlanState::Active);
    EXPECT_EQ(info.Instances, 1u);

    ASSERT_TRUE(plans.Close("mod", 4, plan));
    EXPECT_TRUE(world->Live.empty());
    EXPECT_EQ(plans.Size(), 0u);
}

TEST(BehaviorSelections, DefersACloseRequestedWhileInstalling) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));

    Status nested;
    world->OnInstall = [&] {
        nested = plans.Close("mod", 4, plan);
    };
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_EQ(nested.Code, Error::Busy);
    ASSERT_EQ(world->Live.size(), 1u);

    PlanInfo info;
    ASSERT_TRUE(plans.Read("mod", 4, plan, info));
    EXPECT_EQ(info.State, PlanState::Retiring);

    world->OnInstall = {};
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_TRUE(world->Live.empty());
    EXPECT_EQ(plans.Size(), 0u);
}

TEST(BehaviorSelections, PreservesAScriptChangeObservedWhileInstalling) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 4, {"Gameplay_Events"}, world, plan));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));

    world->OnInstall = [&] {
        world->OnInstall = {};
        EXPECT_TRUE(plans.LoadScript("Gameplay_Events", Target(2)));
    };
    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_EQ(world->Live.size(), 1u);

    ASSERT_TRUE(plans.ProcessFrame());
    EXPECT_EQ(world->Live.size(), 2u);
    PlanInfo info;
    ASSERT_TRUE(plans.Read(plan, info));
    EXPECT_EQ(info.Matches, 2u);
    EXPECT_EQ(info.Instances, 2u);
}

TEST(BehaviorSelections, RetriesAConflictedRetirementWithoutTheCallerHandle) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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

TEST(BehaviorSelections, KeepsThePreviousPlanWhenReplacementCannotRetireIt) {
    Selections plans;
    auto first = std::make_shared<FakeWorld>();
    auto replacement = std::make_shared<FakeWorld>();
    SelectionId oldId = 0;
    ASSERT_TRUE(plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Events"}, first, oldId));
    ASSERT_TRUE(plans.LoadScript("Gameplay_Events", Target(1)));
    ASSERT_TRUE(plans.ProcessFrame());
    first->FailClose.insert(first->Live.begin()->first);

    SelectionId newId = 0;
    const Status status = plans.Submit(
        {"mod", "events"}, 1, {"Gameplay_Ingame"}, replacement, newId);
    EXPECT_EQ(status.Code, Error::RevertConflict);
    EXPECT_EQ(newId, 0u);
    EXPECT_EQ(plans.Size(), 1u);
    PlanInfo old;
    ASSERT_TRUE(plans.Read(oldId, old));
    EXPECT_EQ(old.State, PlanState::Conflicted);
    EXPECT_EQ(old.Instances, 1u);
    EXPECT_TRUE(replacement->Live.empty());
}

TEST(BehaviorSelections, ObjectDeletionRemovesEveryGenerationForThatIdentity) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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
    EXPECT_EQ(info.Instances, 0u);
}

TEST(BehaviorSelections, WorldResetDropsOldInstallationIdentityAfterAConflict) {
    Selections plans;
    auto world = std::make_shared<FakeWorld>();
    SelectionId plan = 0;
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
    EXPECT_EQ(info.Instances, 0u);
    EXPECT_EQ(info.LastStatus.Code, Error::RevertConflict);
    EXPECT_TRUE(info.ApplyFailure);
    EXPECT_EQ(info.RestoreFailure.Code, Error::RevertConflict);
}

} // namespace
