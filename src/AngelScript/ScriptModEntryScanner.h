#ifndef BML_SCRIPTMODENTRYSCANNER_H
#define BML_SCRIPTMODENTRYSCANNER_H

#include <string>
#include <vector>

namespace BML {

enum class ScriptModEntrySourceKind {
    SingleFile,
    Directory,
    ZipPackage
};

struct ScriptModEntry {
    ScriptModEntrySourceKind SourceKind = ScriptModEntrySourceKind::Directory;
    std::wstring SourcePath;
    std::wstring RootDirectory;
    std::wstring EntryPath;
    std::wstring ResourceRootDirectory;
    std::string EntryFilename;
    std::string SyntheticId;
};

struct ScriptModLoadCandidate {
    ScriptModEntrySourceKind SourceKind = ScriptModEntrySourceKind::Directory;
    std::wstring SourcePath;
    std::wstring RootDirectory;
    std::wstring ResourceRootDirectory;
    std::vector<std::wstring> EntryPaths;
    std::string SyntheticId;
};

bool IsScriptModEntryName(const wchar_t *name);
std::vector<std::wstring> FindScriptModEntryPaths(const std::wstring &directory);
size_t FindScriptModCandidates(const std::wstring &path, std::vector<ScriptModLoadCandidate> &candidates);
ScriptModLoadCandidate MakeDirectoryScriptModCandidate(const std::wstring &rootDirectory,
                                                       ScriptModEntrySourceKind sourceKind,
                                                       const std::wstring &sourcePath);
ScriptModEntry MakeScriptModEntry(const ScriptModLoadCandidate &candidate, const std::wstring &entryPath);
std::string MakeSyntheticScriptModId(const std::wstring &rootDirectory);
std::string MakeSyntheticScriptModuleName(const std::wstring &rootDirectory);

} // namespace BML

#endif
