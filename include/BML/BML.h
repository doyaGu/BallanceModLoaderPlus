// Standalone C allocation, UTF-8/encoding, filesystem and string utilities.
// Returned allocations are released by the matching BML_Free family. Utility
// predicates return 0/1; versioned runtime services live in BML/Mod.h and the
// related C tables and use BML_OK/negative error codes instead.
// File operations may run on worker threads. Changing the process directory
// affects the game and other threads; prefer absolute paths from Mod Context.
#ifndef BML_H
#define BML_H

#include "BML/Defines.h"
#include "BML/Version.h"

BML_BEGIN_CDECLS

// The loader that is actually running, which is not necessarily the one whose
// headers the Mod was compiled against. Compare it against BML_MAJOR_VERSION and
// its siblings to find out whether a function added in a later release is there.
BML_EXPORT void BML_CDECL BML_GetVersion(int *major, int *minor, int *patch);
BML_EXPORT const char *BML_CDECL BML_GetVersionString(void);

// The loader's heap. A Mod needs these only for memory that crosses the boundary,
// and for releasing what the functions below return. BML_Malloc and BML_Calloc treat a zero size as a failure and return
// null, and BML_Realloc with a zero size frees and returns null, so a null result
// is not always an out-of-memory answer.
BML_EXPORT void *BML_CDECL BML_Malloc(size_t size);
BML_EXPORT void *BML_CDECL BML_Calloc(size_t count, size_t size);
BML_EXPORT void *BML_CDECL BML_Realloc(void *ptr, size_t size);
BML_EXPORT void BML_CDECL BML_Free(void *ptr);

BML_EXPORT void BML_CDECL BML_FreeString(char *str);
BML_EXPORT void BML_CDECL BML_FreeWString(wchar_t *wstr);
BML_EXPORT void BML_CDECL BML_FreeStringArray(char **strings, size_t count);
BML_EXPORT void BML_CDECL BML_FreeWStringArray(wchar_t **strings, size_t count);

BML_EXPORT char *BML_CDECL BML_Strdup(const char *str);

// String splitting - returns array and sets count
BML_EXPORT char **BML_CDECL BML_SplitString(const char *str, const char *delim, size_t *count);
BML_EXPORT char **BML_CDECL BML_SplitStringChar(const char *str, char delim, size_t *count);

// String trimming
BML_EXPORT void BML_CDECL BML_TrimString(char *str);
BML_EXPORT char *BML_CDECL BML_TrimStringCopy(const char *str);

// String joining
BML_EXPORT char *BML_CDECL BML_JoinString(const char **strings, size_t count, const char *delim);
BML_EXPORT char *BML_CDECL BML_JoinStringChar(const char **strings, size_t count, char delim);

// Case conversion
BML_EXPORT char *BML_CDECL BML_ToLower(const char *str);
BML_EXPORT char *BML_CDECL BML_ToUpper(const char *str);

// String comparison functions
BML_EXPORT int BML_CDECL BML_StartsWith(const char *str, const char *prefix, int caseSensitive);
BML_EXPORT int BML_CDECL BML_EndsWith(const char *str, const char *suffix, int caseSensitive);
BML_EXPORT int BML_CDECL BML_Contains(const char *str, const char *substr, int caseSensitive);

// String conversion functions
BML_EXPORT wchar_t *BML_CDECL BML_ToWString(const char *str, int isUtf8);
BML_EXPORT char *BML_CDECL BML_ToString(const wchar_t *wstr, int toUtf8);

// Legacy conversion functions
BML_EXPORT wchar_t *BML_CDECL BML_Utf8ToUtf16(const char *str);
BML_EXPORT char *BML_CDECL BML_Utf16ToUtf8(const wchar_t *wstr);
BML_EXPORT wchar_t *BML_CDECL BML_AnsiToUtf16(const char *str);
BML_EXPORT char *BML_CDECL BML_Utf16ToAnsi(const wchar_t *wstr);

// Hash functions
BML_EXPORT size_t BML_CDECL BML_HashString(const char *str);
BML_EXPORT size_t BML_CDECL BML_HashWString(const wchar_t *str);

// String escape/unescape
BML_EXPORT char *BML_CDECL BML_UnescapeString(const char *str);
BML_EXPORT char *BML_CDECL BML_EscapeString(const char *str);
BML_EXPORT char *BML_CDECL BML_StripAnsiCodes(const char *str);

