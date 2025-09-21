#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

#include "CKContext.h"

#include "BML/BML.h"
#include "ModManager.h"
#include "RenderHook.h"
#include "Overlay.h"
#include "HookUtils.h"
#include "PathUtils.h"
#include "StringUtils.h"

void BML_GetVersion(int *major, int *minor, int *patch) {
    if (major) *major = BML_MAJOR_VERSION;
    if (minor) *minor = BML_MINOR_VERSION;
    if (patch) *patch = BML_PATCH_VERSION;
}

void BML_GetVersionString(char *version, size_t size) {
    if (version && size > 0) {
        snprintf(version, size, "%d.%d.%d", BML_MAJOR_VERSION, BML_MINOR_VERSION, BML_PATCH_VERSION);
    }
}

void *BML_Malloc(size_t size) {
    return malloc(size);
}

void *BML_Calloc(size_t num, size_t size) {
    return calloc(num, size);
}

void *BML_Realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

void BML_Free(void *ptr) {
    return free(ptr);
}

void BML_FreeString(char *str) {
    free(str);
}

void BML_FreeWString(wchar_t *wstr) {
    free(wstr);
}

void BML_FreeStringArray(char **strings, size_t count) {
    if (!strings) return;
    for (size_t i = 0; i < count; ++i) {
        free(strings[i]);
    }
    free(strings);
}

char *BML_Strdup(const char *str) {
    if (!str)
        return nullptr;

    size_t len = strlen(str);
    char *dup = static_cast<char *>(malloc(len + 1));
    if (dup) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}

// String splitting
char **BML_SplitString(const char *str, const char *delim, size_t *count) {
    if (!str || !delim || !count) {
        if (count) *count = 0;
        return nullptr;
    }

    if (*str == '\0') {
        *count = 0;
        return nullptr;
    }

    if (*delim == '\0') {
        *count = 1;
        char **result = static_cast<char **>(malloc(sizeof(char *)));
        if (!result) return nullptr;
        result[0] = BML_Strdup(str);
        return result;
    }

    // Count occurrences first
    size_t delimLen = strlen(delim);
    const char *ptr = str;
    size_t segments = 1;

    while ((ptr = strstr(ptr, delim)) != nullptr) {
        segments++;
        ptr += delimLen;
    }

    // Allocate result array
    char **result = static_cast<char **>(malloc(segments * sizeof(char *)));
    if (!result) {
        *count = 0;
        return nullptr;
    }

    // Split the string
    const char *start = str;
    const char *end;
    size_t idx = 0;

    while ((end = strstr(start, delim)) != nullptr && idx < segments) {
        size_t segLen = end - start;
        result[idx] = static_cast<char *>(malloc(segLen + 1));
        if (!result[idx]) {
            // Cleanup on failure
            for (size_t i = 0; i < idx; ++i) {
                free(result[i]);
            }
            free(result);
            *count = 0;
            return nullptr;
        }
        memcpy(result[idx], start, segLen);
        result[idx][segLen] = '\0';
        start = end + delimLen;
        idx++;
    }

    // Add final segment
    if (idx < segments) {
        result[idx] = BML_Strdup(start);
        if (!result[idx]) {
            for (size_t i = 0; i < idx; ++i) {
                free(result[i]);
            }
            free(result);
            *count = 0;
            return nullptr;
        }
    }

    *count = segments;
    return result;
}

char **BML_SplitStringChar(const char *str, char delim, size_t *count) {
    if (!str || !count) {
        if (count) *count = 0;
        return nullptr;
    }

    if (*str == '\0') {
        *count = 0;
        return nullptr;
    }

    // Count occurrences first
    const char *ptr = str;
    size_t segments = 1;

    while ((ptr = strchr(ptr, delim)) != nullptr) {
        segments++;
        ptr++;
    }

    // Allocate result array
    char **result = static_cast<char **>(malloc(segments * sizeof(char *)));
    if (!result) {
        *count = 0;
        return nullptr;
    }

    // Split the string
    const char *start = str;
    const char *end;
    size_t idx = 0;

    while ((end = strchr(start, delim)) != nullptr && idx < segments) {
        size_t segLen = end - start;
        result[idx] = static_cast<char *>(malloc(segLen + 1));
        if (!result[idx]) {
            for (size_t i = 0; i < idx; ++i) {
                free(result[i]);
            }
            free(result);
            *count = 0;
            return nullptr;
        }
        memcpy(result[idx], start, segLen);
        result[idx][segLen] = '\0';
        start = end + 1;
        idx++;
    }

    // Add final segment
    if (idx < segments) {
        result[idx] = BML_Strdup(start);
        if (!result[idx]) {
            for (size_t i = 0; i < idx; ++i) {
                free(result[i]);
            }
            free(result);
            *count = 0;
            return nullptr;
        }
    }

    *count = segments;
    return result;
}

