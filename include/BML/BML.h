#ifndef BML_H
#define BML_H

#include "BML/Defines.h"
#include "BML/Version.h"

BML_BEGIN_CDECLS

BML_EXPORT void BML_GetVersion(int *major, int *minor, int *patch);
BML_EXPORT const char *BML_GetVersionString();

// Memory management
BML_EXPORT void *BML_Malloc(size_t size);
BML_EXPORT void *BML_Calloc(size_t count, size_t size);
BML_EXPORT void *BML_Realloc(void *ptr, size_t size);
BML_EXPORT void BML_Free(void *ptr);

BML_EXPORT void BML_FreeString(char *str);
BML_EXPORT void BML_FreeWString(wchar_t *wstr);
BML_EXPORT void BML_FreeStringArray(char **strings, size_t count);

BML_EXPORT char *BML_Strdup(const char *str);

// String splitting - returns array and sets count
BML_EXPORT char **BML_SplitString(const char *str, const char *delim, size_t *count);
BML_EXPORT char **BML_SplitStringChar(const char *str, char delim, size_t *count);

// String trimming
BML_EXPORT void BML_TrimString(char *str);
BML_EXPORT char *BML_TrimStringCopy(const char *str);

// String joining
BML_EXPORT char *BML_JoinString(const char **strings, size_t count, const char *delim);
BML_EXPORT char *BML_JoinStringChar(const char **strings, size_t count, char delim);

// Case conversion
BML_EXPORT char *BML_ToLower(const char *str);
BML_EXPORT char *BML_ToUpper(const char *str);

// String comparison functions
BML_EXPORT int BML_StartsWith(const char *str, const char *prefix, int caseSensitive);
BML_EXPORT int BML_EndsWith(const char *str, const char *suffix, int caseSensitive);
BML_EXPORT int BML_Contains(const char *str, const char *substr, int caseSensitive);

// String conversion functions
BML_EXPORT wchar_t *BML_ToWString(const char *str, int isUtf8);
BML_EXPORT char *BML_ToString(const wchar_t *wstr, int toUtf8);

// Legacy conversion functions
BML_EXPORT wchar_t *BML_Utf8ToUtf16(const char *str);
BML_EXPORT char *BML_Utf16ToUtf8(const wchar_t *wstr);
BML_EXPORT wchar_t *BML_AnsiToUtf16(const char *str);
BML_EXPORT char *BML_Utf16ToAnsi(const wchar_t *wstr);

// Hash functions
BML_EXPORT size_t BML_HashString(const char *str);
BML_EXPORT size_t BML_HashWString(const wchar_t *str);

// String escape/unescape
BML_EXPORT char *BML_UnescapeString(const char *str);
BML_EXPORT char *BML_EscapeString(const char *str);
BML_EXPORT char *BML_StripAnsiCodes(const char *str);

// File existence checks
BML_EXPORT int BML_FileExistsA(const char *file);
BML_EXPORT int BML_FileExistsW(const wchar_t *file);
BML_EXPORT int BML_FileExistsUtf8(const char *file);

// Directory existence checks
BML_EXPORT int BML_DirectoryExistsA(const char *dir);
BML_EXPORT int BML_DirectoryExistsW(const wchar_t *dir);
BML_EXPORT int BML_DirectoryExistsUtf8(const char *dir);

// Path existence checks
BML_EXPORT int BML_PathExistsA(const char *path);
BML_EXPORT int BML_PathExistsW(const wchar_t *path);
BML_EXPORT int BML_PathExistsUtf8(const char *path);

// Directory creation
BML_EXPORT int BML_CreateDirectoryA(const char *dir);
BML_EXPORT int BML_CreateDirectoryW(const wchar_t *dir);
BML_EXPORT int BML_CreateDirectoryUtf8(const char *dir);

// Create directory tree
BML_EXPORT int BML_CreateFileTreeA(const char *path);
BML_EXPORT int BML_CreateFileTreeW(const wchar_t *path);
BML_EXPORT int BML_CreateFileTreeUtf8(const char *path);

// File deletion
BML_EXPORT int BML_DeleteFileA(const char *path);
BML_EXPORT int BML_DeleteFileW(const wchar_t *path);
BML_EXPORT int BML_DeleteFileUtf8(const char *path);

