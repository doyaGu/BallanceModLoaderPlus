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
