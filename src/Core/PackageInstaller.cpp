#include "PackageInstaller.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

#include <zip.h>

#include "HashUtils.h"
#include "JsonUtils.h"
#include "Logging.h"
#include "ModManifest.h"
#include "PackageInstallState.h"
#include "PackagePaths.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "TimeUtils.h"

namespace BML::Core {
    namespace fs = std::filesystem;

    namespace {
        constexpr const wchar_t *kPackageExtension = L".bp";

        struct InstallCandidate {
            fs::path source_bp_path;
            fs::path payload_root;
            fs::path backup_path;
            std::string content_hash;
            ModManifest manifest;
            std::string manifest_hash;
        };

        bool EqualsIgnoreCase(std::wstring lhs, const wchar_t *rhs) {
            if (!rhs) {
                return lhs.empty();
            }
            std::wstring rhsValue(rhs);
            std::transform(lhs.begin(), lhs.end(), lhs.begin(), towlower);
            std::transform(rhsValue.begin(), rhsValue.end(), rhsValue.begin(), towlower);
            return lhs == rhsValue;
        }

        bool HasPackageExtension(const fs::path &path) {
            return EqualsIgnoreCase(path.extension().wstring(), kPackageExtension);
        }

        std::string SanitizeFileComponent(std::string value) {
            for (char &ch : value) {
                const bool safe = std::isalnum(static_cast<unsigned char>(ch)) != 0
                    || ch == '.' || ch == '-' || ch == '_';
                if (!safe) {
                    ch = '_';
                }
            }
            return value.empty() ? "unknown" : value;
        }

        std::string ShortHash(std::string_view value) {
            return std::string(value.substr(0, std::min<size_t>(8, value.size())));
        }

        bool EnsureDirectory(const fs::path &path, std::string &error) {
            std::error_code ec;
            fs::create_directories(path, ec);
            if (ec) {
                error = ec.message();
                return false;
            }
            return true;
        }

        std::vector<fs::path> ScanPendingPackages(const fs::path &packages_dir) {
            std::vector<fs::path> result;
            std::error_code iterEc;
            fs::directory_iterator iter(packages_dir,
                                        fs::directory_options::skip_permission_denied,
                                        iterEc);
            fs::directory_iterator end;
            while (!iterEc && iter != end) {
                if (iter->is_regular_file() && HasPackageExtension(iter->path())) {
                    result.emplace_back(iter->path());
                }
                iter.increment(iterEc);
            }
            std::sort(result.begin(), result.end());
            return result;
        }

        bool IsUnsafeArchiveEntry(std::string_view entryName) {
            if (entryName.empty()) {
                return true;
            }
            if (entryName[0] == '/' || entryName[0] == '\\') {
                return true;
            }
            if (entryName.size() >= 2
                && std::isalpha(static_cast<unsigned char>(entryName[0])) != 0
                && entryName[1] == ':') {
                return true;
            }

            std::string segment;
            for (char ch : entryName) {
                if (ch == '/' || ch == '\\') {
                    if (segment == "..") {
                        return true;
                    }
                    segment.clear();
                    continue;
                }
                segment.push_back(ch);
            }
            return segment == "..";
        }

        bool ValidateArchiveEntries(const fs::path &archivePath, std::string &error) {
            const auto archiveUtf8 = utils::Utf16ToUtf8(archivePath.wstring());
            zip_t *zip = zip_open(archiveUtf8.c_str(), 0, 'r');
            if (!zip) {
                error = "unsupported or corrupt bp package";
                return false;
            }

            struct ZipGuard {
                zip_t *handle;
                ~ZipGuard() {
                    if (handle) {
                        zip_close(handle);
                    }
                }
            } guard{zip};

            const ssize_t totalEntries = zip_entries_total(zip);
            if (totalEntries < 0) {
                error = "unable to enumerate package entries";
                return false;
            }

            for (ssize_t i = 0; i < totalEntries; ++i) {
                if (zip_entry_openbyindex(zip, static_cast<size_t>(i)) != 0) {
                    error = "unable to inspect package entry";
                    return false;
                }

                const char *entryName = zip_entry_name(zip);
                const bool unsafe = !entryName || IsUnsafeArchiveEntry(entryName);
                zip_entry_close(zip);
                if (unsafe) {
                    error = "package contains an unsafe path";
                    return false;
                }
            }

            return true;
        }

