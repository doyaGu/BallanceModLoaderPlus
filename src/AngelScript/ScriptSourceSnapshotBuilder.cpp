#include "ScriptSourceSnapshotBuilder.h"

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <io.h>
#include <set>
#include <sstream>
#include <unordered_set>

#include "Utils/CryptoUtils.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

namespace {

bool EndsWithInsensitiveW(const std::wstring &value, const wchar_t *suffix) {
    if (!suffix)
        return false;
    const size_t suffixLength = std::wcslen(suffix);
    return value.size() >= suffixLength &&
           _wcsicmp(value.c_str() + value.size() - suffixLength, suffix) == 0;
}

std::wstring FoldPathKeyW(std::wstring path) {
    path = utils::ResolvePathW(path);
    std::replace(path.begin(), path.end(), L'/', L'\\');
    std::transform(path.begin(), path.end(), path.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return path;
}

std::string FoldSectionKey(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string ToScriptSectionNameUtf8(const std::wstring &path,
                                    const std::wstring &root) {
    std::wstring relative;
    if (!root.empty() && utils::IsPathInsideRootW(path, root)) {
        relative = utils::MakeRelativePathW(path, root);
    }
    if (relative.empty() || relative == L".")
        relative = utils::GetFileNameW(path);

    std::string section = utils::Utf16ToUtf8(relative);
    for (char &ch : section) {
        if (ch == '\\')
            ch = '/';
    }
    return section;
}

bool RegisterSectionKey(const std::string &sectionName,
                        size_t index,
                        std::unordered_map<std::string, size_t> &sectionIndex,
                        ScriptDiagnostic &diagnostic) {
    const std::string key = FoldSectionKey(sectionName);
    const auto existing = sectionIndex.find(key);
    if (existing != sectionIndex.end()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Duplicate script source section: " + sectionName + ".");
        diagnostic.EntryPath = sectionName;
        return false;
    }
    sectionIndex.emplace(key, index);
    return true;
}

bool AddScriptSourceSection(const std::wstring &path,
                            const std::wstring &sectionRoot,
                            std::set<std::wstring> &seen,
                            ScriptSourceSnapshot &snapshot,
                            std::unordered_map<std::string, size_t> &sectionIndex,
                            ScriptDiagnostic &diagnostic) {
    const std::wstring key = FoldPathKeyW(path);
    if (!seen.insert(key).second)
        return true;

    std::string code;
    const std::string pathUtf8 = utils::Utf16ToUtf8(path);
    if (!utils::ReadFileBytesUtf8(pathUtf8, code)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Failed to read script source into source snapshot.");
        diagnostic.EntryPath = pathUtf8;
        return false;
    }

    ScriptSourceSection section;
    section.Name = ToScriptSectionNameUtf8(path, sectionRoot);
    section.Code = std::move(code);
    if (!RegisterSectionKey(section.Name, snapshot.Sections.size(), sectionIndex, diagnostic))
        return false;

    ScriptSourceDependency dependency;
    dependency.PhysicalPath = utils::ResolvePathW(path);
    dependency.VirtualSection = section.Name;
    dependency.ContentHash = utils::Sha256Hex(section.Code);
    snapshot.Dependencies.push_back(std::move(dependency));
    snapshot.Sections.push_back(std::move(section));
    return true;
}

bool AddScriptSourceDirectory(const std::wstring &root,
                              const std::wstring &sectionRoot,
                              const std::wstring &entryPath,
                              bool includeModEntries,
                              std::set<std::wstring> &seen,
                              ScriptSourceSnapshot &snapshot,
                              std::unordered_map<std::string, size_t> &sectionIndex,
                              ScriptDiagnostic &diagnostic) {
    if (root.empty() || !utils::DirectoryExistsW(root))
        return true;

    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            if (ec)
                break;
            continue;
        }
        const std::wstring path = it->path().wstring();
        if (!EndsWithInsensitiveW(path, L".as"))
            continue;
        if (!includeModEntries &&
            IsScriptModEntryName(utils::GetFileNameW(path).c_str()) &&
            FoldPathKeyW(path) != FoldPathKeyW(entryPath)) {
            continue;
        }
        if (!AddScriptSourceSection(path, sectionRoot, seen, snapshot, sectionIndex, diagnostic))
            return false;
    }

    if (ec) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Failed to enumerate script source snapshot directory.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(root);
        return false;
    }
    return true;
}

