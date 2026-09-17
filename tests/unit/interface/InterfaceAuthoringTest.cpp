#include "BML/Interface.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <utility>

namespace {

struct TestInterface {
    BML_InterfaceHeader Header;
    int(BML_CDECL *Read)(int input, int *outValue);
};

BML_DECLARE_INTERFACE_TRAITS(TestTraits, TestInterface, "test.authoring", 1, Read);
BML_DECLARE_INTERFACE_TRAITS(WrongMajorTraits, TestInterface, "test.authoring", 2, Read);
BML_DECLARE_INTERFACE_TRAITS(MissingTraits, TestInterface, "test.missing", 1, Read);

int BML_CDECL Read(int input, int *outValue) {
    if (!outValue)
        return BML_ERROR_INVALID_PARAMETER;
    *outValue = input + 7;
    return BML_OK;
}

constexpr TestInterface kInterface = BML::Interfaces::MakeInterface<TestTraits>(0, &Read);

const void *g_Registered = nullptr;

void ResetRegistry() { g_Registered = nullptr; }

} // namespace

extern "C" int BML_CDECL BML_GetInterface(const char *interfaceId, std::uint16_t majorVersion,
                                           const void **out) {
    if (!interfaceId || !out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    if (std::strcmp(interfaceId, TestTraits::InterfaceId()) != 0)
        return BML_ERROR_NOT_FOUND;
    if (majorVersion != TestTraits::MajorVersion)
        return BML_ERROR_VERSION_MISMATCH;
    if (!g_Registered)
        return BML_ERROR_NOT_FOUND;
    *out = g_Registered;
    return BML_OK;
}

extern "C" int BML_CDECL BML_RegisterInterface(const char *, const void *interfacePtr) {
    if (!interfacePtr)
        return BML_ERROR_INVALID_PARAMETER;
    if (g_Registered)
        return BML_ERROR_ALREADY_EXISTS;
    g_Registered = interfacePtr;
    return BML_OK;
}

extern "C" int BML_CDECL BML_UnregisterInterface(const char *, const char *interfaceId,
                                                   std::uint16_t majorVersion) {
    if (!interfaceId)
        return BML_ERROR_INVALID_PARAMETER;
    if (std::strcmp(interfaceId, TestTraits::InterfaceId()) != 0 ||
        majorVersion != TestTraits::MajorVersion || !g_Registered)
        return BML_ERROR_NOT_FOUND;
    g_Registered = nullptr;
    return BML_OK;
}

TEST(InterfaceAuthoringTest, MakesAnInterfaceTableFromTraits) {
    EXPECT_EQ(kInterface.Header.StructSize, sizeof(TestInterface));
    EXPECT_EQ(kInterface.Header.MajorVersion, TestTraits::MajorVersion);
    EXPECT_EQ(kInterface.Header.MinorVersion, 0);
    EXPECT_STREQ(kInterface.Header.InterfaceId, TestTraits::InterfaceId());
    EXPECT_EQ(kInterface.Read, &Read);
}

TEST(InterfaceAuthoringTest, PublishesAndAcquiresWithoutLosingStatus) {
    ResetRegistry();
    BML::Interfaces::Publication<TestTraits> publication;
    EXPECT_EQ(publication.Open(kInterface), BML_OK);
    EXPECT_TRUE(publication.IsOpen());

    BML::Interfaces::Reference<TestTraits> reference;
    EXPECT_EQ(reference.Open(), BML_OK);
    ASSERT_TRUE(reference);
    int value = 0;
    EXPECT_EQ(reference->Read(35, &value), BML_OK);
    EXPECT_EQ(value, 42);

    BML::Interfaces::Reference<WrongMajorTraits> wrongMajor;
    EXPECT_EQ(wrongMajor.Open(), BML_ERROR_VERSION_MISMATCH);
    EXPECT_FALSE(wrongMajor);
    EXPECT_EQ(wrongMajor.Status(), BML_ERROR_VERSION_MISMATCH);

    BML::Interfaces::Reference<MissingTraits> missing;
    EXPECT_EQ(missing.Open(), BML_ERROR_NOT_FOUND);
    EXPECT_FALSE(missing);
    EXPECT_EQ(missing.Status(), BML_ERROR_NOT_FOUND);

    EXPECT_EQ(publication.Close(), BML_OK);
    EXPECT_EQ(publication.Close(), BML_OK);
    EXPECT_FALSE(publication);
    EXPECT_EQ(reference.Open(), BML_ERROR_NOT_FOUND);
}

TEST(InterfaceAuthoringTest, RejectsAnIncompleteInterfaceBeforeRegistration) {
    ResetRegistry();
    const TestInterface incomplete = {
        BML_IFACE_HEADER(TestInterface, "test.authoring", 1, 0),
        nullptr,
    };
    BML::Interfaces::Publication<TestTraits> publication;
    EXPECT_EQ(publication.Open(incomplete), BML_ERROR_VERSION_MISMATCH);
    EXPECT_FALSE(publication);
    EXPECT_EQ(g_Registered, nullptr);
}

TEST(InterfaceAuthoringTest, MovesPublicationOwnership) {
    ResetRegistry();
    BML::Interfaces::Publication<TestTraits> original;
    ASSERT_EQ(original.Open(kInterface), BML_OK);

    BML::Interfaces::Publication<TestTraits> moved(std::move(original));
    EXPECT_FALSE(original);
    EXPECT_TRUE(moved);
    EXPECT_EQ(moved.Close(), BML_OK);
    EXPECT_EQ(g_Registered, nullptr);
}
