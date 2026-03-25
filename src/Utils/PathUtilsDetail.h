#ifndef BML_PATHUTILSDETAIL_H
#define BML_PATHUTILSDETAIL_H

// Internal detail header — included only by PathUtils.cpp.
// Provides template implementations for pure-string path manipulation
// that is identical across char/wchar_t, eliminating A/W duplication.

#include <algorithm>
#include <string>
#include <vector>

#ifdef _WIN32
#include <cstring> // _stricmp, _wcsicmp
#endif

namespace utils::detail {

    template <typename CharT>
    struct PathCharTraits;

    template <>
    struct PathCharTraits<char> {
        static constexpr char Colon = ':';
        static constexpr char BackSlash = '\\';
        static constexpr char ForwardSlash = '/';
        static constexpr char Dot = '.';
        static constexpr const char *Separators = "/\\";
        static constexpr const char *BackSlashStr = "\\";
        static constexpr const char *DotStr = ".";
        static constexpr const char *DotDotStr = "..";
        static constexpr const char *EmptyStr = "";
        static constexpr const char *InvalidPathChars = "<>:\"|?*";

        static int CompareIgnoreCase(const char *a, const char *b) {
#ifdef _WIN32
            return _stricmp(a, b);
#else
            return strcasecmp(a, b);
#endif
        }
    };

    template <>
    struct PathCharTraits<wchar_t> {
        static constexpr wchar_t Colon = L':';
        static constexpr wchar_t BackSlash = L'\\';
        static constexpr wchar_t ForwardSlash = L'/';
        static constexpr wchar_t Dot = L'.';
        static constexpr const wchar_t *Separators = L"/\\";
        static constexpr const wchar_t *BackSlashStr = L"\\";
        static constexpr const wchar_t *DotStr = L".";
        static constexpr const wchar_t *DotDotStr = L"..";
        static constexpr const wchar_t *EmptyStr = L"";
        static constexpr const wchar_t *InvalidPathChars = L"<>:\"|?*";

        static int CompareIgnoreCase(const wchar_t *a, const wchar_t *b) {
#ifdef _WIN32
            return _wcsicmp(a, b);
#else
            return wcscasecmp(a, b);
#endif
        }
    };

    template <typename CharT>
    using StringT = std::basic_string<CharT>;

