#include "ScriptLibraryRegistry.h"

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <io.h>
#include <sstream>
#include <unordered_map>

#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

namespace {

std::string FoldAsciiLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool EndsWithInsensitiveW(const std::wstring &value, const wchar_t *suffix) {
    if (!suffix)
        return false;
    const size_t suffixLength = std::wcslen(suffix);
    return value.size() >= suffixLength &&
           _wcsicmp(value.c_str() + value.size() - suffixLength, suffix) == 0;
}

bool IsValidRelativeLibraryPath(const std::string &path) {
    if (path.empty() || path[0] == '/' || path[0] == '\\')
        return false;
    if (path.find('\\') != std::string::npos ||
        path.find(':') != std::string::npos) {
        return false;
    }
    if (!utils::EndsWith(path, ".as"))
        return false;

    size_t start = 0;
    while (start <= path.size()) {
        const size_t slash = path.find('/', start);
        const std::string segment = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (segment.empty() || segment == "." || segment == "..")
            return false;
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return true;
}

std::wstring JoinRelativePathW(const std::wstring &root, const std::string &relativePath) {
    std::wstring path = root;
    size_t start = 0;
    while (start <= relativePath.size()) {
        const size_t slash = relativePath.find('/', start);
        const std::string segmentUtf8 = relativePath.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        path = utils::CombinePathW(path, utils::Utf8ToUtf16(segmentUtf8));
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return path;
}

std::string ParentVirtualDirectory(const std::string &virtualSection) {
    const size_t slash = virtualSection.find_last_of('/');
    if (slash == std::string::npos)
        return {};
    return virtualSection.substr(0, slash + 1);
}

} // namespace

ScriptLibraryRegistry::ScriptLibraryRegistry(std::wstring rootDirectory)
    : m_RootDirectory(std::move(rootDirectory)) {
}

void ScriptLibraryRegistry::SetRootDirectory(std::wstring rootDirectory) {
    m_RootDirectory = std::move(rootDirectory);
    m_Packages.clear();
    m_Index.clear();
}

bool ScriptLibraryRegistry::IsValidLibraryId(const std::string &id) {
    if (id.empty() || id.front() == '.' || id.back() == '.')
        return false;
    bool segmentHasChar = false;
    for (char ch : id) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (ch == '.') {
            if (!segmentHasChar)
                return false;
            segmentHasChar = false;
            continue;
        }
        if ((value >= 'a' && value <= 'z') ||
            (value >= '0' && value <= '9') ||
            ch == '-' ||
            ch == '_') {
            segmentHasChar = true;
            continue;
        }
        return false;
    }
    return segmentHasChar;
}

bool ScriptLibraryRegistry::IsValidLibraryVersion(const std::string &version) {
    if (version.empty())
        return false;
    int part = 0;
    bool digit = false;
    for (char ch : version) {
        if (ch == '.') {
            if (!digit)
                return false;
            ++part;
            digit = false;
            continue;
        }
        if (ch < '0' || ch > '9')
            return false;
        digit = true;
    }
    return digit && part == 2;
}

bool ScriptLibraryRegistry::IsLibraryVirtualPath(const std::string &path) {
    return utils::StartsWith(path, "/bml/libs/");
}

bool ScriptLibraryRegistry::TryParseVirtualInclude(const std::string &include,
                                                   ScriptLibraryInclude &parsed,
                                                   std::string &diagnostic) {
    parsed = ScriptLibraryInclude();
    diagnostic.clear();
    constexpr const char *prefix = "/bml/libs/";
    if (!utils::StartsWith(include, prefix)) {
        diagnostic = "Library include must start with /bml/libs/.";
        return false;
    }

    const size_t specStart = std::char_traits<char>::length(prefix);
    const size_t slash = include.find('/', specStart);
    if (slash == std::string::npos || slash == specStart) {
        diagnostic = "Library include is missing <id>@<version>/<path>.";
        return false;
    }
    const std::string spec = include.substr(specStart, slash - specStart);
    const size_t at = spec.find('@');
    if (at == std::string::npos || at == 0 || at + 1 >= spec.size()) {
        diagnostic = "Library include must use <id>@<version>.";
        return false;
    }

    parsed.Id = spec.substr(0, at);
    parsed.Version = spec.substr(at + 1);
    parsed.RelativePath = include.substr(slash + 1);
    parsed.VirtualSection = include;

    if (!IsValidLibraryId(parsed.Id)) {
        diagnostic = "Invalid script library id '" + parsed.Id + "'.";
        return false;
    }
    if (!IsValidLibraryVersion(parsed.Version)) {
        diagnostic = "Invalid script library version '" + parsed.Version + "'.";
        return false;
    }
    if (!IsValidRelativeLibraryPath(parsed.RelativePath)) {
        diagnostic = "Invalid script library relative path '" + parsed.RelativePath + "'.";
        return false;
    }
    return true;
}

bool ScriptLibraryRegistry::ResolveRelativeInclude(const std::string &includingVirtualSection,
                                                   const std::string &relativeInclude,
                                                   std::string &resolvedVirtualSection,
                                                   std::string &diagnostic) {
    resolvedVirtualSection.clear();
    diagnostic.clear();
    if (!IsLibraryVirtualPath(includingVirtualSection)) {
        diagnostic = "Relative library include requires a library-owned including section.";
        return false;
    }
    if (relativeInclude.empty() ||
        relativeInclude[0] == '/' ||
        relativeInclude[0] == '\\' ||
        relativeInclude.find(':') != std::string::npos ||
        relativeInclude.find('\\') != std::string::npos) {
        diagnostic = "Invalid relative library include '" + relativeInclude + "'.";
        return false;
    }

    const std::string base = ParentVirtualDirectory(includingVirtualSection);
    std::vector<std::string> segments;
    size_t start = 0;
    const std::string combined = base + relativeInclude;
    while (start <= combined.size()) {
        const size_t slash = combined.find('/', start);
        std::string segment = combined.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (segment.empty() && start == 0) {
            segments.push_back(std::string());
        } else if (segment.empty() || segment == ".") {
        } else if (segment == "..") {
            if (segments.size() <= 4) {
                diagnostic = "Relative library include escapes the library virtual root.";
                return false;
            }
            segments.pop_back();
        } else {
            segments.push_back(std::move(segment));
        }
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }

    std::ostringstream stream;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0)
            stream << '/';
        stream << segments[i];
    }
    resolvedVirtualSection = stream.str();
    if (!IsLibraryVirtualPath(resolvedVirtualSection)) {
        diagnostic = "Relative library include escaped the library virtual root.";
        return false;
    }
    ScriptLibraryInclude parsed;
    if (!TryParseVirtualInclude(resolvedVirtualSection, parsed, diagnostic))
        return false;
    return true;
}