// String trimming - in-place modification using the same algorithm as C++
void BML_TrimString(char *str) {
    if (!str) return;

    size_t len = strlen(str);
    if (len == 0) return;

    // Find first non-whitespace character
    char *start = str;
    while (*start && isspace(static_cast<unsigned char>(*start))) {
        start++;
    }

    // If string is all whitespace, make it empty
    if (*start == '\0') {
        str[0] = '\0';
        return;
    }

    // Find last non-whitespace character
    char *end = str + len - 1;
    while (end > start && isspace(static_cast<unsigned char>(*end))) {
        end--;
    }

    // Move trimmed content to beginning of buffer if needed
    if (start != str) {
        size_t trimmedLen = end - start + 1;
        memmove(str, start, trimmedLen);
        str[trimmedLen] = '\0';
    } else {
        // Just null-terminate at the right position
        end[1] = '\0';
    }
}

char *BML_TrimStringCopy(const char *str) {
    if (!str) return nullptr;

    char *copy = BML_Strdup(str);
    if (!copy) return nullptr;

    BML_TrimString(copy);
    return copy;
}

// String joining
char *BML_JoinString(const char **strings, size_t count, const char *delim) {
    if (!strings || count == 0) return BML_Strdup("");
    if (!delim) delim = "";

    // Calculate total length needed
    size_t totalLen = 0;
    size_t delimLen = strlen(delim);

    for (size_t i = 0; i < count; ++i) {
        if (strings[i]) {
            totalLen += strlen(strings[i]);
        }
        if (i > 0) {
            totalLen += delimLen;
        }
    }

    // Allocate result buffer
    char *result = static_cast<char *>(malloc(totalLen + 1));
    if (!result) return nullptr;

    // Build the joined string
    char *ptr = result;
    for (size_t i = 0; i < count; ++i) {
        if (i > 0 && delimLen > 0) {
            memcpy(ptr, delim, delimLen);
            ptr += delimLen;
        }
        if (strings[i]) {
            size_t strLen = strlen(strings[i]);
            memcpy(ptr, strings[i], strLen);
            ptr += strLen;
        }
    }
    *ptr = '\0';

    return result;
}

char *BML_JoinStringChar(const char **strings, size_t count, char delim) {
    if (!strings || count == 0) return BML_Strdup("");

    // Calculate total length needed
    size_t totalLen = count - 1; // delimiters

    for (size_t i = 0; i < count; ++i) {
        if (strings[i]) {
            totalLen += strlen(strings[i]);
        }
    }

    // Allocate result buffer
    char *result = static_cast<char *>(malloc(totalLen + 1));
    if (!result) return nullptr;

    // Build the joined string
    char *ptr = result;
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) {
            *ptr++ = delim;
        }
        if (strings[i]) {
            size_t strLen = strlen(strings[i]);
            memcpy(ptr, strings[i], strLen);
            ptr += strLen;
        }
    }
    *ptr = '\0';

    return result;
}

// Case conversion - use locale-aware conversion like the C++ version
char *BML_ToLower(const char *str) {
    if (!str) return nullptr;

    size_t len = strlen(str);
    char *result = static_cast<char *>(malloc(len + 1));
    if (!result) return nullptr;

    for (size_t i = 0; i < len; ++i) {
        result[i] = static_cast<char>(tolower(static_cast<unsigned char>(str[i])));
    }
    result[len] = '\0';

    return result;
}

char *BML_ToUpper(const char *str) {
    if (!str) return nullptr;

    size_t len = strlen(str);
    char *result = static_cast<char *>(malloc(len + 1));
    if (!result) return nullptr;

    for (size_t i = 0; i < len; ++i) {
        result[i] = static_cast<char>(toupper(static_cast<unsigned char>(str[i])));
    }
    result[len] = '\0';

    return result;
}

// String comparison functions - reuse C++ logic without string conversion
int BML_StartsWith(const char *str, const char *prefix, int caseSensitive) {
    if (!str || !prefix) return 0;

    size_t strLen = strlen(str);
    size_t prefixLen = strlen(prefix);

    if (strLen < prefixLen) return 0;

    if (caseSensitive) {
        return strncmp(str, prefix, prefixLen) == 0 ? 1 : 0;
    } else {
        return _strnicmp(str, prefix, prefixLen) == 0 ? 1 : 0;
    }
}

int BML_EndsWith(const char *str, const char *suffix, int caseSensitive) {
    if (!str || !suffix) return 0;

    size_t strLen = strlen(str);
    size_t suffixLen = strlen(suffix);

    if (strLen < suffixLen) return 0;

    const char *strEnd = str + strLen - suffixLen;

    if (caseSensitive) {
        return strcmp(strEnd, suffix) == 0 ? 1 : 0;
    } else {
        return _stricmp(strEnd, suffix) == 0 ? 1 : 0;
    }
}

int BML_Contains(const char *str, const char *substr, int caseSensitive) {
    if (!str || !substr) return 0;

    if (caseSensitive) {
        return strstr(str, substr) != nullptr ? 1 : 0;
    } else {
        // Case-insensitive search - create temporary lowercase versions
        char *lowerStr = BML_ToLower(str);
        char *lowerSubstr = BML_ToLower(substr);

        if (!lowerStr || !lowerSubstr) {
            free(lowerStr);
            free(lowerSubstr);
            return 0;
        }

        int result = strstr(lowerStr, lowerSubstr) != nullptr ? 1 : 0;

        free(lowerStr);
        free(lowerSubstr);
        return result;
    }
}