    template <typename CharT>
    StringT<CharT> GetDriveImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        if (path.size() >= 2 && path[1] == T::Colon)
            return path.substr(0, 2);
        return StringT<CharT>();
    }

    template <typename CharT>
    StringT<CharT> GetDirectoryImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        if (path.empty())
            return StringT<CharT>();

        size_t pos = path.find_last_of(T::Separators);
        if (pos == StringT<CharT>::npos)
            return StringT<CharT>();

        return path.substr(0, pos);
    }

    template <typename CharT>
    std::pair<StringT<CharT>, StringT<CharT>> GetDriveAndDirectoryImpl(const StringT<CharT> &path) {
        auto drive = GetDriveImpl<CharT>(path);
        auto dir = GetDirectoryImpl<CharT>(path);

        if (!drive.empty() && dir.size() >= drive.size() &&
            dir.substr(0, drive.size()) == drive) {
            dir = dir.substr(drive.size());
        }

        return {drive, dir};
    }

    template <typename CharT>
    StringT<CharT> GetFileNameImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        if (path.empty())
            return StringT<CharT>();

        size_t pos = path.find_last_of(T::Separators);
        if (pos == StringT<CharT>::npos)
            return path;

        return path.substr(pos + 1);
    }

    template <typename CharT>
    StringT<CharT> GetExtensionImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        auto fileName = GetFileNameImpl<CharT>(path);
        size_t pos = fileName.find_last_of(T::Dot);
        if (pos == StringT<CharT>::npos)
            return StringT<CharT>();

        return fileName.substr(pos);
    }

    template <typename CharT>
    StringT<CharT> RemoveExtensionImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        size_t lastDot = path.find_last_of(T::Dot);
        size_t lastSlash = path.find_last_of(T::Separators);

        if (lastDot == StringT<CharT>::npos)
            return path;

        if (lastSlash != StringT<CharT>::npos && lastDot < lastSlash)
            return path;

        return path.substr(0, lastDot);
    }

    template <typename CharT>
    StringT<CharT> CombinePathImpl(const StringT<CharT> &path1, const StringT<CharT> &path2) {
        using T = PathCharTraits<CharT>;
        if (path1.empty())
            return path2;
        if (path2.empty())
            return path1;

        CharT lastChar = path1[path1.size() - 1];
        CharT firstChar = path2[0];

        bool lastIsSep = (lastChar == T::ForwardSlash || lastChar == T::BackSlash);
        bool firstIsSep = (firstChar == T::ForwardSlash || firstChar == T::BackSlash);

        if (lastIsSep && firstIsSep)
            return path1 + path2.substr(1);
        else if (!lastIsSep && !firstIsSep)
            return path1 + T::BackSlash + path2;
        else
            return path1 + path2;
    }

    template <typename CharT>
    StringT<CharT> NormalizePathImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        if (path.empty())
            return StringT<CharT>();

        StringT<CharT> result = path;

        std::replace(result.begin(), result.end(), T::ForwardSlash, T::BackSlash);

        auto it = std::unique(result.begin(), result.end(),
                              [](CharT a, CharT b) {
                                  return a == PathCharTraits<CharT>::BackSlash &&
                                         b == PathCharTraits<CharT>::BackSlash;
                              });
        result.erase(it, result.end());

        return result;
    }

    template <typename CharT>
    bool IsPathValidImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        return path.find_first_of(T::InvalidPathChars) == StringT<CharT>::npos;
    }

    template <typename CharT>
    bool IsAbsolutePathImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        if (path.empty())
            return false;

        // Drive letter: C:\path
        if (path.size() >= 3 && path[1] == T::Colon &&
            (path[2] == T::BackSlash || path[2] == T::ForwardSlash))
            return true;

        // UNC: \\server\share
        if (path.size() >= 2 && path[0] == T::BackSlash && path[1] == T::BackSlash)
            return true;

        return false;
    }

    template <typename CharT>
    bool IsPathRootedImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        if (path.empty())
            return false;

        if (path.size() >= 2 && path[1] == T::Colon)
            return true;

        if (path[0] == T::BackSlash || path[0] == T::ForwardSlash)
            return true;

        return false;
    }

    template <typename CharT>
    StringT<CharT> ResolvePathImpl(const StringT<CharT> &path) {
        using T = PathCharTraits<CharT>;
        StringT<CharT> normalizedPath = NormalizePathImpl<CharT>(path);
        std::vector<StringT<CharT>> parts;

        size_t start = 0;

        // Handle root
        StringT<CharT> root;
        if (normalizedPath.size() >= 2 && normalizedPath[1] == T::Colon) {
            root = normalizedPath.substr(0, 2) + T::BackSlash;
            start = 3;
        } else if (normalizedPath.size() >= 2 &&
                   normalizedPath[0] == T::BackSlash && normalizedPath[1] == T::BackSlash) {
            size_t serverEnd = normalizedPath.find(T::BackSlash, 2);
            if (serverEnd != StringT<CharT>::npos) {
                size_t shareEnd = normalizedPath.find(T::BackSlash, serverEnd + 1);
                if (shareEnd != StringT<CharT>::npos) {
                    root = normalizedPath.substr(0, shareEnd + 1);
                    start = shareEnd + 1;
                } else {
                    return normalizedPath;
                }
            } else {
                return normalizedPath;
            }
        } else if (!normalizedPath.empty() &&
                   (normalizedPath[0] == T::BackSlash || normalizedPath[0] == T::ForwardSlash)) {
            root = StringT<CharT>(1, T::BackSlash);
            start = 1;
        }

        // Process path segments
        while (start < normalizedPath.size()) {
            size_t pos = normalizedPath.find(T::BackSlash, start);
            if (pos == StringT<CharT>::npos)
                pos = normalizedPath.size();

            StringT<CharT> segment = normalizedPath.substr(start, pos - start);

            if (segment == T::DotStr) {
                // Skip current directory
            } else if (segment == T::DotDotStr) {
                if (!parts.empty())
                    parts.pop_back();
            } else if (!segment.empty()) {
                parts.push_back(segment);
            }

            start = pos + 1;
        }

        // Reconstruct
        StringT<CharT> resolvedPath = root;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0)
                resolvedPath += T::BackSlash;
            resolvedPath += parts[i];
        }

        if (resolvedPath.empty() && !root.empty())
            return root;

        return resolvedPath;
    }

    template <typename CharT>
    StringT<CharT> MakeRelativePathImpl(const StringT<CharT> &path, const StringT<CharT> &basePath) {
        using T = PathCharTraits<CharT>;

        StringT<CharT> resolvedPath = ResolvePathImpl<CharT>(path);
        StringT<CharT> resolvedBasePath = ResolvePathImpl<CharT>(basePath);

        if (!IsAbsolutePathImpl<CharT>(resolvedPath) || !IsAbsolutePathImpl<CharT>(resolvedBasePath))
            return resolvedPath;

        StringT<CharT> pathDrive = GetDriveImpl<CharT>(resolvedPath);
        StringT<CharT> baseDrive = GetDriveImpl<CharT>(resolvedBasePath);

        if (pathDrive != baseDrive)
            return resolvedPath;

        // Split into components
        std::vector<StringT<CharT>> pathComponents;
        std::vector<StringT<CharT>> baseComponents;

        size_t pathStart = resolvedPath.find(T::BackSlash, pathDrive.size()) + 1;
        size_t baseStart = resolvedBasePath.find(T::BackSlash, baseDrive.size()) + 1;

        while (pathStart < resolvedPath.size()) {
            size_t nextSlash = resolvedPath.find(T::BackSlash, pathStart);
            if (nextSlash == StringT<CharT>::npos) {
                pathComponents.push_back(resolvedPath.substr(pathStart));
                break;
            }
            pathComponents.push_back(resolvedPath.substr(pathStart, nextSlash - pathStart));
            pathStart = nextSlash + 1;
        }

        while (baseStart < resolvedBasePath.size()) {
            size_t nextSlash = resolvedBasePath.find(T::BackSlash, baseStart);
            if (nextSlash == StringT<CharT>::npos) {
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
               T::CompareIgnoreCase(pathComponents[commonLength].c_str(),
                                    baseComponents[commonLength].c_str()) == 0) {
            commonLength++;
        }

        // Build relative path
        StringT<CharT> relativePath;

        for (size_t i = commonLength; i < baseComponents.size(); ++i) {
            if (!relativePath.empty())
                relativePath += T::BackSlash;
            relativePath += T::DotDotStr;
        }

        for (size_t i = commonLength; i < pathComponents.size(); ++i) {
            if (!relativePath.empty())
                relativePath += T::BackSlash;
            relativePath += pathComponents[i];
        }

        if (relativePath.empty())
            relativePath = T::DotStr;

        return relativePath;
    }

} // namespace utils::detail

#endif // BML_PATHUTILSDETAIL_H