bool IsLineStartDirective(const std::string &code, size_t hash) {
    size_t pos = hash;
    while (pos > 0) {
        const char prev = code[pos - 1];
        if (prev == '\n' || prev == '\r')
            break;
        if (!std::isspace(static_cast<unsigned char>(prev)))
            return false;
        --pos;
    }
    return true;
}

bool ParseIncludeDirectiveAt(const std::string &code, size_t hash, ScriptIncludeDirective &directive) {
    constexpr const char *includeWord = "include";
    size_t pos = hash + 1;
    while (pos < code.size() && std::isspace(static_cast<unsigned char>(code[pos])))
        ++pos;
    for (size_t i = 0; includeWord[i] != '\0'; ++i) {
        if (pos + i >= code.size() || code[pos + i] != includeWord[i])
            return false;
    }
    pos += std::char_traits<char>::length(includeWord);
    if (pos < code.size() &&
        (std::isalnum(static_cast<unsigned char>(code[pos])) || code[pos] == '_')) {
        return false;
    }
    while (pos < code.size() && std::isspace(static_cast<unsigned char>(code[pos])))
        ++pos;
    directive.Offset = hash;
    directive.Quoted = false;
    directive.Include.clear();
    if (pos >= code.size())
        return true;
    if (code[pos] != '"')
        return true;
    directive.Quoted = true;
    ++pos;
    while (pos < code.size()) {
        const char ch = code[pos++];
        if (ch == '"')
            return true;
        if (ch == '\\' && pos < code.size()) {
            directive.Include.push_back(code[pos++]);
            continue;
        }
        directive.Include.push_back(ch);
    }
    return true;
}

std::string LibraryUseKey(const std::string &id, const std::string &version) {
    return id + "@" + version;
}

bool ResolveLocalIncludeSectionName(const std::string &includingSection,
                                    const std::string &include,
                                    std::string &resolvedSection) {
    resolvedSection.clear();
    if (include.empty() ||
        include[0] == '/' ||
        include.find(':') != std::string::npos) {
        return false;
    }

    std::string normalizedInclude = include;
    std::replace(normalizedInclude.begin(), normalizedInclude.end(), '\\', '/');

    std::vector<std::string> segments;
    const size_t parentSlash = includingSection.find_last_of('/');
    if (parentSlash != std::string::npos) {
        size_t start = 0;
        const std::string parent = includingSection.substr(0, parentSlash);
        while (start <= parent.size()) {
            const size_t slash = parent.find('/', start);
            std::string segment = parent.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
            if (!segment.empty())
                segments.push_back(std::move(segment));
            if (slash == std::string::npos)
                break;
            start = slash + 1;
        }
    }

    size_t start = 0;
    while (start <= normalizedInclude.size()) {
        const size_t slash = normalizedInclude.find('/', start);
        std::string segment = normalizedInclude.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (segment.empty() || segment == ".") {
        } else if (segment == "..") {
            if (segments.empty())
                return false;
            segments.pop_back();
        } else {
            segments.push_back(std::move(segment));
        }
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }

    if (segments.empty())
        return false;

    std::ostringstream stream;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i != 0)
            stream << '/';
        stream << segments[i];
    }
    resolvedSection = stream.str();
    return true;
}

} // namespace

ScriptSourceSnapshotBuilder::ScriptSourceSnapshotBuilder(ScriptLibraryRegistry registry)
    : m_LibraryRegistry(std::move(registry)) {
}

