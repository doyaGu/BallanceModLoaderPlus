#include "UpdaterZip.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include <zip.h>

#include "CryptoUtils.h"
#include "UpdaterPaths.h"

namespace bmlupdater {
    namespace {
        struct ZipHandle {
            zip_t *zip{nullptr};
            ~ZipHandle() {
                if (zip) {
                    zip_stream_close(zip);
                }
            }
        };

        Result OpenZipStream(const std::wstring &zipPath, std::vector<char> &bytes, ZipHandle &handle) {
            bytes = ReadBinaryFile(zipPath);
            if (bytes.empty()) {
                return Result::Failure("Unable to read zip package: " + PathUtf8(zipPath));
            }
            int err = 0;
            handle.zip = zip_stream_openwitherror(bytes.data(), bytes.size(), 0, 'r', &err);
            if (!handle.zip) {
                return Result::Failure(std::string("Unable to open zip package: ") + zip_strerror(err));
            }
            return Result::Success();
        }

        std::unordered_set<std::string> ManagedFileSet(const UpdaterManifest &manifest) {
            std::unordered_set<std::string> set;
            for (const ManagedFile &file : manifest.managedFiles) {
                set.insert(ToLowerAscii(file.path));
            }
            return set;
        }

        const ManagedFile *FindManagedFile(const UpdaterManifest &manifest, std::string_view path) {
            const std::string lower = ToLowerAscii(std::string(path));
            for (const ManagedFile &file : manifest.managedFiles) {
                if (ToLowerAscii(file.path) == lower) {
                    return &file;
                }
            }
            return nullptr;
        }
    } // namespace

    Result EnumerateZipEntries(const std::wstring &zipPath, std::vector<ZipEntryInfo> &entries) {
        entries.clear();
        std::vector<char> bytes;
        ZipHandle handle;
        Result opened = OpenZipStream(zipPath, bytes, handle);
        if (!opened.ok) {
            return opened;
        }

        const ssize_t total = zip_entries_total(handle.zip);
        if (total <= 0) {
            return Result::Failure("Zip package contains no entries");
        }

        std::unordered_set<std::string> seen;
        for (ssize_t i = 0; i < total; ++i) {
            if (zip_entry_openbyindex(handle.zip, static_cast<size_t>(i)) < 0) {
                return Result::Failure("Unable to read zip entry");
            }
            const char *name = zip_entry_name(handle.zip);
            const bool isDirectory = zip_entry_isdir(handle.zip) == 1;
            const uint64_t size = zip_entry_uncomp_size(handle.zip);
            std::string error;
            auto normalized = NormalizeArchivePath(name ? name : "", error);
            zip_entry_close(handle.zip);
            if (!normalized) {
                return Result::Failure("Invalid zip entry path: " + error);
            }
            const std::string lower = ToLowerAscii(normalized->normalized);
            if (!seen.insert(lower).second) {
                return Result::Failure("Duplicate zip entry after normalization: " + normalized->normalized);
            }
            entries.push_back({normalized->normalized, size, isDirectory});
        }
        return Result::Success();
    }

    Result ValidateUpdaterZipEntries(const std::vector<ZipEntryInfo> &entries,
                                     const UpdaterManifest &manifest) {
        const std::unordered_set<std::string> managed = ManagedFileSet(manifest);
        std::unordered_set<std::string> packageFiles;

        for (const ZipEntryInfo &entry : entries) {
            if (entry.directory) {
                continue;
            }
            if (IsDisallowedUpdaterPackagePath(entry.path)) {
                return Result::Failure("Updater package contains forbidden path: " + entry.path);
            }
            const std::string lower = ToLowerAscii(entry.path);
            if (!managed.contains(lower)) {
                return Result::Failure("Zip entry is not listed in managedFiles: " + entry.path);
            }
            packageFiles.insert(lower);
        }

        for (const ManagedFile &file : manifest.managedFiles) {
            if (!packageFiles.contains(ToLowerAscii(file.path))) {
                return Result::Failure("managedFiles entry is missing from package: " + file.path);
            }
        }
        return Result::Success();
    }

    Result ExtractUpdaterZipToStaging(const std::wstring &zipPath,
                                      const UpdaterManifest &manifest,
                                      const std::wstring &stagingRoot,
                                      std::vector<StagedFile> &stagedFiles,
                                      ProgressCallback progress) {
        stagedFiles.clear();
        std::vector<ZipEntryInfo> entries;
        Result enumerated = EnumerateZipEntries(zipPath, entries);
        if (!enumerated.ok) {
            return enumerated;
        }
        Result validated = ValidateUpdaterZipEntries(entries, manifest);
        if (!validated.ok) {
            return validated;
        }

        std::string error;
        if (!RemoveDirectoryTree(stagingRoot, error) || !CreateDirectories(stagingRoot)) {
            return Result::Failure(error.empty() ? "Unable to create staging root: " + PathUtf8(stagingRoot) : error);
        }

        std::vector<char> bytes;
        ZipHandle handle;
        Result opened = OpenZipStream(zipPath, bytes, handle);
        if (!opened.ok) {
            return opened;
        }

        const ssize_t total = zip_entries_total(handle.zip);
        for (ssize_t i = 0; i < total; ++i) {
            if (zip_entry_openbyindex(handle.zip, static_cast<size_t>(i)) < 0) {
                return Result::Failure("Unable to read zip entry");
            }
            const char *name = zip_entry_name(handle.zip);
            const bool isDirectory = zip_entry_isdir(handle.zip) == 1;
            std::string error;
            auto normalized = NormalizeArchivePath(name ? name : "", error);
            if (!normalized) {
                zip_entry_close(handle.zip);
                return Result::Failure("Invalid zip entry path: " + error);
            }
            if (isDirectory) {
                zip_entry_close(handle.zip);
                continue;
            }

            const ManagedFile *manifestFile = FindManagedFile(manifest, normalized->normalized);
            if (!manifestFile) {
                zip_entry_close(handle.zip);
                return Result::Failure("Unexpected zip entry: " + normalized->normalized);
            }

            std::wstring outPath;
            if (!ResolveUnderRoot(stagingRoot, normalized->wideRelative, outPath, error)) {
                zip_entry_close(handle.zip);
                return Result::Failure(error);
            }

            void *buffer = nullptr;
            size_t bufferSize = 0;
            const ssize_t read = zip_entry_read(handle.zip, &buffer, &bufferSize);
            zip_entry_close(handle.zip);
            if (read < 0 || !buffer) {
                return Result::Failure("Unable to extract zip entry: " + normalized->normalized);
            }

            const auto freeBuffer = std::unique_ptr<void, decltype(&std::free)>(buffer, &std::free);
            if (!WriteBinaryFile(outPath, buffer, bufferSize, error)) {
                return Result::Failure(error);
            }

            std::array<uint8_t, 32> digest{};
            if (!utils::Sha256File(outPath, digest.data())) {
                return Result::Failure("Unable to hash staged file: " + normalized->normalized);
            }
            const std::string hash = BytesToHex(digest.data(), digest.size());
            if (hash != ToLowerAscii(manifestFile->sha256)) {
                return Result::Failure("Staged file hash does not match manifest: " + normalized->normalized);
            }
            const uint64_t fileSize = FileSize(outPath);
            if (manifestFile->size != 0 && fileSize != manifestFile->size) {
                return Result::Failure("Staged file size does not match manifest: " + normalized->normalized);
            }

            stagedFiles.push_back({normalized->normalized, outPath, hash, fileSize});
            if (progress) {
                progress("staged " + normalized->normalized);
            }
        }

        return Result::Success();
    }
} // namespace bmlupdater
