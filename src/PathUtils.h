#ifndef BML_PATHUTILS_H
#define BML_PATHUTILS_H

#include <string>

namespace utils {
    bool FileExistsA(const std::string &file);
    bool FileExistsW(const std::wstring &file);
    bool FileExistsUtf8(const std::string &file);
    bool DirectoryExistsA(const std::string &dir);
    bool DirectoryExistsW(const std::wstring &dir);
    bool DirectoryExistsUtf8(const std::string &file);

    bool CreateDirA(const std::string &dir);
    bool CreateDirW(const std::wstring &dir);
    bool CreateDirUtf8(const std::wstring &dir);

    bool CreateFileTreeA(const std::string &file);
    bool CreateFileTreeW(const std::wstring &file);
    bool CreateFileTreeUtf8(const std::string &file);

    bool DeleteFileA(const std::string &path);
    bool DeleteFileW(const std::wstring &path);
    bool DeleteFileUtf8(const std::string &path);

    bool DeleteDirA(const std::string &path);
    bool DeleteDirW(const std::wstring &path);
    bool DeleteDirUtf8(const std::string &path);

    bool DuplicateFileA(const std::string &path, const std::string &dest);
    bool DuplicateFileW(const std::wstring &path, const std::wstring &dest);
    bool DuplicateFileUtf8(const std::string &path, const std::string &dest);

    bool ExtractZipW(const std::wstring &path, const std::wstring &dest);
}

#endif // BML_PATHUTILS_H
