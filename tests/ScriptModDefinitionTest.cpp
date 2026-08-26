#include "ScriptModDefinition.h"

#include <limits>
#include <string>

#include <gtest/gtest.h>

IMod::~IMod() = default;
ILogger *IMod::GetLogger() { return nullptr; }
IConfig *IMod::GetConfig() { return nullptr; }

namespace {

void ExpectVersion(const char *text, int major, int minor, int patch) {
    BMLVersion version(-1, -1, -1);
    ASSERT_TRUE(BML::ParseScriptModVersion(text, version));
    EXPECT_EQ(version.major, major);
    EXPECT_EQ(version.minor, minor);
    EXPECT_EQ(version.patch, patch);
}

void ExpectInvalidVersion(const char *text) {
    BMLVersion version(7, 8, 9);
    EXPECT_FALSE(BML::ParseScriptModVersion(text, version));
    EXPECT_TRUE(version == BMLVersion(7, 8, 9));
}

TEST(ScriptModDefinitionTest, AcceptsCanonicalVersions) {
    ExpectVersion("0.0.0", 0, 0, 0);
    ExpectVersion("1.2.3", 1, 2, 3);
    ExpectVersion("2147483647.2147483647.2147483647",
                  (std::numeric_limits<int>::max)(),
                  (std::numeric_limits<int>::max)(),
                  (std::numeric_limits<int>::max)());
}

TEST(ScriptModDefinitionTest, RejectsNonCanonicalVersions) {
    for (const char *text : {"", "1", "1.2", ".1.2", "1..2", "1.2.",
                             "1.2.3.4", "v1.2.3", " 1.2.3", "1.2.3 ",
                             "+1.2.3", "-1.2.3", "01.2.3", "1.02.3",
                             "1.2.03", "1.2.3-rc.1", "1.2.3+build"}) {
        ExpectInvalidVersion(text);
    }
}

TEST(ScriptModDefinitionTest, RejectsOverflowWithoutChangingTheOutput) {
    ExpectInvalidVersion("2147483648.0.0");
    ExpectInvalidVersion("0.2147483648.0");
    ExpectInvalidVersion("0.0.2147483648");
    ExpectInvalidVersion("999999999999999999999999999999.0.0");
}

} // namespace