// Directory deletion
BML_EXPORT int BML_DeleteDirectoryA(const char *path);
BML_EXPORT int BML_DeleteDirectoryW(const wchar_t *path);
BML_EXPORT int BML_DeleteDirectoryUtf8(const char *path);

// File copying
BML_EXPORT int BML_CopyFileA(const char *path, const char *dest);
BML_EXPORT int BML_CopyFileW(const wchar_t *path, const wchar_t *dest);
BML_EXPORT int BML_CopyFileUtf8(const char *path, const char *dest);

// File moving
BML_EXPORT int BML_MoveFileA(const char *path, const char *dest);
BML_EXPORT int BML_MoveFileW(const wchar_t *path, const wchar_t *dest);
BML_EXPORT int BML_MoveFileUtf8(const char *path, const char *dest);

// Zip extraction
BML_EXPORT int BML_ExtractZipA(const char *path, const char *dest);
BML_EXPORT int BML_ExtractZipW(const wchar_t *path, const wchar_t *dest);
BML_EXPORT int BML_ExtractZipUtf8(const char *path, const char *dest);

// Path manipulation
BML_EXPORT char *BML_GetDriveA(const char *path);
BML_EXPORT wchar_t *BML_GetDriveW(const wchar_t *path);
BML_EXPORT char *BML_GetDriveUtf8(const char *path);

BML_EXPORT char *BML_GetDirectoryA(const char *path);
BML_EXPORT wchar_t *BML_GetDirectoryW(const wchar_t *path);
BML_EXPORT char *BML_GetDirectoryUtf8(const char *path);

// Drive and directory as separate outputs
BML_EXPORT int BML_GetDriveAndDirectoryA(const char *path, char **drive, char **directory);
BML_EXPORT int BML_GetDriveAndDirectoryW(const wchar_t *path, wchar_t **drive, wchar_t **directory);
BML_EXPORT int BML_GetDriveAndDirectoryUtf8(const char *path, char **drive, char **directory);

BML_EXPORT char *BML_GetFileNameA(const char *path);
BML_EXPORT wchar_t *BML_GetFileNameW(const wchar_t *path);
BML_EXPORT char *BML_GetFileNameUtf8(const char *path);

BML_EXPORT char *BML_GetExtensionA(const char *path);
BML_EXPORT wchar_t *BML_GetExtensionW(const wchar_t *path);
BML_EXPORT char *BML_GetExtensionUtf8(const char *path);

BML_EXPORT char *BML_RemoveExtensionA(const char *path);
BML_EXPORT wchar_t *BML_RemoveExtensionW(const wchar_t *path);
BML_EXPORT char *BML_RemoveExtensionUtf8(const char *path);

BML_EXPORT char *BML_CombinePathA(const char *path1, const char *path2);
BML_EXPORT wchar_t *BML_CombinePathW(const wchar_t *path1, const wchar_t *path2);
BML_EXPORT char *BML_CombinePathUtf8(const char *path1, const char *path2);

BML_EXPORT char *BML_NormalizePathA(const char *path);
BML_EXPORT wchar_t *BML_NormalizePathW(const wchar_t *path);
BML_EXPORT char *BML_NormalizePathUtf8(const char *path);

// Path validation
BML_EXPORT int BML_IsPathValidA(const char *path);
BML_EXPORT int BML_IsPathValidW(const wchar_t *path);
BML_EXPORT int BML_IsPathValidUtf8(const char *path);

// Path type checks
BML_EXPORT int BML_IsAbsolutePathA(const char *path);
BML_EXPORT int BML_IsAbsolutePathW(const wchar_t *path);
BML_EXPORT int BML_IsAbsolutePathUtf8(const char *path);

BML_EXPORT int BML_IsRelativePathA(const char *path);
BML_EXPORT int BML_IsRelativePathW(const wchar_t *path);
BML_EXPORT int BML_IsRelativePathUtf8(const char *path);

BML_EXPORT int BML_IsPathRootedA(const char *path);
BML_EXPORT int BML_IsPathRootedW(const wchar_t *path);
BML_EXPORT int BML_IsPathRootedUtf8(const char *path);

// Path resolution
BML_EXPORT char *BML_ResolvePathA(const char *path);
BML_EXPORT wchar_t *BML_ResolvePathW(const wchar_t *path);
BML_EXPORT char *BML_ResolvePathUtf8(const char *path);

