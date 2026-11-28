#include "FileSystemNative.h"
#include "FileError.h"
#include "StringUtils.h"
#if BML_RUNTIME_SDL
#include <SDL3/SDL_properties.h>
#endif
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace BML::Platform {
std::uint64_t ProcessId() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}
std::uint32_t LastFileError() {
#ifdef _WIN32
    return GetLastError();
#else
    switch (errno) {
    case 0: return FileError::ReadFault;
    case ENOENT: return FileError::NotFound;
    case ENOTDIR: return FileError::PathNotFound;
    case EACCES: case EPERM: return FileError::AccessDenied;
    case EEXIST: return FileError::FileExists;
    case EINVAL: case ENAMETOOLONG: return FileError::InvalidName;
    default: return FileError::ReadFault;
    }
#endif
}
std::string CanonicalPath(const std::string &path) {
#ifdef _WIN32
    const auto wide = utils::Utf8ToUtf16(path);
    HANDLE file = CreateFileW(wide.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    const DWORD required = GetFinalPathNameByHandleW(file, nullptr, 0, FILE_NAME_NORMALIZED);
    std::wstring result(required, L'\0');
    DWORD written = required ? GetFinalPathNameByHandleW(file, result.data(), required, FILE_NAME_NORMALIZED) : 0;
    CloseHandle(file);
    if (!written || written >= required) return {};
    result.resize(written);
    if (result.starts_with(L"\\\\?\\UNC\\")) result = L"\\\\" + result.substr(8);
    else if (result.starts_with(L"\\\\?\\")) result.erase(0, 4);
    return utils::Utf16ToUtf8(result);
#else
    char *resolved = ::realpath(path.c_str(), nullptr);
    if (!resolved) return {};
    std::string result(resolved);
    std::free(resolved);
    return result;
#endif
}
bool IsLink(const std::string &path) {
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(utils::Utf8ToUtf16(path).c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    struct stat info{};
    return ::lstat(path.c_str(), &info) == 0 && S_ISLNK(info.st_mode);
#endif
}
bool RemoveFile(const std::string &path) {
#ifdef _WIN32
    return ::DeleteFileW(utils::Utf8ToUtf16(path).c_str()) != 0;
#else
    return ::unlink(path.c_str()) == 0;
#endif
}
#ifdef _WIN32
bool ReadFileBytes(const std::wstring &path, std::string &out, std::uint32_t &error) {
    // SDL's file wrapper does not preserve GetLastError across its cleanup.
    // Missing files and failed reads have different persistence policies.
    out.clear();
    if (path.empty() || path.find(L'\0') != std::wstring::npos) {
        error = FileError::InvalidName;
        return false;
    }
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return false;
    }
    const std::unique_ptr<void, decltype(&CloseHandle)> file(handle, CloseHandle);
    std::string bytes;
    std::array<char, 64 * 1024> buffer;
    for (;;) {
        DWORD count = 0;
        if (!::ReadFile(file.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &count, nullptr)) {
            error = GetLastError();
            return false;
        }
        if (count == 0)
            break;
        bytes.append(buffer.data(), count);
    }
    out.swap(bytes);
    error = FileError::Success;
    return true;
}

bool EncodePathForCodePage(std::wstring_view path, std::uint32_t codePage, std::string &out) {
    out.clear();
    if (path.empty() || path.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        path.find(L'\0') != std::wstring_view::npos)
        return false;

    if (codePage == CP_ACP)
        codePage = GetACP();
    const bool unicodePage = codePage == CP_UTF8 || codePage == 54936; // GB18030
    const DWORD encodeFlags = unicodePage ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    BOOL substituted = FALSE;
    BOOL *usedDefault = unicodePage ? nullptr : &substituted;
    const int length = static_cast<int>(path.size());
    const int byteCount = WideCharToMultiByte(codePage, encodeFlags, path.data(), length,
                                              nullptr, 0, nullptr, usedDefault);
    if (byteCount <= 0 || substituted)
        return false;

    std::string encoded(byteCount, '\0');
    if (WideCharToMultiByte(codePage, encodeFlags, path.data(), length,
                            encoded.data(), byteCount, nullptr, usedDefault) != byteCount || substituted)
        return false;

    // An exact round trip also rejects ambiguous legacy encodings.
    std::wstring decoded(length, L'\0');
    const DWORD decodeFlags = unicodePage ? MB_ERR_INVALID_CHARS : 0;
    if (MultiByteToWideChar(codePage, decodeFlags, encoded.data(), byteCount,
                            decoded.data(), length) != length || decoded != path)
        return false;

    out.swap(encoded);
    return true;
}

std::wstring ShortPath(const std::wstring &path) {
    if (path.empty() || path.find(L'\0') != std::wstring::npos)
        return {};
    const DWORD required = GetShortPathNameW(path.c_str(), nullptr, 0);
    if (!required)
        return {};
    std::wstring result(required, L'\0');
    const DWORD written = GetShortPathNameW(path.c_str(), result.data(), required);
    if (!written || written >= required)
        return {};
    result.resize(written);
    return result;
}
#endif
#if BML_RUNTIME_SDL
bool SyncFile(SDL_IOStream *stream) {
    if (!SDL_FlushIO(stream)) return false;
    const auto properties = SDL_GetIOProperties(stream);
#ifdef _WIN32
    auto handle = static_cast<HANDLE>(SDL_GetPointerProperty(properties, SDL_PROP_IOSTREAM_WINDOWS_HANDLE_POINTER, nullptr));
    return handle && FlushFileBuffers(handle);
#else
    int fd = static_cast<int>(SDL_GetNumberProperty(properties, SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER, -1));
    if (fd < 0) {
        auto *file = static_cast<FILE *>(SDL_GetPointerProperty(properties, SDL_PROP_IOSTREAM_STDIO_FILE_POINTER, nullptr));
        if (file) fd = fileno(file);
    }
    return fd >= 0 && ::fsync(fd) == 0;
#endif
}
#endif
#ifdef _WIN32
bool PublishFile(const std::wstring &source, const std::wstring &destination,
                 bool replace, std::uint32_t &error) {
    const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
    constexpr unsigned attempts = 12;
    constexpr DWORD retryDelayMs = 10;
    for (unsigned attempt = 0; attempt < attempts; ++attempt) {
        if (::MoveFileExW(source.c_str(), destination.c_str(), flags)) {
            error = FileError::Success;
            return true;
        }
        error = ::GetLastError();
        // A reader that omitted FILE_SHARE_DELETE can make Windows return
        // ERROR_ACCESS_DENIED instead of ERROR_SHARING_VIOLATION.
        if (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION &&
            error != ERROR_LOCK_VIOLATION)
            return false;
        if (attempt + 1 < attempts)
            ::Sleep(retryDelayMs);
    }
    return false;
}
#endif

bool PublishFile(const std::string &source, const std::string &destination,
                 bool replace, std::uint32_t &error) {
#ifdef _WIN32
    return PublishFile(utils::Utf8ToUtf16(source), utils::Utf8ToUtf16(destination),
                       replace, error);
#else
    const bool published = replace ? std::rename(source.c_str(), destination.c_str()) == 0
                                   : ::link(source.c_str(), destination.c_str()) == 0;
    error = published ? FileError::Success : LastFileError();
    return published;
#endif
}
bool SetWorkingDirectory(const std::string &path) {
#ifdef _WIN32
    return SetCurrentDirectoryW(utils::Utf8ToUtf16(path).c_str()) != 0;
#else
    return ::chdir(path.c_str()) == 0;
#endif
}
std::string ExecutablePath() {
#ifdef _WIN32
    std::wstring path(512, L'\0');
    for (;;) {
        const auto n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (!n) return {};
        if (n < path.size()) { path.resize(n); return utils::Utf16ToUtf8(path); }
        if (path.size() >= 32768) return {};
        path.resize(path.size() * 2);
    }
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> path(size);
    return _NSGetExecutablePath(path.data(), &size) ? std::string{} : CanonicalPath(path.data());
#else
    std::vector<char> path(1024);
    for (;;) {
        const auto n = ::readlink("/proc/self/exe", path.data(), path.size());
        if (n < 0) return {};
        if (static_cast<size_t>(n) < path.size()) return std::string(path.data(), n);
        path.resize(path.size() * 2);
    }
#endif
}
}
