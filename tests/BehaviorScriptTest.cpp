#include "Behavior/Script.h"
#include "Behavior/GraphEdit.h"

#include <algorithm>
#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior::Internal;
using ScriptSet = BML::Behavior::Internal::Scripts;

Status Failure(Error error, const char *message) {
    Status status{error, CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR, message};
    status.Details.Stage = Phase::Execution;
    return status;
}

class FakeScriptWorld final : public ScriptWorld {
public:
    bool InDispatch() const noexcept override { return Dispatching; }

    Status Create(void *owner, std::string_view name, int priority,
                  ScriptIdentity &out) override {
        Events.emplace_back("create");
        Owner = owner;
        Name = name;
        Priority = priority;
        if (!CreateResult)
            return CreateResult;
        const std::uint32_t base = NextObject;
        NextObject += 3;
        out = {{base, base + 1000u, {1, base, 1}},
               {base + 1u, base + 1001u, {1, base + 1u, 1}},
               {base + 2u, base + 1002u, {1, base + 2u, 1}}};
        Identities.push_back(out);
        return {};
    }

    Status Define(const SessionOwner &, const ScriptIdentity &script,
                  GraphEdit, ScriptBodyId &out) override {
        Events.emplace_back("define");
        Defined.push_back(script.Root.Id);
        if (!DefineResult)
            return DefineResult;
        out = NextBody++;
        if (OnDefine) OnDefine();
        return {};
    }

    Status Read(const ScriptIdentity &script, bool &active) override {
        Events.emplace_back("read");
        if (!ReadResult)
            return ReadResult;
        active = ActiveRoot == script.Root.Id;
        return {};
    }

    Status SetActive(const ScriptIdentity &script, bool active,
                     bool reset) override {
        Events.push_back(active ? (reset ? "activate-reset" : "activate")
                                : "deactivate");
        if (OnSetActive)
            OnSetActive();
        if (!ActivityResult)
            return ActivityResult;
        ActiveRoot = active ? script.Root.Id : 0;
        return {};
    }

    Status CloseBody(const SessionOwner &, ScriptBodyId body) override {
        Events.emplace_back("close-body");
        ClosedBodies.push_back(body);
        return CloseBodyResult;
    }

    Status Destroy(const ScriptIdentity &script) override {
        Events.emplace_back("destroy");
        Destroyed.push_back(script.Root.Id);
        if (!DestroyResult)
            return DestroyResult;
        if (ActiveRoot == script.Root.Id)
            ActiveRoot = 0;
        return {};
    }

    bool Dispatching = false;
    void *Owner = nullptr;
    std::string Name;
    int Priority = 0;
    std::uint32_t NextObject = 10;
    ScriptBodyId NextBody = 100;
    std::uint64_t ActiveRoot = 0;
    Status CreateResult;
    Status ReadResult;
    Status ActivityResult;
    Status DefineResult;
    Status CloseBodyResult;
    Status DestroyResult;
    std::function<void()> OnSetActive;
    std::function<void()> OnDefine;
    std::vector<std::string> Events;
    std::vector<ScriptIdentity> Identities;
    std::vector<std::uint64_t> Defined;
    std::vector<ScriptBodyId> ClosedBodies;
    std::vector<std::uint64_t> Destroyed;
};

SessionOwner Owner(std::string id = "mod", std::uint64_t generation = 1) {
    return {std::move(id), generation};
}

TEST(BehaviorScript, CreatesAnInactiveOwnedScriptAndPublishesIt) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    std::string loadedName;
    ObjectRef loadedRoot;
    ScriptSet scripts(
        std::move(world),
        [&](std::string_view name, const ObjectRef &root) {
            loadedName = name;
            loadedRoot = root;
        });

    int ownerObject = 0;
    const ScriptResult opened = scripts.Create(
        Owner(), 41, &ownerObject, "Authored Graph", -7, {});

    ASSERT_TRUE(opened);
    EXPECT_EQ(opened.Info.State, ScriptState::Ready);
    EXPECT_FALSE(opened.Info.Active);
    EXPECT_FALSE(opened.Info.RequestedActive);
    EXPECT_EQ(opened.Info.Priority, -7);
    EXPECT_EQ(opened.Info.Identity.Root.Id, 10u);
    EXPECT_EQ(native->Owner, &ownerObject);
    EXPECT_EQ(native->Name, "Authored Graph");
    EXPECT_EQ(native->Priority, -7);
    EXPECT_EQ(native->Defined, std::vector<std::uint64_t>{10});
    EXPECT_EQ(loadedName, "Authored Graph");
    EXPECT_EQ(loadedRoot, opened.Info.Identity.Root.Reference);

    ScriptInfo read;
    EXPECT_TRUE(scripts.Read(Owner(), opened.Id, read));
    EXPECT_EQ(read.Identity.Root.Reference, loadedRoot);
}