// String conversion functions - direct calls to C++ implementations
wchar_t *BML_ToWString(const char *str, int isUtf8) {
    if (!str) return nullptr;

    std::wstring result = utils::ToWString(str, isUtf8 != 0);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_ToString(const wchar_t *wstr, int toUtf8) {
    if (!wstr) return nullptr;

    std::string result = utils::ToString(wstr, toUtf8 != 0);
    return BML_Strdup(result.c_str());
}

// Legacy conversion functions - direct calls
wchar_t *BML_Utf8ToUtf16(const char *str) {
    if (!str) return nullptr;
    std::wstring result = utils::Utf8ToUtf16(str);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_Utf16ToUtf8(const wchar_t *wstr) {
    if (!wstr) return nullptr;
    std::string result = utils::Utf16ToUtf8(wstr);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_AnsiToUtf16(const char *str) {
    if (!str) return nullptr;
    std::wstring result = utils::AnsiToUtf16(str);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_Utf16ToAnsi(const wchar_t *wstr) {
    if (!wstr) return nullptr;
    std::string result = utils::Utf16ToAnsi(wstr);
    return BML_Strdup(result.c_str());
}

// Hash functions - direct calls
size_t BML_HashString(const char *str) {
    return utils::HashString(str);
}

size_t BML_HashWString(const wchar_t *str) {
    return utils::HashString(str);
}

// String escape/unescape - direct calls to C++ implementations
char *BML_UnescapeString(const char *str) {
    if (!str) return nullptr;
    std::string result = utils::UnescapeString(str);
    return BML_Strdup(result.c_str());
}

char *BML_EscapeString(const char *str) {
    if (!str) return nullptr;
    std::string result = utils::EscapeString(str);
    return BML_Strdup(result.c_str());
}

char *BML_StripAnsiCodes(const char *str) {
    if (!str) return nullptr;
    std::string result = utils::StripAnsiCodes(str);
    return BML_Strdup(result.c_str());
}

// File existence checks - direct calls
int BML_FileExistsA(const char *file) {
    return file ? (utils::FileExistsA(file) ? 1 : 0) : 0;
}

int BML_FileExistsW(const wchar_t *file) {
    return file ? (utils::FileExistsW(file) ? 1 : 0) : 0;
}

int BML_FileExistsUtf8(const char *file) {
    return file ? (utils::FileExistsUtf8(file) ? 1 : 0) : 0;
}

// Directory existence checks
int BML_DirectoryExistsA(const char *dir) {
    return dir ? (utils::DirectoryExistsA(dir) ? 1 : 0) : 0;
}

int BML_DirectoryExistsW(const wchar_t *dir) {
    return dir ? (utils::DirectoryExistsW(dir) ? 1 : 0) : 0;
}

int BML_DirectoryExistsUtf8(const char *dir) {
    return dir ? (utils::DirectoryExistsUtf8(dir) ? 1 : 0) : 0;
}

// Path existence checks
int BML_PathExistsA(const char *path) {
    return path ? (utils::PathExistsA(path) ? 1 : 0) : 0;
}

int BML_PathExistsW(const wchar_t *path) {
    return path ? (utils::PathExistsW(path) ? 1 : 0) : 0;
}

int BML_PathExistsUtf8(const char *path) {
    return path ? (utils::PathExistsUtf8(path) ? 1 : 0) : 0;
}

// Directory creation
int BML_CreateDirectoryA(const char *dir) {
    return dir ? (utils::CreateDirectoryA(dir) ? 1 : 0) : 0;
}

int BML_CreateDirectoryW(const wchar_t *dir) {
    return dir ? (utils::CreateDirectoryW(dir) ? 1 : 0) : 0;
}

int BML_CreateDirectoryUtf8(const char *dir) {
    return dir ? (utils::CreateDirectoryUtf8(dir) ? 1 : 0) : 0;
}

// Create directory tree
int BML_CreateFileTreeA(const char *path) {
    return path ? (utils::CreateFileTreeA(path) ? 1 : 0) : 0;
}

int BML_CreateFileTreeW(const wchar_t *path) {
    return path ? (utils::CreateFileTreeW(path) ? 1 : 0) : 0;
}

int BML_CreateFileTreeUtf8(const char *path) {
    return path ? (utils::CreateFileTreeUtf8(path) ? 1 : 0) : 0;
}

// File deletion
int BML_DeleteFileA(const char *path) {
    return path ? (utils::DeleteFileA(path) ? 1 : 0) : 0;
}

int BML_DeleteFileW(const wchar_t *path) {
    return path ? (utils::DeleteFileW(path) ? 1 : 0) : 0;
}

int BML_DeleteFileUtf8(const char *path) {
    return path ? (utils::DeleteFileUtf8(path) ? 1 : 0) : 0;
}

// Directory deletion
int BML_DeleteDirectoryA(const char *path) {
    return path ? (utils::DeleteDirectoryA(path) ? 1 : 0) : 0;
}

int BML_DeleteDirectoryW(const wchar_t *path) {
    return path ? (utils::DeleteDirectoryW(path) ? 1 : 0) : 0;
}

int BML_DeleteDirectoryUtf8(const char *path) {
    return path ? (utils::DeleteDirectoryUtf8(path) ? 1 : 0) : 0;
}

// File copying
int BML_CopyFileA(const char *path, const char *dest) {
    return (path && dest) ? (utils::CopyFileA(path, dest) ? 1 : 0) : 0;
}

int BML_CopyFileW(const wchar_t *path, const wchar_t *dest) {
    return (path && dest) ? (utils::CopyFileW(path, dest) ? 1 : 0) : 0;
}

int BML_CopyFileUtf8(const char *path, const char *dest) {
    return (path && dest) ? (utils::CopyFileUtf8(path, dest) ? 1 : 0) : 0;
}

// File moving
int BML_MoveFileA(const char *path, const char *dest) {
    return (path && dest) ? (utils::MoveFileA(path, dest) ? 1 : 0) : 0;
}

int BML_MoveFileW(const wchar_t *path, const wchar_t *dest) {
    return (path && dest) ? (utils::MoveFileW(path, dest) ? 1 : 0) : 0;
}

int BML_MoveFileUtf8(const char *path, const char *dest) {
    return (path && dest) ? (utils::MoveFileUtf8(path, dest) ? 1 : 0) : 0;
}

// Zip extraction
int BML_ExtractZipA(const char *path, const char *dest) {
    return (path && dest) ? (utils::ExtractZipA(path, dest) ? 1 : 0) : 0;
}

int BML_ExtractZipW(const wchar_t *path, const wchar_t *dest) {
    return (path && dest) ? (utils::ExtractZipW(path, dest) ? 1 : 0) : 0;
}

int BML_ExtractZipUtf8(const char *path, const char *dest) {
    return (path && dest) ? (utils::ExtractZipUtf8(path, dest) ? 1 : 0) : 0;
}

// Path manipulation - direct calls
char *BML_GetDriveA(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::GetDriveA(path);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_GetDriveW(const wchar_t *path) {
    if (!path) return nullptr;
    std::wstring result = utils::GetDriveW(path);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_GetDriveUtf8(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::GetDriveUtf8(path);
    return BML_Strdup(result.c_str());
}

char *BML_GetDirectoryA(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::GetDirectoryA(path);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_GetDirectoryW(const wchar_t *path) {
    if (!path) return nullptr;
    std::wstring result = utils::GetDirectoryW(path);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_GetDirectoryUtf8(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::GetDirectoryUtf8(path);
    return BML_Strdup(result.c_str());
}

// Drive and directory as separate outputs
int BML_GetDriveAndDirectoryA(const char *path, char **drive, char **directory) {
    if (!path || !drive || !directory) return 0;

    auto result = utils::GetDriveAndDirectoryA(path);
    *drive = BML_Strdup(result.first.c_str());
    *directory = BML_Strdup(result.second.c_str());

    return (*drive && *directory) ? 1 : 0;
}

int BML_GetDriveAndDirectoryW(const wchar_t *path, wchar_t **drive, wchar_t **directory) {
    if (!path || !drive || !directory) return 0;

    auto result = utils::GetDriveAndDirectoryW(path);

    *drive = static_cast<wchar_t *>(malloc((result.first.length() + 1) * sizeof(wchar_t)));
    *directory = static_cast<wchar_t *>(malloc((result.second.length() + 1) * sizeof(wchar_t)));

    if (*drive && *directory) {
        wcscpy(*drive, result.first.c_str());
        wcscpy(*directory, result.second.c_str());
        return 1;
    }

    free(*drive);
    free(*directory);
    *drive = *directory = nullptr;
    return 0;
}

int BML_GetDriveAndDirectoryUtf8(const char *path, char **drive, char **directory) {
    if (!path || !drive || !directory) return 0;

    auto result = utils::GetDriveAndDirectoryUtf8(path);
    *drive = BML_Strdup(result.first.c_str());
    *directory = BML_Strdup(result.second.c_str());

    return (*drive && *directory) ? 1 : 0;
}

char *BML_GetFileNameA(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::GetFileNameA(path);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_GetFileNameW(const wchar_t *path) {
    if (!path) return nullptr;
    std::wstring result = utils::GetFileNameW(path);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_GetFileNameUtf8(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::GetFileNameUtf8(path);
    return BML_Strdup(result.c_str());
}

char *BML_GetExtensionA(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::GetExtensionA(path);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_GetExtensionW(const wchar_t *path) {
    if (!path) return nullptr;
    std::wstring result = utils::GetExtensionW(path);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_GetExtensionUtf8(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::GetExtensionUtf8(path);
    return BML_Strdup(result.c_str());
}

char *BML_RemoveExtensionA(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::RemoveExtensionA(path);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_RemoveExtensionW(const wchar_t *path) {
    if (!path) return nullptr;
    std::wstring result = utils::RemoveExtensionW(path);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_RemoveExtensionUtf8(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::RemoveExtensionUtf8(path);
    return BML_Strdup(result.c_str());
}

char *BML_CombinePathA(const char *path1, const char *path2) {
    if (!path1 || !path2) return nullptr;
    std::string result = utils::CombinePathA(path1, path2);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_CombinePathW(const wchar_t *path1, const wchar_t *path2) {
    if (!path1 || !path2) return nullptr;
    std::wstring result = utils::CombinePathW(path1, path2);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_CombinePathUtf8(const char *path1, const char *path2) {
    if (!path1 || !path2) return nullptr;
    std::string result = utils::CombinePathUtf8(path1, path2);
    return BML_Strdup(result.c_str());
}

char *BML_NormalizePathA(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::NormalizePathA(path);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_NormalizePathW(const wchar_t *path) {
    if (!path) return nullptr;
    std::wstring result = utils::NormalizePathW(path);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_NormalizePathUtf8(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::NormalizePathUtf8(path);
    return BML_Strdup(result.c_str());
}

// Path validation
int BML_IsPathValidA(const char *path) {
    return path ? (utils::IsPathValidA(path) ? 1 : 0) : 0;
}

int BML_IsPathValidW(const wchar_t *path) {
    return path ? (utils::IsPathValidW(path) ? 1 : 0) : 0;
}

int BML_IsPathValidUtf8(const char *path) {
    return path ? (utils::IsPathValidUtf8(path) ? 1 : 0) : 0;
}

// Path type checks
int BML_IsAbsolutePathA(const char *path) {
    return path ? (utils::IsAbsolutePathA(path) ? 1 : 0) : 0;
}

int BML_IsAbsolutePathW(const wchar_t *path) {
    return path ? (utils::IsAbsolutePathW(path) ? 1 : 0) : 0;
}

int BML_IsAbsolutePathUtf8(const char *path) {
    return path ? (utils::IsAbsolutePathUtf8(path) ? 1 : 0) : 0;
}

int BML_IsRelativePathA(const char *path) {
    return path ? (utils::IsRelativePathA(path) ? 1 : 0) : 0;
}

int BML_IsRelativePathW(const wchar_t *path) {
    return path ? (utils::IsRelativePathW(path) ? 1 : 0) : 0;
}

int BML_IsRelativePathUtf8(const char *path) {
    return path ? (utils::IsRelativePathUtf8(path) ? 1 : 0) : 0;
}

int BML_IsPathRootedA(const char *path) {
    return path ? (utils::IsPathRootedA(path) ? 1 : 0) : 0;
}

int BML_IsPathRootedW(const wchar_t *path) {
    return path ? (utils::IsPathRootedW(path) ? 1 : 0) : 0;
}

int BML_IsPathRootedUtf8(const char *path) {
    return path ? (utils::IsPathRootedUtf8(path) ? 1 : 0) : 0;
}

// Path resolution
char *BML_ResolvePathA(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::ResolvePathA(path);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_ResolvePathW(const wchar_t *path) {
    if (!path) return nullptr;
    std::wstring result = utils::ResolvePathW(path);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_ResolvePathUtf8(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::ResolvePathUtf8(path);
    return BML_Strdup(result.c_str());
}

char *BML_MakeRelativePathA(const char *path, const char *basePath) {
    if (!path || !basePath) return nullptr;
    std::string result = utils::MakeRelativePathA(path, basePath);
    return BML_Strdup(result.c_str());
}

wchar_t *BML_MakeRelativePathW(const wchar_t *path, const wchar_t *basePath) {
    if (!path || !basePath) return nullptr;
    std::wstring result = utils::MakeRelativePathW(path, basePath);
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_MakeRelativePathUtf8(const char *path, const char *basePath) {
    if (!path || !basePath) return nullptr;
    std::string result = utils::MakeRelativePathUtf8(path, basePath);
    return BML_Strdup(result.c_str());
}

// System paths
char *BML_GetTempPathA() {
    std::string result = utils::GetTempPathA();
    return BML_Strdup(result.c_str());
}

wchar_t *BML_GetTempPathW() {
    std::wstring result = utils::GetTempPathW();
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_GetTempPathUtf8() {
    std::string result = utils::GetTempPathUtf8();
    return BML_Strdup(result.c_str());
}

char *BML_GetCurrentDirectoryA() {
    std::string result = utils::GetCurrentDirectoryA();
    return BML_Strdup(result.c_str());
}

wchar_t *BML_GetCurrentDirectoryW() {
    std::wstring result = utils::GetCurrentDirectoryW();
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_GetCurrentDirectoryUtf8() {
    std::string result = utils::GetCurrentDirectoryUtf8();
    return BML_Strdup(result.c_str());
}

int BML_SetCurrentDirectoryA(const char *path) {
    return path ? (utils::SetCurrentDirectoryA(path) ? 1 : 0) : 0;
}

int BML_SetCurrentDirectoryW(const wchar_t *path) {
    return path ? (utils::SetCurrentDirectoryW(path) ? 1 : 0) : 0;
}

int BML_SetCurrentDirectoryUtf8(const char *path) {
    return path ? (utils::SetCurrentDirectoryUtf8(path) ? 1 : 0) : 0;
}

char *BML_GetExecutablePathA() {
    std::string result = utils::GetExecutablePathA();
    return BML_Strdup(result.c_str());
}

wchar_t *BML_GetExecutablePathW() {
    std::wstring result = utils::GetExecutablePathW();
    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_GetExecutablePathUtf8() {
    std::string result = utils::GetExecutablePathUtf8();
    return BML_Strdup(result.c_str());
}

// File properties
int64_t BML_GetFileSizeA(const char *path) {
    return path ? utils::GetFileSizeA(path) : -1;
}

int64_t BML_GetFileSizeW(const wchar_t *path) {
    return path ? utils::GetFileSizeW(path) : -1;
}

int64_t BML_GetFileSizeUtf8(const char *path) {
    return path ? utils::GetFileSizeUtf8(path) : -1;
}

int BML_GetFileTimeA(const char *path, int64_t *creationTime, int64_t *lastAccessTime, int64_t *lastWriteTime) {
    if (!path) return 0;

    utils::FileTime ft = utils::GetFileTimeA(path);
    if (creationTime)
        *creationTime = ft.creationTime;
    if (lastAccessTime)
        *lastAccessTime = ft.lastAccessTime;
    if (lastWriteTime)
        *lastWriteTime = ft.lastWriteTime;

    return 1;
}

int BML_GetFileTimeW(const wchar_t *path, int64_t *creationTime, int64_t *lastAccessTime, int64_t *lastWriteTime) {
    if (!path) return 0;

    utils::FileTime ft = utils::GetFileTimeW(path);
    if (creationTime)
        *creationTime = ft.creationTime;
    if (lastAccessTime)
        *lastAccessTime = ft.lastAccessTime;
    if (lastWriteTime)
        *lastWriteTime = ft.lastWriteTime;

    return 1;
}

int BML_GetFileTimeUtf8(const char *path, int64_t *creationTime, int64_t *lastAccessTime, int64_t *lastWriteTime) {
    if (!path) return 0;

    utils::FileTime ft = utils::GetFileTimeUtf8(path);
    if (creationTime)
        *creationTime = ft.creationTime;
    if (lastAccessTime)
        *lastAccessTime = ft.lastAccessTime;
    if (lastWriteTime)
        *lastWriteTime = ft.lastWriteTime;

    return 1;
}

// File I/O
char *BML_ReadTextFileA(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::ReadTextFileA(path);
    return result.empty() ? nullptr : BML_Strdup(result.c_str());
}

wchar_t *BML_ReadTextFileW(const wchar_t *path) {
    if (!path) return nullptr;
    std::wstring result = utils::ReadTextFileW(path);
    if (result.empty()) return nullptr;

    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_ReadTextFileUtf8(const char *path) {
    if (!path) return nullptr;
    std::string result = utils::ReadTextFileUtf8(path);
    return result.empty() ? nullptr : BML_Strdup(result.c_str());
}

int BML_WriteTextFileA(const char *path, const char *content) {
    return (path && content) ? (utils::WriteTextFileA(path, content) ? 1 : 0) : 0;
}

int BML_WriteTextFileW(const wchar_t *path, const wchar_t *content) {
    return (path && content) ? (utils::WriteTextFileW(path, content) ? 1 : 0) : 0;
}

int BML_WriteTextFileUtf8(const char *path, const char *content) {
    return (path && content) ? (utils::WriteTextFileUtf8(path, content) ? 1 : 0) : 0;
}

int BML_ReadBinaryFileA(const char *path, uint8_t **data, size_t *size) {
    if (!path || !data || !size) return 0;

    std::vector<uint8_t> result = utils::ReadBinaryFileA(path);
    if (result.empty()) {
        *data = nullptr;
        *size = 0;
        return 0;
    }

    *size = result.size();
    *data = static_cast<uint8_t *>(malloc(*size));
    if (!*data) {
        *size = 0;
        return 0;
    }

    memcpy(*data, result.data(), *size);
    return 1;
}

int BML_ReadBinaryFileW(const wchar_t *path, uint8_t **data, size_t *size) {
    if (!path || !data || !size) return 0;

    std::vector<uint8_t> result = utils::ReadBinaryFileW(path);
    if (result.empty()) {
        *data = nullptr;
        *size = 0;
        return 0;
    }

    *size = result.size();
    *data = static_cast<uint8_t *>(malloc(*size));
    if (!*data) {
        *size = 0;
        return 0;
    }

    memcpy(*data, result.data(), *size);
    return 1;
}

int BML_ReadBinaryFileUtf8(const char *path, uint8_t **data, size_t *size) {
    if (!path || !data || !size) return 0;

    std::vector<uint8_t> result = utils::ReadBinaryFileUtf8(path);
    if (result.empty()) {
        *data = nullptr;
        *size = 0;
        return 0;
    }

    *size = result.size();
    *data = static_cast<uint8_t *>(malloc(*size));
    if (!*data) {
        *size = 0;
        return 0;
    }

    memcpy(*data, result.data(), *size);
    return 1;
}

int BML_WriteBinaryFileA(const char *path, const uint8_t *data, size_t size) {
    if (!path || !data) return 0;
    std::vector<uint8_t> vec(data, data + size);
    return utils::WriteBinaryFileA(path, vec) ? 1 : 0;
}

int BML_WriteBinaryFileW(const wchar_t *path, const uint8_t *data, size_t size) {
    if (!path || !data) return 0;
    std::vector<uint8_t> vec(data, data + size);
    return utils::WriteBinaryFileW(path, vec) ? 1 : 0;
}

int BML_WriteBinaryFileUtf8(const char *path, const uint8_t *data, size_t size) {
    if (!path || !data) return 0;
    std::vector<uint8_t> vec(data, data + size);
    return utils::WriteBinaryFileUtf8(path, vec) ? 1 : 0;
}

// Create temporary files
char *BML_CreateTempFileA(const char *prefix) {
    std::string prefixStr = prefix ? prefix : "";
    std::string result = utils::CreateTempFileA(prefixStr);
    return result.empty() ? nullptr : BML_Strdup(result.c_str());
}

wchar_t *BML_CreateTempFileW(const wchar_t *prefix) {
    std::wstring prefixStr = prefix ? prefix : L"";
    std::wstring result = utils::CreateTempFileW(prefixStr);
    if (result.empty()) return nullptr;

    wchar_t *copy = static_cast<wchar_t *>(malloc((result.length() + 1) * sizeof(wchar_t)));
    if (copy) wcscpy(copy, result.c_str());
    return copy;
}

char *BML_CreateTempFileUtf8(const char *prefix) {
    std::string prefixStr = prefix ? prefix : "";
    std::string result = utils::CreateTempFileUtf8(prefixStr);
    return result.empty() ? nullptr : BML_Strdup(result.c_str());
}

// Directory listing
char **BML_ListFilesA(const char *dir, const char *pattern, size_t *count) {
    if (!dir || !count) {
        if (count) *count = 0;
        return nullptr;
    }

    std::string patternStr = pattern ? pattern : "*";
    std::vector<std::string> result = utils::ListFilesA(dir, patternStr);
    *count = result.size();

    if (result.empty()) return nullptr;

    char **arr = static_cast<char **>(malloc(result.size() * sizeof(char *)));
    if (!arr) {
        *count = 0;
        return nullptr;
    }

    for (size_t i = 0; i < result.size(); ++i) {
        arr[i] = BML_Strdup(result[i].c_str());
        if (!arr[i]) {
            for (size_t j = 0; j < i; ++j) {
                free(arr[j]);
            }
            free(arr);
            *count = 0;
            return nullptr;
        }
    }

    return arr;
}

wchar_t **BML_ListFilesW(const wchar_t *dir, const wchar_t *pattern, size_t *count) {
    if (!dir || !count) {
        if (count) *count = 0;
        return nullptr;
    }

    std::wstring patternStr = pattern ? pattern : L"*";
    std::vector<std::wstring> result = utils::ListFilesW(dir, patternStr);
    *count = result.size();

    if (result.empty()) return nullptr;

    wchar_t **arr = static_cast<wchar_t **>(malloc(result.size() * sizeof(wchar_t *)));
    if (!arr) {
        *count = 0;
        return nullptr;
    }

    for (size_t i = 0; i < result.size(); ++i) {
        arr[i] = static_cast<wchar_t *>(malloc((result[i].length() + 1) * sizeof(wchar_t)));
        if (arr[i]) {
            wcscpy(arr[i], result[i].c_str());
        } else {
            for (size_t j = 0; j < i; ++j) {
                free(arr[j]);
            }
            free(arr);
            *count = 0;
            return nullptr;
        }
    }

    return arr;
}

char **BML_ListFilesUtf8(const char *dir, const char *pattern, size_t *count) {
    if (!dir || !count) {
        if (count) *count = 0;
        return nullptr;
    }

    std::string patternStr = pattern ? pattern : "*";
    std::vector<std::string> result = utils::ListFilesUtf8(dir, patternStr);
    *count = result.size();

    if (result.empty()) return nullptr;

    char **arr = static_cast<char **>(malloc(result.size() * sizeof(char *)));
    if (!arr) {
        *count = 0;
        return nullptr;
    }

    for (size_t i = 0; i < result.size(); ++i) {
        arr[i] = BML_Strdup(result[i].c_str());
        if (!arr[i]) {
            for (size_t j = 0; j < i; ++j) {
                free(arr[j]);
            }
            free(arr);
            *count = 0;
            return nullptr;
        }
    }

    return arr;
}

char **BML_ListDirectoriesA(const char *dir, const char *pattern, size_t *count) {
    if (!dir || !count) {
        if (count) *count = 0;
        return nullptr;
    }

    std::string patternStr = pattern ? pattern : "*";
    std::vector<std::string> result = utils::ListDirectoriesA(dir, patternStr);
    *count = result.size();

    if (result.empty()) return nullptr;

    char **arr = static_cast<char **>(malloc(result.size() * sizeof(char *)));
    if (!arr) {
        *count = 0;
        return nullptr;
    }

    for (size_t i = 0; i < result.size(); ++i) {
        arr[i] = BML_Strdup(result[i].c_str());
        if (!arr[i]) {
            for (size_t j = 0; j < i; ++j) {
                free(arr[j]);
            }
            free(arr);
            *count = 0;
            return nullptr;
        }
    }

    return arr;
}

wchar_t **BML_ListDirectoriesW(const wchar_t *dir, const wchar_t *pattern, size_t *count) {
    if (!dir || !count) {
        if (count) *count = 0;
        return nullptr;
    }

    std::wstring patternStr = pattern ? pattern : L"*";
    std::vector<std::wstring> result = utils::ListDirectoriesW(dir, patternStr);
    *count = result.size();

    if (result.empty()) return nullptr;

    wchar_t **arr = static_cast<wchar_t **>(malloc(result.size() * sizeof(wchar_t *)));
    if (!arr) {
        *count = 0;
        return nullptr;
    }

    for (size_t i = 0; i < result.size(); ++i) {
        arr[i] = static_cast<wchar_t *>(malloc((result[i].length() + 1) * sizeof(wchar_t)));
        if (arr[i]) {
            wcscpy(arr[i], result[i].c_str());
        } else {
            for (size_t j = 0; j < i; ++j) {
                free(arr[j]);
            }
            free(arr);
            *count = 0;
            return nullptr;
        }
    }

    return arr;
}

char **BML_ListDirectoriesUtf8(const char *dir, const char *pattern, size_t *count) {
    if (!dir || !count) {
        if (count) *count = 0;
        return nullptr;
    }

    std::string patternStr = pattern ? pattern : "*";
    std::vector<std::string> result = utils::ListDirectoriesUtf8(dir, patternStr);
    *count = result.size();

    if (result.empty()) return nullptr;

    char **arr = static_cast<char **>(malloc(result.size() * sizeof(char *)));
    if (!arr) {
        *count = 0;
        return nullptr;
    }

    for (size_t i = 0; i < result.size(); ++i) {
        arr[i] = BML_Strdup(result[i].c_str());
        if (!arr[i]) {
            for (size_t j = 0; j < i; ++j) {
                free(arr[j]);
            }
            free(arr);
            *count = 0;
            return nullptr;
        }
    }

    return arr;
}

CKERROR CreateModManager(CKContext *context) {
    new ModManager(context);
    return CK_OK;
}

CKERROR RemoveModManager(CKContext *context) {
    delete ModManager::GetManager(context);
    return CK_OK;
}

CKPluginInfo g_PluginInfo[2];

PLUGIN_EXPORT int CKGetPluginInfoCount() { return 2; }

PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int Index) {
    g_PluginInfo[0].m_Author = "Kakuty";
    g_PluginInfo[0].m_Description = "Building blocks for hooking";
    g_PluginInfo[0].m_Extension = "";
    g_PluginInfo[0].m_Type = CKPLUGIN_BEHAVIOR_DLL;
    g_PluginInfo[0].m_Version = 0x000001;
    g_PluginInfo[0].m_InitInstanceFct = nullptr;
    g_PluginInfo[0].m_ExitInstanceFct = nullptr;
    g_PluginInfo[0].m_GUID = CKGUID(0x3a086b4d, 0x2f4a4f01);
    g_PluginInfo[0].m_Summary = "Building blocks for hooking";

    g_PluginInfo[1].m_Author = "Kakuty";
    g_PluginInfo[1].m_Description = "Mod Manager";
    g_PluginInfo[1].m_Extension = "";
    g_PluginInfo[1].m_Type = CKPLUGIN_MANAGER_DLL;
    g_PluginInfo[1].m_Version = 0x000001;
    g_PluginInfo[1].m_InitInstanceFct = CreateModManager;
    g_PluginInfo[1].m_ExitInstanceFct = RemoveModManager;
    g_PluginInfo[1].m_GUID = MOD_MANAGER_GUID;
    g_PluginInfo[1].m_Summary = "Mod Manager";

    return &g_PluginInfo[Index];
}

PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg);

void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg) {
    RegisterBehavior(reg, FillBehaviorHookBlockDecl);
}

static bool HookCreateCKBehaviorPrototypeRuntime() {
    HMODULE handle = ::GetModuleHandleA("CK2.dll");
    LPVOID lpCreateCKBehaviorPrototypeRunTimeProc =
        (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototypeRunTime@@YAPAVCKBehaviorPrototype@@PAD@Z");
    LPVOID lpCreateCKBehaviorPrototypeProc =
        (LPVOID) ::GetProcAddress(handle, "?CreateCKBehaviorPrototype@@YAPAVCKBehaviorPrototype@@PAD@Z");
    if (MH_CreateHook(lpCreateCKBehaviorPrototypeRunTimeProc, lpCreateCKBehaviorPrototypeProc, nullptr) != MH_OK ||
        MH_EnableHook(lpCreateCKBehaviorPrototypeRunTimeProc) != MH_OK) {
        return false;
    }
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        if (MH_Initialize() != MH_OK) {
            utils::OutputDebugA("Fatal: Unable to initialize MinHook.\n");
            return FALSE;
        }
        if (!RenderHook::HookRenderEngine()) {
            utils::OutputDebugA("Fatal: Unable to hook Render Engine.\n");
            return FALSE;
        }
        if (!HookCreateCKBehaviorPrototypeRuntime()) {
            utils::OutputDebugA("Fatal: Unable to hook CKBehaviorPrototypeRuntime.\n");
            return FALSE;
        }
        break;
    case DLL_PROCESS_DETACH:
        RenderHook::UnhookRenderEngine();
        if (MH_Uninitialize() != MH_OK) {
            utils::OutputDebugA("Fatal: Unable to uninitialize MinHook.\n");
            return FALSE;
        }
        break;
    default:
        break;
    }

    return TRUE;
}