bool ScriptLibrarySourceCache::CapturePackage(const ScriptLibraryRegistry &registry,
                                              const std::string &id,
                                              const std::string &version,
                                              ScriptDiagnostic &diagnostic) {
    const std::string packageKey = LibraryUseKey(id, version);
    if (m_CapturedPackages.find(packageKey) != m_CapturedPackages.end())
        return true;

    ScriptLibraryPackage package;
    if (!registry.FindPackage(id, version, package)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Script library package not found: " + id + "@" + version + ".");
        diagnostic.EntryPath = id + "@" + version;
        return false;
    }

    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(package.RootDirectory, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            if (ec)
                break;
            continue;
        }

        const std::wstring path = it->path().wstring();
        if (!EndsWithInsensitiveW(path, L".as"))
            continue;

        const std::wstring resolved = utils::ResolvePathW(path);
        if (!utils::IsPathInsideRootW(resolved, package.RootDirectory)) {
            diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                              "Script library source escapes package root while capturing batch source.");
            diagnostic.EntryPath = utils::Utf16ToUtf8(path);
            return false;
        }

        std::string code;
        if (!utils::ReadFileBytesUtf8(utils::Utf16ToUtf8(resolved), code)) {
            diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                              "Failed to read script library source while capturing batch source.");
            diagnostic.EntryPath = utils::Utf16ToUtf8(path);
            return false;
        }
        m_Files[FoldPathKeyW(resolved)] = std::move(code);
    }

    if (ec) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Failed to enumerate script library package while capturing batch source.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(package.RootDirectory);
        return false;
    }
    m_CapturedPackages.insert(packageKey);
    return true;
}

bool ScriptLibrarySourceCache::ReadFileUtf8(const std::wstring &physicalPath,
                                            const std::string &virtualSection,
                                            std::string &code,
                                            ScriptDiagnostic &diagnostic) {
    const std::wstring resolved = utils::ResolvePathW(physicalPath);
    const std::wstring key = FoldPathKeyW(resolved);
    const auto cached = m_Files.find(key);
    if (cached != m_Files.end()) {
        code = cached->second;
        return true;
    }

    if (!utils::ReadFileBytesUtf8(utils::Utf16ToUtf8(resolved), code)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Failed to read script library source: " + virtualSection + ".");
        diagnostic.EntryPath = virtualSection;
        return false;
    }
    m_Files.emplace(key, code);
    return true;
}

void ScriptSourceSnapshotBuilder::SetLibraryRegistry(ScriptLibraryRegistry registry) {
    m_LibraryRegistry = std::move(registry);
}

void ScriptSourceSnapshotBuilder::SetLibrarySourceCache(ScriptLibrarySourceCache *cache) {
    m_LibrarySourceCache = cache;
}

std::vector<ScriptIncludeDirective> ScriptSourceSnapshotBuilder::ScanIncludeDirectives(const std::string &code) {
    std::vector<ScriptIncludeDirective> directives;
    enum class State {
        Normal,
        LineComment,
        BlockComment,
        String,
        CharLiteral,
    };
    State state = State::Normal;
    for (size_t i = 0; i < code.size(); ++i) {
        const char ch = code[i];
        const char next = i + 1 < code.size() ? code[i + 1] : '\0';
        switch (state) {
        case State::Normal:
            if (ch == '/' && next == '/') {
                state = State::LineComment;
                ++i;
            } else if (ch == '/' && next == '*') {
                state = State::BlockComment;
                ++i;
            } else if (ch == '"') {
                state = State::String;
            } else if (ch == '\'') {
                state = State::CharLiteral;
            } else if (ch == '#' && IsLineStartDirective(code, i)) {
                ScriptIncludeDirective directive;
                if (ParseIncludeDirectiveAt(code, i, directive))
                    directives.push_back(std::move(directive));
            }
            break;
        case State::LineComment:
            if (ch == '\n' || ch == '\r')
                state = State::Normal;
            break;
        case State::BlockComment:
            if (ch == '*' && next == '/') {
                state = State::Normal;
                ++i;
            }
            break;
        case State::String:
            if (ch == '\\' && next != '\0') {
                ++i;
            } else if (ch == '"') {
                state = State::Normal;
            }
            break;
        case State::CharLiteral:
            if (ch == '\\' && next != '\0') {
                ++i;
            } else if (ch == '\'') {
                state = State::Normal;
            }
            break;
        }
    }
    return directives;
}

