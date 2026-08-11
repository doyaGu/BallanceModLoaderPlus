// The loader's exported C functions: its version, its heap, and the string, path,
// and file helpers it already carries. They are plain C, so unlike the classes in
// IBML.h and IMod.h they do not depend on the compiler or the standard library a
// Mod is built with, and they can be called from a Mod written in any language that
// can load a DLL.
//
// Three rules cover almost everything here.
//
// Whoever gets a pointer back frees it. Every char *, wchar_t *, and array returned
// below was allocated on the loader's heap, and only the BML_Free family releases
// it: BML_Free, BML_FreeString, and BML_FreeWString are the same operation, while
// BML_FreeStringArray and BML_FreeWStringArray also free each element. The BML_MALLOC
// macros in Defines.h are a different heap, the caller's own, and mixing the two
// corrupts one of them.
//
// The int-returning functions answer 1 for success or true and 0 for failure or
// false. They do not use BML_OK and the BML_ERROR_ codes, and they report nothing
// beyond that 0, so a failed call and a null argument look alike. The functions
// that return a pointer return null instead, and the size queries return -1.
// BML_UnregisterCommand is the one exception: it answers BML_OK or one of the
// BML_ERROR_ codes, because it refuses for more than one reason and the caller has
// to be able to tell them apart.
//
// The A, W, and Utf8 spellings of one name differ only in how they read the strings
// handed to them: A takes the process code page, W takes UTF-16, and Utf8 takes
// UTF-8. Ballance itself is a code page program, so a path that came from the game
// belongs in the A form and a path that came from a config file or the network
// belongs in the Utf8 one.
//
// Almost nothing here touches loader or game state, so unlike the rest of the SDK
// these may be called from any thread. There are four exceptions.
// BML_SetCurrentDirectory moves the whole process and therefore the game with it.
// BML_GetLoaderPath and BML_GetModRoot read what the loader resolved at startup,
// so both answer null before it has initialized. BML_GetModRoot also takes the
// lock that guards the Mod registry, which makes it safe beside a Mod being
// loaded or unloaded but unsafe from inside DllMain. BML_UnregisterCommand changes
// the command table the console reads, and belongs on the game thread.
#ifndef BML_H
#define BML_H

#include "BML/Defines.h"
#include "BML/Version.h"

BML_BEGIN_CDECLS

// The loader that is actually running, which is not necessarily the one whose
// headers the Mod was compiled against. Compare it against BML_MAJOR_VERSION and
// its siblings to find out whether a function added in a later release is there.
BML_EXPORT void BML_GetVersion(int *major, int *minor, int *patch);
BML_EXPORT const char *BML_GetVersionString();

// Where the loader put its own directories, and where a Mod is installed. These
// are the only functions here that ask the loader something; the rest work only on
// the strings and files they are handed.
//
// BML_GetLoaderPath takes one of the five ids below rather than a path, and must
// not be confused with BML_GetDirectory further down, which takes a path apart.
// Its result is const because it is borrowed: the loader owns the string, resolves
// it once at startup, and never changes it, so do not pass it to BML_Free. Only
// the UTF-16 and UTF-8 spellings exist because those are the two forms the loader
// keeps; for a code page copy pass the UTF-16 one through BML_Utf16ToAnsi.
//
// BML_GetModRoot answers with the directory a Mod is installed in, which is where
// its own data files belong. A null or empty modId means the DLL that made the
// call, which is what a Mod normally wants, and is also the only spelling that
// works before the Mod is registered, so it can be used from a constructor. A
// modId asks about another Mod instead: a native Mod answers with the directory of
// the DLL it was loaded from, and a script Mod with its script root. Unlike
// BML_GetLoaderPath these allocate, so release the result with BML_FreeWString or
// BML_FreeString.
//
// Both answer null while the loader is still initializing, when the directory
// could not be resolved, and when modId names no loaded Mod.
typedef enum BML_LoaderDirectory {
    // Where the process was started, read once when the loader initialized. The
    // game does not keep it there: BML_SetCurrentDirectory and the game itself
    // both move the process, and this id keeps answering the original directory.
    BML_DIR_WORKING = 0,
    // A scratch directory of this one run, created empty at startup and deleted
    // when the loader shuts down. Nothing written here survives the process.
    BML_DIR_TEMP = 1,
    // The Ballance installation root, the parent of Bin.
    BML_DIR_GAME = 2,
    // The ModLoader directory under it, which holds Mods, Configs, and
    // ModLoader.log.
    BML_DIR_LOADER = 3,
    // The Configs directory under that, where the loader writes one .cfg per Mod.
    BML_DIR_CONFIG = 4,
} BML_LoaderDirectory;

