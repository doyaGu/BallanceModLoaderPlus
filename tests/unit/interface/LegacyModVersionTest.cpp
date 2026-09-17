#include "Loader/LegacyModVersion.h"

#include <limits>

#include <gtest/gtest.h>

IMod::~IMod() = default;
ILogger *IMod::GetLogger() { return nullptr; }
IConfig *IMod::GetConfig() { return nullptr; }

namespace {

void ExpectVersion(const char *text, int major, int minor, int patch) {
    const BMLVersion version = BML::ParseLegacyModVersion(text);
    EXPECT_EQ(version.major, major);
    EXPECT_EQ(version.minor, minor);
    EXPECT_EQ(version.patch, patch);
}

TEST(LegacyModVersionTest, ReadsTheFirstThreeDigitRuns) {
    ExpectVersion("1.2.3", 1, 2, 3);
    ExpectVersion("1.2", 1, 2, 0);
    ExpectVersion("1", 1, 0, 0);
    ExpectVersion("v1.2.3", 1, 2, 3);
    ExpectVersion("  2.0.1", 2, 0, 1);
    ExpectVersion("beta 3", 3, 0, 0);
    ExpectVersion("1.2.3-rc.4", 1, 2, 3);
    ExpectVersion("1.2.3+build.7", 1, 2, 3);
    ExpectVersion("1.2.3.4", 1, 2, 3);
}

TEST(LegacyModVersionTest, TreatsMissingDigitsAsZero) {
    ExpectVersion(nullptr, 0, 0, 0);
    ExpectVersion("", 0, 0, 0);
    ExpectVersion("no version", 0, 0, 0);
}

TEST(LegacyModVersionTest, SaturatesComponentsAtIntMax) {
    const int maximum = (std::numeric_limits<int>::max)();
    ExpectVersion("2147483648.999999999999999999999.999999999999999999999", maximum, maximum, maximum);
}

} // namespace