// File existence checks
BML_EXPORT int BML_CDECL BML_FileExistsA(const char *file);
BML_EXPORT int BML_CDECL BML_FileExistsW(const wchar_t *file);
BML_EXPORT int BML_CDECL BML_FileExistsUtf8(const char *file);

// Directory existence checks
BML_EXPORT int BML_CDECL BML_DirectoryExistsA(const char *dir);
BML_EXPORT int BML_CDECL BML_DirectoryExistsW(const wchar_t *dir);
BML_EXPORT int BML_CDECL BML_DirectoryExistsUtf8(const char *dir);

// Path existence checks
BML_EXPORT int BML_CDECL BML_PathExistsA(const char *path);
BML_EXPORT int BML_CDECL BML_PathExistsW(const wchar_t *path);
BML_EXPORT int BML_CDECL BML_PathExistsUtf8(const char *path);

// Directory creation
BML_EXPORT int BML_CDECL BML_CreateDirectoryA(const char *dir);
BML_EXPORT int BML_CDECL BML_CreateDirectoryW(const wchar_t *dir);
BML_EXPORT int BML_CDECL BML_CreateDirectoryUtf8(const char *dir);

// Create directory tree
BML_EXPORT int BML_CDECL BML_CreateFileTreeA(const char *path);
BML_EXPORT int BML_CDECL BML_CreateFileTreeW(const wchar_t *path);
BML_EXPORT int BML_CDECL BML_CreateFileTreeUtf8(const char *path);

// File deletion
BML_EXPORT int BML_CDECL BML_DeleteFileA(const char *path);
BML_EXPORT int BML_CDECL BML_DeleteFileW(const wchar_t *path);
BML_EXPORT int BML_CDECL BML_DeleteFileUtf8(const char *path);

// Directory deletion
BML_EXPORT int BML_CDECL BML_DeleteDirectoryA(const char *path);
BML_EXPORT int BML_CDECL BML_DeleteDirectoryW(const wchar_t *path);
BML_EXPORT int BML_CDECL BML_DeleteDirectoryUtf8(const char *path);

// File copying
BML_EXPORT int BML_CDECL BML_CopyFileA(const char *path, const char *dest);
BML_EXPORT int BML_CDECL BML_CopyFileW(const wchar_t *path, const wchar_t *dest);
BML_EXPORT int BML_CDECL BML_CopyFileUtf8(const char *path, const char *dest);

// File moving
BML_EXPORT int BML_CDECL BML_MoveFileA(const char *path, const char *dest);
BML_EXPORT int BML_CDECL BML_MoveFileW(const wchar_t *path, const wchar_t *dest);
BML_EXPORT int BML_CDECL BML_MoveFileUtf8(const char *path, const char *dest);

// Zip extraction
BML_EXPORT int BML_CDECL BML_ExtractZipA(const char *path, const char *dest);
BML_EXPORT int BML_CDECL BML_ExtractZipW(const wchar_t *path, const wchar_t *dest);
BML_EXPORT int BML_CDECL BML_ExtractZipUtf8(const char *path, const char *dest);

// Path manipulation, meaning that all of these take apart or put together the path
// string they are given and touch no file. In particular BML_GetDirectory does not
// report a directory of the loader's: it returns the directory part of path. The
// loader's own directories come from the Mod Context GetDirectory operation.
BML_EXPORT char *BML_CDECL BML_GetDriveA(const char *path);
BML_EXPORT wchar_t *BML_CDECL BML_GetDriveW(const wchar_t *path);
BML_EXPORT char *BML_CDECL BML_GetDriveUtf8(const char *path);

BML_EXPORT char *BML_CDECL BML_GetDirectoryA(const char *path);
BML_EXPORT wchar_t *BML_CDECL BML_GetDirectoryW(const wchar_t *path);
BML_EXPORT char *BML_CDECL BML_GetDirectoryUtf8(const char *path);

// Drive and directory as separate outputs
BML_EXPORT int BML_CDECL BML_GetDriveAndDirectoryA(const char *path, char **drive, char **directory);
BML_EXPORT int BML_CDECL BML_GetDriveAndDirectoryW(const wchar_t *path, wchar_t **drive, wchar_t **directory);
BML_EXPORT int BML_CDECL BML_GetDriveAndDirectoryUtf8(const char *path, char **drive, char **directory);

