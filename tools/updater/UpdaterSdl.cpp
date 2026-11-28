#include "UpdaterSdl.h"
#include "UpdaterPaths.h"
#include "UpdaterService.h"
#include "CryptoUtils.h"
#include "JsonUtils.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "Platform/FileSystemNative.h"

#if BML_RUNTIME_SDL
#include <SDL3/SDL.h>
#else
#include <chrono>
#endif
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#if !BML_RUNTIME_SDL
#include <TlHelp32.h>
#endif
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace bmlupdater {
namespace {
#ifndef _WIN32
int FileDescriptor(SDL_IOStream *stream) {
    const auto props = SDL_GetIOProperties(stream);
    int fd = static_cast<int>(SDL_GetNumberProperty(props, SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER, -1));
    if (fd < 0) {
        auto *file = static_cast<FILE *>(SDL_GetPointerProperty(props, SDL_PROP_IOSTREAM_STDIO_FILE_POINTER, nullptr));
        if (file) fd = fileno(file);
    }
    return fd;
}
#endif
}

uint32_t FileMode(const std::wstring &path) {
#ifdef _WIN32
    auto attributes = GetFileAttributesW(path.c_str());
    return attributes == INVALID_FILE_ATTRIBUTES ? 0 : (attributes & FILE_ATTRIBUTE_READONLY) ? 0444 : 0644;
#else
    struct stat info{};
    return stat(PathUtf8(path).c_str(), &info) == 0 ? info.st_mode & 0777 : 0;
#endif
}
bool SetFileMode(const std::wstring &path, uint32_t mode, std::string &error) {
#ifdef _WIN32
    auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !SetFileAttributesW(path.c_str(),
        (mode & 0200) ? attributes & ~FILE_ATTRIBUTE_READONLY : attributes | FILE_ATTRIBUTE_READONLY)) {
        error = "Unable to set file permissions: " + PathUtf8(path);
        return false;
    }
#else
    if (chmod(PathUtf8(path).c_str(), mode & 0777) != 0) {
        error = "Unable to set file permissions: " + PathUtf8(path);
        return false;
    }
#endif
    return true;
}
bool IsExecutableFile(const std::wstring &path) {
    if (!RegularFileExists(path)) return false;
#ifdef _WIN32
    return true;
#else
    return access(PathUtf8(path).c_str(), X_OK) == 0;
#endif
}
bool SyncDirectory(const std::wstring &path) {
#ifdef _WIN32
    return true;
#else
    int fd = open(PathUtf8(path).c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) return false;
    bool result = fsync(fd) == 0;
    close(fd);
    return result;
#endif
}

UpdaterLock::UpdaterLock(const std::wstring &stateRoot) {
    if (!CreateDirectories(stateRoot)) return;
    auto path = JoinPath(stateRoot, L".lock");
    if (BML::Platform::IsLink(PathUtf8(path))) return;
#if BML_RUNTIME_SDL
    auto *stream = SDL_IOFromFile(PathUtf8(path).c_str(), "ab");
    if (!stream) return;
#ifdef _WIN32
    auto props = SDL_GetIOProperties(stream);
    auto handle = static_cast<HANDLE>(SDL_GetPointerProperty(props, SDL_PROP_IOSTREAM_WINDOWS_HANDLE_POINTER, nullptr));
    if (!handle) {
        auto *file = static_cast<FILE *>(SDL_GetPointerProperty(props, SDL_PROP_IOSTREAM_STDIO_FILE_POINTER, nullptr));
        if (file) handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
    }
    OVERLAPPED overlap{};
    bool locked = handle && LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &overlap);
#else
    const int fd = FileDescriptor(stream);
    bool locked = fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) == 0;
#endif
    if (locked) m_Stream = stream;
    else SDL_CloseIO(stream);
#else
    // Deny other write opens for the life of the transaction.
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                OPEN_ALWAYS, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return;
    BY_HANDLE_FILE_INFORMATION info{};
    if (GetFileInformationByHandle(handle, &info) &&
        !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)))
        m_Stream = handle;
    else
        CloseHandle(handle);