BML_EXPORT char *BML_MakeRelativePathA(const char *path, const char *basePath);
BML_EXPORT wchar_t *BML_MakeRelativePathW(const wchar_t *path, const wchar_t *basePath);
BML_EXPORT char *BML_MakeRelativePathUtf8(const char *path, const char *basePath);

// System paths
BML_EXPORT char *BML_GetTempPathA();
BML_EXPORT wchar_t *BML_GetTempPathW();
BML_EXPORT char *BML_GetTempPathUtf8();

BML_EXPORT char *BML_GetCurrentDirectoryA();
BML_EXPORT wchar_t *BML_GetCurrentDirectoryW();
BML_EXPORT char *BML_GetCurrentDirectoryUtf8();

BML_EXPORT int BML_SetCurrentDirectoryA(const char *path);
BML_EXPORT int BML_SetCurrentDirectoryW(const wchar_t *path);
BML_EXPORT int BML_SetCurrentDirectoryUtf8(const char *path);

BML_EXPORT char *BML_GetExecutablePathA();
BML_EXPORT wchar_t *BML_GetExecutablePathW();
BML_EXPORT char *BML_GetExecutablePathUtf8();

// File properties
BML_EXPORT int64_t BML_GetFileSizeA(const char *path);
BML_EXPORT int64_t BML_GetFileSizeW(const wchar_t *path);
BML_EXPORT int64_t BML_GetFileSizeUtf8(const char *path);

// File time - returns file times via output parameters
BML_EXPORT int BML_GetFileTimeA(const char *path, int64_t *creationTime, int64_t *lastAccessTime, int64_t *lastWriteTime);
BML_EXPORT int BML_GetFileTimeW(const wchar_t *path, int64_t *creationTime, int64_t *lastAccessTime, int64_t *lastWriteTime);
BML_EXPORT int BML_GetFileTimeUtf8(const char *path, int64_t *creationTime, int64_t *lastAccessTime, int64_t *lastWriteTime);

// File I/O
BML_EXPORT char *BML_ReadTextFileA(const char *path);
BML_EXPORT wchar_t *BML_ReadTextFileW(const wchar_t *path);
BML_EXPORT char *BML_ReadTextFileUtf8(const char *path);

BML_EXPORT int BML_WriteTextFileA(const char *path, const char *content);
BML_EXPORT int BML_WriteTextFileW(const wchar_t *path, const wchar_t *content);
BML_EXPORT int BML_WriteTextFileUtf8(const char *path, const char *content);

// Binary file I/O - data and size via output parameters
BML_EXPORT int BML_ReadBinaryFileA(const char *path, uint8_t **data, size_t *size);
BML_EXPORT int BML_ReadBinaryFileW(const wchar_t *path, uint8_t **data, size_t *size);
BML_EXPORT int BML_ReadBinaryFileUtf8(const char *path, uint8_t **data, size_t *size);

BML_EXPORT int BML_WriteBinaryFileA(const char *path, const uint8_t *data, size_t size);
BML_EXPORT int BML_WriteBinaryFileW(const wchar_t *path, const uint8_t *data, size_t size);
BML_EXPORT int BML_WriteBinaryFileUtf8(const char *path, const uint8_t *data, size_t size);

// Create temporary files
BML_EXPORT char *BML_CreateTempFileA(const char *prefix);
BML_EXPORT wchar_t *BML_CreateTempFileW(const wchar_t *prefix);
BML_EXPORT char *BML_CreateTempFileUtf8(const char *prefix);

// Directory listing - returns array and sets count
BML_EXPORT char **BML_ListFilesA(const char *dir, const char *pattern, size_t *count);
BML_EXPORT wchar_t **BML_ListFilesW(const wchar_t *dir, const wchar_t *pattern, size_t *count);
BML_EXPORT char **BML_ListFilesUtf8(const char *dir, const char *pattern, size_t *count);

BML_EXPORT char **BML_ListDirectoriesA(const char *dir, const char *pattern, size_t *count);
BML_EXPORT wchar_t **BML_ListDirectoriesW(const wchar_t *dir, const wchar_t *pattern, size_t *count);
BML_EXPORT char **BML_ListDirectoriesUtf8(const char *dir, const char *pattern, size_t *count);

BML_END_CDECLS

#endif //BML_H
