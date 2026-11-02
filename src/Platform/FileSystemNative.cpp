#include "FileSystemNative.h"
#include "FileError.h"
#include "StringUtils.h"
#include <SDL3/SDL_properties.h>
#include <cstdio>
#include <cstdlib>
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
bool PublishNewFile(const std::string &source, const std::string &destination) {
#ifdef _WIN32
    return MoveFileExW(utils::Utf8ToUtf16(source).c_str(), utils::Utf8ToUtf16(destination).c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#else
    return ::link(source.c_str(), destination.c_str()) == 0;
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
