#include "PathUtils.h"

#include <algorithm>

#include <direct.h>
#include <sys/stat.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include "StringUtils.h"

namespace utils {
    std::string GetCurrentDir() {
        char buf[MAX_PATH];
        getcwd(buf, MAX_PATH);
        return buf;
    }

    std::string GetFileName(const std::string &path) {
        const char *const lastSep = FindLastPathSeparator(path);
        std::string name = (!lastSep) ? path : lastSep + 1;
        return RemoveExtension(name, "");;
    }

    std::string RemoveFileName(const std::string &path) {
        const char *const lastSep = FindLastPathSeparator(path);
        if (!lastSep) return ".\\";
        return {path.c_str(), static_cast<size_t>(lastSep + 1 - path.c_str())};
    }

    std::string RemoveExtension(const std::string &path, const std::string &ext) {
        if (ext.empty() || ext == "*") {
            auto dot = path.rfind('.');
            if (dot != std::string::npos)
                return path.substr(0, path.rfind('.'));
        } else {
            const std::string de = std::string(".") + ext;
            if (EndsWithCaseInsensitive(path, de)) {
                return path.substr(0, path.length() - de.length());
            }
        }
        return path;
    }

    std::string JoinPaths(const std::string &path1, const std::string &path2) {
        if (path1.empty()) return path2;
        return RemoveTrailingPathSeparator(path1) + "\\" + path2;
    }

    std::string MakeFileName(const std::string &dir, const std::string &name, const std::string &ext) {
        return JoinPaths(dir, name + "." + ext);
    }

    bool FileExists(const std::string &file) {
        if (!file.empty()) {
            struct stat fstat = {0};
            memset(&fstat, 0, sizeof(struct stat));
            return stat(file.c_str(), &fstat) == 0 && (fstat.st_mode & S_IFREG);
        }
        return false;
    }

    bool DirectoryExists(const std::string &dir) {
        if (!dir.empty()) {
            struct stat fstat = {0};
            memset(&fstat, 0, sizeof(struct stat));
            return stat(dir.c_str(), &fstat) == 0 && (fstat.st_mode & S_IFDIR);
        }
        return false;
    }

    bool IsAbsolutePath(const std::string &path) {
        if (path.empty()) return false;
        if (path.size() < 2 || !isalpha(path[0]) || path[1] != ':')
            return false;
        return true;
    }

    std::string GetAbsolutePath(const std::string &path, bool trailing) {
        if (path.empty() || IsAbsolutePath(path))
            return path;

        std::vector<char> buf(MAX_PATH);
        ::GetCurrentDirectoryA(MAX_PATH, buf.data());
        std::string absPath(buf.data());
        absPath += '\\';
        absPath.append(path);

        if (HasTrailingPathSeparator(absPath) && !trailing) {
            return absPath.substr(0, absPath.length() - 1);
        } else if (!HasTrailingPathSeparator(absPath) && trailing) {
            return absPath += '\\';
        } else {
            return absPath;
        }
    }

    bool CreateDir(const std::string &dir) {
        int ret = _mkdir(dir.c_str());
        if (ret == -1)
            return DirectoryExists(dir);
        return true;
    }

    bool CreateFileTree(const std::string &file) {
        if (file.size() < 3)
            return false;

        std::string tree = file;
        for (char *pch = &tree[3]; *pch != '\0'; ++pch) {
            if (*pch != '/' && *pch != '\\')
                continue;
            *pch = '\0';
            if (::GetFileAttributesA(tree.c_str()) == -1)
                if (!::CreateDirectoryA(tree.c_str(), nullptr))
                    break;
            *pch = '\\';
        }
        return true;
    }

    bool RemoveDir(const std::string &dir) {
        if (DirectoryExists(dir)) {
            return ::RemoveDirectoryA(dir.c_str()) == TRUE;
        }
        return false;
    }

    const char *FindLastPathSeparator(const std::string &path) {
        const char *const lastSep = strrchr(path.c_str(), '\\');
        const char *const lastAltSep = strrchr(path.c_str(), '/');
        return (lastAltSep && (!lastSep || lastAltSep > lastSep)) ? lastAltSep : lastSep;
    }

    bool HasTrailingPathSeparator(const std::string &path) {
        return !path.empty() && EndsWith(path, "\\");
    }

    std::string RemoveTrailingPathSeparator(const std::string &path) {
        return HasTrailingPathSeparator(path) ? path.substr(0, path.length() - 1) : path;
    }

    void NormalizePath(std::string &path) {
        auto out = path.begin();
        for (const char ch: path) {
            if (!(ch == '\\' || ch == '/')) {
                *(out++) = ch;
            } else if (out == path.begin() || *std::prev(out) != '\\') {
                *(out++) = '\\';
            }
        }
        path.erase(out, path.end());
    }
}
