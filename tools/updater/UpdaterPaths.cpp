#include "UpdaterPaths.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <unordered_set>

#include "StringUtils.h"

namespace bmlupdater {
    namespace {
        bool IsAsciiAlpha(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        bool HasTrailingDotOrSpace(std::string_view value) {
            return !value.empty() && (value.back() == '.' || value.back() == ' ');
        }

        bool IsReservedDosName(std::string segment) {
            const size_t dot = segment.find('.');
            if (dot != std::string::npos) {
                segment.resize(dot);
            }
            segment = ToLowerAscii(segment);
            static const std::unordered_set<std::string> reserved = {
                "con", "prn", "aux", "nul",
                "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
                "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"};
            return reserved.contains(segment);
        }

        std::wstring NormalizeSlashes(std::wstring value) {
            std::replace(value.begin(), value.end(), L'/', L'\\');
            return value;
        }

        std::wstring TrimTrailingSlashes(std::wstring value) {
            while (value.size() > 3 && (value.back() == L'\\' || value.back() == L'/')) {
                value.pop_back();
            }
            return value;
        }

        std::wstring FullPath(const std::wstring &path) {
            const DWORD needed = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
            if (needed == 0) {
                return {};
            }
            std::wstring out(needed, L'\0');
            const DWORD written = GetFullPathNameW(path.c_str(), needed, out.data(), nullptr);
            if (written == 0 || written >= needed) {
                return {};
            }
            out.resize(written);
            return TrimTrailingSlashes(NormalizeSlashes(out));
        }

    } // namespace

    std::string ToLowerAscii(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    std::string BytesToHex(const uint8_t *bytes, size_t length) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string out;
        out.reserve(length * 2);
        for (size_t i = 0; i < length; ++i) {
            out.push_back(digits[bytes[i] >> 4]);
            out.push_back(digits[bytes[i] & 0x0f]);
        }
        return out;
    }

    std::vector<uint8_t> HexToBytes(std::string_view text) {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        if ((text.size() % 2) != 0) {
            return {};
        }
        std::vector<uint8_t> bytes;
        bytes.reserve(text.size() / 2);
        for (size_t i = 0; i < text.size(); i += 2) {
            const int hi = nibble(text[i]);
            const int lo = nibble(text[i + 1]);
            if (hi < 0 || lo < 0) {
                return {};
            }
            bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
        }
        return bytes;
    }

    std::vector<uint8_t> Base64Decode(std::string_view text) {
        static constexpr std::array<int8_t, 256> table = [] {
            std::array<int8_t, 256> values{};
            values.fill(-1);
            for (int i = 0; i < 26; ++i) {
                values[static_cast<size_t>('A' + i)] = static_cast<int8_t>(i);
                values[static_cast<size_t>('a' + i)] = static_cast<int8_t>(26 + i);
            }
            for (int i = 0; i < 10; ++i) {
                values[static_cast<size_t>('0' + i)] = static_cast<int8_t>(52 + i);
            }
            values[static_cast<size_t>('+')] = 62;
            values[static_cast<size_t>('/')] = 63;
            return values;
        }();

        std::string compact;
        compact.reserve(text.size());
        for (char c : text) {
            if (!std::isspace(static_cast<unsigned char>(c))) {
                compact.push_back(c);
            }
        }
        if (compact.empty() || (compact.size() % 4) != 0) {
            return {};
        }

        std::vector<uint8_t> out;
        out.reserve((compact.size() / 4) * 3);
        for (size_t i = 0; i < compact.size(); i += 4) {
            int pad = 0;
            uint32_t accum = 0;
            for (size_t j = 0; j < 4; ++j) {
                const char c = compact[i + j];
                if (c == '=') {
                    ++pad;
                    accum <<= 6;
                    continue;
                }
                const int8_t value = table[static_cast<uint8_t>(c)];
                if (value < 0 || pad > 0) {
                    return {};
                }
                accum = (accum << 6) | static_cast<uint32_t>(value);
            }
            out.push_back(static_cast<uint8_t>((accum >> 16) & 0xff));
            if (pad < 2) out.push_back(static_cast<uint8_t>((accum >> 8) & 0xff));
            if (pad < 1) out.push_back(static_cast<uint8_t>(accum & 0xff));
        }
        return out;
    }

    std::wstring JoinPath(const std::wstring &base, const std::wstring &child) {
        if (base.empty()) {
            return NormalizeSlashes(child);
        }
        if (child.empty()) {
            return NormalizeSlashes(base);
        }
        std::wstring out = NormalizeSlashes(base);
        if (out.back() != L'\\' && out.back() != L'/') {
            out.push_back(L'\\');
        }
        std::wstring normalizedChild = NormalizeSlashes(child);
        while (!normalizedChild.empty() && (normalizedChild.front() == L'\\' || normalizedChild.front() == L'/')) {
            normalizedChild.erase(normalizedChild.begin());
        }
        out += normalizedChild;
        return out;
    }

