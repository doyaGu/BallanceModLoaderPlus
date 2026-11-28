#include <gtest/gtest.h>
#include "UpdaterManifest.h"
#include "UpdaterNetwork.h"
#include "UpdaterPaths.h"
#include "UpdaterRemote.h"
#include "UpdaterService.h"
#include "UpdaterSdl.h"
#include "UpdaterTarget.h"
#include "UpdaterZip.h"
#include "CryptoUtils.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "Platform/FileSystemNative.h"
#if BML_RUNTIME_SDL
#include <SDL3/SDL.h>
#else
#include <process.h>
#endif
#include <chrono>
#include "UpdaterSigning.h"
#include <zip.h>
#include <cstdlib>
#include <cstdio>
#include <array>
#include <memory>
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#endif
#if BML_RUNTIME_SDL
#include <SDL3/SDL_main.h>
#endif

using namespace bmlupdater;
namespace {
constexpr int InterruptedExitCode = 73;
constexpr std::string_view InterruptApply = "--interrupt-apply";
constexpr std::string_view InterruptRollback = "--interrupt-rollback";
std::string testExecutable;

void Require(Result result) { if (!result.ok) throw std::runtime_error(result.message); }

int RunInterruptedTransaction(std::string_view action, const char *gamePath, const char *packagePath) {
    UpdaterService service({utils::Utf8ToUtf16(gamePath), {}});
    int operations = 0;
    if (action == InterruptApply) {
        LocalPackageVerification verification;
        Require(service.VerifyLocalPackage(utils::Utf8ToUtf16(packagePath), verification));
        ApplyPlan plan;
        Require(service.BuildApplyPlan(verification, plan));
        Require(service.Apply(verification, plan, [&](const auto &message) {
            if (message.starts_with("installing ") && ++operations == 2) std::_Exit(InterruptedExitCode);
        }));
    } else {
        Require(service.Rollback([&](const auto &) {
            if (++operations == 2) std::_Exit(InterruptedExitCode);
        }));
    }
    return 74;
}

void Put(const std::wstring &path, std::string_view text) {
    std::string error;
    if (!WriteTextFile(path, text, error)) throw std::runtime_error(error);
}
std::string Sign(std::string_view text) {
    // Public, deterministic TEST key: P-256 private scalar 1.
    auto der = HexToBytes("30770201010420" + std::string(63, '0') + "1a00a06082a8648ce3d030107a14403420004"
        "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
        "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5");
    SigningKey key(der);
    std::array<uint8_t, 32> digest{};
    if (!utils::Sha256(reinterpret_cast<const uint8_t *>(text.data()), text.size(), digest.data()))
        throw std::runtime_error("Unable to hash test manifest");
    const auto raw = key.SignDigest(digest.data());
    constexpr char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string base64;
    for (size_t i = 0; i < raw.size(); i += 3) {
        const uint32_t value = (uint32_t(raw[i]) << 16) |
            (i + 1 < raw.size() ? uint32_t(raw[i + 1]) << 8 : 0) |
            (i + 2 < raw.size() ? uint32_t(raw[i + 2]) : 0);
        base64 += digits[(value >> 18) & 63];
        base64 += digits[(value >> 12) & 63];
        base64 += i + 1 < raw.size() ? digits[(value >> 6) & 63] : '=';
        base64 += i + 2 < raw.size() ? digits[value & 63] : '=';
    }
    return base64;
}
std::string Abi(const UpdateTarget &target, const char *version = "v0") {
    return "{\"schema\":1,\"runtime\":\"" + target.runtime + "\",\"platform\":\"" + target.platform +
        "\",\"architecture\":\"" + target.architecture + "\",\"host_linkage\":\"" + target.hostLinkage +
        "\",\"sdk_abi\":\"" + std::string(64, 'a') + "\",\"configuration\":\"Debug\",\"scripts\":false,\"version\":\"" + version + "\"}";
}

// Small image-header fixtures exercise diagnostics, not executable game code.
std::string Binary(const UpdateTarget &target, bool executable, const std::string &payload = {}) {
    std::string bytes(128, '\0');
    auto word = [&](size_t offset, uint32_t value) {
        for (size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<char>(value >> (i * 8));
    };
    if (target.platform == "windows") {
        bytes[0] = 'M'; bytes[1] = 'Z'; word(0x3c, 64); word(64, 0x4550);
        word(68, target.architecture == "arm64" ? 0xaa64 : target.architecture == "x64" ? 0x8664 : 0x14c);
        bytes[64 + 23] = executable ? 0 : 0x20;
        word(64 + 24, target.architecture == "x86" ? 0x10b : 0x20b);
    } else if (target.platform == "linux") {
        bytes[0] = 0x7f; bytes[1] = 'E'; bytes[2] = 'L'; bytes[3] = 'F';
        bytes[4] = target.architecture == "x86" ? 1 : 2; bytes[5] = 1; bytes[16] = executable ? 2 : 3;
        if (executable) word(24, 0x1000);
        bytes[18] = static_cast<char>(target.architecture == "arm64" ? 183 : target.architecture == "x64" ? 62 : 3);
    } else {
        word(0, 0xfeedfacf); word(4, target.architecture == "arm64" ? 0x100000c : 0x1000007);
        word(12, executable ? 2 : 6);
    }
    return bytes + payload;
}

class UpdaterSdlTest : public ::testing::Test {
protected:
    std::wstring root, game, package, loader;
    UpdateTarget target{BML_UPDATER_RUNTIME, BML_UPDATER_PLATFORM, BML_UPDATER_ARCH, "dynamic"};
    std::unique_ptr<UpdaterService> service;
    void SetUp() override {
        root = JoinPath(utils::GetTempPathW(), L"bml-updater-测试-" + std::to_wstring(BML::Platform::ProcessId()) + L"-" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count()));
        game = JoinPath(root, L"中文 游戏");
        Put(JoinPath(game, L"ModLoader/BML.abi.json"), Abi(target));
        loader = utils::Utf8ToUtf16(LoaderPath(target));
        service = std::make_unique<UpdaterService>(UpdaterContext{game, {}});
    }
    void TearDown() override { std::string error; EXPECT_TRUE(RemoveDirectoryTree(root, error)) << error; }
    UpdaterManifest MakePackage(const std::string &version = "v1", bool extra = true) {
        UpdaterManifest manifest;
        manifest.schemaVersion = 2;
        manifest.target = target;
        manifest.packageType = target.hostLinkage == "static" ? "player-runtime" : "bml-runtime";
        manifest.version = version;
        manifest.packageFileName = "BMLPlus-Update-" + version + ".zip";
        std::vector<std::pair<std::string, std::string>> files{
            {PathUtf8(loader), Binary(target, target.hostLinkage == "static", "new loader " + version)},
            {"ModLoader/BML.abi.json", Abi(target, version.c_str())}};
        if (extra) files.emplace_back("ModLoader/Fonts/测试.txt", "");
        auto *zip = zip_stream_open(nullptr, 0, 6, 'w');
        if (!zip) throw std::runtime_error("Unable to create test ZIP");
        for (const auto &[name, data] : files) {
            if (zip_entry_open(zip, name.c_str()) < 0 || zip_entry_write(zip, data.data(), data.size()) < 0 || zip_entry_close(zip) < 0)
                throw std::runtime_error("Unable to write test ZIP entry");
            manifest.managedFiles.push_back({name, utils::Sha256Hex(data), data.size(), name == PathUtf8(loader) ? 0755u : 0644u});
        }
        void *bytes{}; size_t length{};
        if (zip_stream_copy(zip, &bytes, &length) < 0) throw std::runtime_error("Unable to copy test ZIP");
        package = JoinPath(root, utils::Utf8ToUtf16(manifest.packageFileName));
        std::string error;
        const bool written = WriteBinaryFile(package, bytes, length, error);
        free(bytes); zip_stream_close(zip);
        if (!written) throw std::runtime_error(error);
        manifest.packageSha256 = utils::Sha256FileHex(package);
        SaveManifest(manifest);
        return manifest;
    }
    void SaveManifest(const UpdaterManifest &manifest) {
        std::string error;
        auto text = WriteManifestJson(manifest, error);
        Put(DeriveManifestPath(package), text);
        Put(DeriveSignaturePath(DeriveManifestPath(package)), Sign(text));
    }
    LocalPackageVerification Verify() {
        LocalPackageVerification verification;
        Require(service->VerifyLocalPackage(package, verification));
        return verification;
    }
    ApplyPlan Plan(const LocalPackageVerification &verification) {
        ApplyPlan plan; Require(service->BuildApplyPlan(verification, plan)); return plan;
    }
    void InterruptTransaction(std::string_view action) {
        // The worker must mutate this fixture even on platforms that re-exec.
#if BML_RUNTIME_SDL
        const auto gamePath = PathUtf8(game);
        const auto packagePath = PathUtf8(package);
        const char *arguments[]{testExecutable.c_str(), action.data(),
                                gamePath.c_str(), packagePath.c_str(), nullptr};
        std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
            SDL_CreateProcess(arguments, false), SDL_DestroyProcess);
        ASSERT_TRUE(process) << SDL_GetError();
        int exitCode = 0;
        ASSERT_TRUE(SDL_WaitProcess(process.get(), true, &exitCode)) << SDL_GetError();
#else
        const auto executable = utils::Utf8ToUtf16(testExecutable);
        const auto executableArg = L"\"" + executable + L"\"";
        const auto actionArg = utils::Utf8ToUtf16(std::string(action));
        const auto gameArg = L"\"" + game + L"\"";
        const auto packageArg = L"\"" + package + L"\"";
        const wchar_t *arguments[]{executableArg.c_str(), actionArg.c_str(),
                                   gameArg.c_str(), packageArg.c_str(), nullptr};
        const auto exitCode = _wspawnv(_P_WAIT, executable.c_str(), arguments);
#endif
        ASSERT_EQ(exitCode, InterruptedExitCode);
    }
};

