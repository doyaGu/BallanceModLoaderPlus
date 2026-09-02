#include "Behavior/Lifecycle.h"

#include <algorithm>
#include <map>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

const char *Name(LifecycleCallback callback) {
    switch (callback) {
    case LifecycleCallback::Create: return "CREATE";
    case LifecycleCallback::Attach: return "ATTACH";
    case LifecycleCallback::SettingsEdited: return "SETTINGSEDITED";
    case LifecycleCallback::Edited: return "EDITED";
    case LifecycleCallback::Reset: return "RESET";
    case LifecycleCallback::Detach: return "DETACH";
    case LifecycleCallback::Delete: return "DELETE";
    }
    return "?";
}

class FakeLifecycleAdapter final : public LifecycleAdapter {
public:
    bool InitializeAndReflect(LifecycleLayout &layout,
                              LifecycleFault &fault) override {
        Events.emplace_back("INITIALIZE");
        if (FailAt == "INITIALIZE")
            return Fail(fault, "initialization failed");
        layout.Generation = Generation;
        return true;
    }

    bool WriteSettingStage(std::size_t stage, const LifecycleLayout &layout,
                           LifecycleFault &fault) override {
        Events.push_back("WRITE" + std::to_string(stage) + "@" +
                         std::to_string(layout.Generation));
        if (FailAt == "WRITE" + std::to_string(stage))
            return Fail(fault, "setting write failed");
        Setting = stage == 0 ? AuthorStageZero : LaterSetting;
        ++Writes[stage];
        return true;
    }

    bool AlignRelationsAndPlace(LifecycleFault &fault) override {
        Events.emplace_back("PLACE");
        if (FailAt == "PLACE")
            return Fail(fault, "placement failed");
        RelationsVisible = true;
        return true;
    }

    bool CaptureIdentity(LifecycleIdentity &identity,
                         LifecycleFault &fault) override {
        Events.emplace_back("IDENTITY");
        if (FailAt == "IDENTITY")
            return Fail(fault, "identity capture failed");
        identity = Identity;
        return true;
    }

    bool Invoke(LifecycleCallback callback, LifecycleFault &fault) override {
        const std::string event = Name(callback);
        Events.push_back(event);
        CallbackRelationsVisible.push_back(RelationsVisible);
        ++Callbacks[callback];

        if (callback == LifecycleCallback::Create ||
            callback == LifecycleCallback::Attach) {
            Setting = NativeDefault;
        } else if (callback == LifecycleCallback::SettingsEdited) {
            SettingsSeen.push_back(Setting);
            Setting = NormalizedSetting;
        }

        if (CloseDuring == callback && Coordinator)
            Coordinator->RequestClose(ResetDuringClose);
        if (DestroyDuring == callback)
            Alive = false;
        if (FailCallback == callback &&
            Callbacks[callback] == FailOccurrence) {
            return Fail(fault, event + " failed");
        }
        return true;
    }

    bool Revalidate(const LifecycleIdentity &identity,
                    LifecycleFault &fault) override {
        Events.emplace_back("VALIDATE");
        if (!Alive)
            return Fail(fault, "behavior no longer exists");
        if (DriftAfterCallback && ++ValidationCount == DriftValidation) {
            Identity.Target.Id += 1;
        }
        if (!(identity == Identity))
            return Fail(fault, "identity drifted");
        return true;
    }

    bool Reflect(LifecycleLayout &layout, LifecycleFault &fault) override {
        Events.emplace_back("REFLECT");
        if (FailAt == "REFLECT")
            return Fail(fault, "reflection failed");
        layout.Generation = ++Generation;
        return true;
    }

    bool ApplyBindings(LifecycleFault &fault) override {
        Events.emplace_back("BIND");
        if (FailAt == "BIND")
            return Fail(fault, "binding failed");
        Identity.Sources = {{41, 0x4100}, {42, 0x4200}};
        return true;
    }

    bool ReconcileBindings(const LifecycleLayout &layout,
                           LifecycleFault &fault) override {
        Events.push_back("RECONCILE@" + std::to_string(layout.Generation));
        if (FailAt == "RECONCILE")
            return Fail(fault, "binding reconciliation failed");
        return true;
    }

    bool Deactivate(LifecycleFault &fault) override {
        Events.emplace_back("DEACTIVATE");
        if (FailAt == "DEACTIVATE")
            return Fail(fault, "deactivation failed");
        return true;
    }

    bool DisconnectAndDestroy(LifecycleFault &fault) override {
        Events.emplace_back("DESTROY");
        RelationsVisible = false;
        Alive = false;
        if (FailAt == "DESTROY")
            return Fail(fault, "destruction failed");
        return true;
    }