bool ScriptSourceSnapshotBuilder::ContainsBmlMetadata(const std::string &code, std::string *metadata) {
    enum class State {
        Normal,
        LineComment,
        BlockComment,
        String,
        CharLiteral,
    };
    State state = State::Normal;
    for (size_t i = 0; i < code.size(); ++i) {
        const char ch = code[i];
        const char next = i + 1 < code.size() ? code[i + 1] : '\0';
        switch (state) {
        case State::Normal:
            if (ch == '/' && next == '/') {
                state = State::LineComment;
                ++i;
            } else if (ch == '/' && next == '*') {
                state = State::BlockComment;
                ++i;
            } else if (ch == '"') {
                state = State::String;
            } else if (ch == '\'') {
                state = State::CharLiteral;
            } else if (ch == '[') {
                size_t pos = i + 1;
                while (pos < code.size() && std::isspace(static_cast<unsigned char>(code[pos])))
                    ++pos;
                if (code.compare(pos, 4, "bml.") == 0) {
                    if (metadata) {
                        const size_t end = code.find(']', pos);
                        *metadata = code.substr(i, end == std::string::npos ? std::string::npos : end - i + 1);
                    }
                    return true;
                }
            }
            break;
        case State::LineComment:
            if (ch == '\n' || ch == '\r')
                state = State::Normal;
            break;
        case State::BlockComment:
            if (ch == '*' && next == '/') {
                state = State::Normal;
                ++i;
            }
            break;
        case State::String:
            if (ch == '\\' && next != '\0') {
                ++i;
            } else if (ch == '"') {
                state = State::Normal;
            }
            break;
        case State::CharLiteral:
            if (ch == '\\' && next != '\0') {
                ++i;
            } else if (ch == '\'') {
                state = State::Normal;
            }
            break;
        }
    }
    return false;
}

bool ScriptSourceSnapshotBuilder::Build(const ScriptModEntry &entry,
                                        ScriptSourceSnapshot &snapshot,
                                        ScriptDiagnostic &diagnostic) const {
    snapshot = ScriptSourceSnapshot();
    snapshot.CompileEntry = entry;
    std::unordered_map<std::string, size_t> sectionIndex;
    if (!AddLocalSources(entry, snapshot, sectionIndex, diagnostic))
        return false;
    return ResolveLibraryClosure(snapshot, sectionIndex, diagnostic);
}

bool ScriptSourceSnapshotBuilder::AddLocalSources(const ScriptModEntry &entry,
                                                  ScriptSourceSnapshot &snapshot,
                                                  std::unordered_map<std::string, size_t> &sectionIndex,
                                                  ScriptDiagnostic &diagnostic) const {
    if (entry.EntryPath.empty() || !utils::FileExistsW(entry.EntryPath)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Script entry file no longer exists.");
        diagnostic.EntryPath = utils::Utf16ToUtf8(entry.EntryPath);
        return false;
    }

    std::wstring sectionRoot = entry.RootDirectory;
    if (entry.SourceKind == ScriptModEntrySourceKind::SingleFile || sectionRoot.empty())
        sectionRoot = utils::GetDirectoryW(entry.EntryPath);

    std::set<std::wstring> seen;
    if (!AddScriptSourceSection(entry.EntryPath, sectionRoot, seen, snapshot, sectionIndex, diagnostic))
        return false;
    snapshot.EntrySectionName = snapshot.Sections.front().Name;

    if (entry.SourceKind == ScriptModEntrySourceKind::SingleFile) {
        return AddScriptSourceDirectory(entry.ResourceRootDirectory,
                                        entry.ResourceRootDirectory,
                                        entry.EntryPath,
                                        false,
                                        seen,
                                        snapshot,
                                        sectionIndex,
                                        diagnostic);
    }

    return AddScriptSourceDirectory(entry.RootDirectory,
                                    sectionRoot,
                                    entry.EntryPath,
                                    true,
                                    seen,
                                    snapshot,
                                    sectionIndex,
                                    diagnostic);
}