TEST(BehaviorScript, CoalescesActivityRequestsAtTheFrameBoundary) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    ScriptSet scripts(std::move(world));
    const ScriptResult opened = scripts.Create(
        Owner(), 1, this, "Script", 0, {});
    ASSERT_TRUE(opened);

    ScriptInfo admitted;
    ASSERT_TRUE(scripts.SetActive(Owner(), opened.Id, true, true, admitted));
    EXPECT_TRUE(admitted.RequestedActive);
    EXPECT_FALSE(admitted.Active);
    ASSERT_TRUE(scripts.SetActive(Owner(), opened.Id, false, false, admitted));
    scripts.ProcessFrame();
    EXPECT_TRUE(native->Events.empty() ||
                std::find(native->Events.begin(), native->Events.end(),
                          "activate-reset") == native->Events.end());

    ASSERT_TRUE(scripts.SetActive(Owner(), opened.Id, true, true, admitted));
    scripts.ProcessFrame();
    EXPECT_NE(std::find(native->Events.begin(), native->Events.end(),
                        "activate-reset"), native->Events.end());
    ScriptInfo active;
    ASSERT_TRUE(scripts.Read(Owner(), opened.Id, active));
    EXPECT_TRUE(active.Active);
    EXPECT_TRUE(active.RequestedActive);

    native->Events.clear();
    ASSERT_TRUE(scripts.SetActive(Owner(), opened.Id, true, true, admitted));
    scripts.ProcessFrame();
    EXPECT_NE(std::find(native->Events.begin(), native->Events.end(),
                        "activate-reset"), native->Events.end());

    ASSERT_TRUE(scripts.SetActive(Owner(), opened.Id, false, false, admitted));
    scripts.ProcessFrame();
    ASSERT_TRUE(scripts.Read(Owner(), opened.Id, active));
    EXPECT_FALSE(active.Active);
    EXPECT_FALSE(active.RequestedActive);
}

TEST(BehaviorScript, CloseOnlyRetiresAtASafePoint) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    ScriptSet scripts(std::move(world));
    const ScriptResult opened = scripts.Create(
        Owner(), 7, this, "Script", 0, {});
    ASSERT_TRUE(opened);

    Status close;
    std::thread worker([&] { close = scripts.Close(Owner(), opened.Id); });
    worker.join();
    EXPECT_EQ(close.Code, Error::Busy);
    EXPECT_TRUE(native->Destroyed.empty());

    ScriptInfo closing;
    ASSERT_TRUE(scripts.Read(Owner(), opened.Id, closing));
    EXPECT_EQ(closing.State, ScriptState::Closing);
    scripts.ProcessFrame();
    EXPECT_EQ(native->ClosedBodies, std::vector<ScriptBodyId>{100});
    EXPECT_EQ(native->Destroyed, std::vector<std::uint64_t>{10});
    EXPECT_FALSE(scripts.Read(Owner(), opened.Id, closing));
    EXPECT_TRUE(scripts.Close(Owner(), opened.Id));
}

TEST(BehaviorScript, SelfCloseNeverWaitsForTheActivityCallback) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    ScriptSet scripts(std::move(world));
    const ScriptResult opened = scripts.Create(
        Owner(), 3, this, "Script", 0, {});
    ASSERT_TRUE(opened);
    native->OnSetActive = [&] {
        EXPECT_EQ(scripts.Close(Owner(), opened.Id).Code, Error::Busy);
        EXPECT_TRUE(native->Destroyed.empty());
    };

    ScriptInfo admitted;
    ASSERT_TRUE(scripts.SetActive(Owner(), opened.Id, true, false, admitted));
    scripts.ProcessFrame();
    EXPECT_TRUE(native->Destroyed.empty());
    ScriptInfo closing;
    ASSERT_TRUE(scripts.Read(Owner(), opened.Id, closing));
    EXPECT_EQ(closing.State, ScriptState::Closing);

    scripts.ProcessFrame();
    EXPECT_EQ(native->Destroyed, std::vector<std::uint64_t>{10});
}

