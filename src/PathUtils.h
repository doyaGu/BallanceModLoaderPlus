#ifndef BML_PATHUTILS_H
#define BML_PATHUTILS_H

#include <string>

namespace utils {
    bool FileExists(const std::string &file);
    bool FileExists(const std::wstring &file);
    bool DirectoryExists(const std::string &dir);
    bool DirectoryExists(const std::wstring &dir);

    bool CreateDir(const std::string &dir);
    bool CreateDir(const std::wstring &dir);

    bool CreateFileTree(const std::string &file);
    bool CreateFileTree(const std::wstring &file);

    bool DeleteDir(const std::wstring &path);

    bool DuplicateFile(const std::wstring &path, const std::wstring &dest);

    bool ExtractZip(const std::wstring &path, const std::wstring &dest);
}

#endif // BML_PATHUTILS_H
