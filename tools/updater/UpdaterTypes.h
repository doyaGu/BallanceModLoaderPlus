#ifndef BML_UPDATER_TYPES_H
#define BML_UPDATER_TYPES_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace bmlupdater {
    struct Result {
        bool ok{false};
        std::string message;

        static Result Success(std::string message = {}) {
            return {true, std::move(message)};
        }

        static Result Failure(std::string message) {
            return {false, std::move(message)};
        }
    };

    struct UpdaterContext {
        std::wstring gameRoot;
        std::wstring updaterStateRoot;
    };

    struct ManagedFile {
        std::string path;
        std::string sha256;
        uint64_t size{0};
    };

    struct RemoveFile {
        std::string path;
        std::string sha256;
    };

    struct UpdaterManifest {
        int schemaVersion{1};
        std::string version;
        std::string packageFileName;
        std::string packageSha256;
        std::vector<ManagedFile> managedFiles;
        std::vector<std::string> preserve;
        std::vector<RemoveFile> removeFiles;
    };

    struct StagedFile {
        std::string relativePath;
        std::wstring stagedPath;
        std::string sha256;
        uint64_t size{0};
    };

    enum class OperationKind {
        InstallOrReplace,
        Remove
    };

    struct ApplyOperation {
        OperationKind kind{OperationKind::InstallOrReplace};
        std::string relativePath;
        std::wstring sourcePath;
        std::wstring targetPath;
        std::wstring backupPath;
        std::string expectedOldSha256;
        std::string newSha256;
    };

    struct ApplyPlan {
        std::string version;
        std::vector<ApplyOperation> operations;
        std::vector<std::string> diagnostics;
    };

    struct StatusInfo {
        std::string installedVersion;
        std::string lastError;
        bool pendingTransaction{false};
    };

    struct LocalPackageVerification {
        UpdaterManifest manifest;
        std::wstring manifestPath;
        std::wstring signaturePath;
        std::wstring stagingRoot;
        std::vector<StagedFile> stagedFiles;
        std::vector<std::string> diagnostics;
    };

    using ProgressCallback = std::function<void(const std::string &)>;
} // namespace bmlupdater

#endif // BML_UPDATER_TYPES_H