BML_EXPORT char *BML_CDECL BML_GetFileNameA(const char *path);
BML_EXPORT wchar_t *BML_CDECL BML_GetFileNameW(const wchar_t *path);
BML_EXPORT char *BML_CDECL BML_GetFileNameUtf8(const char *path);

BML_EXPORT char *BML_CDECL BML_GetExtensionA(const char *path);
BML_EXPORT wchar_t *BML_CDECL BML_GetExtensionW(const wchar_t *path);
BML_EXPORT char *BML_CDECL BML_GetExtensionUtf8(const char *path);

BML_EXPORT char *BML_CDECL BML_RemoveExtensionA(const char *path);
BML_EXPORT wchar_t *BML_CDECL BML_RemoveExtensionW(const wchar_t *path);
BML_EXPORT char *BML_CDECL BML_RemoveExtensionUtf8(const char *path);

BML_EXPORT char *BML_CDECL BML_CombinePathA(const char *path1, const char *path2);
BML_EXPORT wchar_t *BML_CDECL BML_CombinePathW(const wchar_t *path1, const wchar_t *path2);
BML_EXPORT char *BML_CDECL BML_CombinePathUtf8(const char *path1, const char *path2);

BML_EXPORT char *BML_CDECL BML_NormalizePathA(const char *path);
BML_EXPORT wchar_t *BML_CDECL BML_NormalizePathW(const wchar_t *path);
BML_EXPORT char *BML_CDECL BML_NormalizePathUtf8(const char *path);

// Path validation
BML_EXPORT int BML_CDECL BML_IsPathValidA(const char *path);
BML_EXPORT int BML_CDECL BML_IsPathValidW(const wchar_t *path);
BML_EXPORT int BML_CDECL BML_IsPathValidUtf8(const char *path);

// Path type checks
BML_EXPORT int BML_CDECL BML_IsAbsolutePathA(const char *path);
BML_EXPORT int BML_CDECL BML_IsAbsolutePathW(const wchar_t *path);
BML_EXPORT int BML_CDECL BML_IsAbsolutePathUtf8(const char *path);

BML_EXPORT int BML_CDECL BML_IsRelativePathA(const char *path);
BML_EXPORT int BML_CDECL BML_IsRelativePathW(const wchar_t *path);
BML_EXPORT int BML_CDECL BML_IsRelativePathUtf8(const char *path);

BML_EXPORT int BML_CDECL BML_IsPathRootedA(const char *path);
BML_EXPORT int BML_CDECL BML_IsPathRootedW(const wchar_t *path);
BML_EXPORT int BML_CDECL BML_IsPathRootedUtf8(const char *path);

// Path resolution
BML_EXPORT char *BML_CDECL BML_ResolvePathA(const char *path);
BML_EXPORT wchar_t *BML_CDECL BML_ResolvePathW(const wchar_t *path);
BML_EXPORT char *BML_CDECL BML_ResolvePathUtf8(const char *path);

BML_EXPORT char *BML_CDECL BML_MakeRelativePathA(const char *path, const char *basePath);
BML_EXPORT wchar_t *BML_CDECL BML_MakeRelativePathW(const wchar_t *path, const wchar_t *basePath);
BML_EXPORT char *BML_CDECL BML_MakeRelativePathUtf8(const char *path, const char *basePath);

// System paths
BML_EXPORT char *BML_CDECL BML_GetTempPathA(void);
BML_EXPORT wchar_t *BML_CDECL BML_GetTempPathW(void);
BML_EXPORT char *BML_CDECL BML_GetTempPathUtf8(void);

BML_EXPORT char *BML_CDECL BML_GetCurrentDirectoryA(void);
BML_EXPORT wchar_t *BML_CDECL BML_GetCurrentDirectoryW(void);
BML_EXPORT char *BML_CDECL BML_GetCurrentDirectoryUtf8(void);

BML_EXPORT int BML_CDECL BML_SetCurrentDirectoryA(const char *path);
BML_EXPORT int BML_CDECL BML_SetCurrentDirectoryW(const wchar_t *path);
BML_EXPORT int BML_CDECL BML_SetCurrentDirectoryUtf8(const char *path);

