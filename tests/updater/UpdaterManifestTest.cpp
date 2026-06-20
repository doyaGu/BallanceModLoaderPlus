#include <gtest/gtest.h>

#include "UpdaterManifest.h"

TEST(UpdaterManifestTest, ParsesValidUpdaterManifest) {
    constexpr const char *json = R"json({
      "schemaVersion": 1,
      "version": "v1.2.3",
      "package": {
        "fileName": "BMLPlus-Update-v1.2.3.zip",
        "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
      },
      "managedFiles": [
        {
          "path": "BuildingBlocks/BMLPlus.dll",
          "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "size": 42
        }
      ],
      "preserve": [],
      "removeFiles": [
        {
          "path": "BuildingBlocks/Old.dll",
          "sha256": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        }
      ]
    })json";

    bmlupdater::UpdaterManifest manifest;
    bmlupdater::Result result = bmlupdater::ParseManifestJson(json, manifest);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(manifest.version, "v1.2.3");
    ASSERT_EQ(manifest.managedFiles.size(), 1u);
    EXPECT_EQ(manifest.managedFiles[0].path, "BuildingBlocks/BMLPlus.dll");
    ASSERT_EQ(manifest.removeFiles.size(), 1u);
}

TEST(UpdaterManifestTest, RejectsManualInstallPackageName) {
    constexpr const char *json = R"json({
      "schemaVersion": 1,
      "version": "v1.2.3",
      "package": {
        "fileName": "BMLPlus-v1.2.3.zip",
        "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
      },
      "managedFiles": [
        {
          "path": "BuildingBlocks/BMLPlus.dll",
          "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "size": 42
        }
      ]
    })json";

    bmlupdater::UpdaterManifest manifest;
    bmlupdater::Result result = bmlupdater::ParseManifestJson(json, manifest);
    EXPECT_FALSE(result.ok);
}

TEST(UpdaterManifestTest, RejectsForbiddenManagedPaths) {
    constexpr const char *json = R"json({
      "schemaVersion": 1,
      "version": "v1.2.3",
      "package": {
        "fileName": "BMLPlus-Update-v1.2.3.zip",
        "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
      },
      "managedFiles": [
        {
          "path": "ModLoader/Mods/CameraUtilities.bmodp",
          "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          "size": 42
        }
      ]
    })json";

    bmlupdater::UpdaterManifest manifest;
    bmlupdater::Result result = bmlupdater::ParseManifestJson(json, manifest);
    EXPECT_FALSE(result.ok);
}
