#include "ScriptLibraryValidator.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>

#include "ScriptSourceSnapshotBuilder.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

namespace {

std::string FoldVirtualSectionKey(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string RelativePathToVirtualPath(const std::wstring &relative) {
    std::string value = utils::Utf16ToUtf8(relative);
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

} // namespace

bool ValidateScriptLibraryPackage(const ScriptLibraryRegistry &registry,
                                  const ScriptLibraryPackage &package,
                                  std::vector<std::string> &lines) {
    std::error_code ec;
    std::set<std::string> sections;
    size_t fileCount = 0;
    size_t includeCount = 0;
    for (std::filesystem::recursive_directory_iterator it(package.RootDirectory, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            if (ec)
                break;
            continue;
        }
        const std::wstring path = it->path().wstring();
        std::string pathUtf8 = utils::Utf16ToUtf8(path);
        if (!utils::EndsWith(pathUtf8, ".as"))
            continue;

        const std::wstring relativeW = utils::MakeRelativePathW(path, package.RootDirectory);
        const std::string relative = RelativePathToVirtualPath(relativeW);
        const std::string section = package.VirtualRoot + relative;
        const std::string sectionKey = FoldVirtualSectionKey(section);
        if (!sections.insert(sectionKey).second) {
            lines.push_back("  error duplicate virtual section: " + section);
            return false;
        }

        std::string code;
        if (!utils::ReadFileBytesUtf8(pathUtf8, code)) {
            lines.push_back("  error failed to read: " + section);
            return false;
        }

        std::string metadata;
        if (ScriptSourceSnapshotBuilder::ContainsBmlMetadata(code, &metadata)) {
            lines.push_back("  error metadata is not allowed in library source: " + section);
            lines.push_back("    " + metadata);
            return false;
        }

        ++fileCount;
        for (const ScriptIncludeDirective &include : ScriptSourceSnapshotBuilder::ScanIncludeDirectives(code)) {
            if (!include.Quoted) {
                lines.push_back("  error non-quoted #include is not supported in library source: " + section);
                return false;
            }
            ++includeCount;
            std::string resolved = include.Include;
            if (resolved.empty()) {
                lines.push_back("  error empty #include in library source: " + section);
                return false;
            }
            if (ScriptLibraryRegistry::IsLibraryVirtualPath(resolved)) {
                ScriptLibraryInclude parsed;
                std::string diagnostic;
                std::wstring includePhysicalPath;
                if (!ScriptLibraryRegistry::TryParseVirtualInclude(resolved, parsed, diagnostic) ||
                    !registry.ResolveInclude(parsed, includePhysicalPath, diagnostic)) {
                    lines.push_back("  error invalid library include in " + section + ": " + diagnostic);
                    return false;
                }
                continue;
            }
            if (resolved[0] == '/') {
                lines.push_back("  error absolute include is not a library virtual path in " + section + ": " + resolved);
                return false;
            }
            std::string relativeResolved;
            std::string diagnostic;
            if (!ScriptLibraryRegistry::ResolveRelativeInclude(section, resolved, relativeResolved, diagnostic)) {
                lines.push_back("  error invalid relative include in " + section + ": " + diagnostic);
                return false;
            }
            ScriptLibraryInclude parsed;
            std::wstring includePhysicalPath;
            if (!ScriptLibraryRegistry::TryParseVirtualInclude(relativeResolved, parsed, diagnostic) ||
                !registry.ResolveInclude(parsed, includePhysicalPath, diagnostic)) {
                lines.push_back("  error unresolved relative include in " + section + ": " + diagnostic);
                return false;
            }
        }
    }

    if (ec) {
        lines.push_back("  error failed to enumerate package source.");
        return false;
    }
    lines.push_back("  check=ok files=" + std::to_string(fileCount) +
                    " includes=" + std::to_string(includeCount));
    return true;
}

} // namespace BML