TEST_F(UpdaterSdlTest, UnicodePathsAndEmptyFilesAreVerified) {
    MakePackage();
    auto verification = Verify();
    ASSERT_EQ(verification.stagedFiles.size(), 3u);
    EXPECT_EQ(verification.stagedFiles.back().size, 0u);
    ApplyPlan plan;
    EXPECT_TRUE(service->BuildApplyPlan(verification, plan).ok);
}
TEST_F(UpdaterSdlTest, SignatureRejectsTamperedAndMalformedPayloads) {
    MakePackage();
    EXPECT_TRUE(VerifyDetachedSignature("payload", Sign("payload")).ok);
    EXPECT_FALSE(VerifyDetachedSignature("changed", Sign("payload")).ok);
    Put(DeriveSignaturePath(DeriveManifestPath(package)), "invalid");
    LocalPackageVerification verification;
    EXPECT_FALSE(service->VerifyLocalPackage(package, verification).ok);
    EXPECT_FALSE(DirectoryExists(JoinPath(service->Context().updaterStateRoot, L"staging")));
}
TEST_F(UpdaterSdlTest, RejectsWin32SchemaAndWrongTargetBeforeExtraction) {
    auto manifest = MakePackage();
    manifest.schemaVersion = 1; SaveManifest(manifest);
    LocalPackageVerification verification;
    EXPECT_FALSE(service->VerifyLocalPackage(package, verification).ok);
    manifest.schemaVersion = 2;
    manifest.target.architecture = target.architecture == "arm64" ? "x64" : "arm64";
    SaveManifest(manifest);
    EXPECT_FALSE(service->VerifyLocalPackage(package, verification).ok);
    EXPECT_FALSE(DirectoryExists(JoinPath(service->Context().updaterStateRoot, L"staging")));
}
TEST_F(UpdaterSdlTest, RejectsMissingUnownedDuplicateAndPreservedManifestEntries) {
    auto manifest = MakePackage();
    std::string error; UpdaterManifest parsed;
    manifest.managedFiles.pop_back(); // The archive still contains it.
    SaveManifest(manifest);
    LocalPackageVerification verification;
    EXPECT_FALSE(service->VerifyLocalPackage(package, verification).ok);
    manifest.managedFiles.push_back(manifest.managedFiles.front());
    EXPECT_FALSE(ParseManifestJson(WriteManifestJson(manifest, error), parsed).ok);
    manifest = MakePackage();
    manifest.preserve = {"ModLoader/Fonts"};
    EXPECT_FALSE(ParseManifestJson(WriteManifestJson(manifest, error), parsed).ok);
    manifest.preserve.clear(); manifest.managedFiles[0].path = "Bin/Player";
    EXPECT_FALSE(ParseManifestJson(WriteManifestJson(manifest, error), parsed).ok);
}
TEST_F(UpdaterSdlTest, AppliesAndRollsBackFilesStateSignaturesAndPermissions) {
    Put(JoinPath(game, loader), "old loader");
    std::string error; ASSERT_TRUE(SetFileMode(JoinPath(game, loader), 0744, error));
    Put(JoinPath(game, L"ModLoader/Configs/user.cfg"), "user configuration");
    Put(JoinPath(game, L"ModLoader/Mods/user.bmodp"), "user mod");
    MakePackage(); auto verification = Verify(); auto plan = Plan(verification);
    Require(service->Apply(verification, plan));
    EXPECT_EQ(ReadTextFile(JoinPath(game, loader)), Binary(target, false, "new loader v1"));
    EXPECT_EQ(service->GetStatus().installedVersion, "v1");
    EXPECT_FALSE(service->GetStatus().pendingTransaction);
#ifndef _WIN32
    EXPECT_EQ(FileMode(JoinPath(game, loader)), 0755u);
#endif
    Require(service->Rollback());
    EXPECT_EQ(ReadTextFile(JoinPath(game, loader)), "old loader");
    EXPECT_EQ(service->GetStatus().installedVersion, "v0");
    EXPECT_FALSE(PathExists(JoinPath(game, L"ModLoader/Fonts/测试.txt")));
    EXPECT_EQ(ReadTextFile(JoinPath(game, L"ModLoader/Configs/user.cfg")), "user configuration");
    EXPECT_EQ(ReadTextFile(JoinPath(game, L"ModLoader/Mods/user.bmodp")), "user mod");
#ifndef _WIN32
    EXPECT_EQ(FileMode(JoinPath(game, loader)), 0744u);
#endif
}
TEST_F(UpdaterSdlTest, FailureAfterFirstInstallRestoresOriginalFiles) {
    Put(JoinPath(game, loader), "old loader");
    MakePackage(); auto verification = Verify(); auto plan = Plan(verification);
    int installs = 0;
    auto result = service->Apply(verification, plan, [&](const auto &message) {
        if (message.starts_with("installing ") && ++installs == 2) {
            std::string error; EXPECT_TRUE(RemoveFileIfPresent(plan.operations[1].sourcePath, error));
        }
    });
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(ReadTextFile(JoinPath(game, loader)), "old loader");
    EXPECT_FALSE(service->GetStatus().pendingTransaction);
    EXPECT_EQ(service->GetStatus().installedVersion, "v0");
}
TEST_F(UpdaterSdlTest, RemovesOnlyAnUnmodifiedFileFromPreviousSignedManifest) {
    MakePackage(); auto first = Verify(); Require(service->Apply(first, Plan(first)));
    auto manifest = MakePackage("v2", false);
    manifest.removeFiles.push_back({"ModLoader/Fonts/测试.txt", {}});
    SaveManifest(manifest);
    auto second = Verify(); auto plan = Plan(second);
    ASSERT_EQ(plan.operations.back().kind, OperationKind::Remove);
    Require(service->Apply(second, plan));
    EXPECT_FALSE(PathExists(JoinPath(game, L"ModLoader/Fonts/测试.txt")));
    Require(service->Rollback());
    EXPECT_TRUE(PathExists(JoinPath(game, L"ModLoader/Fonts/测试.txt")));
    EXPECT_EQ(service->GetStatus().installedVersion, "v1");
    Put(JoinPath(game, L"ModLoader/Fonts/测试.txt"), "user changed this file");
    auto modified = Plan(second);
    EXPECT_EQ(modified.operations.size(), 2u);
    EXPECT_FALSE(modified.diagnostics.empty());
}
TEST_F(UpdaterSdlTest, FailedRollbackRetainsPendingMetadataAndCanBeRetried) {
    Put(JoinPath(game, loader), "old loader");
    MakePackage(); auto verification = Verify(); auto plan = Plan(verification);
    std::wstring damagedBackup;
    int installs = 0;
    auto result = service->Apply(verification, plan, [&](const auto &message) {
        if (message.starts_with("installing ") && ++installs == 2) {
            const auto transactions = JoinPath(service->Context().updaterStateRoot, L"transactions");
            const auto names = utils::ListDirectoriesW(transactions, L"*");
            damagedBackup = JoinPath(JoinPath(transactions, names.back()), L"backup/0");
            std::string error;
            EXPECT_TRUE(RemoveFileIfPresent(damagedBackup, error));
            EXPECT_TRUE(RemoveFileIfPresent(plan.operations[1].sourcePath, error));
        }
    });
    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(service->GetStatus().pendingTransaction);
    EXPECT_FALSE(service->Rollback().ok);
    Put(damagedBackup, "old loader");
    Require(service->Rollback());
    EXPECT_FALSE(service->GetStatus().pendingTransaction);
    EXPECT_EQ(ReadTextFile(JoinPath(game, loader)), "old loader");
}
TEST_F(UpdaterSdlTest, InterruptedProcessCanBeRecoveredFromDurableJournal) {
    Put(JoinPath(game, loader), "old loader");
    MakePackage(); auto verification = Verify(); auto plan = Plan(verification);
    ASSERT_NO_FATAL_FAILURE(InterruptTransaction(InterruptApply));
    ASSERT_TRUE(service->GetStatus().pendingTransaction);
    EXPECT_FALSE(service->Apply(verification, plan).ok);
    Require(service->Rollback());
    EXPECT_FALSE(service->GetStatus().pendingTransaction);
    EXPECT_EQ(ReadTextFile(JoinPath(game, loader)), "old loader");
    EXPECT_EQ(service->GetStatus().installedVersion, "v0");
}
TEST_F(UpdaterSdlTest, InterruptedManualRollbackAlsoRetainsRecoveryHint) {
    Put(JoinPath(game, loader), "old loader");
    MakePackage(); auto verification = Verify(); auto plan = Plan(verification);
    Require(service->Apply(verification, plan));
    ASSERT_NO_FATAL_FAILURE(InterruptTransaction(InterruptRollback));
    ASSERT_TRUE(service->GetStatus().pendingTransaction);
    Require(service->Rollback());
    EXPECT_FALSE(service->GetStatus().pendingTransaction);
    EXPECT_EQ(ReadTextFile(JoinPath(game, loader)), "old loader");
    EXPECT_EQ(service->GetStatus().installedVersion, "v0");
}
TEST_F(UpdaterSdlTest, RejectsChangedStagingOrApplyPlan) {
    MakePackage(); auto verification = Verify(); auto plan = Plan(verification);
    auto tampered = plan; tampered.operations.front().targetPath = JoinPath(root, L"outside.txt");
    EXPECT_FALSE(service->Apply(verification, tampered).ok);
    Put(plan.operations.front().sourcePath, "tampered payload");
    EXPECT_FALSE(service->Apply(verification, plan).ok);
    EXPECT_FALSE(PathExists(JoinPath(game, loader)));
}
TEST_F(UpdaterSdlTest, StateLockHasSingleOwnerAndReleasesAutomatically) {
    { UpdaterLock first(service->Context().updaterStateRoot); ASSERT_TRUE(first);
      UpdaterLock second(service->Context().updaterStateRoot); EXPECT_FALSE(second); }
    UpdaterLock third(service->Context().updaterStateRoot); EXPECT_TRUE(third);
}
TEST_F(UpdaterSdlTest, RejectsTraversalControlsAndMalformedUtf8) {
    std::string error;
    for (const auto &path : {std::string("../outside"), std::string("/absolute"), std::string("C:/drive"),
                            std::string("a\\b"), std::string("a\tb"), std::string("a\0b", 3), std::string("\xff")})
        EXPECT_FALSE(NormalizeArchivePath(path, error)) << path;
    EXPECT_TRUE(NormalizeArchivePath("ModLoader/Fonts/中文.txt", error));
    EXPECT_TRUE(IsDisallowedUpdaterPackagePath("Bin/Player.ini"));
    EXPECT_TRUE(IsDisallowedUpdaterPackagePath("Player.ini"));
}
TEST_F(UpdaterSdlTest, WritableDirectoryProbeLeavesNoFiles) {
    auto directory = JoinPath(root, L"probe");
    EXPECT_TRUE(CanCreateFileInDirectory(directory));
    EXPECT_TRUE(utils::ListFilesW(directory, L"*").empty());
}
TEST_F(UpdaterSdlTest, BinaryHeaderValidationCoversAllSevenTargets) {
    const std::vector<UpdateTarget> targets{
        {"sdl", "windows", "x86", "dynamic"}, {"sdl", "windows", "x64", "dynamic"},
        {"sdl", "windows", "arm64", "dynamic"}, {"sdl", "linux", "x64", "dynamic"},
        {"sdl", "linux", "arm64", "dynamic"}, {"sdl", "macos", "x64", "dynamic"},
        {"sdl", "macos", "arm64", "dynamic"}};
    auto image = JoinPath(root, L"image");
    for (const auto &expected : targets) {
        Put(image, Binary(expected, false));
        EXPECT_TRUE(IsCompatibleBinary(image, expected, false)) << expected.platform << expected.architecture;
        EXPECT_FALSE(IsCompatibleBinary(image, expected, true));
        auto wrong = expected; wrong.architecture = expected.architecture == "arm64" ? "x64" : "arm64";
        EXPECT_FALSE(IsCompatibleBinary(image, wrong, false));
        Put(image, Binary(expected, true));
        EXPECT_TRUE(IsCompatibleBinary(image, expected, true));
        EXPECT_FALSE(IsCompatibleBinary(image, expected, false));
    }
}
TEST_F(UpdaterSdlTest, WrongBinaryArchitectureIsRejectedDespiteMatchingMetadata) {
    auto manifest = MakePackage();
    auto verification = Verify();
    auto wrong = target; wrong.architecture = target.architecture == "arm64" ? "x64" : "arm64";
    auto &file = verification.stagedFiles.front();
    Put(file.stagedPath, Binary(wrong, false));
    verification.manifest.managedFiles.front().sha256 = utils::Sha256FileHex(file.stagedPath);
    verification.manifest.managedFiles.front().size = FileSize(file.stagedPath);
    ApplyPlan plan;
    auto result = service->BuildApplyPlan(verification, plan);
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("format/architecture"), std::string::npos);
}
#ifndef _WIN32
TEST_F(UpdaterSdlTest, SymlinkTargetsCannotEscapeTheGame) {
    auto outside = JoinPath(root, L"outside"); ASSERT_TRUE(CreateDirectories(outside));
    const auto link = JoinPath(game, L"link");
    ASSERT_EQ(::symlink(PathUtf8(outside).c_str(), PathUtf8(link).c_str()), 0);
    std::wstring output; std::string error;
    EXPECT_FALSE(ResolveUnderRoot(game, L"link/evil.txt", output, error));
    EXPECT_FALSE(ResolveUnderRoot(game, L"../evil.txt", output, error));
}
TEST_F(UpdaterSdlTest, SymlinkStagingAncestorIsRejectedBeforeAnyDeletion) {
    auto outside = JoinPath(root, L"outside");
    Put(JoinPath(outside, L"v1/keep.txt"), "untouched");
    ASSERT_TRUE(CreateDirectories(service->Context().updaterStateRoot));
    auto staging = JoinPath(service->Context().updaterStateRoot, L"staging");
    ASSERT_EQ(::symlink(PathUtf8(outside).c_str(), PathUtf8(staging).c_str()), 0);
    MakePackage(); LocalPackageVerification verification;
    EXPECT_FALSE(service->VerifyLocalPackage(package, verification).ok);
    EXPECT_EQ(ReadTextFile(JoinPath(outside, L"v1/keep.txt")), "untouched");
}
TEST_F(UpdaterSdlTest, CaseDistinctPathsRemainDistinctOnCaseSensitiveFilesystems) {
    Put(JoinPath(root, L"Case"), "upper");
    if (PathExists(JoinPath(root, L"case"))) GTEST_SKIP() << "Filesystem is case insensitive";
    Put(JoinPath(root, L"case"), "lower");
    EXPECT_EQ(ReadTextFile(JoinPath(root, L"Case")), "upper");
    EXPECT_NE(ArchivePathKey("Case"), ArchivePathKey("case"));
}
#endif
}

static int RunTests(int argc, char **argv) {
    if (argc == 4 && (argv[1] == InterruptApply || argv[1] == InterruptRollback)) {
        try {
            return RunInterruptedTransaction(argv[1], argv[2], argv[3]);
        } catch (const std::exception &error) {
            std::fprintf(stderr, "Unable to interrupt updater transaction: %s\n", error.what());
            return EXIT_FAILURE;
        }
    }

    testExecutable = argv[0];
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#if BML_RUNTIME_SDL
int main(int argc, char **argv) { return RunTests(argc, argv); }
#else
int wmain(int argc, wchar_t **argv) {
    if (argc == 4 && std::wstring_view(argv[1]) == L"--test-download") {
        const auto result = HttpDownloadFile(argv[2], argv[3], {}, std::chrono::milliseconds(750));
        std::puts(result.message.c_str());
        return result.ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    std::vector<std::string> utf8;
    for (int i = 0; i < argc; ++i)
        utf8.push_back(utils::Utf16ToUtf8(argv[i]));
    std::vector<char *> arguments;
    for (auto &arg : utf8)
        arguments.push_back(arg.data());
    arguments.push_back(nullptr);
    return RunTests(argc, arguments.data());
}
#endif