std::string ScriptLibraryRegistry::MakePackageKey(const std::string &id, const std::string &version) {
    return FoldAsciiLower(id) + "@" + version;
}

void ScriptLibraryRegistry::RebuildIndex() {
    m_Index.clear();
    for (size_t i = 0; i < m_Packages.size(); ++i)
        m_Index.emplace(MakePackageKey(m_Packages[i].Id, m_Packages[i].Version), i);
}

bool ScriptLibraryRegistry::Scan(std::string &diagnostic) {
    diagnostic.clear();
    m_Packages.clear();
    m_Index.clear();
    if (m_RootDirectory.empty() || !utils::DirectoryExistsW(m_RootDirectory))
        return true;

    const std::wstring pattern = m_RootDirectory + L"\\*";
    _wfinddata_t idInfo = {};
    auto idHandle = _wfindfirst(pattern.c_str(), &idInfo);
    if (idHandle == -1)
        return true;

    std::unordered_map<std::string, std::string> seenIds;
    do {
        if ((idInfo.attrib & _A_SUBDIR) == 0 ||
            std::wcscmp(idInfo.name, L".") == 0 ||
            std::wcscmp(idInfo.name, L"..") == 0) {
            continue;
        }

        const std::string id = utils::Utf16ToUtf8(idInfo.name);
        const std::string foldedId = FoldAsciiLower(id);
        auto seenId = seenIds.find(foldedId);
        if (seenId != seenIds.end() && seenId->second != id) {
            diagnostic = "Script library id directory has a Windows case collision: '" +
                         seenId->second + "' and '" + id + "'.";
            _findclose(idHandle);
            return false;
        }
        seenIds.emplace(foldedId, id);
        if (!IsValidLibraryId(id)) {
            diagnostic = "Invalid script library id directory '" + id + "'.";
            _findclose(idHandle);
            return false;
        }
        const std::wstring idRoot = utils::CombinePathW(m_RootDirectory, idInfo.name);
        const std::wstring versionPattern = idRoot + L"\\*";
        _wfinddata_t versionInfo = {};
        auto versionHandle = _wfindfirst(versionPattern.c_str(), &versionInfo);
        if (versionHandle == -1)
            continue;
        do {
            if ((versionInfo.attrib & _A_SUBDIR) == 0 ||
                std::wcscmp(versionInfo.name, L".") == 0 ||
                std::wcscmp(versionInfo.name, L"..") == 0) {
                continue;
            }

            const std::string version = utils::Utf16ToUtf8(versionInfo.name);
            if (!IsValidLibraryVersion(version)) {
                diagnostic = "Invalid script library version directory '" + version + "' under '" + id + "'.";
                _findclose(versionHandle);
                _findclose(idHandle);
                return false;
            }
            const std::string key = MakePackageKey(id, version);
            if (m_Index.find(key) != m_Index.end()) {
                diagnostic = "Duplicate script library package '" + id + "@" + version + "'.";
                _findclose(versionHandle);
                _findclose(idHandle);
                return false;
            }
            ScriptLibraryPackage package;
            package.Id = id;
            package.Version = version;
            package.RootDirectory = utils::ResolvePathW(utils::CombinePathW(idRoot, versionInfo.name));
            package.VirtualRoot = "/bml/libs/" + id + "@" + version + "/";
            m_Index.emplace(key, m_Packages.size());
            m_Packages.push_back(std::move(package));
        } while (_wfindnext(versionHandle, &versionInfo) == 0);
        _findclose(versionHandle);
    } while (_wfindnext(idHandle, &idInfo) == 0);
    _findclose(idHandle);
    return true;
}

