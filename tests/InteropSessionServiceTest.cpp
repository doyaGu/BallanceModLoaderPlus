#include <gtest/gtest.h>

#include "InteropSessionService.h"

namespace {

TEST(InteropSessionServiceTest, ContextBecomesStaleWhenOwnerRotates) {
    BML::InteropSessionService service;
    service.RegisterMod("session-owner");
    const BML_InteropCallContext first = service.CreateContextForOwner("session-owner");
    ASSERT_NE(0u, first.SessionId);
    EXPECT_EQ(BML_OK, service.ValidateContext(&first, true));

    service.RotateMod("session-owner");
    const BML_InteropCallContext second = service.CreateContextForOwner("session-owner");
    ASSERT_NE(0u, second.SessionId);
    EXPECT_NE(first.SessionId, second.SessionId);
    EXPECT_EQ(BML_ERROR_INTEROP_HANDLE_STALE, service.ValidateContext(&first, true));
    EXPECT_EQ(BML_OK, service.ValidateContext(&second, true));
}

TEST(InteropSessionServiceTest, ContextCannotCrossLoaderRuntime) {
    BML::InteropSessionService first;
    BML::InteropSessionService second;
    first.RegisterMod("first-owner");
    second.RegisterMod("second-owner");

    const BML_InteropCallContext firstContext = first.CreateContextForOwner("first-owner");
    const BML_InteropCallContext secondContext = second.CreateContextForOwner("second-owner");
    ASSERT_NE(firstContext.ServiceId, secondContext.ServiceId);

    EXPECT_EQ(BML_ERROR_INTEROP_HANDLE_STALE, first.ValidateContext(&secondContext, true));
    EXPECT_EQ(BML_ERROR_INTEROP_HANDLE_STALE, second.ValidateContext(&firstContext, true));
}

TEST(InteropSessionServiceTest, StatelessContextIsAcceptedOnlyWhenAllowed) {
    BML::InteropSessionService service;
    const BML_InteropCallContext stateless{};

    EXPECT_EQ(BML_OK, service.ValidateContext(&stateless, false));
    EXPECT_EQ(BML_ERROR_INTEROP_UNSUPPORTED, service.ValidateContext(&stateless, true));
}

} // namespace
