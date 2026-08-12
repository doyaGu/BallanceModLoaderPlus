#include "InterfaceRegistry.h"

#include <iterator>
#include <string>

#include <gtest/gtest.h>

namespace {

// Two majors of one interface, and a second interface, so lookup has something to
// pick from. The Grown struct is what a later minor version of Version1 looks
// like: one member appended, everything before it in place.
struct Version1 {
    BML_InterfaceHeader Header;
    int (*First)(void);
};

struct Grown {
    BML_InterfaceHeader Header;
    int (*First)(void);
    int (*Second)(void);
};

struct Version2 {
    BML_InterfaceHeader Header;
    int (*Renamed)(int value);
};

struct Other {
    BML_InterfaceHeader Header;
    int (*Only)(void);
};

int First() { return 1; }
int Second() { return 2; }
int Renamed(int value) { return value; }
int Only() { return 3; }

const Version1 kVersion1 = {BML_IFACE_HEADER(Version1, "test.iface", 1, 0), &First};
const Version2 kVersion2 = {BML_IFACE_HEADER(Version2, "test.iface", 2, 0), &Renamed};
const Other kOther = {BML_IFACE_HEADER(Other, "test.other", 1, 4), &Only};

const BML::InterfaceEntry kTable[] = {
    {"test.iface", 1, &kVersion1},
    {"test.iface", 2, &kVersion2},
    {"test.other", 1, &kOther},
};

int Find(const char *id, uint16_t majorVersion, const void **out) {
    return BML::FindInterface(kTable, std::size(kTable), id, majorVersion, out);
}

} // namespace

TEST(InterfaceRegistryTest, AnswersTheRequestedMajorVersion) {
    const void *found = nullptr;
    ASSERT_EQ(Find("test.iface", 1, &found), BML_OK);
    EXPECT_EQ(found, &kVersion1);

    ASSERT_EQ(Find("test.iface", 2, &found), BML_OK);
    EXPECT_EQ(found, &kVersion2);

    ASSERT_EQ(Find("test.other", 1, &found), BML_OK);
    EXPECT_EQ(found, &kOther);
}

TEST(InterfaceRegistryTest, MatchesIdsByContentRatherThanByPointer) {
    const std::string id = "test.other";
    const void *found = nullptr;
    ASSERT_EQ(Find(id.c_str(), 1, &found), BML_OK);
    EXPECT_EQ(found, &kOther);
}

TEST(InterfaceRegistryTest, SeparatesAnUnknownIdFromAnUnknownMajorVersion) {
    const void *found = &kOther;
    EXPECT_EQ(Find("test.missing", 1, &found), BML_ERROR_NOT_FOUND);
    EXPECT_EQ(found, nullptr);

    found = &kOther;
    EXPECT_EQ(Find("test.iface", 3, &found), BML_ERROR_VERSION_MISMATCH);
    EXPECT_EQ(found, nullptr);
}

TEST(InterfaceRegistryTest, RejectsMissingArguments) {
    const void *found = &kOther;
    EXPECT_EQ(Find(nullptr, 1, &found), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(found, nullptr);

    found = &kOther;
    EXPECT_EQ(Find("", 1, &found), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(found, nullptr);

    EXPECT_EQ(Find("test.other", 1, nullptr), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(BML::FindInterface(nullptr, 0, "test.other", 1, &found), BML_ERROR_NOT_FOUND);
}

TEST(InterfaceRegistryTest, ReportsAppendedMembersByStructSize) {
    // What a Mod built against Grown sees when the running loader still ships
    // Version1: the shared member is there, the appended one is not, and asking
    // is the only thing that keeps the Mod out of reading past the struct.
    const auto *asGrown = reinterpret_cast<const Grown *>(&kVersion1);
    EXPECT_TRUE(BML_IFACE_HAS(asGrown, Grown, First));
    EXPECT_FALSE(BML_IFACE_HAS(asGrown, Grown, Second));

    const Grown grown = {BML_IFACE_HEADER(Grown, "test.iface", 1, 1), &First, &Second};
    const Grown *whole = &grown;
    EXPECT_TRUE(BML_IFACE_HAS(whole, Grown, First));
    EXPECT_TRUE(BML_IFACE_HAS(whole, Grown, Second));
}

TEST(InterfaceRegistryTest, ReportsAMemberTheLoaderLeftEmpty) {
    // A loader is allowed to ship a member it cannot serve as a null pointer, so
    // StructSize alone is not enough of a check.
    const Grown partial = {BML_IFACE_HEADER(Grown, "test.iface", 1, 1), &First, nullptr};
    const Grown *iface = &partial;
    EXPECT_TRUE(BML_IFACE_HAS(iface, Grown, First));
    EXPECT_FALSE(BML_IFACE_HAS(iface, Grown, Second));

    const Grown *missing = nullptr;
    EXPECT_FALSE(BML_IFACE_HAS(missing, Grown, First));
}

TEST(InterfaceRegistryTest, CarriesTheIdAndVersionsItWasBuiltWith) {
    EXPECT_EQ(kVersion1.Header.StructSize, sizeof(Version1));
    EXPECT_STREQ(kVersion1.Header.InterfaceId, "test.iface");
    EXPECT_EQ(kVersion1.Header.MajorVersion, 1);
    EXPECT_EQ(kVersion1.Header.MinorVersion, 0);
    EXPECT_EQ(kOther.Header.MinorVersion, 4);
}