        fs::path ResolvePayloadRoot(const fs::path &extractRoot) {
            const fs::path directManifest = extractRoot / L"mod.toml";
            if (fs::exists(directManifest) && fs::is_regular_file(directManifest)) {
                return extractRoot;
            }

            std::vector<fs::directory_entry> children;
            std::error_code iterEc;
            fs::directory_iterator iter(extractRoot,
                                        fs::directory_options::skip_permission_denied,
                                        iterEc);
            fs::directory_iterator end;
            while (!iterEc && iter != end) {
                children.emplace_back(*iter);
                iter.increment(iterEc);
            }

            if (iterEc || children.size() != 1 || !children.front().is_directory()) {
                return {};
            }

            const fs::path nestedManifest = children.front().path() / L"mod.toml";
            if (!fs::exists(nestedManifest) || !fs::is_regular_file(nestedManifest)) {
                return {};
            }

            return children.front().path();
        }

        bool IsPathWithin(const fs::path &base, const fs::path &candidate) {
            const fs::path normalizedBase = base.lexically_normal();
            const fs::path normalizedCandidate = candidate.lexically_normal();

            auto baseIt = normalizedBase.begin();
            auto candidateIt = normalizedCandidate.begin();
            for (; baseIt != normalizedBase.end() && candidateIt != normalizedCandidate.end();
                 ++baseIt, ++candidateIt) {
                if (*baseIt != *candidateIt) {
                    return false;
                }
            }
            return baseIt == normalizedBase.end();
        }

        bool ValidateInstallTarget(const fs::path &modsDir,
                                   const ModManifest &manifest,
                                   fs::path &outTarget,
                                   std::string &error) {
            outTarget = (modsDir / utils::Utf8ToUtf16(manifest.package.id)).lexically_normal();
            if (!IsPathWithin(modsDir, outTarget)) {
                error = "package id resolves outside Mods directory";
                return false;
            }
            return true;
        }

        bool ValidatePayloadManifest(const fs::path &payloadRoot,
                                     InstallCandidate &candidate,
                                     std::string &error) {
            ManifestParser parser;
            ManifestParseError parseError;
            const fs::path manifestPath = payloadRoot / L"mod.toml";
            if (!parser.ParseFile(manifestPath.wstring(), candidate.manifest, parseError)) {
                error = parseError.message;
                return false;
            }

            const fs::path entryPath = (payloadRoot / utils::Utf8ToUtf16(candidate.manifest.package.entry))
                .lexically_normal();
            if (!IsPathWithin(payloadRoot, entryPath) || !fs::exists(entryPath)) {
                error = "declared entry '" + candidate.manifest.package.entry + "' does not exist";
                return false;
            }

            return true;
        }

        fs::path BuildArchivePath(const PackagePaths &paths,
                                  const std::string &packageId,
                                  const std::string &version,
                                  std::string_view contentHash) {
            const std::string fileName = utils::GetCurrentUtcCompactTimestamp()
                + "_" + SanitizeFileComponent(packageId)
                + "_" + SanitizeFileComponent(version)
                + "_" + ShortHash(contentHash)
                + ".bp";
            return paths.archive_dir / utils::Utf8ToUtf16(fileName);
        }

        fs::path MakeUniqueArchivePath(fs::path path) {
            if (!fs::exists(path)) {
                return path;
            }

            const fs::path stem = path.stem();
            const fs::path extension = path.extension();
            for (int index = 1; index < 10000; ++index) {
                fs::path candidate = path.parent_path()
                    / (stem.wstring() + L"_" + std::to_wstring(index) + extension.wstring());
                if (!fs::exists(candidate)) {
                    return candidate;
                }
            }
            return path;
        }

