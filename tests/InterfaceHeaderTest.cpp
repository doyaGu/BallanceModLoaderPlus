#include "BML/Interface.h"

#include <gtest/gtest.h>

namespace {

struct Version1 {
    BML_InterfaceHeader Header;
    int (*First)(void);
};

struct Grown {
    BML_InterfaceHeader Header;
    int (*First)(void);
    int (*Second)(void);
};

struct Other {
    BML_InterfaceHeader Header;
    int (*Only)(void);
};

int First() { return 1; }
int Second() { return 2; }
int Only() { return 3; }

const Version1 kVersion1 = {BML_IFACE_HEADER(Version1, "test.iface", 1, 0), &First};
const Other kOther = {BML_IFACE_HEADER(Other, "test.other", 1, 4), &Only};

} // namespace

TEST(InterfaceHeaderTest, ReportsAppendedMembersByStructSize) {
    const auto *asGrown = reinterpret_cast<const Grown *>(&kVersion1);
    EXPECT_TRUE(BML_IFACE_HAS(asGrown, Grown, First));
    EXPECT_FALSE(BML_IFACE_HAS(asGrown, Grown, Second));

    const Grown grown = {BML_IFACE_HEADER(Grown, "test.iface", 1, 1), &First, &Second};
    const Grown *whole = &grown;
    EXPECT_TRUE(BML_IFACE_HAS(whole, Grown, First));
    EXPECT_TRUE(BML_IFACE_HAS(whole, Grown, Second));
}

TEST(InterfaceHeaderTest, ReportsAnEmptyMemberAsMissing) {
    const Grown partial = {BML_IFACE_HEADER(Grown, "test.iface", 1, 1), &First, nullptr};
    const Grown *iface = &partial;
    EXPECT_TRUE(BML_IFACE_HAS(iface, Grown, First));
    EXPECT_FALSE(BML_IFACE_HAS(iface, Grown, Second));

    const Grown *missing = nullptr;
    EXPECT_FALSE(BML_IFACE_HAS(missing, Grown, First));
}

TEST(InterfaceHeaderTest, CarriesTheDeclaredIdentityAndVersions) {
    EXPECT_EQ(kVersion1.Header.StructSize, sizeof(Version1));
    EXPECT_STREQ(kVersion1.Header.InterfaceId, "test.iface");
    EXPECT_EQ(kVersion1.Header.MajorVersion, 1);
    EXPECT_EQ(kVersion1.Header.MinorVersion, 0);
    EXPECT_EQ(kOther.Header.MinorVersion, 4);
}