bool ScriptLibraryRegistry::FindPackage(const std::string &id,
                                        const std::string &version,
                                        ScriptLibraryPackage &package) const {
    const auto it = m_Index.find(MakePackageKey(id, version));
    if (it == m_Index.end() || it->second >= m_Packages.size())
        return false;
    package = m_Packages[it->second];
    return true;
}

bool ScriptLibraryRegistry::ResolveInclude(const ScriptLibraryInclude &include,
                                           std::wstring &physicalPath,
                                           std::string &diagnostic) const {
    physicalPath.clear();
    diagnostic.clear();
    ScriptLibraryPackage package;
    if (!FindPackage(include.Id, include.Version, package)) {
        diagnostic = "Script library package not found: " + include.Id + "@" + include.Version + ".";
        return false;
    }
    if (!IsValidRelativeLibraryPath(include.RelativePath)) {
        diagnostic = "Invalid script library relative path '" + include.RelativePath + "'.";
        return false;
    }
    physicalPath = utils::ResolvePathW(JoinRelativePathW(package.RootDirectory, include.RelativePath));
    if (!utils::IsPathInsideRootW(physicalPath, package.RootDirectory)) {
        diagnostic = "Script library include escapes package root: " + include.VirtualSection + ".";
        return false;
    }
    if (!EndsWithInsensitiveW(physicalPath, L".as") || !utils::FileExistsW(physicalPath)) {
        diagnostic = "Script library source file not found: " + include.VirtualSection + ".";
        return false;
    }
    return true;
}

} // namespace BML