    bool Fail(LifecycleFault &fault, std::string message) {
        fault = {LifecycleError::CallbackFailed, 17, std::move(message)};
        return false;
    }

    Lifecycle *Coordinator = nullptr;
    LifecycleIdentity Identity{{1, 0x1000}, {2, 0x2000}, {3, 0x3000},
                               {4, 0x4000}, {5, 0x5000}, {}};
    std::uint64_t Generation = 1;
    int AuthorStageZero = 42;
    int LaterSetting = 84;
    int NativeDefault = 15;
    int NormalizedSetting = 77;
    int Setting = 0;
    bool RelationsVisible = false;
    bool Alive = true;
    bool DriftAfterCallback = false;
    int DriftValidation = 1;
    int ValidationCount = 0;
    bool ResetDuringClose = false;
    std::string FailAt;
    LifecycleCallback FailCallback = static_cast<LifecycleCallback>(-1);
    int FailOccurrence = 1;
    LifecycleCallback CloseDuring = static_cast<LifecycleCallback>(-1);
    LifecycleCallback DestroyDuring = static_cast<LifecycleCallback>(-1);
    std::map<std::size_t, int> Writes;
    std::map<LifecycleCallback, int> Callbacks;
    std::vector<int> SettingsSeen;
    std::vector<bool> CallbackRelationsVisible;
    std::vector<std::string> Events;
};

TEST(BehaviorLifecycle, ConfiguresInFixedOrderAndReplaysStageZero) {
    Lifecycle lifecycle;
    FakeLifecycleAdapter adapter;
    adapter.Coordinator = &lifecycle;

    ASSERT_TRUE(lifecycle.Configure({true, {true, true}}, adapter));
    EXPECT_EQ(lifecycle.State(), LifecycleState::Ready);
    EXPECT_TRUE(lifecycle.Ledger().Placed);
    EXPECT_TRUE(lifecycle.Ledger().Created);
    EXPECT_TRUE(lifecycle.Ledger().Attached);
    EXPECT_EQ(adapter.Writes[0], 3);
    EXPECT_EQ(adapter.Writes[1], 1);
    ASSERT_EQ(adapter.SettingsSeen.size(), 2u);
    EXPECT_EQ(adapter.SettingsSeen[0], adapter.AuthorStageZero);
    EXPECT_EQ(adapter.SettingsSeen[1], adapter.LaterSetting);
    EXPECT_EQ(adapter.Setting, adapter.NormalizedSetting);

    const std::vector<std::string> callbacks = {
        "CREATE", "ATTACH", "SETTINGSEDITED", "SETTINGSEDITED", "EDITED"};
    std::vector<std::string> actual;
    std::copy_if(adapter.Events.begin(), adapter.Events.end(),
                 std::back_inserter(actual), [](const std::string &event) {
                     return event == "CREATE" || event == "ATTACH" ||
                            event == "SETTINGSEDITED" || event == "EDITED";
                 });
    EXPECT_EQ(actual, callbacks);
    EXPECT_GT(adapter.Generation, 1u);
}

TEST(BehaviorLifecycle, OwnerlessEmptySettingsSkipAttachAndSettingsCallback) {
    Lifecycle lifecycle;
    FakeLifecycleAdapter adapter;
    ASSERT_TRUE(lifecycle.Configure({false, {false}}, adapter));
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Create], 1);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Attach], 0);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::SettingsEdited], 0);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Edited], 1);
    EXPECT_TRUE(lifecycle.Ledger().Created);
    EXPECT_FALSE(lifecycle.Ledger().Attached);
}

TEST(BehaviorLifecycle, CallbackIdentityDriftFailsAndCompensatesByLedger) {
    Lifecycle lifecycle;
    FakeLifecycleAdapter adapter;
    adapter.DriftAfterCallback = true;
    adapter.DriftValidation = 1;

    EXPECT_FALSE(lifecycle.Configure({true, {true}}, adapter));
    EXPECT_EQ(lifecycle.State(), LifecycleState::Closed);
    EXPECT_EQ(lifecycle.Failure().Code, LifecycleError::CallbackFailed);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Create], 1);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Attach], 0);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Detach], 0);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Delete], 1);
}

