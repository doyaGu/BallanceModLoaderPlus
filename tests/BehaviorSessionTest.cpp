#include "Behavior/Sessions.h"
#include "Behavior/FrameStore.h"

#include <thread>

#include <gtest/gtest.h>

namespace BML::Behavior {
void AdvanceBehaviorSessionRuntime();
std::size_t LiveBehaviorSessionInstances();
} // namespace BML::Behavior

namespace {

using namespace BML::Behavior;

class FakeGraphSource final : public GraphSource {
public:
    Status Refer(void *object, NativeRef &out) override {
        if (!object)
            return {Error::InvalidState, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "The fake graph root is stale."};
        out = {41, object};
        return {};
    }

    Status Read(const NativeRef &root, GraphView view,
                GraphModel &out) override {
        if (!root)
            return {Error::InvalidState, CKERR_INVALIDOBJECT,
                    CKBR_PARAMETERERROR, "The fake graph root is stale."};
        out = {};
        out.View = view;
        out.Generation = 7;
        GraphNode node;
        node.Id = root.Id;
        node.Name = "Owned Behavior";
        out.Nodes.push_back(std::move(node));
        return {};
    }

    Status ReadLayout(const NativeRef &, Layout &) override {
        return {Error::Unavailable, CKERR_NOTIMPLEMENTED,
                CKBR_PARAMETERERROR, "Not used by this test."};
    }

    Status ReadValue(const NativeRef &, const Slot &, ReadMode,
                     GraphValue &) override {
        return {Error::Unavailable, CKERR_NOTIMPLEMENTED,
                CKBR_PARAMETERERROR, "Not used by this test."};
    }

    Status GraphFingerprint(const NativeRef &, GraphView,
                            std::uint64_t &) override {
        return {Error::Unavailable, CKERR_NOTIMPLEMENTED,
                CKBR_PARAMETERERROR, "Not used by this test."};
    }

    Status LayoutFingerprint(const NativeRef &, std::uint64_t &) override {
        return {Error::Unavailable, CKERR_NOTIMPLEMENTED,
                CKBR_PARAMETERERROR, "Not used by this test."};
    }
};

Slot Input(const char *name) {
    Slot input;
    input.Kind = SlotKind::Input;
    input.Name = name;
    input.RequireUnique = true;
    return input;
}

TEST(BehaviorSessions, OwnerGenerationMakesOldSessionsStale) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t first = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", first));
    ASSERT_NE(first, 0u);

    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    RunInfo info;
    EXPECT_EQ(sessions.ReadRun(first, info).Code, Error::InvalidState);
    std::uintptr_t second = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", second));
    EXPECT_GT(second, first);
}

TEST(BehaviorSessions, SessionExposesItsCurrentOwnerGeneration) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    const std::uint64_t generation = sessions.RegisterOwner("mod");
    ASSERT_NE(generation, 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    SessionOwner owner;
    ASSERT_TRUE(sessions.ReadOwner(session, owner));
    EXPECT_EQ(owner.Id, "mod");
    EXPECT_EQ(owner.Generation, generation);

    sessions.RetireOwner("mod");
    EXPECT_EQ(sessions.ReadOwner(session, owner).Code, Error::InvalidState);
    EXPECT_FALSE(owner);
}

TEST(BehaviorSessions, RetiringOwnerClosesSessionsIdempotently) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    sessions.RetireOwner("mod");
    sessions.RetireOwner("mod");
    sessions.CloseSession(session);

    std::uintptr_t rejected = 0;
    EXPECT_EQ(sessions.OpenSession("mod", rejected).Code,
              Error::InvalidState);
    EXPECT_EQ(rejected, 0u);
}

TEST(BehaviorSessions, CloseSessionMayRunOffTheGameThread) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    std::thread close([&] { sessions.CloseSession(session); });
    close.join();
    sessions.CloseSession(session);
}

TEST(BehaviorSessions, SessionOwnerMayBeReadOffTheGameThread) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    const std::uint64_t generation = sessions.RegisterOwner("mod");
    ASSERT_NE(generation, 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    SessionOwner owner;
    Status status;
    std::thread read([&] { status = sessions.ReadOwner(session, owner); });
    read.join();

    ASSERT_TRUE(status);
    EXPECT_EQ(owner.Id, "mod");
    EXPECT_EQ(owner.Generation, generation);
}

TEST(BehaviorSessions, OtherOperationsRequireTheGameThread) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    Status status;
    std::thread open([&] {
        std::uintptr_t session = 0;
        status = sessions.OpenSession("mod", session);
    });
    open.join();
    EXPECT_EQ(status.Code, Error::WrongThread);
}