    std::wstring ParentPath(const std::wstring &path) {
        std::wstring normalized = TrimTrailingSlashes(NormalizeSlashes(path));
        const size_t slash = normalized.find_last_of(L"\\/");
        if (slash == std::wstring::npos) {
            return {};
        }
        if (slash == 2 && normalized.size() >= 3 && normalized[1] == L':') {
            return normalized.substr(0, 3);
        }
        return normalized.substr(0, slash);
    }

    std::wstring FileName(const std::wstring &path) {
        std::wstring normalized = TrimTrailingSlashes(NormalizeSlashes(path));
        const size_t slash = normalized.find_last_of(L"\\/");
        return slash == std::wstring::npos ? normalized : normalized.substr(slash + 1);
    }

    std::string PathUtf8(const std::wstring &path) {
        return utils::Utf16ToUtf8(path);
    }

    bool PathExists(const std::wstring &path) {
        return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    }

    bool DirectoryExists(const std::wstring &path) {
        const DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool RegularFileExists(const std::wstring &path) {
        const DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool CreateDirectories(const std::wstring &path) {
        if (path.empty() || DirectoryExists(path)) {
            return true;
        }
        const std::wstring parent = ParentPath(path);
        if (!parent.empty() && parent != path && !DirectoryExists(parent) && !CreateDirectories(parent)) {
            return false;
        }
        return CreateDirectoryW(path.c_str(), nullptr) == TRUE || GetLastError() == ERROR_ALREADY_EXISTS;
    }

    bool RemoveFileIfPresent(const std::wstring &path, std::string &error) {
        if (!PathExists(path)) {
            return true;
        }
        if (DeleteFileW(path.c_str()) == TRUE) {
            return true;
        }
        error = "Unable to remove " + PathUtf8(path);
        return false;
    }

    bool RemoveDirectoryTree(const std::wstring &path, std::string &error) {
        if (!PathExists(path)) {
            return true;
        }
        WIN32_FIND_DATAW data{};
        const std::wstring pattern = JoinPath(path, L"*");
        HANDLE find = FindFirstFileW(pattern.c_str(), &data);
        if (find != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) {
                    continue;
                }
                const std::wstring child = JoinPath(path, data.cFileName);
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                    if (!RemoveDirectoryTree(child, error)) {
                        FindClose(find);
                        return false;
                    }
                } else if (DeleteFileW(child.c_str()) == FALSE) {
                    FindClose(find);
                    error = "Unable to remove " + PathUtf8(child);
                    return false;
                }
            } while (FindNextFileW(find, &data));
            FindClose(find);
        }
        if (RemoveDirectoryW(path.c_str()) == TRUE || !PathExists(path)) {
            return true;
        }
        error = "Unable to remove directory: " + PathUtf8(path);
        return false;
    }