#endif
}
UpdaterLock::~UpdaterLock() {
    if (m_Stream) {
#if BML_RUNTIME_SDL
        SDL_CloseIO(m_Stream);
#else
        CloseHandle(m_Stream);
#endif
    }
}

bool CopyFileAtomic(const std::wstring &from, const std::wstring &to, uint32_t mode, std::string &error,
                    std::string_view expectedHash) {
    if (!RegularFileExists(from) || BML::Platform::IsLink(PathUtf8(to)) || !CreateDirectories(ParentPath(to))) {
        error = "Unable to copy regular file: " + PathUtf8(from) + " to " + PathUtf8(to);
        return false;
    }
#if BML_RUNTIME_SDL
    static std::atomic<uint64_t> sequence{0};
    auto temporary = to + L".copy-" + std::to_wstring(BML::Platform::ProcessId()) + L"-" +
                     std::to_wstring(SDL_GetTicksNS()) + L"-" + std::to_wstring(sequence.fetch_add(1));
    struct Cleanup {
        std::wstring path;
        ~Cleanup() {
            if (PathExists(path)) { std::string ignored; (void)SetFileMode(path, 0600, ignored); SDL_RemovePath(PathUtf8(path).c_str()); }
        }
    } cleanup{temporary};
    auto *reservation = SDL_IOFromFile(PathUtf8(temporary).c_str(), "wbx");
    if (!reservation) { error = "Unable to reserve temporary copy"; return false; }
    if (!SDL_CloseIO(reservation) || !SDL_CopyFile(PathUtf8(from).c_str(), PathUtf8(temporary).c_str())) {
        error = "Unable to copy " + PathUtf8(from) + ": " + SDL_GetError(); return false;
    }
    if (!expectedHash.empty() && utils::Sha256FileHex(temporary) != expectedHash) {
        error = "Copy hash does not match verified payload: " + PathUtf8(from); return false;
    }
    if (!SetFileMode(temporary, 0600, error)) return false;
    auto *stream = SDL_IOFromFile(PathUtf8(temporary).c_str(), "rb+");
    if (!stream) { error = "Unable to reopen temporary copy"; return false; }
    bool synced = SetFileMode(temporary, mode, error) && BML::Platform::SyncFile(stream);
    if (!SDL_CloseIO(stream)) synced = false;
    if (!synced || !SDL_RenamePath(PathUtf8(temporary).c_str(), PathUtf8(to).c_str()) || !SyncDirectory(ParentPath(to))) {
        error = "Unable to publish copy: " + PathUtf8(to) + ": " + SDL_GetError(); return false;
    }
#else
    static std::atomic<uint64_t> sequence{0};
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temporary = to + L".copy-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
        std::to_wstring(ticks) + L"-" + std::to_wstring(sequence.fetch_add(1));
    if (!CopyFileW(from.c_str(), temporary.c_str(), TRUE)) {
        error = "Unable to copy " + PathUtf8(from) + " (Win32 error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    struct Cleanup {
        const std::wstring &path;
        ~Cleanup() {
            if (PathExists(path)) {
                std::string ignored;
                (void)SetFileMode(path, 0600, ignored);
                DeleteFileW(path.c_str());
            }
        }
    } cleanup{temporary};
    if (!expectedHash.empty() && utils::Sha256FileHex(temporary) != expectedHash) {
        error = "Copy hash does not match verified payload: " + PathUtf8(from);
        return false;
    }
    if (!SetFileMode(temporary, 0600, error))
        return false;
    HANDLE handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        error = "Unable to open temporary copy for flush: " + PathUtf8(temporary);
        return false;
    }
    const bool flushed = FlushFileBuffers(handle) != FALSE;
    CloseHandle(handle);
    if (!flushed) {
        error = "Unable to flush temporary copy: " + PathUtf8(temporary);
        return false;
    }
    if (!SetFileMode(temporary, mode, error))
        return false;
    uint32_t code{};
    if (!BML::Platform::PublishFile(PathUtf8(temporary), PathUtf8(to), true, code)) {
        error = "Unable to publish copy: " + PathUtf8(to) + " (Win32 error " + std::to_string(code) + ")";
        return false;
    }