BML_EXPORT const wchar_t *BML_GetLoaderPathW(BML_LoaderDirectory directory);
BML_EXPORT const char *BML_GetLoaderPathUtf8(BML_LoaderDirectory directory);

BML_EXPORT wchar_t *BML_GetModRootW(const char *modId);
BML_EXPORT char *BML_GetModRootUtf8(const char *modId);

// Taking a console command back out of the command table. IBML::RegisterCommand has
// no counterpart there, so without this a registered command has to stay until the
// process ends: a Mod cannot drop its commands in OnUnload, and cannot register them
// again if it is loaded a second time.
//
// Only the DLL that registered a command may take it away. The loader remembers
// which module called IBML::RegisterCommand and compares it against the module this
// call comes from, so one Mod cannot remove another Mod's command, nor one that a
// script Mod registered, nor one of the loader's own. The name is matched the way
// the console matches it, ignoring case and accepting the alias as well.
//
// Unlike the other int-returning functions in this header this one says why it
// refused: BML_OK, BML_ERROR_INVALID_PARAMETER for a null or empty name,
// BML_ERROR_NOT_FOUND when no command answers to that name, BML_ERROR_ACCESS_DENIED
// when one does but belongs to another module, and BML_ERROR_FAIL before the loader
// has initialized.
//
// The command object stays the Mod's to delete, and deleting it is only safe once
// this has answered BML_OK. Call it from the game thread, which is the thread
// commands run on; this does not wait for one that is executing.
BML_EXPORT int BML_UnregisterCommand(const char *name);

// The loader's heap. A Mod needs these only for memory that crosses the boundary,
// which is what the ModDependency ids do, and for releasing what the functions
// below return. BML_Malloc and BML_Calloc treat a zero size as a failure and return
// null, and BML_Realloc with a zero size frees and returns null, so a null result
// is not always an out-of-memory answer.
BML_EXPORT void *BML_Malloc(size_t size);
BML_EXPORT void *BML_Calloc(size_t count, size_t size);
BML_EXPORT void *BML_Realloc(void *ptr, size_t size);
BML_EXPORT void BML_Free(void *ptr);

BML_EXPORT void BML_FreeString(char *str);
BML_EXPORT void BML_FreeWString(wchar_t *wstr);
BML_EXPORT void BML_FreeStringArray(char **strings, size_t count);
BML_EXPORT void BML_FreeWStringArray(wchar_t **strings, size_t count);

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

// Path manipulation, meaning that all of these take apart or put together the path
// string they are given and touch no file. In particular BML_GetDirectory does not
// report a directory of the loader's: it returns the directory part of path. The
// loader's own directories come from BML_GetLoaderPath near the top of this header.
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

// Directory listing. The names are returned as an array to be released with
// BML_FreeStringArray, and count is always written, including on failure. A null
// return with count 0 is what an empty or unreadable directory gives, so these two
// cases are not told apart. A null pattern means every entry, and the pattern is a
// Windows wildcard matched by the file system, not a regular expression. The entries
// are bare names rather than paths, so combine each with dir before opening it, and
// the listing does not recurse.
BML_EXPORT char **BML_ListFilesA(const char *dir, const char *pattern, size_t *count);
BML_EXPORT wchar_t **BML_ListFilesW(const wchar_t *dir, const wchar_t *pattern, size_t *count);
BML_EXPORT char **BML_ListFilesUtf8(const char *dir, const char *pattern, size_t *count);

BML_EXPORT char **BML_ListDirectoriesA(const char *dir, const char *pattern, size_t *count);
BML_EXPORT wchar_t **BML_ListDirectoriesW(const wchar_t *dir, const wchar_t *pattern, size_t *count);
BML_EXPORT char **BML_ListDirectoriesUtf8(const char *dir, const char *pattern, size_t *count);

BML_END_CDECLS

#endif //BML_H