TEST(BehaviorSessions, ReadyCallKeepsItsInstanceUntilTheRunCloses) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    Spec block(CKGUID(1, 2));
    OpenRun run = sessions.Call(
        session, nullptr, block, Input("Run"));
    ASSERT_TRUE(run);
    EXPECT_EQ(run.Info.Kind, RunKind::Call);
    EXPECT_EQ(run.Info.State, RunState::Ready);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    std::shared_ptr<FrameStore> frames = sessions.Frames(run.Id);
    ASSERT_NE(frames, nullptr);
    ASSERT_EQ(frames->Read().size(), 1u);
    EXPECT_FALSE(frames->Read().front().NativeContinuation);
    EXPECT_FALSE(frames->Read().front().QueuedInput);
    EXPECT_EQ(frames->Read().front().ActiveOutputs.front().Name, "Done");

    sessions.CloseRun(run.Id);
    RunInfo stale;
    EXPECT_EQ(sessions.ReadRun(run.Id, stale).Code, Error::InvalidState);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, ReadsTheGraphOwnedByTheRun) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime, nullptr, std::make_unique<FakeGraphSource>());
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    OpenRun run = sessions.Spawn(session, nullptr, Spec(CKGUID(1, 2)));
    ASSERT_TRUE(run);

    GraphModel graph;
    ASSERT_TRUE(sessions.ReadGraph(run.Id, GraphView::Live, graph));
    EXPECT_EQ(graph.View, GraphView::Live);
    EXPECT_EQ(graph.Generation, 7u);
    ASSERT_EQ(graph.Nodes.size(), 1u);
    EXPECT_EQ(graph.Nodes.front().Id, 41u);
    EXPECT_EQ(graph.Nodes.front().Name, "Owned Behavior");

    sessions.CloseRun(run.Id);
    EXPECT_EQ(sessions.ReadGraph(run.Id, GraphView::Logical, graph).Code,
              Error::InvalidState);
}

TEST(BehaviorSessions, LiveEditsHonorLayoutGeneration) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));
    OpenRun run = sessions.Spawn(session, nullptr, Spec(CKGUID(1, 2)));
    ASSERT_TRUE(run);

    Slot pin = Slot::Named(SlotKind::InputParameter, "Value", CKPGUID_INT);
    Parameter::Binding value;
    std::uint64_t generation = 0;
    ASSERT_TRUE(sessions.Set(run.Id, 1, pin, value, generation));
    EXPECT_EQ(generation, 1u);

    Status stale = sessions.Set(run.Id, 2, pin, value, generation);
    EXPECT_EQ(stale.Code, Error::StaleLayout);
    EXPECT_EQ(generation, 0u);

    Spec settings;
    ASSERT_TRUE(sessions.Configure(run.Id, settings, generation));
    EXPECT_EQ(generation, 2u);
    EXPECT_EQ(sessions.Set(run.Id, 1, pin, value, generation).Code,
              Error::StaleLayout);
    ASSERT_TRUE(sessions.Set(run.Id, 2, pin, value, generation));

    auto *source = reinterpret_cast<CKBehavior *>(
        static_cast<std::uintptr_t>(77));
    ASSERT_TRUE(sessions.Bind(
        run.Id, 2, pin, source,
        Slot::Named(SlotKind::OutputParameter, "Value"),
        Parameter::BindingKind::Direct, generation));
    EXPECT_EQ(generation, 2u);
}

