#include "ScriptReloadStagingCleanup.h"

#include <cwctype>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <io.h>

#include "BML/ILogger.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

namespace {

bool IsReparsePoint(const std::wstring &path) {
    if (path.empty())
        return false;
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool HasSiblingZipForReloadBase(const std::wstring &root, const std::wstring &baseName) {
    if (root.empty() || baseName.empty())
        return false;
    return utils::FileExistsW(utils::CombinePathW(root, baseName + L".zip"));
}

} // namespace

bool TryParseScriptReloadArtifactName(const std::wstring &name,
                                      const wchar_t *marker,
                                      std::wstring *baseName) {
    if (name.empty() || !marker || marker[0] == L'\0')
        return false;

    const std::wstring markerText(marker);
    const size_t markerPos = name.rfind(markerText);
    if (markerPos == std::wstring::npos || markerPos == 0)
        return false;

    const size_t suffixPos = markerPos + markerText.size();
    if (suffixPos >= name.size())
        return false;

    for (size_t i = suffixPos; i < name.size(); ++i) {
        if (!std::iswdigit(name[i]))
            return false;
    }

    if (baseName)
        *baseName = name.substr(0, markerPos);
    return true;
}

ScriptReloadCleanupResult CleanupStaleScriptReloadDirectoryChildren(const std::wstring &root,
                                                                    const wchar_t *marker,
                                                                    bool requireSiblingZip,
                                                                    ILogger *logger) {
    ScriptReloadCleanupResult result;
    if (root.empty() || !marker || !utils::DirectoryExistsW(root))
        return result;

    const std::wstring pattern = utils::CombinePathW(root, L"*");
    _wfinddata_t fileinfo = {};
    auto handle = _wfindfirst(pattern.c_str(), &fileinfo);
    if (handle == -1)
        return result;

    do {
        if ((fileinfo.attrib & _A_SUBDIR) == 0)
            continue;
        if (wcscmp(fileinfo.name, L".") == 0 || wcscmp(fileinfo.name, L"..") == 0)
            continue;

        std::wstring baseName;
        if (!TryParseScriptReloadArtifactName(fileinfo.name, marker, &baseName))
            continue;
        if (requireSiblingZip && !HasSiblingZipForReloadBase(root, baseName))
            continue;

        const std::wstring stale = utils::CombinePathW(root, fileinfo.name);
        if (IsReparsePoint(stale))
            continue;

        if (utils::DeleteDirectoryW(stale)) {
            ++result.Removed;
        } else {
            ++result.Failed;
            if (logger) {
                logger->Warn("Failed to remove stale script reload directory: %s",
                             utils::Utf16ToUtf8(stale).c_str());
            }
        }
    } while (_wfindnext(handle, &fileinfo) == 0);

    _findclose(handle);
    return result;
}

ScriptReloadCleanupResult CleanupStaleScriptReloadArtifacts(const std::wstring &modsRoot,
                                                            ILogger *logger) {
    ScriptReloadCleanupResult result;
    ScriptReloadCleanupResult snapshotResult =
        CleanupStaleScriptReloadDirectoryChildren(utils::CombinePathW(utils::GetTempPathW(), L"BMLScriptReload"),
                                                  L".snapshot.",
                                                  false,
                                                  logger);
    result.Removed += snapshotResult.Removed;
    result.Failed += snapshotResult.Failed;

    ScriptReloadCleanupResult zipResult =
        CleanupStaleScriptReloadDirectoryChildren(modsRoot,
                                                  L".reload.",
                                                  true,
                                                  logger);
    result.Removed += zipResult.Removed;
    result.Failed += zipResult.Failed;
    return result;
}

} // namespace BML
