#include "gtest/gtest.h"

#include "Core/LeaseManager.h"
#include "TestKernel.h"

namespace {

using BML::Core::LeaseManager;
using BML::Core::Testing::TestKernel;

TEST(LeaseManagerTests, CleanupConsumerDestroysOutstandingHandles) {
    LeaseManager manager;
    TestKernel kernel;
    BML_InterfaceLease lease = nullptr;
    BML_InterfaceRegistration registration = nullptr;

    ASSERT_EQ(manager.CreateInterfaceLease(kernel.get(), "iface.test", "provider.a", "consumer.a", &lease),
              BML_RESULT_OK);
    ASSERT_EQ(manager.CreateInterfaceRegistration(kernel.get(),
                                                  "iface.test",
                                                  "provider.a",
                                                  "consumer.a",
                                                  &registration),
              BML_RESULT_OK);
    ASSERT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 1u);
    ASSERT_EQ(manager.GetOutstandingRegistrationHandlesForTest(), 1u);

    manager.CleanupConsumer("consumer.a");

    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 0u);
    EXPECT_EQ(manager.GetOutstandingRegistrationHandlesForTest(), 0u);
    EXPECT_EQ(manager.ReleaseInterfaceLease(lease), BML_RESULT_INVALID_HANDLE);
    EXPECT_EQ(manager.ReleaseInterfaceRegistration(registration), BML_RESULT_INVALID_HANDLE);
}

TEST(LeaseManagerTests, CleanupProviderDestroysOutstandingHandles) {
    LeaseManager manager;
    TestKernel kernel;
    BML_InterfaceLease lease = nullptr;
    BML_InterfaceRegistration registration = nullptr;

    ASSERT_EQ(manager.CreateInterfaceLease(kernel.get(), "iface.test", "provider.a", "consumer.a", &lease),
              BML_RESULT_OK);
    ASSERT_EQ(manager.CreateInterfaceRegistration(kernel.get(),
                                                  "iface.test",
                                                  "provider.a",
                                                  "consumer.a",
                                                  &registration),
              BML_RESULT_OK);

    manager.CleanupProvider("provider.a");

    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 0u);
    EXPECT_EQ(manager.GetOutstandingRegistrationHandlesForTest(), 0u);
    EXPECT_EQ(manager.ReleaseInterfaceLease(lease), BML_RESULT_INVALID_HANDLE);
    EXPECT_EQ(manager.ReleaseInterfaceRegistration(registration), BML_RESULT_INVALID_HANDLE);
}

TEST(LeaseManagerTests, ResetDestroysOutstandingHandles) {
    LeaseManager manager;
    TestKernel kernel;
    BML_InterfaceLease lease = nullptr;
    BML_InterfaceRegistration registration = nullptr;

    ASSERT_EQ(manager.CreateInterfaceLease(kernel.get(), "iface.reset", "provider.b", "consumer.b", &lease),
              BML_RESULT_OK);
    ASSERT_EQ(manager.CreateInterfaceRegistration(kernel.get(),
                                                  "iface.reset",
                                                  "provider.b",
                                                  "consumer.b",
                                                  &registration),
              BML_RESULT_OK);

    manager.Reset();

    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 0u);
    EXPECT_EQ(manager.GetOutstandingRegistrationHandlesForTest(), 0u);
    EXPECT_EQ(manager.ReleaseInterfaceLease(lease), BML_RESULT_INVALID_HANDLE);
    EXPECT_EQ(manager.ReleaseInterfaceRegistration(registration), BML_RESULT_INVALID_HANDLE);
}

} // namespace