TEST(BehaviorSessions, ContinuePromotesTheSameCallAndRetainsBothFrames) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    Spec block(CKGUID(3, 4));
    OpenRun run = sessions.Call(
        session, nullptr, block, Input("Pending"));
    ASSERT_TRUE(run);
    ASSERT_EQ(run.Info.Kind, RunKind::Call);
    ASSERT_EQ(run.Info.State, RunState::Pending);
    ASSERT_EQ(LiveBehaviorSessionInstances(), 1u);

    RunResult continued = sessions.Continue(run.Id);
    ASSERT_TRUE(continued.Detail);
    EXPECT_EQ(continued.State, RunState::Pending);
    RunInfo info;
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.Kind, RunKind::Task);
    EXPECT_EQ(info.State, RunState::Pending);

    AdvanceBehaviorSessionRuntime();
    sessions.ProcessFrame();
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.Kind, RunKind::Task);
    EXPECT_EQ(info.State, RunState::Ready);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    std::shared_ptr<FrameStore> frames = sessions.Frames(run.Id);
    ASSERT_NE(frames, nullptr);
    const std::vector<RunFrame> captured = frames->Read();
    ASSERT_EQ(captured.size(), 2u);
    EXPECT_TRUE(captured[0].NativeContinuation);
    EXPECT_FALSE(captured[1].NativeContinuation);
    EXPECT_FALSE(captured[1].QueuedInput);

    RunResult pulsed = sessions.Pulse(run.Id, Input("Run"));
    ASSERT_TRUE(pulsed);
    EXPECT_EQ(pulsed.State, RunState::Ready);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    sessions.CloseRun(run.Id);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, FailedActivationStillUsesRunOwnedTeardown) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    OpenRun run = sessions.Call(
        session, nullptr, Spec(CKGUID(11, 12)), Input("Fail"));
    ASSERT_TRUE(run);
    EXPECT_EQ(run.Info.State, RunState::Failed);
    EXPECT_EQ(run.Info.LastStatus.Code, Error::ExecutionFailed);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    const std::shared_ptr<FrameStore> frames = sessions.Frames(run.Id);
    ASSERT_NE(frames, nullptr);
    ASSERT_EQ(frames->Read().size(), 1u);
    EXPECT_EQ(frames->Read().front().Fault.Code,
              ExecutionError::NativeFailed);

    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    sessions.CloseRun(run.Id);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, StartIsManagedFromItsFirstExecution) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    OpenRun run = sessions.Start(
        session, nullptr, Spec(CKGUID(5, 6)),
        Input("Pending"));
    ASSERT_TRUE(run);
    EXPECT_EQ(run.Info.Kind, RunKind::Task);
    EXPECT_EQ(run.Info.State, RunState::Pending);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    AdvanceBehaviorSessionRuntime();
    sessions.ProcessFrame();
    RunInfo info;
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.State, RunState::Ready);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    ASSERT_NE(sessions.Frames(run.Id), nullptr);
    EXPECT_EQ(sessions.Frames(run.Id)->Read().size(), 2u);
    sessions.CloseRun(run.Id);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, RejectedPulseDoesNotChangeTheRunState) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));
    OpenRun run = sessions.Spawn(
        session, nullptr, Spec(CKGUID(13, 14)));
    ASSERT_TRUE(run);

    RunResult rejected = sessions.Pulse(run.Id, Input("Missing"));
    EXPECT_FALSE(rejected);
    EXPECT_EQ(rejected.Admission, AdmissionState::Failed);
    RunInfo info;
    ASSERT_TRUE(sessions.ReadRun(run.Id, info));
    EXPECT_EQ(info.State, RunState::Ready);
    EXPECT_EQ(info.LastStatus.Code, Error::SlotNotFound);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    RunResult accepted = sessions.Pulse(run.Id, Input("Run"));
    ASSERT_TRUE(accepted);
    EXPECT_EQ(accepted.State, RunState::Ready);
    sessions.CloseRun(run.Id);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

TEST(BehaviorSessions, WorldResetClosesRunsButKeepsTheSession) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    OpenRun instance = sessions.Spawn(session, nullptr, Spec(CKGUID(7, 8)));
    ASSERT_TRUE(instance);
    ASSERT_EQ(LiveBehaviorSessionInstances(), 1u);
    RunResult pulse = sessions.Pulse(
        instance.Id, Input("Run"));
    ASSERT_TRUE(pulse.Detail);
    EXPECT_EQ(pulse.State, RunState::Ready);
    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);

    sessions.ResetWorld();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
    RunInfo stale;
    EXPECT_EQ(sessions.ReadRun(instance.Id, stale).Code, Error::InvalidState);

    OpenRun afterReset = sessions.Spawn(
        session, nullptr, Spec(CKGUID(7, 8)));
    EXPECT_TRUE(afterReset);
}

TEST(BehaviorSessions, OffThreadSessionCloseDefersNativeTeardown) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));
    OpenRun run = sessions.Spawn(session, nullptr, Spec(CKGUID(9, 10)));
    ASSERT_TRUE(run);
    ASSERT_EQ(LiveBehaviorSessionInstances(), 1u);

    std::thread close([&] { sessions.CloseSession(session); });
    close.join();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 1u);
    RunInfo stale;
    EXPECT_EQ(sessions.ReadRun(run.Id, stale).Code, Error::InvalidState);

    sessions.ProcessFrame();
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
}

} // namespace
