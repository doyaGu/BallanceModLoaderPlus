#include "PathUtils.h"
#include "StringUtils.h"
#include "Platform/FileError.h"
#include "Platform/FileSystemNative.h"
#include <SDL3/SDL.h>
#include <zip.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <unordered_set>

namespace utils {
namespace {
namespace Native = BML::Platform;
namespace E = BML::Platform::FileError;
std::string Slashes(std::string path) { std::replace(path.begin(), path.end(), '\\', '/'); return path; }
std::string Path(const std::wstring &path) { return Slashes(Utf16ToUtf8(path)); }
size_t RootLength(const std::string &path) {
#ifdef _WIN32
    if (path.size() >= 3 && path[1] == ':' && path[2] == '/') return 3;
    if (path.starts_with("//")) {
        auto server = path.find('/', 2);
        if (server == std::string::npos) return path.size();
        auto share = path.find('/', server + 1);
        return share == std::string::npos ? path.size() : share + 1;
    }
#endif
    return path.starts_with('/') ? 1 : 0;
}
std::string Lexical(const std::string &input) {
    auto path = Slashes(input);
    size_t rootLength = RootLength(path);
    std::vector<std::string> parts;
    for (size_t begin = rootLength; begin < path.size();) {
        size_t end = path.find('/', begin);
        if (end == std::string::npos) end = path.size();
        auto part = path.substr(begin, end - begin);
        if (part == ".." && !parts.empty() && parts.back() != "..") parts.pop_back();
        else if (part != "." && !part.empty() && (part != ".." || !rootLength)) parts.push_back(part);
        begin = end + 1;
    }
    std::string out = path.substr(0, rootLength);
    for (const auto &part : parts) { if (!out.empty() && out.back() != '/') out += '/'; out += part; }
    return out;
}
std::string Join(const std::string &a, const std::string &b) {
    if (a.empty()) return Slashes(b);
    if (b.empty()) return Slashes(a);
    auto result = Slashes(a), tail = Slashes(b);
    if (result.back() != '/') result += '/';
    size_t begin = tail.find_first_not_of('/');
    if (begin != std::string::npos) result += tail.substr(begin);
    return result;
}
bool EqualPath(const std::string &a, const std::string &b) {
#ifdef _WIN32
    return SDL_strcasecmp(a.c_str(), b.c_str()) == 0;
#else
    return a == b;
#endif
}
std::string Unique(const std::string &prefix) {
    static std::atomic<std::uint64_t> counter{0};
    return prefix + std::to_string(SDL_GetTicksNS()) + "-" + std::to_string(counter.fetch_add(1));
}
bool Enumerate(const std::string &path, const std::function<bool(const std::string &)> &visit) {
    struct State { const std::function<bool(const std::string &)> &visit; } state{visit};
    return SDL_EnumerateDirectory(path.c_str(), [](void *data, const char *directory, const char *name) {
        try {
            return static_cast<State *>(data)->visit(Join(directory, name)) ? SDL_ENUM_CONTINUE : SDL_ENUM_FAILURE;
        } catch (...) { SDL_SetError("Directory visitor failed"); return SDL_ENUM_FAILURE; }
    }, &state);
}
bool RemoveTree(const std::string &path) {
    if (Native::IsLink(path)) return SDL_RemovePath(path.c_str());
    SDL_PathInfo info{};
    if (!SDL_GetPathInfo(path.c_str(), &info)) return Native::LastFileError() == E::NotFound;
    if (info.type == SDL_PATHTYPE_DIRECTORY && !Enumerate(path, RemoveTree)) return false;
    return SDL_RemovePath(path.c_str());
}
std::vector<std::wstring> List(const std::wstring &directory, const std::wstring &pattern, bool directories) {
    std::vector<std::wstring> result;
    int count = 0;
    auto path = Path(directory), filter = Path(pattern);
#ifdef _WIN32
    constexpr SDL_GlobFlags flags = SDL_GLOB_CASEINSENSITIVE;
#else
    constexpr SDL_GlobFlags flags = 0;
#endif
    char **entries = SDL_GlobDirectory(path.c_str(), filter.c_str(), flags, &count);
    if (!entries) return result;
    std::unique_ptr<void, decltype(&SDL_free)> owned(entries, SDL_free);
    for (int i = 0; i < count; ++i) {
        // The public list APIs describe the immediate directory only.
        if (SDL_strchr(entries[i], '/')) continue;
        SDL_PathInfo info{};
        if (SDL_GetPathInfo(Join(path, entries[i]).c_str(), &info) &&
            (info.type == SDL_PATHTYPE_DIRECTORY) == directories) result.push_back(Utf8ToUtf16(entries[i]));
    }
    std::sort(result.begin(), result.end());
    return result;
}
}

bool FileExistsW(const std::wstring &p) { SDL_PathInfo i{}; return SDL_GetPathInfo(Path(p).c_str(), &i) && i.type == SDL_PATHTYPE_FILE; }
bool DirectoryExistsW(const std::wstring &p) { SDL_PathInfo i{}; return SDL_GetPathInfo(Path(p).c_str(), &i) && i.type == SDL_PATHTYPE_DIRECTORY; }
bool PathExistsW(const std::wstring &p) { return SDL_GetPathInfo(Path(p).c_str(), nullptr); }
bool CreateDirectoryW(const std::wstring &p) { return SDL_CreateDirectory(Path(p).c_str()); }
bool CreateFileTreeW(const std::wstring &p) { return CreateDirectoryW(p); }
bool DeleteFileW(const std::wstring &p) { return !DirectoryExistsW(p) && SDL_RemovePath(Path(p).c_str()); }
bool DeleteDirectoryW(const std::wstring &p) { return !p.empty() && RemoveTree(Path(p)); }
bool CopyFileW(const std::wstring &a, const std::wstring &b) { return SDL_CopyFile(Path(a).c_str(), Path(b).c_str()); }
bool MoveFileW(const std::wstring &a, const std::wstring &b) { return SDL_RenamePath(Path(a).c_str(), Path(b).c_str()); }
std::wstring GetDriveW(const std::wstring &p) {
#ifdef _WIN32
    auto s = Path(p); if (s.size() >= 2 && s[1] == ':') return Utf8ToUtf16(s.substr(0, 2));
#endif
    return {};
}
std::wstring GetDirectoryW(const std::wstring &p) { auto s = Path(p); auto at = s.find_last_of('/'); return at == std::string::npos ? L"" : Utf8ToUtf16(s.substr(0, at == 0 ? 1 : at)); }
std::wstring TrimTrailingSeparatorsW(const std::wstring &p) { auto s = Path(p); while (s.size() > RootLength(s) && s.back() == '/') s.pop_back(); return Utf8ToUtf16(s); }
std::wstring GetParentDirectoryW(const std::wstring &p) { auto s = Path(TrimTrailingSeparatorsW(p)); return s.size() == RootLength(s) ? L"" : GetDirectoryW(Utf8ToUtf16(s)); }
std::wstring GetFileNameW(const std::wstring &p) { auto s = Path(p); auto at = s.find_last_of('/'); return Utf8ToUtf16(at == std::string::npos ? s : s.substr(at + 1)); }
std::wstring GetExtensionW(const std::wstring &p) { auto name = GetFileNameW(p); auto at = name.find_last_of(L'.'); return at == std::wstring::npos ? L"" : name.substr(at); }
std::wstring RemoveExtensionW(const std::wstring &p) { return p.substr(0, p.size() - GetExtensionW(p).size()); }
std::wstring CombinePathW(const std::wstring &a, const std::wstring &b) { return Utf8ToUtf16(Join(Path(a), Path(b))); }
std::wstring NormalizePathW(const std::wstring &p) { return Utf8ToUtf16(Lexical(Path(p))); }
std::pair<std::wstring, std::wstring> GetDriveAndDirectoryW(const std::wstring &p) { auto drive = GetDriveW(p), dir = GetDirectoryW(p); if (!drive.empty() && dir.starts_with(drive)) dir.erase(0, drive.size()); return {drive, dir}; }
bool IsPathValidW(const std::wstring &p) { return !p.empty() && p.find(L'\0') == std::wstring::npos && Utf8ToUtf16(Utf16ToUtf8(p)) == p; }
bool IsAbsolutePathW(const std::wstring &p) { return RootLength(Path(p)) != 0; }
bool IsRelativePathW(const std::wstring &p) { return !IsAbsolutePathW(p); }
bool IsPathRootedW(const std::wstring &p) { return IsAbsolutePathW(p) || !GetDriveW(p).empty(); }
std::wstring ResolvePathW(const std::wstring &p) { return NormalizePathW(IsAbsolutePathW(p) ? p : CombinePathW(GetCurrentDirectoryW(), p)); }
std::wstring MakeRelativePathW(const std::wstring &p, const std::wstring &base) {
    const auto target = Path(ResolvePathW(p)), origin = Path(ResolvePathW(base));
    if (!EqualPath(target.substr(0, RootLength(target)), origin.substr(0, RootLength(origin)))) return p;
    auto split = [](const std::string &path) {
        std::vector<std::string> parts;
        for (size_t begin = RootLength(path); begin < path.size();) {
            auto end = path.find('/', begin);
            if (end == std::string::npos) end = path.size();
            if (end != begin) parts.push_back(path.substr(begin, end - begin));
            begin = end + 1;
        }
        return parts;
    };
    const auto a = split(target), b = split(origin);
    size_t common = 0;
    while (common < a.size() && common < b.size() && EqualPath(a[common], b[common])) ++common;
    std::string result;
    for (size_t i = common; i < b.size(); ++i) result = Join(result, "..");
    for (size_t i = common; i < a.size(); ++i) result = Join(result, a[i]);
    return result.empty() ? L"." : Utf8ToUtf16(result);
}
bool IsPathInsideRootW(const std::wstring &p, const std::wstring &root) {
    if (p.empty() || root.empty()) return false;
    const auto a = Path(TrimTrailingSeparatorsW(ResolvePathW(p))), b = Path(TrimTrailingSeparatorsW(ResolvePathW(root)));
    return EqualPath(a, b) || (a.size() > b.size() && EqualPath(a.substr(0, b.size()), b) && (b.back() == '/' || a[b.size()] == '/'));
}
bool TryGetFinalPathW(const std::wstring &p, std::wstring &out) { out = Utf8ToUtf16(Slashes(Native::CanonicalPath(Path(p)))); return !out.empty(); }
bool TryGetFinalPathInsideRootW(const std::wstring &p, const std::wstring &root, std::wstring &out) {
    std::wstring finalRoot;
    if (!TryGetFinalPathW(root, finalRoot) || !TryGetFinalPathW(p, out) || !IsPathInsideRootW(out, finalRoot)) { out.clear(); return false; }
    return true;
}
bool TryEncodePathForCodePage(std::wstring_view p, std::uint32_t page, std::string &out) {
    out.clear(); if (page != 0 && page != 65001) return false;
    if (!IsPathValidW(std::wstring(p))) return false;
    out = Utf16ToUtf8(std::wstring(p)); return true;
}
bool TryEncodePathForActiveCodePage(std::wstring_view p, std::string &out) { return TryEncodePathForCodePage(p, 65001, out); }
std::wstring GetShortPathW(const std::wstring &) { return {}; }
std::wstring GetTempPathW() {
    for (const char *name : {"TMPDIR", "TEMP", "TMP"}) if (const char *value = SDL_getenv(name); value && *value) return Utf8ToUtf16(Slashes(value));
#ifdef _WIN32
    char *path = SDL_GetPrefPath("Ballance", "BML");
    std::wstring out = path ? Utf8ToUtf16(Slashes(path)) : L""; SDL_free(path); return out;
#else
    return L"/tmp";
#endif
}
std::wstring GetCurrentDirectoryW() { char *path = SDL_GetCurrentDirectory(); auto result = path ? Utf8ToUtf16(Slashes(path)) : L""; SDL_free(path); return TrimTrailingSeparatorsW(result); }
bool SetCurrentDirectoryW(const std::wstring &p) { return Native::SetWorkingDirectory(Path(p)); }
std::wstring GetExecutablePathW() { return Utf8ToUtf16(Slashes(Native::ExecutablePath())); }
int64_t GetFileSizeW(const std::wstring &p) { SDL_PathInfo info{}; return SDL_GetPathInfo(Path(p).c_str(), &info) && info.size <= INT64_MAX ? static_cast<int64_t>(info.size) : -1; }
FileTime GetFileTimeW(const std::wstring &p) {
    SDL_PathInfo info{}; if (!SDL_GetPathInfo(Path(p).c_str(), &info)) return {};
    auto ticks = [](SDL_Time time) -> int64_t { return time == 0 ? 0 : time / 100 + 116444736000000000LL; };
    return {ticks(info.create_time), ticks(info.access_time), ticks(info.modify_time)};
}
bool ReadFileBytesW(const std::wstring &p, std::string &out, std::uint32_t &error) {
#ifdef _WIN32
    return Native::ReadFileBytes(p, out, error);
#else
    out.clear(); size_t size = 0;
    void *bytes = SDL_LoadFile(Path(p).c_str(), &size);
    if (!bytes) { error = Native::LastFileError(); return false; }
    std::unique_ptr<void, decltype(&SDL_free)> owned(bytes, SDL_free);
    out.assign(static_cast<const char *>(bytes), size); error = 0; return true;
#endif
}
bool ReadFileBytesW(const std::wstring &p, std::string &out) { std::uint32_t e; return ReadFileBytesW(p, out, e); }
bool ReadFileBytesUtf8(const std::string &p, std::string &out) { return ReadFileBytesW(Utf8ToUtf16(p), out); }
bool WriteFileBytesAtomicW(const std::wstring &p, std::string_view bytes, std::uint32_t &error, bool replace) {
    error = E::InvalidName; if (!IsPathValidW(p)) return false;
    const auto destination = Path(p);
    std::string temporary;
    SDL_IOStream *stream = nullptr;
    for (int attempt = 0; attempt < 32 && !stream; ++attempt) {
        temporary = Unique(destination + ".tmp-");
        stream = SDL_IOFromFile(temporary.c_str(), "wbx");
        if (!stream) {
            const auto openError = Native::LastFileError();
            if (openError != E::FileExists) {
                error = openError == E::Success ? E::WriteFault : openError;
                return false;
            }
        }
    }
    if (!stream) { error = E::FileExists; return false; }
    bool ok = SDL_WriteIO(stream, bytes.data(), bytes.size()) == bytes.size() && Native::SyncFile(stream);
    if (!SDL_CloseIO(stream)) ok = false;
    if (!ok) error = E::WriteFault;
    if (ok) ok = Native::PublishFile(temporary, destination, replace, error);
    SDL_RemovePath(temporary.c_str());
    return ok;
}
std::vector<uint8_t> ReadBinaryFileW(const std::wstring &p) { std::string bytes; if (!ReadFileBytesW(p, bytes)) return {}; return {bytes.begin(), bytes.end()}; }
std::wstring ReadTextFileW(const std::wstring &p) { std::string bytes; if (!ReadFileBytesW(p, bytes)) return {}; return Utf8ToUtf16(bytes); }
bool WriteBinaryFileW(const std::wstring &p, const std::vector<uint8_t> &data) { std::uint32_t e; return WriteFileBytesAtomicW(p, {reinterpret_cast<const char *>(data.data()), data.size()}, e); }
bool WriteTextFileW(const std::wstring &p, const std::wstring &text) { std::uint32_t e; return WriteFileBytesAtomicW(p, Utf16ToUtf8(text), e); }
std::wstring CreateTempFileW(const std::wstring &prefix) {
    if (prefix.find_first_of(L"/\\") != std::wstring::npos) return {};
    for (int i = 0; i < 32; ++i) {
        auto path = Unique(Join(Path(GetTempPathW()), Utf16ToUtf8(prefix)));
        if (auto *stream = SDL_IOFromFile(path.c_str(), "wbx")) { if (SDL_CloseIO(stream)) return Utf8ToUtf16(path); SDL_RemovePath(path.c_str()); return {}; }
        if (Native::LastFileError() != E::FileExists) return {};
    }
    return {};
}
std::vector<std::wstring> ListFilesW(const std::wstring &p, const std::wstring &pattern) { return List(p, pattern, false); }
std::vector<std::wstring> ListDirectoriesW(const std::wstring &p, const std::wstring &pattern) { return List(p, pattern, true); }
bool ListFilePathsRecursiveW(const std::wstring &p, std::vector<std::wstring> &files) {
    files.clear();
    std::function<bool(const std::string &)> visit = [&](const std::string &path) {
        SDL_PathInfo info{}; if (!SDL_GetPathInfo(path.c_str(), &info)) return false;
        // Keep logical file aliases so source consumers can check their final
        // path against their own package root. Do not recurse through linked
        // directories: they can escape the tree or form cycles.
        if (Native::IsLink(path) && info.type != SDL_PATHTYPE_FILE) return true;
        if (info.type == SDL_PATHTYPE_DIRECTORY) return Enumerate(path, visit);
        if (info.type == SDL_PATHTYPE_FILE) files.push_back(Utf8ToUtf16(path));
        return true;
    };
    if (!visit(Path(p))) { files.clear(); return false; }
    std::sort(files.begin(), files.end()); return true;
}

bool ExtractZipW(const std::wstring &archive, const std::wstring &destination) {
    std::string bytes;
    if (!ReadFileBytesW(archive, bytes) || bytes.empty() || destination.empty()) return false;
    std::unique_ptr<zip_t, decltype(&zip_stream_close)> zip(zip_stream_open(bytes.data(), bytes.size(), 0, 'r'), zip_stream_close);
    if (!zip) return false;
    const auto dest = ResolvePathW(destination);
    // An exclusively created marker owns the staging name in a writable parent.
    if (!CreateFileTreeW(GetDirectoryW(dest))) return false;
    auto marker = Utf8ToUtf16(Unique(Path(dest) + ".extract-"));
    std::uint32_t writeError = 0;
    if (!WriteFileBytesAtomicW(marker, {}, writeError, false)) return false;
    const auto staging = marker + L".dir";
    struct Cleanup { std::wstring marker, staging; ~Cleanup() { DeleteDirectoryW(staging); DeleteFileW(marker); } } cleanup{marker, staging};
    if (PathExistsW(staging) || !CreateFileTreeW(staging)) return false;
    const auto count = zip_entries_total(zip.get());
    if (count < 0) return false;
    std::unordered_set<std::string> names;
    for (ssize_t i = 0; i < count; ++i) {
        if (zip_entry_openbyindex(zip.get(), static_cast<size_t>(i)) < 0) return false;
        struct Entry { zip_t *zip; ~Entry() { zip_entry_close(zip); } } close{zip.get()};
        const char *raw = zip_entry_name(zip.get());
        if (!raw || !IsValidUtf8(raw)) return false;
        const auto name = Slashes(raw);
        if (name.empty() || name.front() == '/' || name.find(':') != std::string::npos) return false;
        for (size_t start = 0; start < name.size();) {
            auto end = name.find('/', start); if (end == std::string::npos) end = name.size();
            if (name.substr(start, end - start) == "..") return false;
            start = end + 1;
        }
        auto normalized = Lexical(name);
        if (normalized.empty() || !names.insert(normalized).second) return false;
        const auto output = CombinePathW(staging, Utf8ToUtf16(normalized));
        if (!IsPathInsideRootW(output, staging)) return false;
        if (zip_entry_isdir(zip.get())) { if (!CreateFileTreeW(output)) return false; continue; }
        if (!CreateFileTreeW(GetDirectoryW(output))) return false;
        auto *stream = SDL_IOFromFile(Path(output).c_str(), "wbx");
        if (!stream) return false;
        const auto extracted = zip_entry_extract(zip.get(), [](void *context, std::uint64_t, const void *data, size_t size) -> size_t {
            return SDL_WriteIO(static_cast<SDL_IOStream *>(context), data, size);
        }, stream);
        const bool closed = SDL_CloseIO(stream);
        if (extracted < 0 || !closed) return false;
    }
    if (Native::IsLink(Path(dest)) || !CreateFileTreeW(dest)) return false;
    std::function<bool(const std::wstring &)> publish = [&](const std::wstring &source) {
        const auto target = CombinePathW(dest, MakeRelativePathW(source, staging));
        for (auto parent = target; !parent.empty() && IsPathInsideRootW(parent, dest); parent = GetParentDirectoryW(parent))
            if (Native::IsLink(Path(parent))) return false;
        if (DirectoryExistsW(source)) {
            if (!CreateFileTreeW(target)) return false;
            return Enumerate(Path(source), [&](const std::string &child) { return publish(Utf8ToUtf16(child)); });
        }
        if (!CreateFileTreeW(GetDirectoryW(target))) return false;
        std::wstring resolved;
        return TryGetFinalPathInsideRootW(GetDirectoryW(target), dest, resolved) && CopyFileW(source, target);
    };
    if (!Enumerate(Path(staging), [&](const std::string &child) { return publish(Utf8ToUtf16(child)); })) return false;
    return true;
}
bool FileExistsA(const std::string &p) { return FileExistsW(Utf8ToUtf16(p)); }
bool FileExistsUtf8(const std::string &p) { return FileExistsW(Utf8ToUtf16(p)); }
bool DirectoryExistsA(const std::string &p) { return DirectoryExistsW(Utf8ToUtf16(p)); }
bool DirectoryExistsUtf8(const std::string &p) { return DirectoryExistsW(Utf8ToUtf16(p)); }
bool PathExistsA(const std::string &p) { return PathExistsW(Utf8ToUtf16(p)); }
bool PathExistsUtf8(const std::string &p) { return PathExistsW(Utf8ToUtf16(p)); }
bool CreateDirectoryA(const std::string &p) { return CreateDirectoryW(Utf8ToUtf16(p)); }
bool CreateDirectoryUtf8(const std::string &p) { return CreateDirectoryW(Utf8ToUtf16(p)); }
bool CreateFileTreeA(const std::string &p) { return CreateFileTreeW(Utf8ToUtf16(p)); }
bool CreateFileTreeUtf8(const std::string &p) { return CreateFileTreeW(Utf8ToUtf16(p)); }
bool DeleteFileA(const std::string &p) { return DeleteFileW(Utf8ToUtf16(p)); }
bool DeleteFileUtf8(const std::string &p) { return DeleteFileW(Utf8ToUtf16(p)); }
bool DeleteDirectoryA(const std::string &p) { return DeleteDirectoryW(Utf8ToUtf16(p)); }
bool DeleteDirectoryUtf8(const std::string &p) { return DeleteDirectoryW(Utf8ToUtf16(p)); }
bool IsPathValidA(const std::string &p) { return IsPathValidW(Utf8ToUtf16(p)); }
bool IsPathValidUtf8(const std::string &p) { return IsPathValidW(Utf8ToUtf16(p)); }
bool IsAbsolutePathA(const std::string &p) { return IsAbsolutePathW(Utf8ToUtf16(p)); }
bool IsAbsolutePathUtf8(const std::string &p) { return IsAbsolutePathW(Utf8ToUtf16(p)); }
bool IsRelativePathA(const std::string &p) { return IsRelativePathW(Utf8ToUtf16(p)); }
bool IsRelativePathUtf8(const std::string &p) { return IsRelativePathW(Utf8ToUtf16(p)); }
bool IsPathRootedA(const std::string &p) { return IsPathRootedW(Utf8ToUtf16(p)); }
bool IsPathRootedUtf8(const std::string &p) { return IsPathRootedW(Utf8ToUtf16(p)); }
bool SetCurrentDirectoryA(const std::string &p) { return SetCurrentDirectoryW(Utf8ToUtf16(p)); }
bool SetCurrentDirectoryUtf8(const std::string &p) { return SetCurrentDirectoryW(Utf8ToUtf16(p)); }
bool CopyFileA(const std::string &a, const std::string &b) { return CopyFileW(Utf8ToUtf16(a), Utf8ToUtf16(b)); }
bool CopyFileUtf8(const std::string &a, const std::string &b) { return CopyFileW(Utf8ToUtf16(a), Utf8ToUtf16(b)); }
bool MoveFileA(const std::string &a, const std::string &b) { return MoveFileW(Utf8ToUtf16(a), Utf8ToUtf16(b)); }
bool MoveFileUtf8(const std::string &a, const std::string &b) { return MoveFileW(Utf8ToUtf16(a), Utf8ToUtf16(b)); }
bool ExtractZipA(const std::string &a, const std::string &b) { return ExtractZipW(Utf8ToUtf16(a), Utf8ToUtf16(b)); }
bool ExtractZipUtf8(const std::string &a, const std::string &b) { return ExtractZipW(Utf8ToUtf16(a), Utf8ToUtf16(b)); }
std::string GetDriveA(const std::string &p) { return Utf16ToUtf8(GetDriveW(Utf8ToUtf16(p))); }
std::string GetDriveUtf8(const std::string &p) { return Utf16ToUtf8(GetDriveW(Utf8ToUtf16(p))); }
std::string GetDirectoryA(const std::string &p) { return Utf16ToUtf8(GetDirectoryW(Utf8ToUtf16(p))); }
std::string GetDirectoryUtf8(const std::string &p) { return Utf16ToUtf8(GetDirectoryW(Utf8ToUtf16(p))); }
std::string GetFileNameA(const std::string &p) { return Utf16ToUtf8(GetFileNameW(Utf8ToUtf16(p))); }
std::string GetFileNameUtf8(const std::string &p) { return Utf16ToUtf8(GetFileNameW(Utf8ToUtf16(p))); }
std::string GetExtensionA(const std::string &p) { return Utf16ToUtf8(GetExtensionW(Utf8ToUtf16(p))); }
std::string GetExtensionUtf8(const std::string &p) { return Utf16ToUtf8(GetExtensionW(Utf8ToUtf16(p))); }
std::string RemoveExtensionA(const std::string &p) { return Utf16ToUtf8(RemoveExtensionW(Utf8ToUtf16(p))); }
std::string RemoveExtensionUtf8(const std::string &p) { return Utf16ToUtf8(RemoveExtensionW(Utf8ToUtf16(p))); }
std::string NormalizePathA(const std::string &p) { return Utf16ToUtf8(NormalizePathW(Utf8ToUtf16(p))); }
std::string NormalizePathUtf8(const std::string &p) { return Utf16ToUtf8(NormalizePathW(Utf8ToUtf16(p))); }
std::string ResolvePathA(const std::string &p) { return Utf16ToUtf8(ResolvePathW(Utf8ToUtf16(p))); }
std::string ResolvePathUtf8(const std::string &p) { return Utf16ToUtf8(ResolvePathW(Utf8ToUtf16(p))); }
std::string CreateTempFileA(const std::string &p) { return Utf16ToUtf8(CreateTempFileW(Utf8ToUtf16(p))); }
std::string CreateTempFileUtf8(const std::string &p) { return Utf16ToUtf8(CreateTempFileW(Utf8ToUtf16(p))); }
std::string ReadTextFileA(const std::string &p) { return Utf16ToUtf8(ReadTextFileW(Utf8ToUtf16(p))); }
std::string ReadTextFileUtf8(const std::string &p) { return Utf16ToUtf8(ReadTextFileW(Utf8ToUtf16(p))); }
std::string CombinePathA(const std::string &a, const std::string &b) { return Utf16ToUtf8(CombinePathW(Utf8ToUtf16(a), Utf8ToUtf16(b))); }
std::string CombinePathUtf8(const std::string &a, const std::string &b) { return Utf16ToUtf8(CombinePathW(Utf8ToUtf16(a), Utf8ToUtf16(b))); }
std::string MakeRelativePathA(const std::string &a, const std::string &b) { return Utf16ToUtf8(MakeRelativePathW(Utf8ToUtf16(a), Utf8ToUtf16(b))); }
std::string MakeRelativePathUtf8(const std::string &a, const std::string &b) { return Utf16ToUtf8(MakeRelativePathW(Utf8ToUtf16(a), Utf8ToUtf16(b))); }
std::string GetTempPathA() { return Utf16ToUtf8(GetTempPathW()); }
std::string GetTempPathUtf8() { return Utf16ToUtf8(GetTempPathW()); }
std::string GetCurrentDirectoryA() { return Utf16ToUtf8(GetCurrentDirectoryW()); }
std::string GetCurrentDirectoryUtf8() { return Utf16ToUtf8(GetCurrentDirectoryW()); }
std::string GetExecutablePathA() { return Utf16ToUtf8(GetExecutablePathW()); }
std::string GetExecutablePathUtf8() { return Utf16ToUtf8(GetExecutablePathW()); }
int64_t GetFileSizeA(const std::string &p) { return GetFileSizeW(Utf8ToUtf16(p)); }
int64_t GetFileSizeUtf8(const std::string &p) { return GetFileSizeW(Utf8ToUtf16(p)); }
FileTime GetFileTimeA(const std::string &p) { return GetFileTimeW(Utf8ToUtf16(p)); }
FileTime GetFileTimeUtf8(const std::string &p) { return GetFileTimeW(Utf8ToUtf16(p)); }
std::vector<uint8_t> ReadBinaryFileA(const std::string &p) { return ReadBinaryFileW(Utf8ToUtf16(p)); }
std::vector<uint8_t> ReadBinaryFileUtf8(const std::string &p) { return ReadBinaryFileW(Utf8ToUtf16(p)); }
std::pair<std::string, std::string> GetDriveAndDirectoryA(const std::string &p) { return {GetDriveA(p), GetDirectoryA(p)}; }
bool WriteBinaryFileA(const std::string &p, const std::vector<uint8_t> &data) { return WriteBinaryFileW(Utf8ToUtf16(p), data); }
bool WriteTextFileA(const std::string &p, const std::string &data) { std::uint32_t e; return WriteFileBytesAtomicW(Utf8ToUtf16(p), data, e); }
std::vector<std::string> ListFilesA(const std::string &p, const std::string &pattern) {
    std::vector<std::string> result;
    for (const auto &name : ListFilesW(Utf8ToUtf16(p), Utf8ToUtf16(pattern))) result.push_back(Utf16ToUtf8(name));
    return result;
}
std::vector<std::string> ListDirectoriesA(const std::string &p, const std::string &pattern) {
    std::vector<std::string> result;
    for (const auto &name : ListDirectoriesW(Utf8ToUtf16(p), Utf8ToUtf16(pattern))) result.push_back(Utf16ToUtf8(name));
    return result;
}
std::pair<std::string, std::string> GetDriveAndDirectoryUtf8(const std::string &p) { return {GetDriveUtf8(p), GetDirectoryUtf8(p)}; }
bool WriteBinaryFileUtf8(const std::string &p, const std::vector<uint8_t> &data) { return WriteBinaryFileW(Utf8ToUtf16(p), data); }
bool WriteTextFileUtf8(const std::string &p, const std::string &data) { std::uint32_t e; return WriteFileBytesAtomicW(Utf8ToUtf16(p), data, e); }
std::vector<std::string> ListFilesUtf8(const std::string &p, const std::string &pattern) {
    std::vector<std::string> result;
    for (const auto &name : ListFilesW(Utf8ToUtf16(p), Utf8ToUtf16(pattern))) result.push_back(Utf16ToUtf8(name));
    return result;
}
std::vector<std::string> ListDirectoriesUtf8(const std::string &p, const std::string &pattern) {
    std::vector<std::string> result;
    for (const auto &name : ListDirectoriesW(Utf8ToUtf16(p), Utf8ToUtf16(pattern))) result.push_back(Utf16ToUtf8(name));
    return result;
}
}