TEST(BehaviorLifecycle, CallbackFailuresUseExactCompensationLedger) {
    struct Case {
        LifecycleCallback Failure;
        int Detach;
        int Delete;
    };
    const Case cases[] = {
        {LifecycleCallback::Create, 0, 0},
        {LifecycleCallback::Attach, 0, 1},
        {LifecycleCallback::SettingsEdited, 1, 1},
        {LifecycleCallback::Edited, 1, 1},
    };

    for (const Case &test : cases) {
        Lifecycle lifecycle;
        FakeLifecycleAdapter adapter;
        adapter.FailCallback = test.Failure;
        ASSERT_FALSE(lifecycle.Configure({true, {true}}, adapter));
        EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Detach], test.Detach);
        EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Delete], test.Delete);
        EXPECT_EQ(lifecycle.State(), LifecycleState::Closed);
    }
}

TEST(BehaviorLifecycle, NormalAndResetTeardownPreserveRelationsThroughCallbacks) {
    for (bool reset : {false, true}) {
        Lifecycle lifecycle;
        FakeLifecycleAdapter adapter;
        ASSERT_TRUE(lifecycle.Configure({true, {true}}, adapter));
        const std::size_t callbackVisibilityStart =
            adapter.CallbackRelationsVisible.size();

        lifecycle.RequestClose(reset);
        ASSERT_TRUE(lifecycle.Drain(adapter));
        EXPECT_EQ(lifecycle.State(), LifecycleState::Closed);
        EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Reset], reset ? 1 : 0);
        EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Detach], 1);
        EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Delete], 1);
        for (std::size_t index = callbackVisibilityStart;
             index < adapter.CallbackRelationsVisible.size(); ++index) {
            EXPECT_TRUE(adapter.CallbackRelationsVisible[index]);
        }

        const auto events = adapter.Events;
        EXPECT_LT(std::find(events.begin(), events.end(), "DEACTIVATE"),
                  std::find(events.begin(), events.end(), "DETACH"));
        EXPECT_LT(std::find(events.begin(), events.end(), "DELETE"),
                  std::find(events.begin(), events.end(), "DESTROY"));
        EXPECT_TRUE(lifecycle.Drain(adapter));
        EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Delete], 1);
    }
}

TEST(BehaviorLifecycle, CallbackSelfCloseQueuesTeardownWithoutSelfWait) {
    Lifecycle lifecycle;
    FakeLifecycleAdapter adapter;
    adapter.Coordinator = &lifecycle;
    adapter.CloseDuring = LifecycleCallback::Edited;

    EXPECT_FALSE(lifecycle.Configure({true, {true}}, adapter));
    EXPECT_EQ(lifecycle.State(), LifecycleState::Closed);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Detach], 1);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Delete], 1);
    EXPECT_EQ(lifecycle.Failure().Code, LifecycleError::Cancelled);
}

TEST(BehaviorLifecycle, ExternalThreadCloseOnlyMutatesAtDrainSafePoint) {
    Lifecycle lifecycle;
    FakeLifecycleAdapter adapter;
    ASSERT_TRUE(lifecycle.Configure({true, {false}}, adapter));

    std::thread closer([&] { lifecycle.RequestClose(true); });
    closer.join();

    EXPECT_EQ(lifecycle.State(), LifecycleState::Ready);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Reset], 0);
    EXPECT_TRUE(lifecycle.Drain(adapter));
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Reset], 1);
    EXPECT_EQ(lifecycle.State(), LifecycleState::Closed);
}

TEST(BehaviorLifecycle, SelfDestructionNeverPreventsBestEffortNativeCleanup) {
    Lifecycle lifecycle;
    FakeLifecycleAdapter adapter;
    ASSERT_TRUE(lifecycle.Configure({true, {false}}, adapter));
    adapter.DestroyDuring = LifecycleCallback::Detach;

    lifecycle.RequestClose();
    EXPECT_TRUE(lifecycle.Drain(adapter));
    EXPECT_EQ(lifecycle.State(), LifecycleState::Closed);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Detach], 1);
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Delete], 1);
    EXPECT_TRUE(lifecycle.Failure());
    EXPECT_EQ(adapter.Events.back(), "DESTROY");
}

TEST(BehaviorLifecycle, FirstTeardownDiagnosticWinsWhileCleanupContinues) {
    Lifecycle lifecycle;
    FakeLifecycleAdapter adapter;
    ASSERT_TRUE(lifecycle.Configure({true, {false}}, adapter));
    adapter.FailAt = "DEACTIVATE";
    adapter.FailCallback = LifecycleCallback::Detach;

    lifecycle.RequestClose();
    ASSERT_TRUE(lifecycle.Drain(adapter));
    EXPECT_EQ(lifecycle.Failure().Message, "deactivation failed");
    EXPECT_EQ(adapter.Callbacks[LifecycleCallback::Delete], 1);
    EXPECT_EQ(adapter.Events.back(), "DESTROY");
}

} // namespace
