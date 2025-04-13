#ifndef BML_PATHUTILS_H
#define BML_PATHUTILS_H

#include <string>
#include <vector>

namespace utils {
    // File existence checks
    bool FileExistsA(const std::string &file);
    bool FileExistsW(const std::wstring &file);
    bool FileExistsUtf8(const std::string &file);

    // Directory existence checks
    bool DirectoryExistsA(const std::string &dir);
    bool DirectoryExistsW(const std::wstring &dir);
    bool DirectoryExistsUtf8(const std::string &dir);

    // Path existence checks (either file or directory)
    bool PathExistsA(const std::string &path);
    bool PathExistsW(const std::wstring &path);
    bool PathExistsUtf8(const std::string &path);

    // Directory creation
    bool CreateDirectoryA(const std::string &dir);
    bool CreateDirectoryW(const std::wstring &dir);
    bool CreateDirectoryUtf8(const std::string &dir);

    // Create directory tree (creates all directories in path)
    bool CreateFileTreeA(const std::string &path);
    bool CreateFileTreeW(const std::wstring &path);
    bool CreateFileTreeUtf8(const std::string &path);

    // File deletion
    bool DeleteFileA(const std::string &path);
    bool DeleteFileW(const std::wstring &path);
    bool DeleteFileUtf8(const std::string &path);

    // Directory deletion (recursive)
    bool DeleteDirectoryA(const std::string &path);
    bool DeleteDirectoryW(const std::wstring &path);
    bool DeleteDirectoryUtf8(const std::string &path);

    // File copying
    bool CopyFileA(const std::string &path, const std::string &dest);
    bool CopyFileW(const std::wstring &path, const std::wstring &dest);
    bool CopyFileUtf8(const std::string &path, const std::string &dest);

    // File moving/renaming
    bool MoveFileA(const std::string &path, const std::string &dest);
    bool MoveFileW(const std::wstring &path, const std::wstring &dest);
    bool MoveFileUtf8(const std::string &path, const std::string &dest);

    // Zip extraction
    bool ExtractZipA(const std::string &path, const std::string &dest);
    bool ExtractZipW(const std::wstring &path, const std::wstring &dest);
    bool ExtractZipUtf8(const std::string &path, const std::string &dest);

    // Path manipulation
    std::string GetDriveA(const std::string &path);
    std::wstring GetDriveW(const std::wstring &path);
    std::string GetDriveUtf8(const std::string &path);

    std::string GetDirectoryA(const std::string &path);
    std::wstring GetDirectoryW(const std::wstring &path);
    std::string GetDirectoryUtf8(const std::string &path);

    std::pair<std::string, std::string> GetDriveAndDirectoryA(const std::string &path);
    std::pair<std::wstring, std::wstring> GetDriveAndDirectoryW(const std::wstring &path);
    std::pair<std::string, std::string> GetDriveAndDirectoryUtf8(const std::string &path);

    std::string GetFileNameA(const std::string &path);
    std::wstring GetFileNameW(const std::wstring &path);
    std::string GetFileNameUtf8(const std::string &path);

    std::string GetExtensionA(const std::string &path);
    std::wstring GetExtensionW(const std::wstring &path);
    std::string GetExtensionUtf8(const std::string &path);

    std::string RemoveExtensionA(const std::string &path);
    std::wstring RemoveExtensionW(const std::wstring &path);
    std::string RemoveExtensionUtf8(const std::string &path);

    std::string CombinePathA(const std::string &path1, const std::string &path2);
    std::wstring CombinePathW(const std::wstring &path1, const std::wstring &path2);
    std::string CombinePathUtf8(const std::string &path1, const std::string &path2);

    std::string NormalizePathA(const std::string &path);
    std::wstring NormalizePathW(const std::wstring &path);
    std::string NormalizePathUtf8(const std::string &path);

    // Path validation
    bool IsPathValidA(const std::string &path);
    bool IsPathValidW(const std::wstring &path);
    bool IsPathValidUtf8(const std::string &path);

