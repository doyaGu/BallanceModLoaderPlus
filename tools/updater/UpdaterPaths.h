#ifndef BML_UPDATER_PATHS_H
#define BML_UPDATER_PATHS_H

#include <optional>
#include <string>
#include <vector>

#include "UpdaterTypes.h"

namespace bmlupdater {
    struct NormalizedRelativePath {
        std::string normalized;
        std::wstring wideRelative;
    };

    [[nodiscard]] std::string ToLowerAscii(std::string value);
    [[nodiscard]] std::string BytesToHex(const uint8_t *bytes, size_t length);
    [[nodiscard]] std::vector<uint8_t> HexToBytes(std::string_view text);
    [[nodiscard]] std::vector<uint8_t> Base64Decode(std::string_view text);
    [[nodiscard]] std::wstring JoinPath(const std::wstring &base, const std::wstring &child);
    [[nodiscard]] std::wstring ParentPath(const std::wstring &path);
    [[nodiscard]] std::wstring FileName(const std::wstring &path);
    [[nodiscard]] std::string PathUtf8(const std::wstring &path);
    [[nodiscard]] bool PathExists(const std::wstring &path);
    [[nodiscard]] bool DirectoryExists(const std::wstring &path);
    [[nodiscard]] bool RegularFileExists(const std::wstring &path);
    [[nodiscard]] bool CreateDirectories(const std::wstring &path);
    [[nodiscard]] bool RemoveFileIfPresent(const std::wstring &path, std::string &error);
    [[nodiscard]] bool RemoveDirectoryTree(const std::wstring &path, std::string &error);
    [[nodiscard]] uint64_t FileSize(const std::wstring &path);
    [[nodiscard]] std::string ReadTextFile(const std::wstring &path);
    [[nodiscard]] std::vector<char> ReadBinaryFile(const std::wstring &path);
    [[nodiscard]] bool WriteTextFile(const std::wstring &path, std::string_view text, std::string &error);
    [[nodiscard]] bool WriteBinaryFile(const std::wstring &path, const void *data, size_t size, std::string &error);
    [[nodiscard]] std::optional<NormalizedRelativePath> NormalizeArchivePath(std::string_view rawPath, std::string &error);
    [[nodiscard]] bool IsDisallowedUpdaterPackagePath(std::string_view normalizedPath);
    [[nodiscard]] bool ResolveUnderRoot(const std::wstring &root,
                                        const std::wstring &relative,
                                        std::wstring &out,
                                        std::string &error);
    [[nodiscard]] bool PathMatchesPrefix(std::string_view path, std::string_view prefix);
    [[nodiscard]] std::wstring DefaultStateRoot(const std::wstring &gameRoot);
    [[nodiscard]] std::wstring MakeTransactionRoot(const UpdaterContext &context);
} // namespace bmlupdater

#endif // BML_UPDATER_PATHS_H
