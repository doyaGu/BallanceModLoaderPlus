#ifndef BML_PATHUTILS_H
#define BML_PATHUTILS_H

#include <string>

namespace utils {
    std::string GetCurrentDir();

    std::string GetFileName(const std::string &path);

    std::string RemoveFileName(const std::string &path);
    std::string RemoveExtension(const std::string &path, const std::string &ext);

    std::string JoinPaths(const std::string &path1, const std::string &path2);
    std::string MakeFileName(const std::string &dir, const std::string &name, const std::string &ext);

    bool FileExists(const std::string &file);
    bool DirectoryExists(const std::string &dir);

    bool IsAbsolutePath(const std::string &path);
    std::string GetAbsolutePath(const std::string &path, bool trailing = false);

    bool CreateDir(const std::string &dir);
    bool CreateFileTree(const std::string &file);

    bool RemoveDir(const std::string &dir);

    const char *FindLastPathSeparator(const std::string &path);
    bool HasTrailingPathSeparator(const std::string &path);
    std::string RemoveTrailingPathSeparator(const std::string &path);
    void NormalizePath(std::string &path);
}

#endif // BML_PATHUTILS_H