BML_EXPORT char *BML_CDECL BML_GetExecutablePathA(void);
BML_EXPORT wchar_t *BML_CDECL BML_GetExecutablePathW(void);
BML_EXPORT char *BML_CDECL BML_GetExecutablePathUtf8(void);

// File properties
BML_EXPORT int64_t BML_CDECL BML_GetFileSizeA(const char *path);
BML_EXPORT int64_t BML_CDECL BML_GetFileSizeW(const wchar_t *path);
BML_EXPORT int64_t BML_CDECL BML_GetFileSizeUtf8(const char *path);

// File time - returns file times via output parameters
BML_EXPORT int BML_CDECL BML_GetFileTimeA(const char *path, int64_t *creationTime, int64_t *lastAccessTime, int64_t *lastWriteTime);
BML_EXPORT int BML_CDECL BML_GetFileTimeW(const wchar_t *path, int64_t *creationTime, int64_t *lastAccessTime, int64_t *lastWriteTime);
BML_EXPORT int BML_CDECL BML_GetFileTimeUtf8(const char *path, int64_t *creationTime, int64_t *lastAccessTime, int64_t *lastWriteTime);

// File I/O
BML_EXPORT char *BML_CDECL BML_ReadTextFileA(const char *path);
BML_EXPORT wchar_t *BML_CDECL BML_ReadTextFileW(const wchar_t *path);
BML_EXPORT char *BML_CDECL BML_ReadTextFileUtf8(const char *path);

BML_EXPORT int BML_CDECL BML_WriteTextFileA(const char *path, const char *content);
BML_EXPORT int BML_CDECL BML_WriteTextFileW(const wchar_t *path, const wchar_t *content);
BML_EXPORT int BML_CDECL BML_WriteTextFileUtf8(const char *path, const char *content);

// Binary file I/O - data and size via output parameters
BML_EXPORT int BML_CDECL BML_ReadBinaryFileA(const char *path, uint8_t **data, size_t *size);
BML_EXPORT int BML_CDECL BML_ReadBinaryFileW(const wchar_t *path, uint8_t **data, size_t *size);
BML_EXPORT int BML_CDECL BML_ReadBinaryFileUtf8(const char *path, uint8_t **data, size_t *size);

BML_EXPORT int BML_CDECL BML_WriteBinaryFileA(const char *path, const uint8_t *data, size_t size);
BML_EXPORT int BML_CDECL BML_WriteBinaryFileW(const wchar_t *path, const uint8_t *data, size_t size);
BML_EXPORT int BML_CDECL BML_WriteBinaryFileUtf8(const char *path, const uint8_t *data, size_t size);

// Create temporary files
BML_EXPORT char *BML_CDECL BML_CreateTempFileA(const char *prefix);
BML_EXPORT wchar_t *BML_CDECL BML_CreateTempFileW(const wchar_t *prefix);
BML_EXPORT char *BML_CDECL BML_CreateTempFileUtf8(const char *prefix);

// Directory listing. The names are returned as an array to be released with
// BML_FreeStringArray, and count is always written, including on failure. A null
// return with count 0 is what an empty or unreadable directory gives, so these two
// cases are not told apart. A null pattern means every entry, and the pattern is a
// Windows wildcard matched by the file system, not a regular expression. The entries
// are bare names rather than paths, so combine each with dir before opening it, and
// the listing does not recurse.
BML_EXPORT char **BML_CDECL BML_ListFilesA(const char *dir, const char *pattern, size_t *count);
BML_EXPORT wchar_t **BML_CDECL BML_ListFilesW(const wchar_t *dir, const wchar_t *pattern, size_t *count);
BML_EXPORT char **BML_CDECL BML_ListFilesUtf8(const char *dir, const char *pattern, size_t *count);

BML_EXPORT char **BML_CDECL BML_ListDirectoriesA(const char *dir, const char *pattern, size_t *count);
BML_EXPORT wchar_t **BML_CDECL BML_ListDirectoriesW(const wchar_t *dir, const wchar_t *pattern, size_t *count);
BML_EXPORT char **BML_CDECL BML_ListDirectoriesUtf8(const char *dir, const char *pattern, size_t *count);

BML_END_CDECLS

#endif //BML_H