TEST(BehaviorScript, ExternalDeletionInvalidatesWithoutNativeTeardown) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    ScriptSet scripts(std::move(world));
    const ScriptResult rootDeleted = scripts.Create(
        Owner(), 1, this, "Root", 0, {});
    const ScriptResult ownerDeleted = scripts.Create(
        Owner(), 1, this, "Owner", 0, {});
    ASSERT_TRUE(rootDeleted);
    ASSERT_TRUE(ownerDeleted);

    scripts.ObjectToBeDeleted(rootDeleted.Info.Identity.Root.Id);
    scripts.ObjectToBeDeleted(ownerDeleted.Info.Identity.Owner.Id);
    ScriptInfo info;
    EXPECT_FALSE(scripts.Read(Owner(), rootDeleted.Id, info));
    EXPECT_FALSE(scripts.Read(Owner(), ownerDeleted.Id, info));
    EXPECT_TRUE(native->Destroyed.empty());
}

TEST(BehaviorScript, SessionOwnerAndWorldBoundariesAreIndependent) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    ScriptSet scripts(std::move(world));
    const ScriptResult first = scripts.Create(
        Owner(), 11, this, "First", 0, {});
    const ScriptResult second = scripts.Create(
        Owner(), 12, this, "Second", 0, {});
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);

    ScriptInfo info;
    EXPECT_FALSE(scripts.Read(Owner("mod", 2), first.Id, info));
    scripts.CloseSession(11);
    scripts.ProcessFrame();
    EXPECT_FALSE(scripts.Read(Owner(), first.Id, info));
    EXPECT_TRUE(scripts.Read(Owner(), second.Id, info));

    scripts.ResetWorld();
    EXPECT_FALSE(scripts.Read(Owner(), second.Id, info));
    EXPECT_EQ(native->Destroyed.size(), 2u);
    EXPECT_TRUE(scripts.Create(Owner(), 13, this, "Next World", 0, {}));
}

TEST(BehaviorScript, DispatchAndNativeFailuresRemainExplicit) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    ScriptSet scripts(std::move(world));
    native->Dispatching = true;
    ScriptResult opened = scripts.Create(
        Owner(), 1, this, "Script", 0, {});
    EXPECT_FALSE(opened);
    EXPECT_EQ(opened.Result.Code, Error::Busy);

    native->Dispatching = false;
    opened = scripts.Create(Owner(), 1, this, "Script", 0, {});
    ASSERT_TRUE(opened);
    native->ActivityResult = Failure(Error::ExecutionFailed, "activate failed");
    ScriptInfo info;
    ASSERT_TRUE(scripts.SetActive(Owner(), opened.Id, true, false, info));
    scripts.ProcessFrame();
    ASSERT_TRUE(scripts.Read(Owner(), opened.Id, info));
    EXPECT_EQ(info.State, ScriptState::Failed);
    EXPECT_EQ(info.LastStatus.Code, Error::ExecutionFailed);
    EXPECT_FALSE(scripts.SetActive(Owner(), opened.Id, false, false, info));
    EXPECT_EQ(scripts.Close(Owner(), opened.Id).Code, Error::Busy);
    scripts.ProcessFrame();
}

TEST(BehaviorScript, DoesNotPublishARejectedInitialGraph) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    native->DefineResult = Failure(
        Error::QueryNotFound, "initial graph was rejected");
    std::size_t publications = 0;
    ScriptSet scripts(
        std::move(world),
        [&](std::string_view, const ObjectRef &) { ++publications; });

    const ScriptResult opened = scripts.Create(
        Owner(), 1, this, "Rejected", 0, {});

    EXPECT_FALSE(opened);
    EXPECT_EQ(opened.Result.Code, Error::QueryNotFound);
    EXPECT_EQ(publications, 0u);
    EXPECT_EQ(native->Defined, std::vector<std::uint64_t>{10});
    EXPECT_TRUE(native->ClosedBodies.empty());
    EXPECT_EQ(native->Destroyed, std::vector<std::uint64_t>{10});
    ScriptInfo info;
    EXPECT_FALSE(scripts.Read(Owner(), 1, info));
}

