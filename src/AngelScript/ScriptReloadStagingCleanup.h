#ifndef BML_SCRIPTRELOADSTAGINGCLEANUP_H
#define BML_SCRIPTRELOADSTAGINGCLEANUP_H

#include <cstddef>
#include <string>

class Logger;

namespace BML {

struct ScriptReloadCleanupResult {
    size_t Removed = 0;
    size_t Failed = 0;
};

bool TryParseScriptReloadArtifactName(const std::wstring &name,
                                      const wchar_t *marker,
                                      std::wstring *baseName = nullptr);

ScriptReloadCleanupResult CleanupStaleScriptReloadDirectoryChildren(const std::wstring &root,
                                                                    const wchar_t *marker,
                                                                    bool requireSiblingZip,
                                                                    Logger *logger = nullptr);

ScriptReloadCleanupResult CleanupStaleScriptReloadArtifacts(const std::wstring &modsRoot,
                                                            Logger *logger = nullptr);

} // namespace BML

#endif
