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

namespace {

void KnownAddress() {}

TEST(SdkUtils, ModuleFromAddressAnswersIdentity) {
    EXPECT_EQ(utils::GetModuleFromAddress(nullptr), nullptr);

    // An address inside this test binary belongs to the test binary; the
    // handle must be the same identity GetModuleHandleA reports for it, and
    // it must survive as a plain comparison value without refcounting.
    void *self = utils::GetModuleFromAddress(reinterpret_cast<const void *>(&KnownAddress));
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
        utils::GetModuleDirectoryFromAddress(reinterpret_cast<const void *>(&KnownAddress));
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
