#include "PathUtils.h"
#include "SdkUtils.h"

// For the module-identity tests: GetModuleHandleA/GetModuleFileNameW. SdkUtils.h
// deliberately keeps Windows.h out of its interface, so the test brings it.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <gtest/gtest.h>

// IMod.h ships alongside BMLVersion but its loader-owned members have no
// definition here; stub them like every other test that includes IMod.h.
IMod::~IMod() = default;
ILogger *IMod::GetLogger() { return nullptr; }
IConfig *IMod::GetConfig() { return nullptr; }

using utils::ParseVersion;

namespace {

BMLVersion Parse(const char *value) {
    return ParseVersion(value);
}

void ExpectVersion(const BMLVersion &version, int major, int minor, int patch) {
    EXPECT_EQ(version.major, major);
    EXPECT_EQ(version.minor, minor);
    EXPECT_EQ(version.patch, patch);
}

TEST(SdkUtils, ReadsUpToThreeNumericGroups) {
    ExpectVersion(Parse("1.2.3"), 1, 2, 3);
    ExpectVersion(Parse("1.2"), 1, 2, 0);
    ExpectVersion(Parse("1"), 1, 0, 0);
    ExpectVersion(Parse("12.34.56"), 12, 34, 56);
}

TEST(SdkUtils, SkipsNonDigitsAnywhere) {
    // These are the strings the two retired parsers disagreed on: anything not
    // starting with a digit used to answer (0,0,0) from the sscanf copy while
    // the load-time lambda read it fine. The unified parser keeps the lenient
    // answer everywhere.
    ExpectVersion(Parse("v1.2.3"), 1, 2, 3);
    ExpectVersion(Parse("  2.0.1"), 2, 0, 1);
    ExpectVersion(Parse("beta 3"), 3, 0, 0);
    ExpectVersion(Parse("1.2.3-rc.4"), 1, 2, 3);
    ExpectVersion(Parse("1.2.3+build.7"), 1, 2, 3);
}

TEST(SdkUtils, ExtraGroupsAndGarbage) {
    // A fourth group is ignored, matching both retired parsers.
    ExpectVersion(Parse("1.2.3.4"), 1, 2, 3);
    ExpectVersion(Parse("abc"), 0, 0, 0);
    ExpectVersion(Parse("-"), 0, 0, 0);
    ExpectVersion(Parse(nullptr), 0, 0, 0);
    ExpectVersion(Parse(""), 0, 0, 0);
    ExpectVersion(ParseVersion(std::string("7.8")), 7, 8, 0);
}

TEST(SdkUtils, SameAnswerBothOverloads) {
    const char *raw = "v2.5.1";
    const std::string wrapped(raw);
    const BMLVersion fromChar = ParseVersion(raw);
    const BMLVersion fromString = ParseVersion(wrapped);
    EXPECT_TRUE(fromChar == fromString);
    ExpectVersion(fromString, 2, 5, 1);
}

TEST(SdkUtils, ModuleFromAddressAnswersIdentity) {
    EXPECT_EQ(utils::GetModuleFromAddress(nullptr), nullptr);

    // An address inside this test binary belongs to the test binary; the
    // handle must be the same identity GetModuleHandleA reports for it, and
    // it must survive as a plain comparison value without refcounting.
    void *self = utils::GetModuleFromAddress(reinterpret_cast<const void *>(&ExpectVersion));
    void *reported = ::GetModuleHandleA(nullptr);
    EXPECT_NE(self, nullptr);
    EXPECT_EQ(self, reported);
}

TEST(SdkUtils, ModuleDirectoryFromAddressMatchesExecutableDirectory) {
    EXPECT_TRUE(utils::GetModuleDirectory(nullptr).empty());

    wchar_t executable[MAX_PATH] = {};
    ASSERT_GT(::GetModuleFileNameW(nullptr, executable, MAX_PATH), 0u);
    const std::wstring expected = utils::GetParentDirectoryW(executable);

    const std::wstring fromAddress =
        utils::GetModuleDirectoryFromAddress(reinterpret_cast<const void *>(&ExpectVersion));
    EXPECT_FALSE(fromAddress.empty());
    EXPECT_EQ(fromAddress, expected);
}

TEST(SdkUtils, ParentDirectoryEdgeCases) {
    EXPECT_EQ(utils::GetParentDirectoryW(L"C:\\dir\\file.txt"), L"C:\\dir");
    // The separator directly after a drive letter is the last one left, so a
    // first-level directory answers the bare drive -- shipped behaviour, kept
    // verbatim when this helper moved out of ModContext.
    EXPECT_EQ(utils::GetParentDirectoryW(L"C:\\dir\\"), L"C:");
    EXPECT_EQ(utils::GetParentDirectoryW(L"C:\\"), L"");
    EXPECT_EQ(utils::GetParentDirectoryW(L"file.txt"), L"");
    EXPECT_EQ(utils::GetParentDirectoryW(L"dir\\file.txt"), L"dir");
}

} // namespace
