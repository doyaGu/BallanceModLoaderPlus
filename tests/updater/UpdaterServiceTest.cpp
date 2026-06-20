#include <gtest/gtest.h>

#include <Windows.h>

#include <array>
#include <memory>
#include <string>

#include "CryptoUtils.h"
#include "UpdaterPaths.h"
#include "UpdaterService.h"

namespace {
    std::wstring MakeTempRoot() {
        wchar_t temp[MAX_PATH]{};
        EXPECT_NE(GetTempPathW(MAX_PATH, temp), 0u);
        const std::wstring root = bmlupdater::JoinPath(
            temp,
            L"bml-updater-service-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
        EXPECT_TRUE(bmlupdater::CreateDirectories(root));
        return root;
    }

    void WriteText(const std::wstring &path, const std::string &text) {
        std::string error;
        ASSERT_TRUE(bmlupdater::WriteTextFile(path, text, error)) << error;
    }

    std::string Sha256Hex(const std::wstring &path) {
        std::array<uint8_t, 32> digest{};
        EXPECT_TRUE(utils::Sha256File(path, digest.data()));
        return bmlupdater::BytesToHex(digest.data(), digest.size());
    }
}

TEST(UpdaterServiceTest, RollbackRestoresUpdaterStateWrittenByApply) {
    const std::wstring tempRoot = MakeTempRoot();
    const auto cleanup = std::unique_ptr<void, void (*)(void *)>(
        const_cast<std::wstring *>(&tempRoot),
        [](void *path) {
            std::string error;
            (void)bmlupdater::RemoveDirectoryTree(*static_cast<std::wstring *>(path), error);
        });

    const std::wstring gameRoot = bmlupdater::JoinPath(tempRoot, L"game");
    const std::wstring stateRoot = bmlupdater::JoinPath(bmlupdater::JoinPath(gameRoot, L"ModLoader"), L"Updater");
    const std::wstring stagedPath = bmlupdater::JoinPath(
        bmlupdater::JoinPath(
            bmlupdater::JoinPath(
                bmlupdater::JoinPath(
                    bmlupdater::JoinPath(stateRoot, L"staging"),
                    L"v-test"),
                L"ModLoader"),
            L"UpdaterSimulation"),
        L"smoke.txt");
    constexpr const char *content = "simulated updater payload\n";
    WriteText(stagedPath, content);

    bmlupdater::UpdaterManifest manifest;
    manifest.schemaVersion = 1;
    manifest.version = "v-test";
    manifest.packageFileName = "BMLPlus-Update-v-test.zip";
    manifest.packageSha256 = std::string(64, '0');
    manifest.managedFiles.push_back({
        "ModLoader/UpdaterSimulation/smoke.txt",
        Sha256Hex(stagedPath),
        bmlupdater::FileSize(stagedPath)});

    bmlupdater::LocalPackageVerification verification;
    verification.manifest = manifest;
    verification.stagingRoot = bmlupdater::ParentPath(stagedPath);
    verification.stagedFiles.push_back({
        "ModLoader/UpdaterSimulation/smoke.txt",
        stagedPath,
        manifest.managedFiles[0].sha256,
        manifest.managedFiles[0].size});

    bmlupdater::UpdaterService service({gameRoot, stateRoot});
    bmlupdater::ApplyPlan plan;
    bmlupdater::Result result = service.BuildApplyPlan(verification, plan);
    ASSERT_TRUE(result.ok) << result.message;

    result = service.Apply(verification, plan, nullptr);
    ASSERT_TRUE(result.ok) << result.message;

    const std::wstring target = bmlupdater::JoinPath(
        bmlupdater::JoinPath(bmlupdater::JoinPath(gameRoot, L"ModLoader"), L"UpdaterSimulation"),
        L"smoke.txt");
    EXPECT_TRUE(bmlupdater::PathExists(target));
    EXPECT_TRUE(bmlupdater::PathExists(bmlupdater::JoinPath(stateRoot, L"state.json")));
    EXPECT_TRUE(bmlupdater::PathExists(bmlupdater::JoinPath(stateRoot, L"installed.manifest.json")));
    EXPECT_EQ(service.GetStatus().installedVersion, "v-test");

    result = service.Rollback(nullptr);
    ASSERT_TRUE(result.ok) << result.message;

    EXPECT_FALSE(bmlupdater::PathExists(target));
    EXPECT_FALSE(bmlupdater::PathExists(bmlupdater::JoinPath(stateRoot, L"state.json")));
    EXPECT_FALSE(bmlupdater::PathExists(bmlupdater::JoinPath(stateRoot, L"installed.manifest.json")));
    EXPECT_EQ(service.GetStatus().installedVersion, "unknown");
}

TEST(UpdaterServiceTest, RemoteSourceConfigurationRoundTripsThroughSourcesJson) {
    const std::wstring tempRoot = MakeTempRoot();
    const auto cleanup = std::unique_ptr<void, void (*)(void *)>(
        const_cast<std::wstring *>(&tempRoot),
        [](void *path) {
            std::string error;
            (void)bmlupdater::RemoveDirectoryTree(*static_cast<std::wstring *>(path), error);
        });

    const std::wstring gameRoot = bmlupdater::JoinPath(tempRoot, L"game");
    const std::wstring stateRoot = bmlupdater::JoinPath(bmlupdater::JoinPath(gameRoot, L"ModLoader"), L"Updater");
    bmlupdater::UpdaterService service({gameRoot, stateRoot});

    std::string baseUrl = "unchanged";
    bmlupdater::Result result = service.GetSourceBaseUrl(baseUrl);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_TRUE(baseUrl.empty());

    result = service.SetSourceBaseUrl("https://updates.example.test/bml/");
    ASSERT_TRUE(result.ok) << result.message;
    result = service.GetSourceBaseUrl(baseUrl);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(baseUrl, "https://updates.example.test/bml");

    bmlupdater::UpdaterSourceConfig config;
    result = service.GetSourceConfig(config);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(config.baseUrl, "https://updates.example.test/bml");
    EXPECT_EQ(config.defaultChannel, "stable");

    result = service.SetSourceBaseUrl("https://updates.example.test/bml", "beta");
    ASSERT_TRUE(result.ok) << result.message;
    result = service.GetSourceConfig(config);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(config.defaultChannel, "beta");

    result = service.SetSourceBaseUrl("https://updates.example.test/bml", "nightly");
    EXPECT_FALSE(result.ok);

    result = service.SetSourceBaseUrl("http://updates.example.test/bml");
    EXPECT_FALSE(result.ok);

    result = service.ClearSource();
    ASSERT_TRUE(result.ok) << result.message;
    result = service.GetSourceBaseUrl(baseUrl);
    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_TRUE(baseUrl.empty());
}