    uint64_t FileSize(const std::wstring &path) {
        HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            return 0;
        }
        LARGE_INTEGER size{};
        const BOOL ok = GetFileSizeEx(handle, &size);
        CloseHandle(handle);
        return ok ? static_cast<uint64_t>(size.QuadPart) : 0;
    }

    std::vector<char> ReadBinaryFile(const std::wstring &path) {
        HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            return {};
        }
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(handle, &size) || size.QuadPart < 0 || size.QuadPart > static_cast<LONGLONG>(SIZE_MAX)) {
            CloseHandle(handle);
            return {};
        }
        std::vector<char> bytes(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        bool ok = true;
        if (!bytes.empty()) {
            ok = ReadFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) && read == bytes.size();
        }
        CloseHandle(handle);
        if (!ok) {
            return {};
        }
        return bytes;
    }

    std::string ReadTextFile(const std::wstring &path) {
        std::vector<char> bytes = ReadBinaryFile(path);
        return {bytes.begin(), bytes.end()};
    }

    bool WriteBinaryFile(const std::wstring &path, const void *data, size_t size, std::string &error) {
        if (!CreateDirectories(ParentPath(path))) {
            error = "Unable to create directory: " + PathUtf8(ParentPath(path));
            return false;
        }
        HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            error = "Unable to open file for writing: " + PathUtf8(path);
            return false;
        }
        bool ok = true;
        if (size > 0) {
            DWORD written = 0;
            ok = WriteFile(handle, data, static_cast<DWORD>(size), &written, nullptr) && written == size;
        }
        CloseHandle(handle);
        if (!ok) {
            error = "Unable to write file: " + PathUtf8(path);
            return false;
        }
        return true;
    }

    bool WriteTextFile(const std::wstring &path, std::string_view text, std::string &error) {
        return WriteBinaryFile(path, text.data(), text.size(), error);
    }

    std::optional<NormalizedRelativePath> NormalizeArchivePath(std::string_view rawPath, std::string &error) {
        error.clear();
        if (rawPath.empty()) {
            error = "empty path";
            return std::nullopt;
        }
        if (rawPath.find('\\') != std::string_view::npos) {
            error = "backslashes are not allowed";
            return std::nullopt;
        }
        if (rawPath.starts_with('/') || rawPath.starts_with("//") || rawPath.starts_with("\\\\") ||
            rawPath.starts_with("\\\\?\\") || rawPath.starts_with("//?/")) {
            error = "absolute or device paths are not allowed";
            return std::nullopt;
        }
        if (rawPath.size() >= 2 && IsAsciiAlpha(rawPath[0]) && rawPath[1] == ':') {
            error = "drive prefixes are not allowed";
            return std::nullopt;
        }
        if (rawPath.find(':') != std::string_view::npos) {
            error = "alternate data streams are not allowed";
            return std::nullopt;
        }

        std::vector<std::string> segments;
        size_t start = 0;
        while (start <= rawPath.size()) {
            const size_t slash = rawPath.find('/', start);
            const size_t end = slash == std::string_view::npos ? rawPath.size() : slash;
            std::string segment(rawPath.substr(start, end - start));
            if (segment.empty() || segment == "." || segment == "..") {
                error = "empty, current, or parent path segments are not allowed";
                return std::nullopt;
            }
            if (HasTrailingDotOrSpace(segment)) {
                error = "segments may not end with dot or space";
                return std::nullopt;
            }
            if (IsReservedDosName(segment)) {
                error = "reserved DOS device names are not allowed";
                return std::nullopt;
            }
            segments.push_back(std::move(segment));
            if (slash == std::string_view::npos) {
                break;
            }
            start = slash + 1;
        }

        std::string normalized;
        std::wstring wideRelative;
        for (size_t i = 0; i < segments.size(); ++i) {
            if (i > 0) {
                normalized.push_back('/');
            }
            normalized += segments[i];
            wideRelative = JoinPath(wideRelative, utils::Utf8ToUtf16(segments[i]));
        }
        return NormalizedRelativePath{normalized, wideRelative};
    }

    bool IsDisallowedUpdaterPackagePath(std::string_view normalizedPath) {
        const std::string lower = ToLowerAscii(std::string(normalizedPath));
        return lower == "bin/updater.exe" ||
               PathMatchesPrefix(lower, "modloader/updater/") ||
               PathMatchesPrefix(lower, "modloader/mods/") ||
               PathMatchesPrefix(lower, "modloader/configs/");
    }

    bool ResolveUnderRoot(const std::wstring &root,
                          const std::wstring &relative,
                          std::wstring &out,
                          std::string &error) {
        const std::wstring rootText = FullPath(root);
        const std::wstring combinedText = FullPath(JoinPath(rootText, relative));
        if (rootText.empty() || combinedText.empty()) {
            error = "unable to normalize path";
            return false;
        }
        if (combinedText.size() < rootText.size() ||
            _wcsnicmp(combinedText.c_str(), rootText.c_str(), rootText.size()) != 0 ||
            (combinedText.size() > rootText.size() &&
             combinedText[rootText.size()] != L'\\' &&
             combinedText[rootText.size()] != L'/')) {
            error = "path resolves outside root";
            return false;
        }
        out = combinedText;
        return true;
    }

    bool PathMatchesPrefix(std::string_view path, std::string_view prefix) {
        return path.size() >= prefix.size() && path.substr(0, prefix.size()) == prefix;
    }

    std::wstring DefaultStateRoot(const std::wstring &gameRoot) {
        return JoinPath(JoinPath(gameRoot, L"ModLoader"), L"Updater");
    }

    std::wstring MakeTransactionRoot(const UpdaterContext &context) {
        SYSTEMTIME time{};
        GetSystemTime(&time);
        wchar_t buffer[64]{};
        swprintf_s(buffer,
                  L"%04u%02u%02u-%02u%02u%02u-%03u",
                  time.wYear,
                  time.wMonth,
                  time.wDay,
                  time.wHour,
                  time.wMinute,
                  time.wSecond,
                  time.wMilliseconds);
        return JoinPath(JoinPath(context.updaterStateRoot, L"transactions"), buffer);
    }
} // namespace bmlupdater
