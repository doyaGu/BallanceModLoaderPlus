#include "PathUtils.h"
#include "PathUtilsDetail.h"
#include "StringUtils.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <memory>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <zip.h>

#undef max
#undef min

namespace utils {
    namespace {
        constexpr DWORD kInitialPathBufferSize = MAX_PATH + 1;
        constexpr wchar_t kBinDirectoryName[] = L"Bin";
        constexpr wchar_t kBuildingBlocksDirectoryName[] = L"BuildingBlocks";

        bool EqualsIgnoreCase(const std::wstring &lhs, const wchar_t *rhs) {
            if (!rhs) {
                return false;
            }

            size_t index = 0;
            while (index < lhs.size() && rhs[index] != L'\0') {
                if (towlower(lhs[index]) != towlower(rhs[index])) {
                    return false;
                }
                ++index;
            }

            return index == lhs.size() && rhs[index] == L'\0';
        }

        std::filesystem::path GetExecutableDirectoryPath(const std::filesystem::path &executablePath) {
            if (executablePath.empty()) {
                return {};
            }

            return executablePath.has_filename() ? executablePath.parent_path() : executablePath;
        }

        std::filesystem::path GetGameDirectoryFromExecutablePath(
                const std::filesystem::path &executablePath) {
            const std::filesystem::path executableDir = GetExecutableDirectoryPath(executablePath);
            if (executableDir.empty()) {
                return {};
            }

            const std::wstring leaf = executableDir.filename().wstring();
            if ((EqualsIgnoreCase(leaf, kBinDirectoryName) ||
                 EqualsIgnoreCase(leaf, kBuildingBlocksDirectoryName)) &&
                executableDir.has_parent_path()) {
                return executableDir.parent_path().lexically_normal();
            }

            return executableDir.lexically_normal();
        }

        std::filesystem::path NormalizePathIfPresent(const std::filesystem::path &path) {
            return path.empty() ? std::filesystem::path() : path.lexically_normal();
        }

        template <typename CharT, typename Api>
        std::basic_string<CharT> CallResizableWin32PathApi(Api api) {
            std::vector<CharT> buffer(kInitialPathBufferSize, CharT{});
            while (true) {
                const DWORD copied = api(buffer.data(), static_cast<DWORD>(buffer.size()));
                if (copied == 0) {
                    return {};
                }

                if (copied < buffer.size()) {
                    return std::basic_string<CharT>(buffer.data(), copied);
                }

                buffer.assign(static_cast<size_t>(copied) + 1, CharT{});
            }
        }

        template <typename CharT, typename Api>
        std::basic_string<CharT> CallResizableModulePathApi(Api api) {
            std::vector<CharT> buffer(kInitialPathBufferSize, CharT{});
            while (true) {
                ::SetLastError(ERROR_SUCCESS);
                const DWORD copied = api(buffer.data(), static_cast<DWORD>(buffer.size()));
                if (copied == 0) {
                    return {};
                }

                const DWORD lastError = ::GetLastError();
                if (copied < buffer.size() - 1 || lastError != ERROR_INSUFFICIENT_BUFFER) {
                    return std::basic_string<CharT>(buffer.data(), copied);
                }

                buffer.assign(buffer.size() * 2, CharT{});
            }
        }

        RuntimeLayout BuildRuntimeLayout(const std::filesystem::path &gameDirectory,
                                         const std::filesystem::path &runtimeDirectory,
                                         const RuntimeLayoutNames &names) {
            RuntimeLayout layout;
            layout.game_directory = NormalizePathIfPresent(gameDirectory);
            layout.runtime_directory = NormalizePathIfPresent(runtimeDirectory);
            layout.mods_directory = (layout.runtime_directory / names.mods_directory).lexically_normal();
            layout.packages_directory =
                (layout.runtime_directory / names.packages_directory).lexically_normal();
            layout.crash_dumps_directory =
                (layout.runtime_directory / names.crash_dumps_directory).lexically_normal();
            layout.fault_log_path =
                (layout.runtime_directory / names.fault_log_file).lexically_normal();
            return layout;
        }
    } // namespace

    // ========================================================================
    // File existence checks (W-primary)
    // ========================================================================

    bool FileExistsA(const std::string &file) {
        if (file.empty()) return false;
        return FileExistsW(AnsiToUtf16(file));
    }