        bool WriteRejectSidecar(const fs::path &sidecarPath,
                                const fs::path &sourceBpPath,
                                const PackageRejectInfo &reject,
                                std::string &error) {
            error.clear();
            utils::MutableJsonDocument document;
            yyjson_mut_val *root = document.CreateObject();
            if (!root) {
                error = "unable to allocate reject sidecar JSON document";
                return false;
            }
            document.SetRoot(root);

            if (!document.AddString(root, "time_utc", utils::GetCurrentUtcIso8601())
                || !document.AddString(root, "source_name", utils::Utf16ToUtf8(sourceBpPath.filename().wstring()))
                || !document.AddString(root,
                                       "content_hash",
                                       reject.content_hash.empty() ? "unknown" : reject.content_hash)
                || !document.AddString(root, "stage", reject.stage)
                || !(reject.package_id.empty()
                     ? document.AddNull(root, "package_id")
                     : document.AddString(root, "package_id", reject.package_id))
                || !(reject.version.empty()
                     ? document.AddNull(root, "version")
                     : document.AddString(root, "version", reject.version))
                || !document.AddString(root, "message", reject.message)) {
                error = "unable to serialize reject sidecar JSON";
                return false;
            }

            const std::string payload = document.Write(true, error);
            if (payload.empty()) {
                return false;
            }

            std::ofstream ofs(sidecarPath, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open()) {
                error = "unable to open reject sidecar file";
                return false;
            }
            ofs << payload;
            ofs.flush();
            if (!ofs.good()) {
                error = "unable to write reject sidecar file";
                return false;
            }
            return true;
        }

        bool MovePath(const fs::path &from, const fs::path &to, std::string &error) {
            std::error_code ec;
            fs::rename(from, to, ec);
            if (ec) {
                error = ec.message();
                return false;
            }
            return true;
        }

        void RemovePathIfExists(const fs::path &path) {
            std::error_code ec;
            fs::remove_all(path, ec);
        }

        bool RejectPackage(const PackagePaths &paths,
                           const fs::path &sourceBpPath,
                           PackageRejectInfo reject,
                           PackageSyncDiagnostics &diag,
                           std::string &outError) {
            std::string error;
            if (!EnsureDirectory(paths.rejected_dir, error)) {
                outError = "failed to prepare rejected package directory: " + error;
                return false;
            }

            const std::string baseName = utils::GetCurrentUtcCompactTimestamp() + "_"
                + SanitizeFileComponent(utils::Utf16ToUtf8(sourceBpPath.stem().wstring()));
            const fs::path rejectedPath = paths.rejected_dir / utils::Utf8ToUtf16(baseName + ".bp");
            const fs::path sidecarPath = paths.rejected_dir / utils::Utf8ToUtf16(baseName + ".json");

            std::error_code renameEc;
            fs::rename(sourceBpPath, rejectedPath, renameEc);
            if (renameEc) {
                outError = "failed to quarantine rejected package: " + renameEc.message();
                return false;
            }

            reject.rejected_bp_path = rejectedPath.wstring();
            reject.sidecar_json_path = sidecarPath.wstring();
            std::string sidecarError;
            if (!WriteRejectSidecar(sidecarPath, sourceBpPath, reject, sidecarError)) {
                reject.sidecar_json_path.clear();
                diag.warnings.emplace_back(
                    "Rejected package '" + utils::Utf16ToUtf8(sourceBpPath.filename().wstring())
                    + "' but failed to write sidecar: " + sidecarError);
            }
            diag.rejected.emplace_back(std::move(reject));
            diag.warnings.emplace_back(
                "Rejected package '" + utils::Utf16ToUtf8(sourceBpPath.filename().wstring())
                + "' during " + diag.rejected.back().stage + ": " + diag.rejected.back().message);
            return true;
        }
    } // namespace

