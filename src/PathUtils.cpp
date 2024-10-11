#include "PathUtils.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <zip.h>

#include "StringUtils.h"

namespace utils {
    bool FileExistsA(const std::string &file) {
        if (file.empty())
            return false;
        DWORD attr = ::GetFileAttributesA(file.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
    }

    bool FileExistsW(const std::wstring &file) {
        if (file.empty())
            return false;
        DWORD attr = ::GetFileAttributesW(file.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
    }

    bool FileExistsUtf8(const std::string &file) {
        if (file.empty())
            return false;
        return FileExistsW(utils::Utf8ToUtf16(file));
    }

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
        return DirectoryExistsW(utils::Utf8ToUtf16(dir));
    }

    bool CreateDirA(const std::string &dir) {
        if (dir.empty())
            return false;
        BOOL result = ::CreateDirectoryA(dir.c_str(), nullptr);
        if (result == FALSE)
            return DirectoryExistsA(dir);
        return true;
    }

    bool CreateDirW(const std::wstring &dir) {
        if (dir.empty())
            return false;
        BOOL result = ::CreateDirectoryW(dir.c_str(), nullptr);
        if (result == FALSE)
            return DirectoryExistsW(dir);
        return true;
    }

    bool CreateDirUtf8(const std::string &dir) {
        if (dir.empty())
            return false;
        return CreateDirW(utils::Utf8ToUtf16(dir));
    }

    bool CreateFileTreeA(const std::string &file) {
        if (file.size() < 3)
            return false;

        std::string tree = file;
        for (char *pch = &tree[3]; *pch != '\0'; ++pch) {
            if (*pch != '/' && *pch != '\\')
                continue;
            *pch = '\0';
            if (::GetFileAttributesA(tree.c_str()) == INVALID_FILE_ATTRIBUTES)
                if (!::CreateDirectoryA(tree.c_str(), nullptr))
                    break;
            *pch = '\\';
        }
        return true;
    }

    bool CreateFileTreeW(const std::wstring &file) {
        if (file.size() < 3)
            return false;

        std::wstring tree = file;
        for (wchar_t *pch = &tree[3]; *pch != '\0'; ++pch) {
            if (*pch != '/' && *pch != '\\')
                continue;
            *pch = '\0';
            if (::GetFileAttributesW(tree.c_str()) == INVALID_FILE_ATTRIBUTES)
                if (!::CreateDirectoryW(tree.c_str(), nullptr))
                    break;
            *pch = '\\';
        }
        return true;
    }

    bool CreateFileTreeUtf8(const std::string &file) {
        if (file.size() < 3)
            return false;
        return CreateFileTreeW(utils::Utf8ToUtf16(file));
    }

    bool DeleteFileA(const std::string &path) {
        if (path.empty())
            return false;
        return ::DeleteFileA(path.c_str()) == TRUE;
    }

    bool DeleteFileW(const std::wstring &path) {
        if (path.empty())
            return false;
        return ::DeleteFileW(path.c_str()) == TRUE;
    }

    bool DeleteFileUtf8(const std::string &path) {
        if (path.empty())
            return false;
        const std::wstring file = Utf8ToUtf16(path);
        return ::DeleteFileW(file.c_str()) == TRUE;
    }

    bool DeleteDirA(const std::string &path) {
        if (path.empty())
            return false;

        WIN32_FIND_DATAA findData;
        std::string searchMask = path + "\\*";
        HANDLE searchHandle = ::FindFirstFileExA(searchMask.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, 0);
        if (searchHandle == INVALID_HANDLE_VALUE) {
            if (::GetLastError() != ERROR_FILE_NOT_FOUND)
                return false;
        }

        if (searchHandle != INVALID_HANDLE_VALUE) {
            for (;;) {
                if (findData.cFileName[0] != '.') {
                    std::string filePath = path + '\\' + findData.cFileName;
                    if (((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ||
                        ((findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)) {
                        DeleteDirA(filePath);
                    } else {
                        BOOL result = ::DeleteFileA(filePath.c_str());
                        if (result == FALSE) {
                            ::FindClose(searchHandle);
                            return false;
                        }
                    }
                }

                BOOL result = ::FindNextFileA(searchHandle, &findData);
                if (result == FALSE) {
                    if (::GetLastError() != ERROR_NO_MORE_FILES) {
                        ::FindClose(searchHandle);
                        return false;
                    }
                    break;
                }
            }
        }

        BOOL result = ::RemoveDirectoryA(path.c_str());
        ::FindClose(searchHandle);
        return result == TRUE;
    }

    bool DeleteDirW(const std::wstring &path) {
        if (path.empty())
            return false;

        WIN32_FIND_DATAW findData;
        std::wstring searchMask = path + L"\\*";
        HANDLE searchHandle = ::FindFirstFileExW(searchMask.c_str(), FindExInfoBasic, &findData, FindExSearchNameMatch, nullptr, 0);
        if (searchHandle == INVALID_HANDLE_VALUE) {
            if (::GetLastError() != ERROR_FILE_NOT_FOUND)
                return false;
        }

        if (searchHandle != INVALID_HANDLE_VALUE) {
            for (;;) {
                if (findData.cFileName[0] != '.') {
                    std::wstring filePath = path + L'\\' + findData.cFileName;
                    if (((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) ||
                        ((findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)) {
                        DeleteDirW(filePath);
                    } else {
                        BOOL result = ::DeleteFileW(filePath.c_str());
                        if (result == FALSE) {
                            ::FindClose(searchHandle);
                            return false;
                        }
                    }
                }

                BOOL result = ::FindNextFileW(searchHandle, &findData);
                if (result == FALSE) {
                    if (::GetLastError() != ERROR_NO_MORE_FILES) {
                        ::FindClose(searchHandle);
                        return false;
                    }
                    break;
                }
            }
        }

        BOOL result = ::RemoveDirectoryW(path.c_str());
        ::FindClose(searchHandle);
        return result == TRUE;
    }

    bool DeleteDirUtf8(const std::string &path) {
        if (path.empty())
            return false;
        return DeleteDirW(utils::Utf8ToUtf16(path));
    }

    bool DuplicateFileA(const std::string &path, const std::string &dest) {
        if (!FileExistsA(path) || dest.empty())
            return false;

        if (!CreateFileTreeA(dest))
            return false;

        return ::CopyFileA(path.c_str(), dest.c_str(), FALSE) == TRUE;
    }

    bool DuplicateFileW(const std::wstring &path, const std::wstring &dest) {
        if (!FileExistsW(path) || dest.empty())
            return false;

        if (!CreateFileTreeW(dest))
            return false;

        return ::CopyFileW(path.c_str(), dest.c_str(), FALSE) == TRUE;
    }

    bool DuplicateFileUtf8(const std::string &path, const std::string &dest) {
        return DuplicateFileW(utils::Utf8ToUtf16(path), utils::Utf8ToUtf16(dest));
    }

    bool ExtractZipW(const std::wstring &path, const std::wstring &dest) {
        if (!FileExistsW(path) || dest.empty())
            return false;

        CreateFileTreeW(dest);

        if (!StringEndsWithCaseInsensitive(path, L".zip"))
            return false;

        errno_t err;
        FILE *fp;
        err = _wfopen_s(&fp, path.c_str(), L"rb");
        if (err != 0)
            return false;

        fseek(fp, 0, SEEK_END);
        size_t sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char *buf = new char[sz];

        if (fread(buf, sizeof(char), sz, fp) != sz) {
            delete[] buf;
            fclose(fp);
            return false;
        }

        std::string destPath = Utf16ToUtf8(dest);
        if (zip_stream_extract(buf, sz, destPath.c_str(), nullptr, nullptr) < 0)
            return false;

        return true;
    }
}
