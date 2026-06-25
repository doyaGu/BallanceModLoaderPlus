#include "ScriptModHotReloadPathFilter.h"

#include <cwchar>

#include "Utils/PathUtils.h"

namespace BML {

namespace {

bool EndsWithInsensitive(const std::wstring &value, const wchar_t *suffix) {
    if (!suffix)
        return false;
    const size_t suffixLength = std::wcslen(suffix);
    if (value.size() < suffixLength)
        return false;
    return _wcsicmp(value.c_str() + value.size() - suffixLength, suffix) == 0;
}

bool SamePathInsensitive(const std::wstring &left, const std::wstring &right) {
    if (left.empty() || right.empty())
        return false;
    const std::wstring resolvedLeft = utils::ResolvePathW(left);
    const std::wstring resolvedRight = utils::ResolvePathW(right);
    return _wcsicmp(resolvedLeft.c_str(), resolvedRight.c_str()) == 0;
}

bool IsPathInsideOrSameRoot(const std::wstring &path, const std::wstring &root) {
    return SamePathInsensitive(path, root) || utils::IsPathInsideRootW(path, root);
}

bool IsPathUnderWatchedRoot(const ScriptFileWatcherWin32::Event &event,
                            const std::wstring &path) {
    if (path.empty())
        return false;
    if (event.Root.empty())
        return true;
    if (event.Recursive)
        return IsPathInsideOrSameRoot(path, event.Root);
    return SamePathInsensitive(utils::GetDirectoryW(path), event.Root);
}

bool IsDirectoryAffectedByWatchedRoot(const ScriptFileWatcherWin32::Event &event,
                                      const std::wstring &directory) {
    if (directory.empty())
        return false;
    if (event.Root.empty())
        return true;
    if (event.Recursive)
        return IsPathInsideOrSameRoot(directory, event.Root);
    return SamePathInsensitive(directory, event.Root) ||
           SamePathInsensitive(utils::GetDirectoryW(directory), event.Root);
}

bool IsRootLifecycleAction(DWORD action) {
    return action == FILE_ACTION_ADDED ||
           action == FILE_ACTION_REMOVED ||
           action == FILE_ACTION_RENAMED_OLD_NAME ||
           action == FILE_ACTION_RENAMED_NEW_NAME;
}

bool IsSourceRootLifecycleEvent(const ScriptFileWatcherWin32::Event &event,
                                const ScriptModEntry &entry) {
    if (!IsRootLifecycleAction(event.Action))
        return false;

    if (entry.SourceKind == ScriptModEntrySourceKind::Directory)
        return SamePathInsensitive(event.Path, entry.RootDirectory);

    if (entry.SourceKind == ScriptModEntrySourceKind::SingleFile)
        return !entry.ResourceRootDirectory.empty() &&
               SamePathInsensitive(event.Path, entry.ResourceRootDirectory);

    return false;
}

} // namespace

bool ScriptHotReloadOverflowCanAffectEntry(const ScriptFileWatcherWin32::Event &event,
                                           const ScriptModEntry &entry) {
    if (event.Root.empty())
        return true;

    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage)
        return IsPathUnderWatchedRoot(event, entry.SourcePath);

    if (entry.SourceKind == ScriptModEntrySourceKind::SingleFile) {
        return IsPathUnderWatchedRoot(event, entry.EntryPath) ||
               IsDirectoryAffectedByWatchedRoot(event, entry.ResourceRootDirectory);
    }

    return IsDirectoryAffectedByWatchedRoot(event, entry.RootDirectory);
}

bool ScriptHotReloadEventBelongsToEntry(const std::wstring &path,
                                        const ScriptModEntry &entry) {
    if (path.empty())
        return false;

    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage)
        return SamePathInsensitive(path, entry.SourcePath);

    if (entry.SourceKind == ScriptModEntrySourceKind::SingleFile) {
        if (SamePathInsensitive(path, entry.EntryPath))
            return true;
        if (!entry.ResourceRootDirectory.empty() &&
            IsPathInsideOrSameRoot(path, entry.ResourceRootDirectory)) {
            return true;
        }
        return false;
    }

    return !entry.RootDirectory.empty() && IsPathInsideOrSameRoot(path, entry.RootDirectory);
}

bool ScriptHotReloadEventLooksRelevant(const ScriptFileWatcherWin32::Event &event,
                                       const ScriptModEntry &entry) {
    if (event.Overflow)
        return ScriptHotReloadOverflowCanAffectEntry(event, entry);
    if (IsSourceRootLifecycleEvent(event, entry))
        return true;
    if (!ScriptHotReloadEventBelongsToEntry(event.Path, entry))
        return false;

    if (entry.SourceKind == ScriptModEntrySourceKind::ZipPackage)
        return EndsWithInsensitive(event.Path, L".zip") &&
               SamePathInsensitive(event.Path, entry.SourcePath);

    return EndsWithInsensitive(event.Path, L".as");
}

bool ScriptHotReloadEventLooksRelevantToLibraryUse(const ScriptFileWatcherWin32::Event &event,
                                                   const ScriptLibraryUse &library) {
    if (library.RootDirectory.empty())
        return false;

    if (event.Overflow)
        return IsDirectoryAffectedByWatchedRoot(event, library.RootDirectory);

    if (IsRootLifecycleAction(event.Action) &&
        SamePathInsensitive(event.Path, library.RootDirectory)) {
        return true;
    }

    if (!IsPathInsideOrSameRoot(event.Path, library.RootDirectory))
        return false;

    return EndsWithInsensitive(event.Path, L".as");
}

} // namespace BML