    bool FileExistsW(const std::wstring &file) {
        if (file.empty())
            return false;
        DWORD attr = ::GetFileAttributesW(file.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES &&
            !(attr & FILE_ATTRIBUTE_DIRECTORY));
    }

    bool FileExistsUtf8(const std::string &file) {
        if (file.empty()) return false;
        return FileExistsW(Utf8ToUtf16(file));
    }

    // ========================================================================
    // Directory existence checks (W-primary)
    // ========================================================================

    bool DirectoryExistsA(const std::string &dir) {
        if (dir.empty()) return false;
        return DirectoryExistsW(AnsiToUtf16(dir));
    }

    bool DirectoryExistsW(const std::wstring &dir) {
        if (dir.empty())
            return false;
        DWORD attr = ::GetFileAttributesW(dir.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES &&
            ((attr & FILE_ATTRIBUTE_DIRECTORY) || (attr & FILE_ATTRIBUTE_REPARSE_POINT)));
    }

    bool DirectoryExistsUtf8(const std::string &dir) {
        if (dir.empty()) return false;
        return DirectoryExistsW(Utf8ToUtf16(dir));
    }

    // ========================================================================
    // Path existence checks (W-primary)
    // ========================================================================

    bool PathExistsA(const std::string &path) {
        if (path.empty()) return false;
        return PathExistsW(AnsiToUtf16(path));
    }

    bool PathExistsW(const std::wstring &path) {
        if (path.empty())
            return false;
        DWORD attr = ::GetFileAttributesW(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
    }

    bool PathExistsUtf8(const std::string &path) {
        if (path.empty()) return false;
        return PathExistsW(Utf8ToUtf16(path));
    }

    // ========================================================================
    // Directory creation (W-primary)
    // ========================================================================

    bool CreateDirectoryA(const std::string &dir) {
        if (dir.empty()) return false;
        return CreateDirectoryW(AnsiToUtf16(dir));
    }

    bool CreateDirectoryW(const std::wstring &dir) {
        if (dir.empty())
            return false;
        if (DirectoryExistsW(dir))
            return true;
        return ::CreateDirectoryW(dir.c_str(), nullptr) == TRUE;
    }

    bool CreateDirectoryUtf8(const std::string &dir) {
        if (dir.empty()) return false;
        return CreateDirectoryW(Utf8ToUtf16(dir));
    }

    // ========================================================================
    // Create directory tree (W-primary)
    // ========================================================================

    bool CreateFileTreeA(const std::string &path) {
        if (path.empty()) return false;
        return CreateFileTreeW(AnsiToUtf16(path));
    }

    bool CreateFileTreeW(const std::wstring &path) {
        if (path.empty())
            return false;

        std::wstring normalizedPath = path;
        std::replace(normalizedPath.begin(), normalizedPath.end(), L'/', L'\\');

        size_t startPos = 0;

        // Handle drive letter (e.g., "C:")
        std::wstring drivePart;
        if (normalizedPath.size() > 2 && normalizedPath[1] == L':') {
            drivePart = normalizedPath.substr(0, 2);
            startPos = 2;
            if (normalizedPath.size() > 2 && normalizedPath[2] == L'\\')
                startPos = 3;
        }

        // Split by backslashes
        std::wstring currentPath = drivePart;
        size_t pos;

        while ((pos = normalizedPath.find(L'\\', startPos)) != std::wstring::npos) {
            if (pos > startPos) {
                std::wstring segment = normalizedPath.substr(startPos, pos - startPos);
                currentPath += L"\\" + segment;

                if (!DirectoryExistsW(currentPath)) {
                    if (!CreateDirectoryW(currentPath))
                        return false;
                }
            }
            startPos = pos + 1;
        }

        // Handle the last segment
        if (startPos < normalizedPath.size()) {
            std::wstring segment = normalizedPath.substr(startPos);
            currentPath += L"\\" + segment;

            if (!DirectoryExistsW(currentPath)) {
                if (!CreateDirectoryW(currentPath))
                    return false;
            }
        }

        return true;
    }

    bool CreateFileTreeUtf8(const std::string &path) {
        if (path.size() < 3) return false;
        return CreateFileTreeW(Utf8ToUtf16(path));
    }

    // ========================================================================
    // File deletion (W-primary)
    // ========================================================================

    bool DeleteFileA(const std::string &path) {
        if (path.empty()) return false;
        return DeleteFileW(AnsiToUtf16(path));
    }

    bool DeleteFileW(const std::wstring &path) {
        if (path.empty() || !FileExistsW(path))
            return false;
        return ::DeleteFileW(path.c_str()) == TRUE;
    }

    bool DeleteFileUtf8(const std::string &path) {
        if (path.empty()) return false;
        return DeleteFileW(Utf8ToUtf16(path));
    }

    // ========================================================================
    // Directory deletion (W-primary)
    // ========================================================================

    bool DeleteDirectoryA(const std::string &path) {
        if (path.empty()) return false;
        return DeleteDirectoryW(AnsiToUtf16(path));
    }

    bool DeleteDirectoryW(const std::wstring &path) {
        if (path.empty() || !DirectoryExistsW(path))
            return false;

        WIN32_FIND_DATAW findData;
        std::wstring searchMask = path + L"\\*";
        HANDLE searchHandle = ::FindFirstFileExW(searchMask.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch,
                                                 nullptr, 0);

        if (searchHandle == INVALID_HANDLE_VALUE) {
            if (::GetLastError() == ERROR_FILE_NOT_FOUND)
                return ::RemoveDirectoryW(path.c_str()) == TRUE;
            return false;
        }

        bool success = true;
        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
                continue;

            std::wstring filePath = path + L'\\' + findData.cFileName;

            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                success = DeleteDirectoryW(filePath) && success;
            } else {
                success = ::DeleteFileW(filePath.c_str()) == TRUE && success;
            }
        } while (::FindNextFileW(searchHandle, &findData));