TEST(BehaviorScript, ClosesItsInitialGraphBeforeDestroyingTheRoot) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    ScriptSet scripts(std::move(world));
    const ScriptResult opened = scripts.Create(
        Owner(), 1, this, "Script", 0, {});
    ASSERT_TRUE(opened);
    native->Events.clear();
    native->CloseBodyResult = Failure(Error::Busy, "body is closing");

    EXPECT_EQ(scripts.Close(Owner(), opened.Id).Code, Error::Busy);
    scripts.ProcessFrame();
    EXPECT_EQ(native->Events, std::vector<std::string>{"close-body"});
    EXPECT_TRUE(native->Destroyed.empty());

    native->CloseBodyResult = {};
    scripts.ProcessFrame();
    EXPECT_EQ(native->Events,
              (std::vector<std::string>{"close-body", "close-body",
                                        "destroy"}));
}

TEST(BehaviorScript, KeepsFailedUnpublishedCleanupOutsideTheHandleTable) {
    auto world = std::make_unique<FakeScriptWorld>();
    FakeScriptWorld *native = world.get();
    native->DefineResult = Failure(
        Error::QueryNotFound, "initial graph was rejected");
    native->DestroyResult = Failure(
        Error::ExecutionFailed, "root is still busy");
    ScriptSet scripts(std::move(world));

    const ScriptResult opened = scripts.Create(
        Owner(), 1, this, "Rejected", 0, {});

    EXPECT_FALSE(opened);
    ScriptInfo info;
    EXPECT_FALSE(scripts.Read(Owner(), 1, info));
    EXPECT_EQ(native->Destroyed.size(), 1u);
    native->DestroyResult = {};
    scripts.ProcessFrame();
    EXPECT_EQ(native->Destroyed.size(), 2u);
}

} // namespace


TEST(BehaviorScript, CloseAtEndOfDefineCannotPublishAnOrphanRoot) {
    auto world = std::make_unique<FakeScriptWorld>();
    auto *native = world.get();
    int publications = 0;
    ScriptSet scripts(std::move(world), [&](std::string_view, const ObjectRef &) {
        ++publications;
    });
    auto admission = std::make_shared<SessionAdmission>(41);
    SessionOwner owner{"mod", 1, admission};
    native->OnDefine = [&] {
        std::thread worker([&] {
            // C CloseSession invalidates Sessions first, then closes Scripts.
            admission->Open.store(false, std::memory_order_release);
            scripts.CloseSession(41);
        });
        worker.join();
    };
    auto opened = scripts.Create(owner, 41, this, "Closing Session", 0, {});
    EXPECT_FALSE(opened);
    EXPECT_EQ(publications, 0);
    scripts.ProcessFrame();
    EXPECT_EQ(native->Destroyed.size(), 1u);
    // Observe before Scripts' destructor retires every surviving root.
}

TEST(BehaviorScript, CloseDuringCreateRetriesUnpublishedBodyBeforeDestroyingRoot) {
    auto world = std::make_unique<FakeScriptWorld>();
    auto *native = world.get();
    ScriptSet scripts(std::move(world));
    auto admission = std::make_shared<SessionAdmission>(41);
    SessionOwner owner{"mod", 1, admission};
    native->OnDefine = [&] {
        admission->Close();
        scripts.CloseSession(41);
    };
    native->CloseBodyResult = Failure(Error::Busy, "body still closing");
    EXPECT_FALSE(scripts.Create(owner, 41, this, "Closing Session", 0, {}));
    scripts.ProcessFrame();
    EXPECT_TRUE(native->Destroyed.empty());
    native->CloseBodyResult = {};
    scripts.ProcessFrame();
    ASSERT_EQ(native->Destroyed.size(), 1u);
    scripts.ProcessFrame();
    EXPECT_EQ(native->Destroyed.size(), 1u);
}
