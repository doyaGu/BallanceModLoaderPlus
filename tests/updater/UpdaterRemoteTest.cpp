#include <gtest/gtest.h>

#include "UpdaterRemote.h"

namespace {
    constexpr const char *kValidChannelJson = R"json({
      "version": "v1.2.3",
      "packageUrl": "https://updates.example.test/releases/v1.2.3/BMLPlus-Update-v1.2.3.zip",
      "manifestUrl": "https://updates.example.test/releases/v1.2.3/BMLPlus-Update-v1.2.3.manifest.json",
      "revokedVersions": [],
      "revokedManifestHashes": []
    })json";

    bmlupdater::Result Parse(const char *json, bool force, const std::string &trustedVersion, bmlupdater::RemoteUpdateInfo &info) {
        return bmlupdater::ParseRemoteChannelJson(
            json,
            "stable",
            "https://updates.example.test/stable.json",
            "https://updates.example.test/stable.json.sig",
            force,
            trustedVersion,
            info);
    }
}

TEST(UpdaterRemoteTest, ParsesValidChannelAndReportsUpdateAvailable) {
    bmlupdater::RemoteUpdateInfo info;
    bmlupdater::Result result = Parse(kValidChannelJson, false, "", info);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(info.latestVersion, "v1.2.3");
    EXPECT_EQ(info.channel, "stable");
    EXPECT_EQ(info.manifestSignatureUrl, "https://updates.example.test/releases/v1.2.3/BMLPlus-Update-v1.2.3.manifest.json.sig");
    EXPECT_TRUE(info.updateAvailable);
}

TEST(UpdaterRemoteTest, RequiresHttpsPackageAndManifestUrls) {
    constexpr const char *json = R"json({
      "version": "v1.2.3",
      "packageUrl": "http://updates.example.test/releases/v1.2.3/BMLPlus-Update-v1.2.3.zip",
      "manifestUrl": "https://updates.example.test/releases/v1.2.3/BMLPlus-Update-v1.2.3.manifest.json",
      "revokedVersions": [],
      "revokedManifestHashes": []
    })json";
    bmlupdater::RemoteUpdateInfo info;
    bmlupdater::Result result = Parse(json, false, "", info);
    EXPECT_FALSE(result.ok);
}

TEST(UpdaterRemoteTest, RequiresRevocationArrays) {
    constexpr const char *json = R"json({
      "version": "v1.2.3",
      "packageUrl": "https://updates.example.test/releases/v1.2.3/BMLPlus-Update-v1.2.3.zip",
      "manifestUrl": "https://updates.example.test/releases/v1.2.3/BMLPlus-Update-v1.2.3.manifest.json"
    })json";
    bmlupdater::RemoteUpdateInfo info;
    bmlupdater::Result result = Parse(json, false, "", info);
    EXPECT_FALSE(result.ok);
}

TEST(UpdaterRemoteTest, RejectsRevokedLatestVersion) {
    constexpr const char *json = R"json({
      "version": "v1.2.3",
      "packageUrl": "https://updates.example.test/releases/v1.2.3/BMLPlus-Update-v1.2.3.zip",
      "manifestUrl": "https://updates.example.test/releases/v1.2.3/BMLPlus-Update-v1.2.3.manifest.json",
      "revokedVersions": ["v1.2.3"],
      "revokedManifestHashes": []
    })json";
    bmlupdater::RemoteUpdateInfo info;
    bmlupdater::Result result = Parse(json, false, "", info);
    EXPECT_FALSE(result.ok);
}

TEST(UpdaterRemoteTest, SameTrustedVersionSkipsUnlessForced) {
    bmlupdater::RemoteUpdateInfo info;
    bmlupdater::Result result = Parse(kValidChannelJson, false, "v1.2.3", info);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_TRUE(info.sameTrustedVersion);
    EXPECT_FALSE(info.updateAvailable);

    result = Parse(kValidChannelJson, true, "v1.2.3", info);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_TRUE(info.sameTrustedVersion);
    EXPECT_TRUE(info.updateAvailable);
}