        ::FindClose(searchHandle);

        if (success) {
            success = ::RemoveDirectoryW(path.c_str()) == TRUE;
        }

        return success;
    }

    bool DeleteDirectoryUtf8(const std::string &path) {
        if (path.empty()) return false;
        return DeleteDirectoryW(Utf8ToUtf16(path));
    }

    // ========================================================================
    // File copying (W-primary)
    // ========================================================================

    bool CopyFileA(const std::string &path, const std::string &dest) {
        if (path.empty() || dest.empty()) return false;
        return CopyFileW(AnsiToUtf16(path), AnsiToUtf16(dest));
    }

    bool CopyFileW(const std::wstring &path, const std::wstring &dest) {
        if (!FileExistsW(path) || dest.empty())
            return false;

        std::wstring destDir = GetDirectoryW(dest);
        if (!destDir.empty() && !DirectoryExistsW(destDir)) {
            if (!CreateFileTreeW(destDir))
                return false;
        }

        return ::CopyFileW(path.c_str(), dest.c_str(), FALSE) == TRUE;
    }

    bool CopyFileUtf8(const std::string &path, const std::string &dest) {
        return CopyFileW(Utf8ToUtf16(path), Utf8ToUtf16(dest));
    }

    // ========================================================================
    // File moving/renaming (W-primary)
    // ========================================================================

    bool MoveFileA(const std::string &path, const std::string &dest) {
        if (path.empty() || dest.empty()) return false;
        return MoveFileW(AnsiToUtf16(path), AnsiToUtf16(dest));
    }

    bool MoveFileW(const std::wstring &path, const std::wstring &dest) {
        if (!PathExistsW(path) || dest.empty())
            return false;

        std::wstring destDir = GetDirectoryW(dest);
        if (!destDir.empty() && !DirectoryExistsW(destDir)) {
            if (!CreateFileTreeW(destDir))
                return false;
        }

        return ::MoveFileExW(path.c_str(), dest.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) == TRUE;
    }

    bool MoveFileUtf8(const std::string &path, const std::string &dest) {
        return MoveFileW(Utf8ToUtf16(path), Utf8ToUtf16(dest));
    }

    // ========================================================================
    // Zip extraction
    // ========================================================================

    bool ExtractZipA(const std::string &path, const std::string &dest) {
        return ExtractZipW(Utf8ToUtf16(path), Utf8ToUtf16(dest));
    }

    bool ExtractZipW(const std::wstring &path, const std::wstring &dest) {
        if (!FileExistsW(path) || dest.empty())
            return false;

        if (!DirectoryExistsW(dest)) {
            if (!CreateFileTreeW(dest))
                return false;
        }

        FILE *fp = nullptr;
        if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || !fp)
            return false;

        std::unique_ptr<FILE, decltype(&fclose)> filePtr(fp, &fclose);

        fseek(fp, 0, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        std::vector<char> buffer(size);
        if (fread(buffer.data(), sizeof(char), size, fp) != size)
            return false;

        std::string destPathUtf8 = Utf16ToUtf8(dest);
        return zip_stream_extract(buffer.data(), size, destPathUtf8.c_str(), nullptr, nullptr) >= 0;
    }

    bool ExtractZipUtf8(const std::string &path, const std::string &dest) {
        return ExtractZipW(Utf8ToUtf16(path), Utf8ToUtf16(dest));
    }

    // ========================================================================
    // Path manipulation (template-based)
    // ========================================================================

    std::string GetDriveA(const std::string &path) { return detail::GetDriveImpl<char>(path); }
    std::wstring GetDriveW(const std::wstring &path) { return detail::GetDriveImpl<wchar_t>(path); }
    std::string GetDriveUtf8(const std::string &path) { return detail::GetDriveImpl<char>(path); }

    std::string GetDirectoryA(const std::string &path) { return detail::GetDirectoryImpl<char>(path); }
    std::wstring GetDirectoryW(const std::wstring &path) { return detail::GetDirectoryImpl<wchar_t>(path); }
    std::string GetDirectoryUtf8(const std::string &path) { return detail::GetDirectoryImpl<char>(path); }

    std::pair<std::string, std::string> GetDriveAndDirectoryA(const std::string &path) { return detail::GetDriveAndDirectoryImpl<char>(path); }
    std::pair<std::wstring, std::wstring> GetDriveAndDirectoryW(const std::wstring &path) { return detail::GetDriveAndDirectoryImpl<wchar_t>(path); }
    std::pair<std::string, std::string> GetDriveAndDirectoryUtf8(const std::string &path) { return detail::GetDriveAndDirectoryImpl<char>(path); }

    std::string GetFileNameA(const std::string &path) { return detail::GetFileNameImpl<char>(path); }
    std::wstring GetFileNameW(const std::wstring &path) { return detail::GetFileNameImpl<wchar_t>(path); }
    std::string GetFileNameUtf8(const std::string &path) { return detail::GetFileNameImpl<char>(path); }

    std::string GetExtensionA(const std::string &path) { return detail::GetExtensionImpl<char>(path); }
    std::wstring GetExtensionW(const std::wstring &path) { return detail::GetExtensionImpl<wchar_t>(path); }
    std::string GetExtensionUtf8(const std::string &path) { return detail::GetExtensionImpl<char>(path); }

    std::string RemoveExtensionA(const std::string &path) { return detail::RemoveExtensionImpl<char>(path); }
    std::wstring RemoveExtensionW(const std::wstring &path) { return detail::RemoveExtensionImpl<wchar_t>(path); }
    std::string RemoveExtensionUtf8(const std::string &path) { return detail::RemoveExtensionImpl<char>(path); }

    std::string CombinePathA(const std::string &path1, const std::string &path2) { return detail::CombinePathImpl<char>(path1, path2); }
    std::wstring CombinePathW(const std::wstring &path1, const std::wstring &path2) { return detail::CombinePathImpl<wchar_t>(path1, path2); }
    std::string CombinePathUtf8(const std::string &path1, const std::string &path2) { return detail::CombinePathImpl<char>(path1, path2); }

    std::string NormalizePathA(const std::string &path) { return detail::NormalizePathImpl<char>(path); }
    std::wstring NormalizePathW(const std::wstring &path) { return detail::NormalizePathImpl<wchar_t>(path); }
    std::string NormalizePathUtf8(const std::string &path) { return detail::NormalizePathImpl<char>(path); }

    // ========================================================================
    // Path validation (template-based)
    // ========================================================================

    bool IsPathValidA(const std::string &path) { return detail::IsPathValidImpl<char>(path); }
    bool IsPathValidW(const std::wstring &path) { return detail::IsPathValidImpl<wchar_t>(path); }
    bool IsPathValidUtf8(const std::string &path) { return detail::IsPathValidImpl<char>(path); }

    // ========================================================================
    // Path type checks (template-based)
    // ========================================================================

    bool IsAbsolutePathA(const std::string &path) { return detail::IsAbsolutePathImpl<char>(path); }
    bool IsAbsolutePathW(const std::wstring &path) { return detail::IsAbsolutePathImpl<wchar_t>(path); }
    bool IsAbsolutePathUtf8(const std::string &path) { return detail::IsAbsolutePathImpl<char>(path); }

    bool IsRelativePathA(const std::string &path) { return !IsAbsolutePathA(path); }
    bool IsRelativePathW(const std::wstring &path) { return !IsAbsolutePathW(path); }
    bool IsRelativePathUtf8(const std::string &path) { return !IsAbsolutePathUtf8(path); }

    bool IsPathRootedA(const std::string &path) { return detail::IsPathRootedImpl<char>(path); }
    bool IsPathRootedW(const std::wstring &path) { return detail::IsPathRootedImpl<wchar_t>(path); }
    bool IsPathRootedUtf8(const std::string &path) { return detail::IsPathRootedImpl<char>(path); }

    // ========================================================================
    // Path resolution (template-based)
    // ========================================================================

    std::string ResolvePathA(const std::string &path) { return detail::ResolvePathImpl<char>(path); }
    std::wstring ResolvePathW(const std::wstring &path) { return detail::ResolvePathImpl<wchar_t>(path); }
    std::string ResolvePathUtf8(const std::string &path) { return detail::ResolvePathImpl<char>(path); }

    std::string MakeRelativePathA(const std::string &path, const std::string &basePath) { return detail::MakeRelativePathImpl<char>(path, basePath); }
    std::wstring MakeRelativePathW(const std::wstring &path, const std::wstring &basePath) { return detail::MakeRelativePathImpl<wchar_t>(path, basePath); }
    std::string MakeRelativePathUtf8(const std::string &path, const std::string &basePath) { return detail::MakeRelativePathImpl<char>(path, basePath); }

    // ========================================================================
    // System paths
    // ========================================================================

    std::string GetTempPathA() {
        return CallResizableWin32PathApi<char>(
            [](char *buffer, DWORD size) {
                return ::GetTempPathA(size, buffer);
            });
    }

    std::wstring GetTempPathW() {
        return CallResizableWin32PathApi<wchar_t>(
            [](wchar_t *buffer, DWORD size) {
                return ::GetTempPathW(size, buffer);
            });
    }

    std::string GetTempPathUtf8() {
        return Utf16ToUtf8(GetTempPathW());
    }

    std::string GetCurrentDirectoryA() {
        return CallResizableWin32PathApi<char>(
            [](char *buffer, DWORD size) {
                return ::GetCurrentDirectoryA(size, buffer);
            });
    }

    std::wstring GetCurrentDirectoryW() {
        return CallResizableWin32PathApi<wchar_t>(
            [](wchar_t *buffer, DWORD size) {
                return ::GetCurrentDirectoryW(size, buffer);
            });
    }

    std::string GetCurrentDirectoryUtf8() {
        return Utf16ToUtf8(GetCurrentDirectoryW());
    }

    bool SetCurrentDirectoryA(const std::string &path) {
        if (path.empty()) return false;
        return SetCurrentDirectoryW(AnsiToUtf16(path));
    }

    bool SetCurrentDirectoryW(const std::wstring &path) {
        if (path.empty()) return false;
        return ::SetCurrentDirectoryW(path.c_str()) == TRUE;
    }

    bool SetCurrentDirectoryUtf8(const std::string &path) {
        return SetCurrentDirectoryW(Utf8ToUtf16(path));
    }

    std::string GetExecutablePathA() {
        return CallResizableModulePathApi<char>(
            [](char *buffer, DWORD size) {
                return ::GetModuleFileNameA(nullptr, buffer, size);
            });
    }

    std::wstring GetExecutablePathW() {
        return CallResizableModulePathApi<wchar_t>(
            [](wchar_t *buffer, DWORD size) {
                return ::GetModuleFileNameW(nullptr, buffer, size);
            });
    }

    std::string GetExecutablePathUtf8() {
        return Utf16ToUtf8(GetExecutablePathW());
    }

    // ========================================================================
    // Runtime layout resolution
    // ========================================================================

    RuntimeLayout ResolveRuntimeLayoutFromExecutable(const std::filesystem::path &executablePath,
                                                     const RuntimeLayoutNames &names) {
        if (executablePath.empty()) {
            return {};
        }

        const std::filesystem::path gameDirectory =
            GetGameDirectoryFromExecutablePath(executablePath);
        if (gameDirectory.empty()) {
            return {};
        }

        return BuildRuntimeLayout(gameDirectory,
                                  gameDirectory / names.runtime_directory,
                                  names);
    }

    RuntimeLayout ResolveRuntimeLayoutFromRuntimeDirectory(const std::filesystem::path &runtimeDirectory,
                                                           const RuntimeLayoutNames &names) {
        if (runtimeDirectory.empty()) {
            return {};
        }

        const std::filesystem::path normalizedRuntimeDirectory = runtimeDirectory.lexically_normal();
        std::filesystem::path gameDirectory;
        if (normalizedRuntimeDirectory.has_parent_path()) {
            gameDirectory = normalizedRuntimeDirectory.parent_path().lexically_normal();
        }

        return BuildRuntimeLayout(gameDirectory, normalizedRuntimeDirectory, names);
    }

    RuntimeLayout ResolveRuntimeLayoutFromModsDirectory(const std::filesystem::path &modsDirectory,
                                                        const RuntimeLayoutNames &names) {
        if (modsDirectory.empty()) {
            return {};
        }

        const std::filesystem::path normalizedModsDirectory = modsDirectory.lexically_normal();
        if (normalizedModsDirectory.empty()) {
            return {};
        }

        std::filesystem::path runtimeDirectory;
        if (normalizedModsDirectory.has_filename() &&
            EqualsIgnoreCase(normalizedModsDirectory.filename().wstring(), names.mods_directory.c_str()) &&
            normalizedModsDirectory.has_parent_path()) {
            runtimeDirectory = normalizedModsDirectory.parent_path().lexically_normal();
            RuntimeLayout layout = ResolveRuntimeLayoutFromRuntimeDirectory(runtimeDirectory, names);
            layout.mods_directory = normalizedModsDirectory;
            return layout;
        }

        RuntimeLayout layout = GetRuntimeLayout(names);
        if (layout.runtime_directory.empty()) {
            if (normalizedModsDirectory.has_parent_path()) {
                runtimeDirectory = normalizedModsDirectory.parent_path().lexically_normal();
            } else {
                runtimeDirectory = normalizedModsDirectory;
            }
            layout = ResolveRuntimeLayoutFromRuntimeDirectory(runtimeDirectory, names);
        }
        layout.mods_directory = normalizedModsDirectory;
        return layout;
    }

    RuntimeLayout GetRuntimeLayout(const RuntimeLayoutNames &names) {
        const std::wstring executablePath = GetExecutablePathW();
        if (!executablePath.empty()) {
            return ResolveRuntimeLayoutFromExecutable(std::filesystem::path(executablePath), names);
        }

        const std::wstring currentDirectory = GetCurrentDirectoryW();
        if (currentDirectory.empty()) {
            return {};
        }

        return ResolveRuntimeLayoutFromRuntimeDirectory(
            std::filesystem::path(currentDirectory) / names.runtime_directory,
            names);
    }

    // ========================================================================
    // File properties (W-primary)
    // ========================================================================

    int64_t GetFileSizeA(const std::string &path) {
        if (path.empty()) return -1;
        return GetFileSizeW(AnsiToUtf16(path));
    }

    int64_t GetFileSizeW(const std::wstring &path) {
        if (!FileExistsW(path))
            return -1;

        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileInfo))
            return -1;

        LARGE_INTEGER size;
        size.LowPart = fileInfo.nFileSizeLow;
        size.HighPart = fileInfo.nFileSizeHigh;
        return size.QuadPart;
    }

    int64_t GetFileSizeUtf8(const std::string &path) {
        return GetFileSizeW(Utf8ToUtf16(path));
    }

    FileTime GetFileTimeA(const std::string &path) {
        if (path.empty()) return {0, 0, 0};
        return GetFileTimeW(AnsiToUtf16(path));
    }

    FileTime GetFileTimeW(const std::wstring &path) {
        FileTime result = {0, 0, 0};

        if (!PathExistsW(path))
            return result;

        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileInfo))
            return result;

        LARGE_INTEGER li;

        li.LowPart = fileInfo.ftCreationTime.dwLowDateTime;
        li.HighPart = fileInfo.ftCreationTime.dwHighDateTime;
        result.creationTime = li.QuadPart;

        li.LowPart = fileInfo.ftLastAccessTime.dwLowDateTime;
        li.HighPart = fileInfo.ftLastAccessTime.dwHighDateTime;
        result.lastAccessTime = li.QuadPart;

        li.LowPart = fileInfo.ftLastWriteTime.dwLowDateTime;
        li.HighPart = fileInfo.ftLastWriteTime.dwHighDateTime;
        result.lastWriteTime = li.QuadPart;

        return result;
    }

    FileTime GetFileTimeUtf8(const std::string &path) {
        return GetFileTimeW(Utf8ToUtf16(path));
    }

    // ========================================================================
    // File I/O
    // ========================================================================

    std::string ReadTextFileA(const std::string &path) {
        if (!FileExistsA(path))
            return "";

        std::ifstream file(path);
        if (!file.is_open())
            return "";

        return std::string(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        );
    }

    std::wstring ReadTextFileW(const std::wstring &path) {
        if (!FileExistsW(path))
            return L"";

        std::wifstream file(path.c_str());
        if (!file.is_open())
            return L"";

        return std::wstring(
            std::istreambuf_iterator<wchar_t>(file),
            std::istreambuf_iterator<wchar_t>()
        );
    }

    std::string ReadTextFileUtf8(const std::string &path) {
        return ReadTextFileA(path);
    }

    bool WriteTextFileA(const std::string &path, const std::string &content) {
        std::string dir = GetDirectoryA(path);
        if (!dir.empty() && !DirectoryExistsA(dir)) {
            if (!CreateFileTreeA(dir))
                return false;
        }

        std::ofstream file(path);
        if (!file.is_open())
            return false;

        file << content;
        return file.good();
    }

    bool WriteTextFileW(const std::wstring &path, const std::wstring &content) {
        std::wstring dir = GetDirectoryW(path);
        if (!dir.empty() && !DirectoryExistsW(dir)) {
            if (!CreateFileTreeW(dir))
                return false;
        }

        std::wofstream file(path.c_str());
        if (!file.is_open())
            return false;

        file << content;
        return file.good();
    }

    bool WriteTextFileUtf8(const std::string &path, const std::string &content) {
        return WriteTextFileA(path, content);
    }

    std::vector<uint8_t> ReadBinaryFileA(const std::string &path) {
        if (!FileExistsA(path))
            return {};

        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return {};

        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (file.read(reinterpret_cast<char *>(buffer.data()), size))
            return buffer;

        return {};
    }

    std::vector<uint8_t> ReadBinaryFileW(const std::wstring &path) {
        if (!FileExistsW(path))
            return {};

        FILE *file = nullptr;
        _wfopen_s(&file, path.c_str(), L"rb");
        if (!file)
            return {};

        std::unique_ptr<FILE, decltype(&fclose)> filePtr(file, &fclose);

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (fread(buffer.data(), 1, size, file) == static_cast<size_t>(size))
            return buffer;

        return {};
    }

    std::vector<uint8_t> ReadBinaryFileUtf8(const std::string &path) {
        return ReadBinaryFileW(Utf8ToUtf16(path));
    }

    bool WriteBinaryFileA(const std::string &path, const std::vector<uint8_t> &data) {
        std::string dir = GetDirectoryA(path);
        if (!dir.empty() && !DirectoryExistsA(dir)) {
            if (!CreateFileTreeA(dir))
                return false;
        }

        std::ofstream file(path, std::ios::binary);
        if (!file.is_open())
            return false;

        file.write(reinterpret_cast<const char *>(data.data()), data.size());
        return file.good();
    }

    bool WriteBinaryFileW(const std::wstring &path, const std::vector<uint8_t> &data) {
        std::wstring dir = GetDirectoryW(path);
        if (!dir.empty() && !DirectoryExistsW(dir)) {
            if (!CreateFileTreeW(dir))
                return false;
        }

        FILE *file = nullptr;
        _wfopen_s(&file, path.c_str(), L"wb");
        if (!file)
            return false;

        std::unique_ptr<FILE, decltype(&fclose)> filePtr(file, &fclose);

        return fwrite(data.data(), 1, data.size(), file) == data.size();
    }

    bool WriteBinaryFileUtf8(const std::string &path, const std::vector<uint8_t> &data) {
        return WriteBinaryFileW(Utf8ToUtf16(path), data);
    }

    // ========================================================================
    // Temporary files
    // ========================================================================

    std::string CreateTempFileA(const std::string &prefix) {
        char tempPath[MAX_PATH];
        char tempFileName[MAX_PATH];

        DWORD pathLen = ::GetTempPathA(MAX_PATH, tempPath);
        if (pathLen == 0 || pathLen > MAX_PATH) {
            return "";
        }

        std::string prefixToUse = prefix.empty() ? "tmp" : prefix;
        UINT uniqueNum = ::GetTempFileNameA(tempPath, prefixToUse.c_str(), 0, tempFileName);
        if (uniqueNum == 0) {
            return "";
        }

        return std::string(tempFileName);
    }

    std::wstring CreateTempFileW(const std::wstring &prefix) {
        wchar_t tempPath[MAX_PATH];
        wchar_t tempFileName[MAX_PATH];

        DWORD pathLen = ::GetTempPathW(MAX_PATH, tempPath);
        if (pathLen == 0 || pathLen > MAX_PATH) {
            return L"";
        }

        std::wstring prefixToUse = prefix.empty() ? L"tmp" : prefix;
        UINT uniqueNum = ::GetTempFileNameW(tempPath, prefixToUse.c_str(), 0, tempFileName);
        if (uniqueNum == 0) {
            return L"";
        }

        return std::wstring(tempFileName);
    }

    std::string CreateTempFileUtf8(const std::string &prefix) {
        return Utf16ToUtf8(CreateTempFileW(Utf8ToUtf16(prefix)));
    }

    // ========================================================================
    // Directory listing (W-primary)
    // ========================================================================

    std::vector<std::string> ListFilesA(const std::string &dir, const std::string &pattern) {
        auto wideFiles = ListFilesW(AnsiToUtf16(dir), AnsiToUtf16(pattern));
        std::vector<std::string> files;
        files.reserve(wideFiles.size());
        for (const auto &wf : wideFiles) {
            files.push_back(Utf16ToAnsi(wf));
        }
        return files;
    }

    std::vector<std::wstring> ListFilesW(const std::wstring &dir, const std::wstring &pattern) {
        std::vector<std::wstring> files;

        if (!DirectoryExistsW(dir))
            return files;

        std::wstring searchPath = CombinePathW(dir, pattern);
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

        if (hFind == INVALID_HANDLE_VALUE)
            return files;

        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                files.push_back(findData.cFileName);
            }
        } while (FindNextFileW(hFind, &findData));

        FindClose(hFind);
        return files;
    }

    std::vector<std::string> ListFilesUtf8(const std::string &dir, const std::string &pattern) {
        std::vector<std::wstring> wideFiles = ListFilesW(Utf8ToUtf16(dir), Utf8ToUtf16(pattern));
        std::vector<std::string> files;
        files.reserve(wideFiles.size());
        for (const auto &wideFile : wideFiles) {
            files.push_back(Utf16ToUtf8(wideFile));
        }
        return files;
    }

    std::vector<std::string> ListDirectoriesA(const std::string &dir, const std::string &pattern) {
        auto wideDirs = ListDirectoriesW(AnsiToUtf16(dir), AnsiToUtf16(pattern));
        std::vector<std::string> directories;
        directories.reserve(wideDirs.size());
        for (const auto &wd : wideDirs) {
            directories.push_back(Utf16ToAnsi(wd));
        }
        return directories;
    }

    std::vector<std::wstring> ListDirectoriesW(const std::wstring &dir, const std::wstring &pattern) {
        std::vector<std::wstring> directories;

        if (!DirectoryExistsW(dir))
            return directories;

        std::wstring searchPath = CombinePathW(dir, pattern);
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

        if (hFind == INVALID_HANDLE_VALUE)
            return directories;

        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                wcscmp(findData.cFileName, L".") != 0 &&
                wcscmp(findData.cFileName, L"..") != 0) {
                directories.push_back(findData.cFileName);
            }
        } while (FindNextFileW(hFind, &findData));

        FindClose(hFind);
        return directories;
    }

    std::vector<std::string> ListDirectoriesUtf8(const std::string &dir, const std::string &pattern) {
        std::vector<std::wstring> wideDirs = ListDirectoriesW(Utf8ToUtf16(dir), Utf8ToUtf16(pattern));
        std::vector<std::string> directories;
        directories.reserve(wideDirs.size());
        for (const auto &wideDir : wideDirs) {
            directories.push_back(Utf16ToUtf8(wideDir));
        }
        return directories;
    }
}
