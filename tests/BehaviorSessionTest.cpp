#include "Behavior/Sessions.h"

#include <thread>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

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

TEST(BehaviorSessions, RunIdsAreNeverCreatedForRejectedAdmission) {
    Runtime runtime(nullptr);
    Sessions sessions(runtime);
    ASSERT_NE(sessions.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(sessions.OpenSession("mod", session));

    Spec block(CKGUID(1, 2));
    OpenRun run = sessions.Spawn(session, nullptr, block);
    EXPECT_EQ(run.Id, 0u);
    EXPECT_EQ(run.Result.Code, Error::ContextExpired);
}

} // namespace