    // Path type checks
    bool IsAbsolutePathA(const std::string &path);
    bool IsAbsolutePathW(const std::wstring &path);
    bool IsAbsolutePathUtf8(const std::string &path);

    bool IsRelativePathA(const std::string &path);
    bool IsRelativePathW(const std::wstring &path);
    bool IsRelativePathUtf8(const std::string &path);

    bool IsPathRootedA(const std::string &path);
    bool IsPathRootedW(const std::wstring &path);
    bool IsPathRootedUtf8(const std::string &path);

    // Path resolution
    std::string ResolvePathA(const std::string &path);
    std::wstring ResolvePathW(const std::wstring &path);
    std::string ResolvePathUtf8(const std::string &path);

    std::string MakeRelativePathA(const std::string &path, const std::string &basePath);
    std::wstring MakeRelativePathW(const std::wstring &path, const std::wstring &basePath);
    std::string MakeRelativePathUtf8(const std::string &path, const std::string &basePath);

    // System paths
    std::string GetTempPathA();
    std::wstring GetTempPathW();
    std::string GetTempPathUtf8();

    std::string GetCurrentDirectoryA();
    std::wstring GetCurrentDirectoryW();
    std::string GetCurrentDirectoryUtf8();

    bool SetCurrentDirectoryA(const std::string &path);
    bool SetCurrentDirectoryW(const std::wstring &path);
    bool SetCurrentDirectoryUtf8(const std::string &path);

    std::string GetExecutablePathA();
    std::wstring GetExecutablePathW();
    std::string GetExecutablePathUtf8();

    // File properties
    int64_t GetFileSizeA(const std::string &path);
    int64_t GetFileSizeW(const std::wstring &path);
    int64_t GetFileSizeUtf8(const std::string &path);

    struct FileTime {
        int64_t creationTime;
        int64_t lastAccessTime;
        int64_t lastWriteTime;
    };

    FileTime GetFileTimeA(const std::string &path);
    FileTime GetFileTimeW(const std::wstring &path);
    FileTime GetFileTimeUtf8(const std::string &path);

    // File I/O
    std::string ReadTextFileA(const std::string &path);
    std::wstring ReadTextFileW(const std::wstring &path);
    std::string ReadTextFileUtf8(const std::string &path);

    bool WriteTextFileA(const std::string &path, const std::string &content);
    bool WriteTextFileW(const std::wstring &path, const std::wstring &content);
    bool WriteTextFileUtf8(const std::string &path, const std::string &content);

    std::vector<uint8_t> ReadBinaryFileA(const std::string &path);
    std::vector<uint8_t> ReadBinaryFileW(const std::wstring &path);
    std::vector<uint8_t> ReadBinaryFileUtf8(const std::string &path);

    bool WriteBinaryFileA(const std::string &path, const std::vector<uint8_t> &data);
    bool WriteBinaryFileW(const std::wstring &path, const std::vector<uint8_t> &data);
    bool WriteBinaryFileUtf8(const std::string &path, const std::vector<uint8_t> &data);

    // Create temporary files
    std::string CreateTempFileA(const std::string &prefix = "");
    std::wstring CreateTempFileW(const std::wstring &prefix = L"");
    std::string CreateTempFileUtf8(const std::string &prefix = "");

    // Directory listing
    std::vector<std::string> ListFilesA(const std::string &dir, const std::string &pattern = "*");
    std::vector<std::wstring> ListFilesW(const std::wstring &dir, const std::wstring &pattern = L"*");
    std::vector<std::string> ListFilesUtf8(const std::string &dir, const std::string &pattern = "*");

    std::vector<std::string> ListDirectoriesA(const std::string &dir, const std::string &pattern = "*");
    std::vector<std::wstring> ListDirectoriesW(const std::wstring &dir, const std::wstring &pattern = L"*");
    std::vector<std::string> ListDirectoriesUtf8(const std::string &dir, const std::string &pattern = "*");
}

#endif // BML_PATHUTILS_H