    bool PackageInstaller::SyncPackages(const std::wstring &modloader_dir,
                                        PackageSyncDiagnostics &out_diag,
                                        std::string &out_error) {
        out_diag = {};
        out_error.clear();

        const PackagePaths paths = GetPackagePaths(modloader_dir);
        std::string ensureError;
        if (!EnsureDirectory(paths.mods_dir, ensureError)
            || !EnsureDirectory(paths.packages_dir, ensureError)
            || !EnsureDirectory(paths.archive_dir, ensureError)
            || !EnsureDirectory(paths.rejected_dir, ensureError)
            || !EnsureDirectory(paths.staging_dir, ensureError)) {
            out_error = ensureError;
            return false;
        }

        RemovePathIfExists(paths.staging_dir);
        if (!EnsureDirectory(paths.staging_dir, ensureError)) {
            out_error = ensureError;
            return false;
        }

        PackageInstallState state;
        if (!state.Load(paths.state_path.wstring(), out_error)) {
            return false;
        }
        if (!state.GetLastLoadWarning().empty()) {
            out_diag.warnings.push_back(state.GetLastLoadWarning());
        }

        for (const auto &bpPath : ScanPendingPackages(paths.packages_dir)) {
            const auto bytes = utils::ReadBinaryFileW(bpPath.wstring());
            if (bytes.empty()) {
                std::string rejectError;
                if (!RejectPackage(paths, bpPath,
                                   {{}, {}, "hash", "unable to read package bytes", "", "", ""},
                                   out_diag,
                                   rejectError)) {
                    out_error = rejectError;
                    return false;
                }
                continue;
            }

            const std::string contentHash = utils::Fnv1a64Hex(bytes);
            if (const PackageRecord *existing = state.FindPackageByHash(contentHash)) {
                std::string moveError;
                const fs::path archivePath = MakeUniqueArchivePath(BuildArchivePath(
                    paths, existing->package_id, existing->version, contentHash));
                if (!MovePath(bpPath, archivePath, moveError)) {
                    std::string rejectError;
                    if (!RejectPackage(paths, bpPath,
                                       {{}, {}, "archive", moveError,
                                        existing->package_id, existing->version, contentHash},
                                       out_diag,
                                       rejectError)) {
                        out_error = rejectError;
                        return false;
                    }
                    continue;
                }

                PackageRecord updated = *existing;
                updated.last_seen_at_utc = utils::GetCurrentUtcIso8601();
                state.UpsertPackage(std::move(updated));
                state.AppendEvent({
                    utils::GetCurrentUtcIso8601(),
                    "already_installed",
                    existing->package_id,
                    contentHash,
                    "Duplicate package archived without reinstall",
                });
                if (!state.Save(paths.state_path.wstring(), out_error)) {
                    return false;
                }
                continue;
            }

            const fs::path txnDir = paths.staging_dir / utils::Utf8ToUtf16(contentHash);
            const fs::path extractRoot = txnDir / L"payload";
            if (!EnsureDirectory(txnDir, ensureError)) {
                out_error = ensureError;
                return false;
            }

            std::string stageError;
            if (!ValidateArchiveEntries(bpPath, stageError)
                || !utils::ExtractZipW(bpPath.wstring(), extractRoot.wstring())) {
                std::string rejectError;
                if (!RejectPackage(paths, bpPath,
                                   {{}, {}, "extract",
                                    stageError.empty() ? "unsupported or corrupt bp package" : stageError,
                                    "", "", contentHash},
                                   out_diag,
                                   rejectError)) {
                    out_error = rejectError;
                    RemovePathIfExists(txnDir);
                    return false;
                }
                RemovePathIfExists(txnDir);
                continue;
            }

            InstallCandidate candidate;
            candidate.source_bp_path = bpPath;
            candidate.payload_root = ResolvePayloadRoot(extractRoot);
            candidate.content_hash = contentHash;
            candidate.manifest_hash = contentHash;

            if (candidate.payload_root.empty()) {
                std::string rejectError;
                if (!RejectPackage(paths, bpPath,
                                   {{}, {}, "validate",
                                    "archive must contain mod.toml at the root or in one wrapper directory",
                                    "", "", contentHash},
                                   out_diag,
                                   rejectError)) {
                    out_error = rejectError;
                    RemovePathIfExists(txnDir);
                    return false;
                }
                RemovePathIfExists(txnDir);
                continue;
            }

            if (!ValidatePayloadManifest(candidate.payload_root, candidate, stageError)) {
                std::string rejectError;
                if (!RejectPackage(paths, bpPath,
                                   {{}, {}, "validate", stageError,
                                    candidate.manifest.package.id, candidate.manifest.package.version,
                                    contentHash},
                                   out_diag,
                                   rejectError)) {
                    out_error = rejectError;
                    RemovePathIfExists(txnDir);
                    return false;
                }
                RemovePathIfExists(txnDir);
                continue;
            }

            fs::path installTarget;
            if (!ValidateInstallTarget(paths.mods_dir, candidate.manifest, installTarget, stageError)) {
                std::string rejectError;
                if (!RejectPackage(paths, bpPath,
                                   {{}, {}, "validate", stageError,
                                    candidate.manifest.package.id, candidate.manifest.package.version,
                                    contentHash},
                                   out_diag,
                                   rejectError)) {
                    out_error = rejectError;
                    RemovePathIfExists(txnDir);
                    return false;
                }
                RemovePathIfExists(txnDir);
                continue;
            }

            const fs::path archivePath = MakeUniqueArchivePath(BuildArchivePath(
                paths, candidate.manifest.package.id, candidate.manifest.package.version, contentHash));
            const bool hadExistingInstall = fs::exists(installTarget);
            PackageInstallState originalState = state;

            if (hadExistingInstall) {
                candidate.backup_path = txnDir / L"backup";
                if (!MovePath(installTarget, candidate.backup_path, stageError)) {
                    std::string rejectError;
                    if (!RejectPackage(paths, bpPath,
                                       {{}, {}, "install", stageError,
                                        candidate.manifest.package.id, candidate.manifest.package.version,
                                        contentHash},
                                       out_diag,
                                       rejectError)) {
                        out_error = rejectError;
                        RemovePathIfExists(txnDir);
                        return false;
                    }
                    RemovePathIfExists(txnDir);
                    continue;
                }
            }

            if (!MovePath(candidate.payload_root, installTarget, stageError)) {
                if (hadExistingInstall) {
                    std::string rollbackError;
                    MovePath(candidate.backup_path, installTarget, rollbackError);
                }
                std::string rejectError;
                if (!RejectPackage(paths, bpPath,
                                   {{}, {}, "install", stageError,
                                    candidate.manifest.package.id, candidate.manifest.package.version,
                                    contentHash},
                                   out_diag,
                                   rejectError)) {
                    out_error = rejectError;
                    RemovePathIfExists(txnDir);
                    return false;
                }
                RemovePathIfExists(txnDir);
                continue;
            }

            const std::string nowIso = utils::GetCurrentUtcIso8601();
            state.UpsertInstalled({
                candidate.manifest.package.id,
                contentHash,
                candidate.manifest.package.version,
                installTarget.wstring(),
                candidate.manifest.package.entry,
                nowIso,
            });
            state.UpsertPackage({
                candidate.manifest.package.id,
                candidate.manifest.package.version,
                contentHash,
                archivePath.wstring(),
                "installed",
                candidate.manifest.package.entry,
                candidate.manifest_hash,
                nowIso,
                nowIso,
            });
            state.AppendEvent({
                nowIso,
                hadExistingInstall ? "updated" : "installed",
                candidate.manifest.package.id,
                contentHash,
                hadExistingInstall ? "Updated installed package" : "Installed package",
            });

            if (!state.Save(paths.state_path.wstring(), out_error)) {
                RemovePathIfExists(installTarget);
                if (hadExistingInstall) {
                    std::string rollbackError;
                    MovePath(candidate.backup_path, installTarget, rollbackError);
                }
                state = originalState;
                std::string rejectError;
                if (!RejectPackage(paths, bpPath,
                                   {{}, {}, "state_write", out_error,
                                    candidate.manifest.package.id, candidate.manifest.package.version,
                                    contentHash},
                                   out_diag,
                                   rejectError)) {
                    out_error = rejectError;
                    RemovePathIfExists(txnDir);
                    return false;
                }
                out_error.clear();
                RemovePathIfExists(txnDir);
                continue;
            }

            if (!MovePath(bpPath, archivePath, stageError)) {
                RemovePathIfExists(installTarget);
                if (hadExistingInstall) {
                    std::string rollbackError;
                    MovePath(candidate.backup_path, installTarget, rollbackError);
                }
                state = originalState;
                std::string restoreError;
                const bool restored = state.Save(paths.state_path.wstring(), restoreError);
                (void) restored;
                std::string rejectError;
                if (!RejectPackage(paths, bpPath,
                                   {{}, {}, "archive", stageError,
                                    candidate.manifest.package.id, candidate.manifest.package.version,
                                    contentHash},
                                   out_diag,
                                   rejectError)) {
                    out_error = rejectError;
                    RemovePathIfExists(txnDir);
                    return false;
                }
                RemovePathIfExists(txnDir);
                continue;
            }

            RemovePathIfExists(txnDir);
        }

        return true;
    }
} // namespace BML::Core