#endif
    return true;
}

Result CheckPlayerStopped() {
#if BML_RUNTIME_SDL
#ifdef _WIN32
    const char *args[] = {"tasklist", "/FI", "IMAGENAME eq Player.exe", "/NH", nullptr};
#else
    const char *args[] = {"ps", "-A", "-o", "comm=", nullptr};
#endif
    auto *process = SDL_CreateProcess(args, true);
    if (!process) return Result::Failure("Unable to check Player processes: " + std::string(SDL_GetError()));
    size_t size{}; int status{};
    void *output = SDL_ReadProcess(process, &size, &status);
    SDL_DestroyProcess(process);
    std::string text = output ? std::string(static_cast<const char *>(output), size) : std::string();
    SDL_free(output);
    if (status != 0 || text.empty()) return Result::Failure("Unable to read Player process list");
#ifdef _WIN32
    if (text.find("Player.exe") != std::string::npos) return Result::Failure("Close Player before applying or rolling back an update");
#else
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        auto start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos) continue;
        line = line.substr(start, line.find_last_not_of(" \t\r") - start + 1);
        const auto name = FileName(utils::Utf8ToUtf16(line));
        if (name == L"Player" || name == L"Player.exe")
            return Result::Failure("Close Player before applying or rolling back an update");
    }
#endif
#else
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return Result::Failure("Unable to check Player processes");
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        return Result::Failure("Unable to read Player process list");
    }
    do {
        if (_wcsicmp(entry.szExeFile, L"Player.exe") == 0) {
            CloseHandle(snapshot);
            return Result::Failure("Close Player before applying or rolling back an update");
        }
    } while (Process32NextW(snapshot, &entry));
    const auto status = GetLastError();
    CloseHandle(snapshot);
    if (status != ERROR_NO_MORE_FILES)
        return Result::Failure("Unable to read all Player processes");
#endif
    return Result::Success();
}

Result CheckUpdaterPaths(const UpdaterContext &context) {
    if (!DirectoryExists(context.gameRoot)) return Result::Failure("Game root does not exist");
    std::wstring resolved;
    std::string error;
    auto relative = utils::MakeRelativePathUtf8(PathUtf8(context.updaterStateRoot), PathUtf8(context.gameRoot));
#ifdef _WIN32
    std::replace(relative.begin(), relative.end(), '\\', '/');
#endif
    if (!ResolveUnderRoot(context.gameRoot, utils::Utf8ToUtf16(relative), resolved, error)) return Result::Failure(error);
    for (const auto *path : {L".lock", L"state.json", L"sources.json", L"pending.json", L"installed.manifest.json",
                            L"installed.manifest.json.sig", L"transactions", L"staging", L"downloads"})
        if (!ResolveUnderRoot(context.updaterStateRoot, path, resolved, error)) return Result::Failure(error);
    return Result::Success();
}

std::wstring FindPlayer(const std::wstring &root) {
#ifdef _WIN32
    constexpr auto name = L"Player.exe";
#else
    constexpr auto name = L"Player";
#endif
    const auto standard = JoinPath(JoinPath(root, L"Bin"), name);
    const auto flat = JoinPath(root, name);
    if (RegularFileExists(standard) == RegularFileExists(flat)) return {};
    if (RegularFileExists(standard)) return standard;
    return RegularFileExists(JoinPath(root, L"base.cmo")) ? flat : std::wstring();
}
std::string DetectInstalledVersionFromRuntime(const std::wstring &root) {
    std::string error;
    auto doc = utils::JsonDocument::ParseFile(JoinPath(root, L"ModLoader/BML.abi.json"), error);
    return doc.IsValid() ? utils::JsonGetString(doc.Root(), "version", "") : std::string();
}
UpdaterContext CreateDefaultContext() {
    auto directory = ParentPath(utils::GetExecutablePathW());
    const auto root = FileName(directory) == L"Bin" ? ParentPath(directory) :
        RegularFileExists(JoinPath(directory, L"ModLoader/BML.abi.json")) ? directory : utils::GetCurrentDirectoryW();
    return {root, DefaultStateRoot(root)};
}
}
