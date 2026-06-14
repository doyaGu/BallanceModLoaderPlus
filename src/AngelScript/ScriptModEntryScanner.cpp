#include "ScriptModEntryScanner.h"

#include <cctype>
#include <cwchar>
#include <io.h>

#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

namespace {

std::wstring RemoveScriptEntrySuffix(const std::wstring &entryPath) {
    std::wstring fileName = utils::GetFileNameW(entryPath);
    constexpr wchar_t suffix[] = L".mod.as";
    constexpr size_t suffixLength = (sizeof(suffix) / sizeof(suffix[0])) - 1;
    if (fileName.size() > suffixLength &&
        _wcsicmp(fileName.c_str() + fileName.size() - suffixLength, suffix) == 0) {
        fileName.resize(fileName.size() - suffixLength);
    }
    return fileName;
}

ScriptModLoadCandidate MakeSingleFileCandidate(const std::wstring &entryPath) {
    const std::wstring parent = utils::GetDirectoryW(entryPath);
    const std::wstring stem = RemoveScriptEntrySuffix(entryPath);

    ScriptModLoadCandidate candidate;
    candidate.SourceKind = ScriptModEntrySourceKind::SingleFile;
    candidate.SourcePath = entryPath;
    candidate.RootDirectory = utils::CombinePathW(parent, stem);
    candidate.ResourceRootDirectory = candidate.RootDirectory;
    candidate.EntryPaths.push_back(entryPath);
    candidate.SyntheticId = MakeSyntheticScriptModId(candidate.RootDirectory);
    return candidate;
}

} // namespace

bool IsScriptModEntryName(const wchar_t *name) {
    if (!name || name[0] == L'\0')
        return false;
    const std::wstring value = name;
    constexpr wchar_t suffix[] = L".mod.as";
    constexpr size_t suffixLength = (sizeof(suffix) / sizeof(suffix[0])) - 1;
    return value.size() > suffixLength &&
           _wcsicmp(value.c_str() + value.size() - suffixLength, suffix) == 0;
}

std::vector<std::wstring> FindScriptModEntryPaths(const std::wstring &directory) {
    std::vector<std::wstring> entries;
    if (directory.empty() || !utils::DirectoryExistsW(directory))
        return entries;

    const std::wstring pattern = directory + L"\\*.mod.as";
    _wfinddata_t fileinfo = {};
    auto handle = _wfindfirst(pattern.c_str(), &fileinfo);
    if (handle == -1)
        return entries;

    do {
        if ((fileinfo.attrib & _A_SUBDIR) == 0 && IsScriptModEntryName(fileinfo.name))
            entries.push_back(directory + L"\\" + fileinfo.name);
    } while (_wfindnext(handle, &fileinfo) == 0);

    _findclose(handle);
    return entries;
}

size_t FindScriptModCandidates(const std::wstring &path, std::vector<ScriptModLoadCandidate> &candidates) {
    if (path.empty() || !utils::DirectoryExistsW(path))
        return 0;

    const std::wstring pattern = path + L"\\*";
    _wfinddata_t fileinfo = {};
    auto handle = _wfindfirst(pattern.c_str(), &fileinfo);
    if (handle == -1)
        return candidates.size();

    do {
        if ((fileinfo.attrib & _A_SUBDIR) == 0) {
            if (IsScriptModEntryName(fileinfo.name))
                candidates.push_back(MakeSingleFileCandidate(path + L"\\" + fileinfo.name));
            continue;
        }
        if (wcscmp(fileinfo.name, L".") == 0 || wcscmp(fileinfo.name, L"..") == 0)
            continue;

        const std::wstring child = path + L"\\" + fileinfo.name;
        std::vector<std::wstring> entries = FindScriptModEntryPaths(child);
        if (!entries.empty()) {
            ScriptModLoadCandidate candidate = MakeDirectoryScriptModCandidate(
                child,
                ScriptModEntrySourceKind::Directory,
                child);
            candidate.EntryPaths = std::move(entries);
            candidates.push_back(std::move(candidate));
        }
    } while (_wfindnext(handle, &fileinfo) == 0);

    _findclose(handle);
    return candidates.size();
}

std::string MakeSyntheticScriptModId(const std::wstring &rootDirectory) {
    const std::wstring directoryName = utils::GetFileNameW(rootDirectory);
    return "script:" + utils::Utf16ToUtf8(directoryName.empty() ? rootDirectory : directoryName);
}

std::string MakeSyntheticScriptModuleName(const std::wstring &rootDirectory) {
    const std::wstring directoryName = utils::GetFileNameW(rootDirectory);
    std::string suffix = utils::Utf16ToUtf8(directoryName.empty() ? rootDirectory : directoryName);
    for (char &ch : suffix) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (!std::isalnum(value) && ch != '.' && ch != '_' && ch != '-')
            ch = '_';
    }
    return "bml.script.entry." + suffix;
}

ScriptModLoadCandidate MakeDirectoryScriptModCandidate(const std::wstring &rootDirectory,
                                                       ScriptModEntrySourceKind sourceKind,
                                                       const std::wstring &sourcePath) {
    ScriptModLoadCandidate candidate;
    candidate.SourceKind = sourceKind;
    candidate.SourcePath = sourcePath.empty() ? rootDirectory : sourcePath;
    candidate.RootDirectory = rootDirectory;
    candidate.ResourceRootDirectory = rootDirectory;
    candidate.EntryPaths = FindScriptModEntryPaths(rootDirectory);
    candidate.SyntheticId = MakeSyntheticScriptModId(rootDirectory);
    return candidate;
}

ScriptModEntry MakeScriptModEntry(const ScriptModLoadCandidate &candidate, const std::wstring &entryPath) {
    ScriptModEntry entry;
    entry.SourceKind = candidate.SourceKind;
    entry.SourcePath = candidate.SourcePath;
    entry.RootDirectory = candidate.RootDirectory;
    entry.EntryPath = entryPath;
    entry.ResourceRootDirectory = candidate.ResourceRootDirectory.empty()
                                      ? candidate.RootDirectory
                                      : candidate.ResourceRootDirectory;
    entry.EntryFilename = utils::Utf16ToUtf8(utils::GetFileNameW(entryPath));
    entry.SyntheticId = candidate.SyntheticId.empty()
                            ? MakeSyntheticScriptModId(candidate.RootDirectory)
                            : candidate.SyntheticId;
    return entry;
}

} // namespace BML
