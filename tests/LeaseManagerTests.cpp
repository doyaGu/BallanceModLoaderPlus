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
    // Note: lease and registration pointers are dangling after cleanup.
    // Do not call Release on them -- the handles were forcibly destroyed.
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
    // Note: lease and registration pointers are dangling after cleanup.
    // Do not call Release on them -- the handles were forcibly destroyed.
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
    // Note: lease and registration pointers are dangling after reset.
    // Do not call Release on them -- the handles were forcibly destroyed.
}

TEST(LeaseManagerTests, AddRefPreventsDestructionOnSingleRelease) {
    LeaseManager manager;
    TestKernel kernel;
    BML_InterfaceLease lease = nullptr;

    ASSERT_EQ(manager.CreateInterfaceLease(kernel.get(), "iface.refcount", "provider.a", "consumer.a", &lease),
              BML_RESULT_OK);
    ASSERT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 1u);

    // AddRef: ref_count goes from 1 to 2
    manager.AddRefInterfaceLease(lease);

    // First release: ref_count goes from 2 to 1, handle stays alive
    EXPECT_EQ(manager.ReleaseInterfaceLease(lease), BML_RESULT_OK);
    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 1u);
    EXPECT_TRUE(manager.HasActiveInterfaceLease("consumer.a", "iface.refcount"));

    // Second release: ref_count goes from 1 to 0, handle destroyed
    EXPECT_EQ(manager.ReleaseInterfaceLease(lease), BML_RESULT_OK);
    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 0u);
    EXPECT_FALSE(manager.HasActiveInterfaceLease("consumer.a", "iface.refcount"));
}

TEST(LeaseManagerTests, OutstandingCountIsOneRegardlessOfRefCount) {
    LeaseManager manager;
    TestKernel kernel;
    BML_InterfaceLease lease = nullptr;

    ASSERT_EQ(manager.CreateInterfaceLease(kernel.get(), "iface.count", "provider.a", "consumer.a", &lease),
              BML_RESULT_OK);

    // AddRef 3 times: ref_count = 4
    manager.AddRefInterfaceLease(lease);
    manager.AddRefInterfaceLease(lease);
    manager.AddRefInterfaceLease(lease);

    // Outstanding handle count is still 1 (one unique lease)
    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 1u);
    EXPECT_EQ(manager.GetLeaseCountForInterface("iface.count"), 1u);

    // Release 4 times to fully destroy
    EXPECT_EQ(manager.ReleaseInterfaceLease(lease), BML_RESULT_OK);
    EXPECT_EQ(manager.ReleaseInterfaceLease(lease), BML_RESULT_OK);
    EXPECT_EQ(manager.ReleaseInterfaceLease(lease), BML_RESULT_OK);
    EXPECT_EQ(manager.ReleaseInterfaceLease(lease), BML_RESULT_OK);
    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 0u);
}

TEST(LeaseManagerTests, AddRefOnNullIsNoOp) {
    LeaseManager manager;
    // Should not crash
    manager.AddRefInterfaceLease(nullptr);
}

TEST(LeaseManagerTests, CleanupConsumerDestroysRegardlessOfRefCount) {
    LeaseManager manager;
    TestKernel kernel;
    BML_InterfaceLease lease = nullptr;

    ASSERT_EQ(manager.CreateInterfaceLease(kernel.get(), "iface.cleanup", "provider.a", "consumer.a", &lease),
              BML_RESULT_OK);

    // AddRef to bump ref_count
    manager.AddRefInterfaceLease(lease);
    manager.AddRefInterfaceLease(lease);

    // Cleanup forcibly destroys regardless of ref_count
    manager.CleanupConsumer("consumer.a");
    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 0u);
}

TEST(LeaseManagerTests, GetLeaseCountReflectsUniqueLeasesNotRefCount) {
    LeaseManager manager;
    TestKernel kernel;
    BML_InterfaceLease lease1 = nullptr;
    BML_InterfaceLease lease2 = nullptr;

    ASSERT_EQ(manager.CreateInterfaceLease(kernel.get(), "iface.multi", "provider.a", "consumer.a", &lease1),
              BML_RESULT_OK);
    ASSERT_EQ(manager.CreateInterfaceLease(kernel.get(), "iface.multi", "provider.a", "consumer.b", &lease2),
              BML_RESULT_OK);

    // AddRef first lease -- does NOT create a new unique lease
    manager.AddRefInterfaceLease(lease1);

    // Count should be 2 (unique leases), not 3
    EXPECT_EQ(manager.GetLeaseCountForInterface("iface.multi"), 2u);
    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 2u);

    // Clean up: release lease1 twice (ref_count 2->1->0), lease2 once
    manager.ReleaseInterfaceLease(lease1);
    manager.ReleaseInterfaceLease(lease1);
    manager.ReleaseInterfaceLease(lease2);
    EXPECT_EQ(manager.GetOutstandingLeaseHandlesForTest(), 0u);
}

} // namespace
