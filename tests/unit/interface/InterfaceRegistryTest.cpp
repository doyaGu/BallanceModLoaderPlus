#include "Api/InterfaceRegistry.h"

#include <gtest/gtest.h>

namespace {

struct TestInterface {
    BML_InterfaceHeader Header;
    int (*Read)(void);
};

int ReadOne() { return 1; }
int ReadTwo() { return 2; }

const TestInterface kOne = {
    BML_IFACE_HEADER(TestInterface, "test.provider", 1, 0), &ReadOne};
const TestInterface kOneDuplicate = {
    BML_IFACE_HEADER(TestInterface, "test.provider", 1, 1), &ReadTwo};
const TestInterface kTwo = {
    BML_IFACE_HEADER(TestInterface, "test.provider", 2, 0), &ReadTwo};
const TestInterface kOther = {
    BML_IFACE_HEADER(TestInterface, "test.other", 1, 0), &ReadTwo};

} // namespace

TEST(InterfaceRegistryTest, PublishesExactProviderPointerByIdAndMajor) {
    BML::Api::InterfaceRegistry registry;
    ASSERT_EQ(registry.Register("provider", &kOne), BML_OK);

    const void *found = nullptr;
    EXPECT_EQ(registry.Find("test.provider", 1, &found), BML_OK);
    EXPECT_EQ(found, &kOne);
    EXPECT_EQ(static_cast<const TestInterface *>(found)->Read(), 1);

    found = &kOther;
    EXPECT_EQ(registry.Find("test.provider", 3, &found),
              BML_ERROR_VERSION_MISMATCH);
    EXPECT_EQ(found, nullptr);
    EXPECT_EQ(registry.Find("missing", 1, &found), BML_ERROR_NOT_FOUND);
}

TEST(InterfaceRegistryTest, RejectsMalformedAndDuplicateRegistrations) {
    BML::Api::InterfaceRegistry registry;
    BML_InterfaceHeader shortHeader{
        sizeof(BML_InterfaceHeader) - 1, 1, 0, "test.short"};
    BML_InterfaceHeader zeroMajor{
        sizeof(BML_InterfaceHeader), 0, 0, "test.zero"};
    BML_InterfaceHeader emptyId{
        sizeof(BML_InterfaceHeader), 1, 0, ""};

    EXPECT_EQ(registry.Register("", &kOne), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(registry.Register("provider", nullptr),
              BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(registry.Register("provider", &shortHeader),
              BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(registry.Register("provider", &zeroMajor),
              BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(registry.Register("provider", &emptyId),
              BML_ERROR_INVALID_PARAMETER);

    ASSERT_EQ(registry.Register("provider", &kOne), BML_OK);
    EXPECT_EQ(registry.Register("provider", &kOneDuplicate),
              BML_ERROR_ALREADY_EXISTS);
    EXPECT_EQ(registry.Register("other-provider", &kOneDuplicate),
              BML_ERROR_ALREADY_EXISTS);
}

TEST(InterfaceRegistryTest, SupportsMajorVersionsAndEnforcesOwnership) {
    BML::Api::InterfaceRegistry registry;
    ASSERT_EQ(registry.Register("provider", &kOne), BML_OK);
    ASSERT_EQ(registry.Register("provider", &kTwo), BML_OK);

    EXPECT_EQ(registry.Unregister("other", "test.provider", 1),
              BML_ERROR_ACCESS_DENIED);
    EXPECT_EQ(registry.Unregister("provider", "test.provider", 1), BML_OK);

    const void *found = nullptr;
    EXPECT_EQ(registry.Find("test.provider", 1, &found),
              BML_ERROR_VERSION_MISMATCH);
    EXPECT_EQ(registry.Find("test.provider", 2, &found), BML_OK);
    EXPECT_EQ(found, &kTwo);
}

TEST(InterfaceRegistryTest, OwnerCleanupDoesNotRemoveOtherProviders) {
    BML::Api::InterfaceRegistry registry;
    ASSERT_EQ(registry.Register("provider", &kOne), BML_OK);
    ASSERT_EQ(registry.Register("provider", &kTwo), BML_OK);
    ASSERT_EQ(registry.Register("other", &kOther), BML_OK);

    EXPECT_EQ(registry.CleanupOwner("provider"), 2u);
    EXPECT_EQ(registry.CleanupOwner("provider"), 0u);

    const void *found = nullptr;
    EXPECT_EQ(registry.Find("test.provider", 1, &found), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(registry.Find("test.other", 1, &found), BML_OK);
    EXPECT_EQ(found, &kOther);
}
