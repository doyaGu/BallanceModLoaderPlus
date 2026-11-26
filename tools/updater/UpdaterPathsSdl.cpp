#include "UpdaterPaths.h"
#include "UpdaterSdl.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "Platform/FileSystemNative.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>

namespace bmlupdater {
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

    std::wstring JoinPath(const std::wstring &base, const std::wstring &child) { return utils::CombinePathW(base, child); }
    std::wstring ParentPath(const std::wstring &path) { return utils::GetParentDirectoryW(path); }
    std::wstring FileName(const std::wstring &path) { return utils::GetFileNameW(path); }
    std::string PathUtf8(const std::wstring &path) { return utils::Utf16ToUtf8(path); }
    bool PathExists(const std::wstring &path) { return BML::Platform::IsLink(PathUtf8(path)) || utils::PathExistsW(path); }
    bool DirectoryExists(const std::wstring &path) { return utils::DirectoryExistsW(path); }
    bool RegularFileExists(const std::wstring &path) {
        if (BML::Platform::IsLink(PathUtf8(path))) return false;
        SDL_PathInfo info{};
        return SDL_GetPathInfo(PathUtf8(path).c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
    }
    bool CreateDirectories(const std::wstring &path) {
        return path.empty() || (!BML::Platform::IsLink(PathUtf8(path)) && utils::CreateFileTreeW(path));
    }
    bool CanCreateFileInDirectory(const std::wstring &path) {
        if (!CreateDirectories(path)) return false;
        static std::atomic<uint64_t> counter{0};
        for (int attempt = 0; attempt < 8; ++attempt) {
            const auto local = JoinPath(path, L".bml-write-test-" + std::to_wstring(BML::Platform::ProcessId()) +
                L"-" + std::to_wstring(SDL_GetTicksNS()) + L"-" + std::to_wstring(counter.fetch_add(1)));
            auto *stream = SDL_IOFromFile(PathUtf8(local).c_str(), "wbx");
            if (!stream) continue;
            bool ok = SDL_CloseIO(stream);
            return SDL_RemovePath(PathUtf8(local).c_str()) && ok;
        }
        return false;
    }
    bool RemoveFileIfPresent(const std::wstring &path, std::string &error) {
        if (!PathExists(path)) return true;
        if (SDL_RemovePath(PathUtf8(path).c_str()) && SyncDirectory(ParentPath(path))) return true;
        error = "Unable to remove " + PathUtf8(path) + ": " + SDL_GetError();
        return false;
    }
    bool RemoveDirectoryTree(const std::wstring &path, std::string &error) {
        if (!PathExists(path)) return true;
        if (utils::DeleteDirectoryW(path)) return true;
        error = "Unable to remove directory " + PathUtf8(path) + ": " + SDL_GetError();
        return false;
    }
    uint64_t FileSize(const std::wstring &path) { auto size = utils::GetFileSizeW(path); return size < 0 ? 0 : static_cast<uint64_t>(size); }
    std::vector<char> ReadBinaryFile(const std::wstring &path) {
        std::string bytes;
        if (!utils::ReadFileBytesW(path, bytes)) return {};
        return {bytes.begin(), bytes.end()};
    }
    std::string ReadTextFile(const std::wstring &path) { std::string bytes; (void)utils::ReadFileBytesW(path, bytes); return bytes; }
    bool WriteBinaryFile(const std::wstring &path, const void *data, size_t size, std::string &error) {
        if (!CreateDirectories(ParentPath(path)) || BML::Platform::IsLink(PathUtf8(path))) {
            error = "Unable to create file without following a symlink: " + PathUtf8(path);
            return false;
        }
        uint32_t code{};
        if (utils::WriteFileBytesAtomicW(path, {data ? static_cast<const char *>(data) : "", size}, code) && SyncDirectory(ParentPath(path))) return true;
        error = "Unable to atomically write " + PathUtf8(path) + ": " + SDL_GetError();
        return false;
    }
    bool WriteTextFile(const std::wstring &path, std::string_view text, std::string &error) { return WriteBinaryFile(path, text.data(), text.size(), error); }
    std::optional<NormalizedRelativePath> NormalizeArchivePath(std::string_view rawPath, std::string &error) {
        error.clear();
        if (rawPath.empty()) {
            error = "empty path";
            return std::nullopt;
        }
        if (!utils::IsValidUtf8(std::string(rawPath)) ||
            std::any_of(rawPath.begin(), rawPath.end(), [](unsigned char c) { return c < 32 || c == 127; })) {
            error = "invalid UTF-8 or control character in path";
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
        if (rawPath.size() >= 2 && ((rawPath[0] >= 'a' && rawPath[0] <= 'z') || (rawPath[0] >= 'A' && rawPath[0] <= 'Z')) && rawPath[1] == ':') {
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
#ifdef _WIN32
            if (segment.back() == '.' || segment.back() == ' ' || segment.find_first_of("<>\"|?*") != std::string::npos) {
                error = "ambiguous Windows path segment";
                return std::nullopt;
            }
            auto device = ToLowerAscii(segment.substr(0, segment.find('.')));
            if (device == "con" || device == "prn" || device == "aux" || device == "nul" ||
                (device.size() == 4 && (device.starts_with("com") || device.starts_with("lpt")) && device[3] >= '1' && device[3] <= '9')) {
                error = "reserved Windows device name";
                return std::nullopt;
            }
#endif
            segments.push_back(std::move(segment));
            if (slash == std::string_view::npos) {
                break;
            }
            start = slash + 1;
        }

        std::string normalized;
        for (size_t i = 0; i < segments.size(); ++i) {
            if (i > 0) {
                normalized.push_back('/');
            }
            normalized += segments[i];
        }
        return NormalizedRelativePath{normalized, utils::Utf8ToUtf16(normalized)};
    }

    bool IsDisallowedUpdaterPackagePath(std::string_view path) {
        const auto lower = ToLowerAscii(std::string(path));
        return lower == "updater" || lower == "updater.exe" || lower == "bin/updater" || lower == "bin/updater.exe" ||
            lower == "player.ini" || lower == "bin/player.ini" || lower == "player.log" || lower == "bin/player.log" ||
            lower == "modloader/updater" || lower == "modloader/mods" || lower == "modloader/configs" ||
            PathMatchesPrefix(lower, "modloader/updater/") || PathMatchesPrefix(lower, "modloader/mods/") ||
            PathMatchesPrefix(lower, "modloader/configs/");
    }
    bool ResolveUnderRoot(const std::wstring &root, const std::wstring &relative, std::wstring &out, std::string &error) {
        out.clear();
        const auto normalized = NormalizeArchivePath(PathUtf8(relative), error);
        if (!normalized) return false;
        const auto absoluteRoot = utils::ResolvePathW(root);
        auto path = absoluteRoot;
        // Validate each existing component, including dangling symlinks. No
        // lexical or physical alias may redirect a package or rollback write.
        size_t start = 0;
        while (start < normalized->normalized.size()) {
            const auto end = normalized->normalized.find('/', start);
            path = JoinPath(path, utils::Utf8ToUtf16(normalized->normalized.substr(start, end - start)));
            if (BML::Platform::IsLink(PathUtf8(path))) { error = "Symlink in managed path: " + PathUtf8(path); return false; }
            if (end == std::string::npos) break;
            if (PathExists(path) && !DirectoryExists(path)) { error = "Non-directory in managed path"; return false; }
            start = end + 1;
        }
        if (!utils::IsPathInsideRootW(path, absoluteRoot)) { error = "Path resolves outside root"; return false; }
        out = path;
        return true;
    }
    bool PathMatchesPrefix(std::string_view path, std::string_view prefix) { return path.starts_with(prefix); }
    std::wstring DefaultStateRoot(const std::wstring &root) { return JoinPath(root, L"ModLoader/Updater"); }
    std::wstring MakeTransactionRoot(const UpdaterContext &context) {
        static std::atomic<uint64_t> count{0};
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return JoinPath(JoinPath(context.updaterStateRoot, L"transactions"), std::to_wstring(ns) + L"-" +
                        std::to_wstring(BML::Platform::ProcessId()) + L"-" + std::to_wstring(count.fetch_add(1)));
    }
}
