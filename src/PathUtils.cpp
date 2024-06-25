#include "PathUtils.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <zip.h>

#include "StringUtils.h"

namespace utils {
    bool FileExists(const std::string &file) {
        DWORD attr = ::GetFileAttributesA(file.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
    }

    bool FileExists(const std::wstring &file) {
        DWORD attr = ::GetFileAttributesW(file.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
    }

    bool DirectoryExists(const std::string &dir) {
        DWORD attr = ::GetFileAttributesA(dir.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES &&
            ((attr & FILE_ATTRIBUTE_DIRECTORY) || (attr & FILE_ATTRIBUTE_REPARSE_POINT)));
    }

    bool DirectoryExists(const std::wstring &dir) {
        DWORD attr = ::GetFileAttributesW(dir.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES &&
            ((attr & FILE_ATTRIBUTE_DIRECTORY) || (attr & FILE_ATTRIBUTE_REPARSE_POINT)));
    }

    bool CreateDir(const std::string &dir) {
        BOOL result = ::CreateDirectoryA(dir.c_str(), nullptr);
        if (result == FALSE)
            return DirectoryExists(dir);
        return true;
    }

    bool CreateDir(const std::wstring &dir) {
        BOOL result = ::CreateDirectoryW(dir.c_str(), nullptr);
        if (result == FALSE)
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
            if (::GetFileAttributesA(tree.c_str()) == INVALID_FILE_ATTRIBUTES)
                if (!::CreateDirectoryA(tree.c_str(), nullptr))
                    break;
            *pch = '\\';
        }
        return true;
    }

    bool CreateFileTree(const std::wstring &file) {
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

    bool DeleteDir(const std::wstring &path) {
        WIN32_FIND_DATAW findData;
        std::wstring searchMask = path + L"\\*";
        HANDLE searchHandle = ::FindFirstFileExW(searchMask.c_str(),
                                                 FindExInfoBasic, &findData,
                                                 FindExSearchNameMatch, nullptr, 0);
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
                        DeleteDir(filePath);
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

    bool DuplicateFile(const std::wstring &path, const std::wstring &dest) {
        if (!FileExists(path) || dest.empty())
            return false;

        if (!CreateFileTree(dest))
            return false;

        return ::CopyFileW(path.c_str(), dest.c_str(), FALSE) == TRUE;
    }

    bool ExtractZip(const std::wstring &path, const std::wstring &dest) {
        if (!FileExists(path) || dest.empty())
            return false;

        CreateFileTree(dest);

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

        char dp[1024];
        Utf16ToAnsi(dest.c_str(), dp, 1024);
        if (zip_stream_extract(buf, sz, dp, nullptr, nullptr) < 0)
            return false;

        return true;
    }
}
