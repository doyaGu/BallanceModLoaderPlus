#include "Behavior/Authoring.h"

#include <thread>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

TEST(BehaviorAuthoring, OwnerGenerationMakesOldSessionsStale) {
    Runtime runtime(nullptr);
    Authoring authoring(runtime);
    ASSERT_NE(authoring.RegisterOwner("mod"), 0u);
    std::uintptr_t first = 0;
    ASSERT_TRUE(authoring.OpenSession("mod", first));
    ASSERT_NE(first, 0u);

    ASSERT_NE(authoring.RegisterOwner("mod"), 0u);
    RunInfo info;
    EXPECT_EQ(authoring.ReadRun(first, info).Code, Error::InvalidState);
    std::uintptr_t second = 0;
    ASSERT_TRUE(authoring.OpenSession("mod", second));
    EXPECT_GT(second, first);
}

TEST(BehaviorAuthoring, RetiringOwnerClosesSessionsIdempotently) {
    Runtime runtime(nullptr);
    Authoring authoring(runtime);
    ASSERT_NE(authoring.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(authoring.OpenSession("mod", session));

    authoring.RetireOwner("mod");
    authoring.RetireOwner("mod");
    authoring.CloseSession(session);

    std::uintptr_t rejected = 0;
    EXPECT_EQ(authoring.OpenSession("mod", rejected).Code,
              Error::InvalidState);
    EXPECT_EQ(rejected, 0u);
}

TEST(BehaviorAuthoring, CloseSessionMayRunOffTheGameThread) {
    Runtime runtime(nullptr);
    Authoring authoring(runtime);
    ASSERT_NE(authoring.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(authoring.OpenSession("mod", session));

    std::thread close([&] { authoring.CloseSession(session); });
    close.join();
    authoring.CloseSession(session);
}

TEST(BehaviorAuthoring, OtherOperationsRequireTheGameThread) {
    Runtime runtime(nullptr);
    Authoring authoring(runtime);
    ASSERT_NE(authoring.RegisterOwner("mod"), 0u);
    Status status;
    std::thread open([&] {
        std::uintptr_t session = 0;
        status = authoring.OpenSession("mod", session);
    });
    open.join();
    EXPECT_EQ(status.Code, Error::WrongThread);
}

TEST(BehaviorAuthoring, RunIdsAreNeverCreatedForRejectedAdmission) {
    Runtime runtime(nullptr);
    Authoring authoring(runtime);
    ASSERT_NE(authoring.RegisterOwner("mod"), 0u);
    std::uintptr_t session = 0;
    ASSERT_TRUE(authoring.OpenSession("mod", session));

    Spec block(CKGUID(1, 2));
    OpenRun run = authoring.Spawn(session, nullptr, block);
    EXPECT_EQ(run.Id, 0u);
    EXPECT_EQ(run.Result.Code, Error::ContextExpired);
}

} // namespace
