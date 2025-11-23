#include "PathUtils.h"
#include "StringUtils.h"

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
    // File existence checks
    bool FileExistsA(const std::string &file) {
        if (file.empty())
            return false;
        DWORD attr = ::GetFileAttributesA(file.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES &&
            !(attr & FILE_ATTRIBUTE_DIRECTORY));
    }

    bool FileExistsW(const std::wstring &file) {
        if (file.empty())
            return false;
        DWORD attr = ::GetFileAttributesW(file.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES &&
            !(attr & FILE_ATTRIBUTE_DIRECTORY));
    }

    bool FileExistsUtf8(const std::string &file) {
        if (file.empty())
            return false;
        return FileExistsW(Utf8ToUtf16(file));
    }

    // Directory existence checks
    bool DirectoryExistsA(const std::string &dir) {
        if (dir.empty())
            return false;
        DWORD attr = ::GetFileAttributesA(dir.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES &&
            ((attr & FILE_ATTRIBUTE_DIRECTORY) || (attr & FILE_ATTRIBUTE_REPARSE_POINT)));
    }

    bool DirectoryExistsW(const std::wstring &dir) {
        if (dir.empty())
            return false;
        DWORD attr = ::GetFileAttributesW(dir.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES &&
            ((attr & FILE_ATTRIBUTE_DIRECTORY) || (attr & FILE_ATTRIBUTE_REPARSE_POINT)));
    }

    bool DirectoryExistsUtf8(const std::string &dir) {
        if (dir.empty())
            return false;
        return DirectoryExistsW(Utf8ToUtf16(dir));
    }

    // Path existence checks
    bool PathExistsA(const std::string &path) {
        if (path.empty())
            return false;
        DWORD attr = ::GetFileAttributesA(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
    }

    bool PathExistsW(const std::wstring &path) {
        if (path.empty())
            return false;
        DWORD attr = ::GetFileAttributesW(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
    }

    bool PathExistsUtf8(const std::string &path) {
        if (path.empty())
            return false;
        return PathExistsW(Utf8ToUtf16(path));
    }

    // Directory creation
    bool CreateDirectoryA(const std::string &dir) {
        if (dir.empty())
            return false;
        if (DirectoryExistsA(dir))
            return true;
        return ::CreateDirectoryA(dir.c_str(), nullptr) == TRUE;
    }

    bool CreateDirectoryW(const std::wstring &dir) {
        if (dir.empty())
            return false;
        if (DirectoryExistsW(dir))
            return true;
        return ::CreateDirectoryW(dir.c_str(), nullptr) == TRUE;
    }

    bool CreateDirectoryUtf8(const std::string &dir) {
        if (dir.empty())
            return false;
        return CreateDirectoryW(Utf8ToUtf16(dir));
    }

    // Create directory tree
    bool CreateFileTreeA(const std::string &path) {
        if (path.empty())
            return false;

        std::string normalizedPath = path;
        std::replace(normalizedPath.begin(), normalizedPath.end(), '/', '\\');

        // Split the path into components
        std::vector<std::string> components;
        size_t startPos = 0;

        // Handle drive letter (e.g., "C:")
        std::string drivePart;
        if (normalizedPath.size() > 2 && normalizedPath[1] == ':') {
            drivePart = normalizedPath.substr(0, 2);
            startPos = 2;
            // Skip backslash after drive if present
            if (normalizedPath.size() > 2 && normalizedPath[2] == '\\')
                startPos = 3;
        }

        // Split by backslashes
        std::string currentPath = drivePart;
        size_t pos;

        while ((pos = normalizedPath.find('\\', startPos)) != std::string::npos) {
            if (pos > startPos) {
                std::string segment = normalizedPath.substr(startPos, pos - startPos);
                currentPath += "\\" + segment;

                // Create this directory level if it doesn't exist
                if (!DirectoryExistsA(currentPath)) {
                    if (!CreateDirectoryA(currentPath))
                        return false;
                }
            }
            startPos = pos + 1;
        }

        // Handle the last segment if not ending with backslash
        if (startPos < normalizedPath.size()) {
            std::string segment = normalizedPath.substr(startPos);
            currentPath += "\\" + segment;

            // Is this the final part a directory or file?
            // For simplicity, we'll treat it as a directory since we can't tell
            if (!DirectoryExistsA(currentPath)) {
                if (!CreateDirectoryA(currentPath))
                    return false;
            }
        }

        return true;
    }

    bool CreateFileTreeW(const std::wstring &path) {
        if (path.empty())
            return false;

        std::wstring normalizedPath = path;
        std::replace(normalizedPath.begin(), normalizedPath.end(), L'/', L'\\');

        // Split the path into components
        std::vector<std::wstring> components;
        size_t startPos = 0;

        // Handle drive letter (e.g., "C:")
        std::wstring drivePart;
        if (normalizedPath.size() > 2 && normalizedPath[1] == L':') {
            drivePart = normalizedPath.substr(0, 2);
            startPos = 2;
            // Skip backslash after drive if present
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

                // Create this directory level if it doesn't exist
                if (!DirectoryExistsW(currentPath)) {
                    if (!CreateDirectoryW(currentPath))
                        return false;
                }
            }
            startPos = pos + 1;
        }

        // Handle the last segment if not ending with backslash
        if (startPos < normalizedPath.size()) {
            std::wstring segment = normalizedPath.substr(startPos);
            currentPath += L"\\" + segment;

            // Is this the final part a directory or file?
            // For simplicity, we'll treat it as a directory since we can't tell
            if (!DirectoryExistsW(currentPath)) {
                if (!CreateDirectoryW(currentPath))
                    return false;
            }
        }

        return true;
    }

    bool CreateFileTreeUtf8(const std::string &path) {
        if (path.size() < 3)
            return false;
        return CreateFileTreeW(Utf8ToUtf16(path));
    }

    // File deletion
    bool DeleteFileA(const std::string &path) {
        if (path.empty() || !FileExistsA(path))
            return false;
        return ::DeleteFileA(path.c_str()) == TRUE;
    }

    bool DeleteFileW(const std::wstring &path) {
        if (path.empty() || !FileExistsW(path))
            return false;
        return ::DeleteFileW(path.c_str()) == TRUE;
    }

    bool DeleteFileUtf8(const std::string &path) {
        if (path.empty())
            return false;
        return DeleteFileW(Utf8ToUtf16(path));
    }

    // Directory deletion
    bool DeleteDirectoryA(const std::string &path) {
        if (path.empty() || !DirectoryExistsA(path))
            return false;

        WIN32_FIND_DATAA findData;
        std::string searchMask = path + "\\*";
        HANDLE searchHandle = ::FindFirstFileExA(searchMask.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch,
                                                 nullptr, 0);

        if (searchHandle == INVALID_HANDLE_VALUE) {
            if (::GetLastError() == ERROR_FILE_NOT_FOUND)
                return ::RemoveDirectoryA(path.c_str()) == TRUE;
            return false;
        }

        bool success = true;
        do {
            // Skip "." and ".." entries
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
                continue;

            std::string filePath = path + '\\' + findData.cFileName;

            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                // Recursive delete for directories
                success = DeleteDirectoryA(filePath) && success;
            } else {
                // Delete files
                success = ::DeleteFileA(filePath.c_str()) == TRUE && success;
            }
        } while (::FindNextFileA(searchHandle, &findData));

        ::FindClose(searchHandle);

        if (success) {
            success = ::RemoveDirectoryA(path.c_str()) == TRUE;
        }

        return success;
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
            // Skip "." and ".." entries
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0)
                continue;

            std::wstring filePath = path + L'\\' + findData.cFileName;

            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
                // Recursive delete for directories
                success = DeleteDirectoryW(filePath) && success;
            } else {
                // Delete files
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
        if (path.empty())
            return false;
        return DeleteDirectoryW(Utf8ToUtf16(path));
    }

    // File copying
    bool CopyFileA(const std::string &path, const std::string &dest) {
        if (!FileExistsA(path) || dest.empty())
            return false;

        // Ensure destination directory exists
        std::string destDir = GetDirectoryA(dest);
        if (!destDir.empty() && !DirectoryExistsA(destDir)) {
            if (!CreateFileTreeA(destDir))
                return false;
        }

        return ::CopyFileA(path.c_str(), dest.c_str(), FALSE) == TRUE;
    }

    bool CopyFileW(const std::wstring &path, const std::wstring &dest) {
        if (!FileExistsW(path) || dest.empty())
            return false;

        // Ensure destination directory exists
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

    // File moving/renaming
    bool MoveFileA(const std::string &path, const std::string &dest) {
        if (!PathExistsA(path) || dest.empty())
            return false;

        // Ensure destination directory exists
        std::string destDir = GetDirectoryA(dest);
        if (!destDir.empty() && !DirectoryExistsA(destDir)) {
            if (!CreateFileTreeA(destDir))
                return false;
        }

        return ::MoveFileExA(path.c_str(), dest.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) == TRUE;
    }

    bool MoveFileW(const std::wstring &path, const std::wstring &dest) {
        if (!PathExistsW(path) || dest.empty())
            return false;

        // Ensure destination directory exists
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

    // Zip extraction
    bool ExtractZipA(const std::string &path, const std::string &dest) {
        return ExtractZipW(Utf8ToUtf16(path), Utf8ToUtf16(dest));
    }

    bool ExtractZipW(const std::wstring &path, const std::wstring &dest) {
        if (!FileExistsW(path) || dest.empty())
            return false;

        // Create destination directory if needed
        if (!DirectoryExistsW(dest)) {
            if (!CreateFileTreeW(dest))
                return false;
        }

        // Check if file is a zip
        if (!StringEndsWithCaseInsensitive(path, L".zip"))
            return false;

        // Open the zip file
        FILE *fp = nullptr;
        if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || !fp)
            return false;

        // Use unique_ptr with custom deleter for automatic cleanup
        std::unique_ptr<FILE, decltype(&fclose)> filePtr(fp, &fclose);

        // Get file size
        fseek(fp, 0, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        // Read zip file into memory
        std::vector<char> buffer(size);
        if (fread(buffer.data(), sizeof(char), size, fp) != size)
            return false;

        // Extract the zip
        std::string destPathUtf8 = Utf16ToUtf8(dest);
        return zip_stream_extract(buffer.data(), size, destPathUtf8.c_str(), nullptr, nullptr) >= 0;
    }

    bool ExtractZipUtf8(const std::string &path, const std::string &dest) {
        return ExtractZipW(Utf8ToUtf16(path), Utf8ToUtf16(dest));
    }

    // Path manipulation
    std::string GetDriveA(const std::string &path) {
        if (path.size() >= 2 && path[1] == ':')
            return path.substr(0, 2);
        return "";
    }

    std::wstring GetDriveW(const std::wstring &path) {
        if (path.size() >= 2 && path[1] == L':')
            return path.substr(0, 2);
        return L"";
    }

    std::string GetDriveUtf8(const std::string &path) {
        return GetDriveA(path);
    }

    std::string GetDirectoryA(const std::string &path) {
        if (path.empty())
            return "";

        size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos)
            return "";

        return path.substr(0, pos);
    }

    std::wstring GetDirectoryW(const std::wstring &path) {
        if (path.empty())
            return L"";

        size_t pos = path.find_last_of(L"/\\");
        if (pos == std::wstring::npos)
            return L"";

        return path.substr(0, pos);
    }

    std::string GetDirectoryUtf8(const std::string &path) {
        if (path.empty())
            return "";

        size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos)
            return "";

        return path.substr(0, pos);
    }

    std::pair<std::string, std::string> GetDriveAndDirectoryA(const std::string &path) {
        std::string drive = GetDriveA(path);
        std::string dir = GetDirectoryA(path);

        // If we have a drive letter, remove it from directory
        if (!drive.empty() && dir.size() >= drive.size() &&
            dir.substr(0, drive.size()) == drive) {
            dir = dir.substr(drive.size());
        }

        return {drive, dir};
    }

    std::pair<std::wstring, std::wstring> GetDriveAndDirectoryW(const std::wstring &path) {
        std::wstring drive = GetDriveW(path);
        std::wstring dir = GetDirectoryW(path);

        // If we have a drive letter, remove it from directory
        if (!drive.empty() && dir.size() >= drive.size() &&
            dir.substr(0, drive.size()) == drive) {
            dir = dir.substr(drive.size());
        }

        return {drive, dir};
    }

    std::pair<std::string, std::string> GetDriveAndDirectoryUtf8(const std::string &path) {
        return GetDriveAndDirectoryA(path);
    }

    std::string GetFileNameA(const std::string &path) {
        if (path.empty())
            return "";

        size_t pos = path.find_last_of("/\\");
        if (pos == std::string::npos)
            return path;

        return path.substr(pos + 1);
    }

    std::wstring GetFileNameW(const std::wstring &path) {
        if (path.empty())
            return L"";

        size_t pos = path.find_last_of(L"/\\");
        if (pos == std::wstring::npos)
            return path;

        return path.substr(pos + 1);
    }

    std::string GetFileNameUtf8(const std::string &path) {
        return GetFileNameA(path);
    }

    std::string GetExtensionA(const std::string &path) {
        std::string fileName = GetFileNameA(path);
        size_t pos = fileName.find_last_of('.');
        if (pos == std::string::npos)
            return "";

        return fileName.substr(pos);
    }

    std::wstring GetExtensionW(const std::wstring &path) {
        std::wstring fileName = GetFileNameW(path);
        size_t pos = fileName.find_last_of(L'.');
        if (pos == std::wstring::npos)
            return L"";

        return fileName.substr(pos);
    }

    std::string GetExtensionUtf8(const std::string &path) {
        return GetExtensionA(path);
    }

    std::string RemoveExtensionA(const std::string &path) {
        size_t lastDot = path.find_last_of('.');
        size_t lastSlash = path.find_last_of("/\\");

        // If there's no dot, return the original path
        if (lastDot == std::string::npos)
            return path;

        // If dot is before the last slash, it's part of a directory name
        if (lastSlash != std::string::npos && lastDot < lastSlash)
            return path;

        return path.substr(0, lastDot);
    }

    std::wstring RemoveExtensionW(const std::wstring &path) {
        size_t lastDot = path.find_last_of(L'.');
        size_t lastSlash = path.find_last_of(L"/\\");

        // If there's no dot, return the original path
        if (lastDot == std::wstring::npos)
            return path;

        // If dot is before the last slash, it's part of a directory name
        if (lastSlash != std::wstring::npos && lastDot < lastSlash)
            return path;

        return path.substr(0, lastDot);
    }

    std::string RemoveExtensionUtf8(const std::string &path) {
        return RemoveExtensionA(path);
    }

    std::string CombinePathA(const std::string &path1, const std::string &path2) {
        if (path1.empty())
            return path2;
        if (path2.empty())
            return path1;

        char lastChar = path1[path1.size() - 1];
        char firstChar = path2[0];

        if ((lastChar == '/' || lastChar == '\\') && (firstChar == '/' || firstChar == '\\'))
            return path1 + path2.substr(1);
        else if (lastChar != '/' && lastChar != '\\' && firstChar != '/' && firstChar != '\\')
            return path1 + '\\' + path2;
        else
            return path1 + path2;
    }

    std::wstring CombinePathW(const std::wstring &path1, const std::wstring &path2) {
        if (path1.empty())
            return path2;
        if (path2.empty())
            return path1;

        wchar_t lastChar = path1[path1.size() - 1];
        wchar_t firstChar = path2[0];

        if ((lastChar == L'/' || lastChar == L'\\') && (firstChar == L'/' || firstChar == L'\\'))
            return path1 + path2.substr(1);
        else if (lastChar != L'/' && lastChar != L'\\' && firstChar != L'/' && firstChar != L'\\')
            return path1 + L'\\' + path2;
        else
            return path1 + path2;
    }

    std::string CombinePathUtf8(const std::string &path1, const std::string &path2) {
        return CombinePathA(path1, path2);
    }

    std::string NormalizePathA(const std::string &path) {
        if (path.empty())
            return "";

        std::string result = path;

        // Convert forward slashes to backslashes
        std::replace(result.begin(), result.end(), '/', '\\');

        // Remove duplicate backslashes
        auto it = std::unique(result.begin(), result.end(),
                              [](char a, char b) { return a == '\\' && b == '\\'; });
        result.erase(it, result.end());

        return result;
    }

    std::wstring NormalizePathW(const std::wstring &path) {
        if (path.empty())
            return L"";

        std::wstring result = path;

        // Convert forward slashes to backslashes
        std::replace(result.begin(), result.end(), L'/', L'\\');

        // Remove duplicate backslashes
        auto it = std::unique(result.begin(), result.end(),
                              [](wchar_t a, wchar_t b) { return a == L'\\' && b == L'\\'; });
        result.erase(it, result.end());

        return result;
    }

    std::string NormalizePathUtf8(const std::string &path) {
        return NormalizePathA(path);
    }

    // Path validation
    bool IsPathValidA(const std::string &path) {
        static const char *invalidChars = "<>:\"|?*";
        return path.find_first_of(invalidChars) == std::string::npos;
    }

    bool IsPathValidW(const std::wstring &path) {
        static const wchar_t *invalidChars = L"<>:\"|?*";
        return path.find_first_of(invalidChars) == std::wstring::npos;
    }

    bool IsPathValidUtf8(const std::string &path) {
        return IsPathValidA(path);
    }

    // Path type checks
    bool IsAbsolutePathA(const std::string &path) {
        if (path.empty())
            return false;

        // Case 1: Starts with drive letter (e.g., "C:\path")
        if (path.size() >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
            return true;

        // Case 2: UNC path (e.g., "\\server\share")
        if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\')
            return true;

        return false;
    }

    bool IsAbsolutePathW(const std::wstring &path) {
        if (path.empty())
            return false;

        // Case 1: Starts with drive letter (e.g., "C:\path")
        if (path.size() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/'))
            return true;

        // Case 2: UNC path (e.g., "\\server\share")
        if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\')
            return true;

        return false;
    }

    bool IsAbsolutePathUtf8(const std::string &path) {
        return IsAbsolutePathA(path);
    }

    bool IsRelativePathA(const std::string &path) {
        return !IsAbsolutePathA(path);
    }

    bool IsRelativePathW(const std::wstring &path) {
        return !IsAbsolutePathW(path);
    }

    bool IsRelativePathUtf8(const std::string &path) {
        return !IsAbsolutePathUtf8(path);
    }

    bool IsPathRootedA(const std::string &path) {
        if (path.empty())
            return false;

        // Case 1: Starts with drive letter
        if (path.size() >= 2 && path[1] == ':')
            return true;

        // Case 2: Starts with a slash
        if (path[0] == '\\' || path[0] == '/')
            return true;

        return false;
    }

    bool IsPathRootedW(const std::wstring &path) {
        if (path.empty())
            return false;

        // Case 1: Starts with drive letter
        if (path.size() >= 2 && path[1] == L':')
            return true;

        // Case 2: Starts with a slash
        if (path[0] == L'\\' || path[0] == L'/')
            return true;

        return false;
    }

    bool IsPathRootedUtf8(const std::string &path) {
        return IsPathRootedA(path);
    }

    // Path resolution
    std::string ResolvePathA(const std::string &path) {
        std::string normalizedPath = NormalizePathA(path);
        std::vector<std::string> parts;

        size_t start = 0;
        size_t pos = normalizedPath.find('\\');

        // Handle root
        std::string root;
        if (normalizedPath.size() >= 2 && normalizedPath[1] == ':') {
            // Drive letter root
            root = normalizedPath.substr(0, 2) + '\\';
            start = 3; // Skip drive letter and first slash
        } else if (normalizedPath.size() >= 2 && normalizedPath[0] == '\\' && normalizedPath[1] == '\\') {
            // UNC path, find server and share
            size_t serverEnd = normalizedPath.find('\\', 2);
            if (serverEnd != std::string::npos) {
                size_t shareEnd = normalizedPath.find('\\', serverEnd + 1);
                if (shareEnd != std::string::npos) {
                    root = normalizedPath.substr(0, shareEnd + 1);
                    start = shareEnd + 1;
                } else {
                    // Just the server\share with no trailing slash
                    return normalizedPath;
                }
            } else {
                // Just the server with no trailing slash
                return normalizedPath;
            }
        } else if (!normalizedPath.empty() && (normalizedPath[0] == '\\' || normalizedPath[0] == '/')) {
            // Root slash
            root = "\\";
            start = 1;
        }

        // Process path segments
        while (start < normalizedPath.size()) {
            pos = normalizedPath.find('\\', start);
            if (pos == std::string::npos) {
                pos = normalizedPath.size();
            }

            std::string segment = normalizedPath.substr(start, pos - start);

            if (segment == ".") {
                // Skip current directory
            } else if (segment == "..") {
                // Go up one level
                if (!parts.empty()) {
                    parts.pop_back();
                }
            } else if (!segment.empty()) {
                parts.push_back(segment);
            }

            start = pos + 1;
        }

        // Reconstruct the path
        std::string resolvedPath = root;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                resolvedPath += '\\';
            }
            resolvedPath += parts[i];
        }

        // If the path was absolute and now it's empty, return the root
        if (resolvedPath.empty() && !root.empty()) {
            return root;
        }

        return resolvedPath;
    }

    std::wstring ResolvePathW(const std::wstring &path) {
        std::wstring normalizedPath = NormalizePathW(path);
        std::vector<std::wstring> parts;

        size_t start = 0;
        size_t pos = normalizedPath.find(L'\\');

        // Handle root
        std::wstring root;
        if (normalizedPath.size() >= 2 && normalizedPath[1] == L':') {
            // Drive letter root
            root = normalizedPath.substr(0, 2) + L'\\';
            start = 3; // Skip drive letter and first slash
        } else if (normalizedPath.size() >= 2 && normalizedPath[0] == L'\\' && normalizedPath[1] == L'\\') {
            // UNC path, find server and share
            size_t serverEnd = normalizedPath.find(L'\\', 2);
            if (serverEnd != std::wstring::npos) {
                size_t shareEnd = normalizedPath.find(L'\\', serverEnd + 1);
                if (shareEnd != std::wstring::npos) {
                    root = normalizedPath.substr(0, shareEnd + 1);
                    start = shareEnd + 1;
                } else {
                    // Just the server\share with no trailing slash
                    return normalizedPath;
                }
            } else {
                // Just the server with no trailing slash
                return normalizedPath;
            }
        } else if (!normalizedPath.empty() && (normalizedPath[0] == L'\\' || normalizedPath[0] == L'/')) {
            // Root slash
            root = L"\\";
            start = 1;
        }

        // Process path segments
        while (start < normalizedPath.size()) {
            pos = normalizedPath.find(L'\\', start);
            if (pos == std::wstring::npos) {
                pos = normalizedPath.size();
            }

            std::wstring segment = normalizedPath.substr(start, pos - start);

            if (segment == L".") {
                // Skip current directory
            } else if (segment == L"..") {
                // Go up one level
                if (!parts.empty()) {
                    parts.pop_back();
                }
            } else if (!segment.empty()) {
                parts.push_back(segment);
            }

            start = pos + 1;
        }

        // Reconstruct the path
        std::wstring resolvedPath = root;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                resolvedPath += L'\\';
            }
            resolvedPath += parts[i];
        }

        // If the path was absolute and now it's empty, return the root
        if (resolvedPath.empty() && !root.empty()) {
            return root;
        }

        return resolvedPath;
    }

    std::string ResolvePathUtf8(const std::string &path) {
        return ResolvePathA(path);
    }

    std::string MakeRelativePathA(const std::string &path, const std::string &basePath) {
        // Resolve both paths to canonical form
        std::string resolvedPath = ResolvePathA(path);
        std::string resolvedBasePath = ResolvePathA(basePath);

        // Make sure paths are absolute and have the same root
        if (!IsAbsolutePathA(resolvedPath) || !IsAbsolutePathA(resolvedBasePath)) {
            return resolvedPath; // Can't make relative if either is already relative
        }

        std::string pathDrive = GetDriveA(resolvedPath);
        std::string baseDrive = GetDriveA(resolvedBasePath);

        if (pathDrive != baseDrive) {
            return resolvedPath; // Can't make relative across different drives
        }

        // Split paths into components
        std::vector<std::string> pathComponents;
        std::vector<std::string> baseComponents;

        size_t pathStart = resolvedPath.find('\\', pathDrive.size()) + 1;
        size_t baseStart = resolvedBasePath.find('\\', baseDrive.size()) + 1;

        while (pathStart < resolvedPath.size()) {
            size_t nextSlash = resolvedPath.find('\\', pathStart);
            if (nextSlash == std::string::npos) {
                pathComponents.push_back(resolvedPath.substr(pathStart));
                break;
            }
            pathComponents.push_back(resolvedPath.substr(pathStart, nextSlash - pathStart));
            pathStart = nextSlash + 1;
        }

        while (baseStart < resolvedBasePath.size()) {
            size_t nextSlash = resolvedBasePath.find('\\', baseStart);
            if (nextSlash == std::string::npos) {
                baseComponents.push_back(resolvedBasePath.substr(baseStart));
                break;
            }
            baseComponents.push_back(resolvedBasePath.substr(baseStart, nextSlash - baseStart));
            baseStart = nextSlash + 1;
        }

        // Find common prefix
        size_t commonLength = 0;
        size_t minLength = std::min(pathComponents.size(), baseComponents.size());

        while (commonLength < minLength &&
            _stricmp(pathComponents[commonLength].c_str(), baseComponents[commonLength].c_str()) == 0) {
            commonLength++;
        }

        // Build relative path
        std::string relativePath;

        // Add ".." for each remaining component in the base path
        for (size_t i = commonLength; i < baseComponents.size(); ++i) {
            if (!relativePath.empty()) {
                relativePath += '\\';
            }
            relativePath += "..";
        }

        // Add remaining components from the path
        for (size_t i = commonLength; i < pathComponents.size(); ++i) {
            if (!relativePath.empty()) {
                relativePath += '\\';
            }
            relativePath += pathComponents[i];
        }

        if (relativePath.empty()) {
            relativePath = ".";
        }

        return relativePath;
    }

    std::wstring MakeRelativePathW(const std::wstring &path, const std::wstring &basePath) {
        // Resolve both paths to canonical form
        std::wstring resolvedPath = ResolvePathW(path);
        std::wstring resolvedBasePath = ResolvePathW(basePath);

        // Make sure paths are absolute and have the same root
        if (!IsAbsolutePathW(resolvedPath) || !IsAbsolutePathW(resolvedBasePath)) {
            return resolvedPath; // Can't make relative if either is already relative
        }

        std::wstring pathDrive = GetDriveW(resolvedPath);
        std::wstring baseDrive = GetDriveW(resolvedBasePath);

        if (pathDrive != baseDrive) {
            return resolvedPath; // Can't make relative across different drives
        }

        // Split paths into components
        std::vector<std::wstring> pathComponents;
        std::vector<std::wstring> baseComponents;

        size_t pathStart = resolvedPath.find(L'\\', pathDrive.size()) + 1;
        size_t baseStart = resolvedBasePath.find(L'\\', baseDrive.size()) + 1;

        while (pathStart < resolvedPath.size()) {
            size_t nextSlash = resolvedPath.find(L'\\', pathStart);
            if (nextSlash == std::wstring::npos) {
                pathComponents.push_back(resolvedPath.substr(pathStart));
                break;
            }
            pathComponents.push_back(resolvedPath.substr(pathStart, nextSlash - pathStart));
            pathStart = nextSlash + 1;
        }

        while (baseStart < resolvedBasePath.size()) {
            size_t nextSlash = resolvedBasePath.find(L'\\', baseStart);
            if (nextSlash == std::wstring::npos) {
                baseComponents.push_back(resolvedBasePath.substr(baseStart));
                break;
            }
            baseComponents.push_back(resolvedBasePath.substr(baseStart, nextSlash - baseStart));
            baseStart = nextSlash + 1;
        }

        // Find common prefix
        size_t commonLength = 0;
        size_t minLength = std::min(pathComponents.size(), baseComponents.size());

        while (commonLength < minLength &&
            _wcsicmp(pathComponents[commonLength].c_str(), baseComponents[commonLength].c_str()) == 0) {
            commonLength++;
        }

        // Build relative path
        std::wstring relativePath;

        // Add ".." for each remaining component in the base path
        for (size_t i = commonLength; i < baseComponents.size(); ++i) {
            if (!relativePath.empty()) {
                relativePath += L'\\';
            }
            relativePath += L"..";
        }

        // Add remaining components from the path
        for (size_t i = commonLength; i < pathComponents.size(); ++i) {
            if (!relativePath.empty()) {
                relativePath += L'\\';
            }
            relativePath += pathComponents[i];
        }

        if (relativePath.empty()) {
            relativePath = L".";
        }

        return relativePath;
    }

    std::string MakeRelativePathUtf8(const std::string &path, const std::string &basePath) {
        return MakeRelativePathA(path, basePath);
    }

    // System paths
    std::string GetTempPathA() {
        char buffer[MAX_PATH + 1];
        DWORD length = ::GetTempPathA(MAX_PATH, buffer);
        if (length == 0) {
            return "";
        }
        return std::string(buffer, length);
    }

    std::wstring GetTempPathW() {
        wchar_t buffer[MAX_PATH + 1];
        DWORD length = ::GetTempPathW(MAX_PATH, buffer);
        if (length == 0) {
            return L"";
        }
        return std::wstring(buffer, length);
    }

    std::string GetTempPathUtf8() {
        return Utf16ToUtf8(GetTempPathW());
    }

    std::string GetCurrentDirectoryA() {
        char buffer[MAX_PATH + 1];
        DWORD length = ::GetCurrentDirectoryA(MAX_PATH, buffer);
        if (length == 0) {
            return "";
        }
        return std::string(buffer, length);
    }

    std::wstring GetCurrentDirectoryW() {
        wchar_t buffer[MAX_PATH + 1];
        DWORD length = ::GetCurrentDirectoryW(MAX_PATH, buffer);
        if (length == 0) {
            return L"";
        }
        return std::wstring(buffer, length);
    }

    std::string GetCurrentDirectoryUtf8() {
        return Utf16ToUtf8(GetCurrentDirectoryW());
    }

    bool SetCurrentDirectoryA(const std::string &path) {
        if (path.empty()) {
            return false;
        }
        return ::SetCurrentDirectoryA(path.c_str()) == TRUE;
    }

    bool SetCurrentDirectoryW(const std::wstring &path) {
        if (path.empty()) {
            return false;
        }
        return ::SetCurrentDirectoryW(path.c_str()) == TRUE;
    }

    bool SetCurrentDirectoryUtf8(const std::string &path) {
        return SetCurrentDirectoryW(Utf8ToUtf16(path));
    }

    std::string GetExecutablePathA() {
        char buffer[MAX_PATH + 1];
        DWORD length = ::GetModuleFileNameA(NULL, buffer, MAX_PATH);
        if (length == 0) {
            return "";
        }
        return std::string(buffer, length);
    }

    std::wstring GetExecutablePathW() {
        wchar_t buffer[MAX_PATH + 1];
        DWORD length = ::GetModuleFileNameW(NULL, buffer, MAX_PATH);
        if (length == 0) {
            return L"";
        }
        return std::wstring(buffer, length);
    }

    std::string GetExecutablePathUtf8() {
        return Utf16ToUtf8(GetExecutablePathW());
    }

    // File properties
    int64_t GetFileSizeA(const std::string &path) {
        if (!FileExistsA(path)) {
            return -1;
        }

        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        if (!::GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
            return -1;
        }

        LARGE_INTEGER size;
        size.LowPart = fileInfo.nFileSizeLow;
        size.HighPart = fileInfo.nFileSizeHigh;
        return size.QuadPart;
    }

    int64_t GetFileSizeW(const std::wstring &path) {
        if (!FileExistsW(path)) {
            return -1;
        }

        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
            return -1;
        }

        LARGE_INTEGER size;
        size.LowPart = fileInfo.nFileSizeLow;
        size.HighPart = fileInfo.nFileSizeHigh;
        return size.QuadPart;
    }

    int64_t GetFileSizeUtf8(const std::string &path) {
        return GetFileSizeW(Utf8ToUtf16(path));
    }

    FileTime GetFileTimeA(const std::string &path) {
        FileTime result = {0, 0, 0};

        if (!PathExistsA(path)) {
            return result;
        }

        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        if (!::GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
            return result;
        }

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

    FileTime GetFileTimeW(const std::wstring &path) {
        FileTime result = {0, 0, 0};

        if (!PathExistsW(path)) {
            return result;
        }

        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileInfo)) {
            return result;
        }

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

    // File I/O
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
        // Create directory structure if needed
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
        // Create directory structure if needed
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

        // Get file size
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Read file content
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

        // Get file size
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        // Read file content
        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (fread(buffer.data(), 1, size, file) == static_cast<size_t>(size))
            return buffer;

        return {};
    }

    std::vector<uint8_t> ReadBinaryFileUtf8(const std::string &path) {
        return ReadBinaryFileW(Utf8ToUtf16(path));
    }

    bool WriteBinaryFileA(const std::string &path, const std::vector<uint8_t> &data) {
        // Create directory structure if needed
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
        // Create directory structure if needed
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

    // Create temporary files
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

    // Directory listing
    std::vector<std::string> ListFilesA(const std::string &dir, const std::string &pattern) {
        std::vector<std::string> files;

        if (!DirectoryExistsA(dir))
            return files;

        std::string searchPath = CombinePathA(dir, pattern);
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

        if (hFind == INVALID_HANDLE_VALUE)
            return files;

        do {
            // Skip directories
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                files.push_back(findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData));

        FindClose(hFind);
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
            // Skip directories
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
        std::vector<std::string> directories;

        if (!DirectoryExistsA(dir))
            return directories;

        std::string searchPath = CombinePathA(dir, pattern);
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

        if (hFind == INVALID_HANDLE_VALUE)
            return directories;

        do {
            // Skip files and special directories
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                strcmp(findData.cFileName, ".") != 0 &&
                strcmp(findData.cFileName, "..") != 0) {
                directories.push_back(findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData));

        FindClose(hFind);
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
            // Skip files and special directories
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