bool ScriptSourceSnapshotBuilder::ResolveLibraryClosure(ScriptSourceSnapshot &snapshot,
                                                        std::unordered_map<std::string, size_t> &sectionIndex,
                                                        ScriptDiagnostic &diagnostic) const {
    std::vector<size_t> pending;
    if (!snapshot.Sections.empty())
        pending.push_back(0);
    std::unordered_set<size_t> processed;

    for (size_t cursor = 0; cursor < pending.size(); ++cursor) {
        const size_t sectionIndexValue = pending[cursor];
        if (sectionIndexValue >= snapshot.Sections.size())
            continue;
        if (!processed.insert(sectionIndexValue).second)
            continue;
        const ScriptSourceSection &section = snapshot.Sections[sectionIndexValue];
        const bool libraryOwned = ScriptLibraryRegistry::IsLibraryVirtualPath(section.Name);
        if (libraryOwned) {
            std::string metadata;
            if (ContainsBmlMetadata(section.Code, &metadata)) {
                diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata,
                                                  "BML metadata is not allowed in script library source: " + metadata);
                diagnostic.EntryPath = section.Name;
                return false;
            }
        }

        const std::vector<ScriptIncludeDirective> includes = ScanIncludeDirectives(section.Code);
        for (const ScriptIncludeDirective &include : includes) {
            if (!include.Quoted) {
                diagnostic = MakeScriptDiagnostic(
                    ScriptDiagnosticPhase::Entry,
                    libraryOwned
                        ? "Script library source only supports direct quoted #include directives."
                        : "Script source only supports direct quoted #include directives.");
                diagnostic.EntryPath = section.Name;
                return false;
            }
            std::string virtualInclude = include.Include;
            if (virtualInclude.empty()) {
                diagnostic = MakeScriptDiagnostic(
                    ScriptDiagnosticPhase::Entry,
                    libraryOwned
                        ? "Script library source does not allow empty #include directives."
                        : "Script source does not allow empty #include directives.");
                diagnostic.EntryPath = section.Name;
                return false;
            }
            if (ScriptLibraryRegistry::IsLibraryVirtualPath(virtualInclude)) {
                ScriptLibraryInclude parsed;
                std::string parseDiagnostic;
                if (!ScriptLibraryRegistry::TryParseVirtualInclude(virtualInclude, parsed, parseDiagnostic)) {
                    diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, parseDiagnostic);
                    diagnostic.EntryPath = section.Name;
                    return false;
                }
                if (!AddLibrarySection(parsed, snapshot, sectionIndex, pending, diagnostic))
                    return false;
                continue;
            }
            if (libraryOwned) {
                if (virtualInclude[0] == '/') {
                    diagnostic = MakeScriptDiagnostic(
                        ScriptDiagnosticPhase::Entry,
                        "Script library source cannot include physical or non-library absolute paths: " + virtualInclude);
                    diagnostic.EntryPath = section.Name;
                    return false;
                }
                std::string resolved;
                std::string parseDiagnostic;
                if (!ScriptLibraryRegistry::ResolveRelativeInclude(section.Name, virtualInclude, resolved, parseDiagnostic)) {
                    diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, parseDiagnostic);
                    diagnostic.EntryPath = section.Name;
                    return false;
                }
                ScriptLibraryInclude parsed;
                if (!ScriptLibraryRegistry::TryParseVirtualInclude(resolved, parsed, parseDiagnostic)) {
                    diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, parseDiagnostic);
                    diagnostic.EntryPath = section.Name;
                    return false;
                }
                if (!AddLibrarySection(parsed, snapshot, sectionIndex, pending, diagnostic))
                    return false;
                continue;
            }

            std::string localSection;
            if (!ResolveLocalIncludeSectionName(section.Name, virtualInclude, localSection)) {
                const std::string message =
                    virtualInclude[0] == '/'
                        ? "Script source cannot include physical or non-library absolute paths: " + virtualInclude
                        : "Script source include escapes the script package or uses an unsupported path: " + virtualInclude;
                diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, message);
                diagnostic.EntryPath = section.Name;
                return false;
            }
            const auto localIt = sectionIndex.find(FoldSectionKey(localSection));
            if (localIt != sectionIndex.end())
                pending.push_back(localIt->second);
        }
    }
    return true;
}

