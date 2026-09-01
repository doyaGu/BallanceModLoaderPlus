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

TEST(BehaviorSessions, CompletedCallKeepsFramesAfterNativeTeardown) {
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
    EXPECT_EQ(run.Info.State, RunState::Completed);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);

    std::shared_ptr<FrameStore> frames = sessions.Frames(run.Id);
    ASSERT_NE(frames, nullptr);
    ASSERT_EQ(frames->Read().size(), 1u);
    EXPECT_TRUE(frames->Read().front().Terminal);
    EXPECT_EQ(frames->Read().front().ActiveOutputs.front().Name, "Done");

    sessions.CloseRun(run.Id);
    RunInfo stale;
    EXPECT_EQ(sessions.ReadRun(run.Id, stale).Code, Error::InvalidState);
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
    EXPECT_EQ(info.State, RunState::Completed);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);

    std::shared_ptr<FrameStore> frames = sessions.Frames(run.Id);
    ASSERT_NE(frames, nullptr);
    const std::vector<RunFrame> captured = frames->Read();
    ASSERT_EQ(captured.size(), 2u);
    EXPECT_TRUE(captured[0].NativeContinuation);
    EXPECT_FALSE(captured[0].Terminal);
    EXPECT_TRUE(captured[1].Terminal);
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
    EXPECT_EQ(info.State, RunState::Completed);
    EXPECT_EQ(LiveBehaviorSessionInstances(), 0u);
    ASSERT_NE(sessions.Frames(run.Id), nullptr);
    EXPECT_EQ(sessions.Frames(run.Id)->Read().size(), 2u);
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
    EXPECT_EQ(pulse.State, RunState::Completed);
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
