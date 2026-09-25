#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <cwchar>

#include <gtest/gtest.h>

#include "UpdaterPaths.h"

namespace {
    bool Accepts(const char *path) {
        std::string error;
        return bmlupdater::NormalizeArchivePath(path, error).has_value();
    }
}

TEST(UpdaterPathTest, AcceptsNormalForwardSlashRelativePath) {
    std::string error;
    auto result = bmlupdater::NormalizeArchivePath("BuildingBlocks/BMLPlus.dll", error);
    ASSERT_TRUE(result.has_value()) << error;
    EXPECT_EQ(result->normalized, "BuildingBlocks/BMLPlus.dll");
}

TEST(UpdaterPathTest, RejectsTraversalAndAbsolutePaths) {
    EXPECT_FALSE(Accepts("../BMLPlus.dll"));
    EXPECT_FALSE(Accepts("/BuildingBlocks/BMLPlus.dll"));
    EXPECT_FALSE(Accepts("C:/Ballance/BMLPlus.dll"));
    EXPECT_FALSE(Accepts("//server/share/file"));
}

TEST(UpdaterPathTest, RejectsWindowsSpecificAmbiguity) {
    EXPECT_FALSE(Accepts("ModLoader\\file.txt"));
    EXPECT_FALSE(Accepts("file.txt:stream"));
    EXPECT_FALSE(Accepts("CON"));
    EXPECT_FALSE(Accepts("folder/NUL.txt"));
    EXPECT_FALSE(Accepts("folder/name."));
    EXPECT_FALSE(Accepts("folder/name "));
}

TEST(UpdaterPathTest, RejectsUpdaterPackageForbiddenAreas) {
    EXPECT_TRUE(bmlupdater::IsDisallowedUpdaterPackagePath("Bin/Updater.exe"));
    EXPECT_TRUE(bmlupdater::IsDisallowedUpdaterPackagePath("ModLoader/Updater/sources.json"));
    EXPECT_TRUE(bmlupdater::IsDisallowedUpdaterPackagePath("ModLoader/Mods/CameraUtilities.bmodp"));
    EXPECT_TRUE(bmlupdater::IsDisallowedUpdaterPackagePath("ModLoader/Configs/BML.cfg"));
    EXPECT_FALSE(bmlupdater::IsDisallowedUpdaterPackagePath("BuildingBlocks/BMLPlus.dll"));
}

TEST(UpdaterPathTest, WritableDirectoryProbeCreatesNoPersistentFile) {
    wchar_t tempPath[MAX_PATH]{};
    ASSERT_NE(::GetTempPathW(MAX_PATH, tempPath), 0u);
    const std::wstring directory = bmlupdater::JoinPath(
        tempPath, L"BMLUpdaterPathTest-" + std::to_wstring(::GetCurrentProcessId()) + L"-" +
                      std::to_wstring(::GetTickCount64()));

    ASSERT_TRUE(bmlupdater::CanCreateFileInDirectory(directory));
    EXPECT_TRUE(bmlupdater::DirectoryExists(directory));

    WIN32_FIND_DATAW entry{};
    const std::wstring pattern = bmlupdater::JoinPath(directory, L"*");
    HANDLE search = ::FindFirstFileW(pattern.c_str(), &entry);
    ASSERT_NE(search, INVALID_HANDLE_VALUE);
    unsigned int files = 0;
    do {
        if (wcscmp(entry.cFileName, L".") != 0 && wcscmp(entry.cFileName, L"..") != 0)
            ++files;
    } while (::FindNextFileW(search, &entry) == TRUE);
    ::FindClose(search);
    EXPECT_EQ(files, 0u);

    std::string error;
    EXPECT_TRUE(bmlupdater::RemoveDirectoryTree(directory, error)) << error;
}