bool ScriptSourceSnapshotBuilder::AddLibrarySection(const ScriptLibraryInclude &include,
                                                    ScriptSourceSnapshot &snapshot,
                                                    std::unordered_map<std::string, size_t> &sectionIndex,
                                                    std::vector<size_t> &pending,
                                                    ScriptDiagnostic &diagnostic) const {
    const std::string key = FoldSectionKey(include.VirtualSection);
    if (sectionIndex.find(key) != sectionIndex.end())
        return true;

    std::wstring physicalPath;
    std::string resolveDiagnostic;
    if (!m_LibraryRegistry.ResolveInclude(include, physicalPath, resolveDiagnostic)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry, resolveDiagnostic);
        diagnostic.EntryPath = include.VirtualSection;
        return false;
    }
    std::string code;
    if (m_LibrarySourceCache) {
        if (!m_LibrarySourceCache->CapturePackage(m_LibraryRegistry, include.Id, include.Version, diagnostic))
            return false;
        if (!m_LibrarySourceCache->ReadFileUtf8(physicalPath, include.VirtualSection, code, diagnostic))
            return false;
    } else {
        if (!utils::ReadFileBytesUtf8(utils::Utf16ToUtf8(physicalPath), code)) {
            diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                              "Failed to read script library source: " + include.VirtualSection + ".");
            diagnostic.EntryPath = include.VirtualSection;
            return false;
        }
    }

    ScriptLibraryPackage package;
    if (!m_LibraryRegistry.FindPackage(include.Id, include.Version, package)) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Entry,
                                          "Script library package not found: " + include.Id + "@" + include.Version + ".");
        diagnostic.EntryPath = include.VirtualSection;
        return false;
    }

    const size_t newIndex = snapshot.Sections.size();
    sectionIndex.emplace(key, newIndex);

    ScriptSourceSection section;
    section.Name = include.VirtualSection;
    section.Code = std::move(code);

    ScriptSourceDependency dependency;
    dependency.PhysicalPath = physicalPath;
    dependency.VirtualSection = include.VirtualSection;
    dependency.ContentHash = utils::Sha256Hex(section.Code);
    dependency.LibraryOwned = true;
    dependency.LibraryId = include.Id;
    dependency.LibraryVersion = include.Version;

    snapshot.Sections.push_back(std::move(section));
    pending.push_back(newIndex);
    snapshot.Dependencies.push_back(std::move(dependency));

    const std::string useKey = LibraryUseKey(include.Id, include.Version);
    const bool known = std::any_of(snapshot.Libraries.begin(), snapshot.Libraries.end(), [&](const ScriptLibraryUse &use) {
        return LibraryUseKey(use.Id, use.Version) == useKey;
    });
    if (!known) {
        ScriptLibraryUse use;
        use.Id = package.Id;
        use.Version = package.Version;
        use.RootDirectory = package.RootDirectory;
        use.VirtualRoot = package.VirtualRoot;
        snapshot.Libraries.push_back(std::move(use));
    }

    return true;
}

} // namespace BML
