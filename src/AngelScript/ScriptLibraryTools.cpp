#include "ScriptLibraryTools.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>

#include "ScriptSourceSnapshotBuilder.h"
#include "Utils/CryptoUtils.h"
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

size_t LineNumberFromOffset(const std::string &code, size_t offset) {
    size_t line = 1;
    const size_t end = std::min(offset, code.size());
    for (size_t i = 0; i < end; ++i) {
        if (code[i] == '\n')
            ++line;
    }
    return line;
}

void AddError(ScriptLibraryPackageCheckReport &report, const std::string &message) {
    report.Errors.push_back(message);
    report.Success = false;
}

std::string PackageKey(const ScriptLibraryPackage &package) {
    return package.Id + "@" + package.Version;
}

} // namespace

bool BuildScriptLibraryPackageCheckReport(const ScriptLibraryRegistry &registry,
                                          const ScriptLibraryPackage &package,
                                          ScriptLibraryPackageCheckReport &report) {
    report = ScriptLibraryPackageCheckReport();
    report.Package = package;

    std::error_code ec;
    std::set<std::string> sections;
    for (std::filesystem::recursive_directory_iterator it(package.RootDirectory, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (!it->is_regular_file(ec)) {
            if (ec)
                break;
            continue;
        }
        const std::wstring path = it->path().wstring();
        const std::wstring resolvedPath = utils::ResolvePathW(path);
        std::string pathUtf8 = utils::Utf16ToUtf8(resolvedPath);
        if (!utils::EndsWith(pathUtf8, ".as"))
            continue;
        if (!utils::IsPathInsideRootW(resolvedPath, package.RootDirectory)) {
            AddError(report, "source escapes package root: " + pathUtf8);
            return false;
        }

        const std::wstring relativeW = utils::MakeRelativePathW(resolvedPath, package.RootDirectory);
        const std::string relative = RelativePathToVirtualPath(relativeW);
        const std::string section = package.VirtualRoot + relative;
        const std::string sectionKey = FoldVirtualSectionKey(section);
        if (!sections.insert(sectionKey).second) {
            AddError(report, "duplicate virtual section: " + section);
            return false;
        }

        std::string code;
        if (!utils::ReadFileBytesUtf8(pathUtf8, code)) {
            AddError(report, "failed to read: " + section);
            return false;
        }

        std::string metadata;
        if (ScriptSourceSnapshotBuilder::ContainsBmlMetadata(code, &metadata)) {
            AddError(report, "metadata is not allowed in library source: " + section);
            if (!metadata.empty())
                report.ErrorDetails.push_back(metadata);
            return false;
        }

        ScriptLibraryToolFileReport file;
        file.VirtualSection = section;
        file.PhysicalPath = resolvedPath;
        file.ContentHash = utils::Sha256Hex(code);
        report.Files.push_back(std::move(file));

        for (const ScriptIncludeDirective &include : ScriptSourceSnapshotBuilder::ScanIncludeDirectives(code)) {
            if (!include.Quoted) {
                AddError(report, "non-quoted #include is not supported in library source: " + section);
                return false;
            }
            ++report.IncludeCount;
            std::string resolved = include.Include;
            if (resolved.empty()) {
                AddError(report, "empty #include in library source: " + section);
                return false;
            }

            // Manifests/catalogs can enrich reports, but the include path resolver remains the source of truth.
            ScriptLibraryInclude parsed;
            std::string diagnostic;
            if (ScriptLibraryRegistry::IsLibraryVirtualPath(resolved)) {
                std::wstring includePhysicalPath;
                if (!ScriptLibraryRegistry::TryParseVirtualInclude(resolved, parsed, diagnostic) ||
                    !registry.ResolveInclude(parsed, includePhysicalPath, diagnostic)) {
                    AddError(report, "invalid library include in " + section + ": " + diagnostic);
                    return false;
                }
            } else {
                if (resolved[0] == '/') {
                    AddError(report, "absolute include is not a library virtual path in " + section + ": " + resolved);
                    return false;
                }
                std::string relativeResolved;
                if (!ScriptLibraryRegistry::ResolveRelativeInclude(section, resolved, relativeResolved, diagnostic)) {
                    AddError(report, "invalid relative include in " + section + ": " + diagnostic);
                    return false;
                }
                std::wstring includePhysicalPath;
                if (!ScriptLibraryRegistry::TryParseVirtualInclude(relativeResolved, parsed, diagnostic) ||
                    !registry.ResolveInclude(parsed, includePhysicalPath, diagnostic)) {
                    AddError(report, "unresolved relative include in " + section + ": " + diagnostic);
                    return false;
                }
            }

            ScriptSourceIncludeEdge edge;
            edge.FromSection = section;
            edge.Include = include.Include;
            edge.ToSection = parsed.VirtualSection;
            edge.Line = LineNumberFromOffset(code, include.Offset);
            edge.LibraryOwned = true;
            edge.LibraryId = parsed.Id;
            edge.LibraryVersion = parsed.Version;
            report.IncludeEdges.push_back(std::move(edge));
        }
    }

    if (ec) {
        AddError(report, "failed to enumerate package source.");
        return false;
    }
    std::sort(report.Files.begin(), report.Files.end(), [](const ScriptLibraryToolFileReport &left,
                                                           const ScriptLibraryToolFileReport &right) {
        return FoldVirtualSectionKey(left.VirtualSection) < FoldVirtualSectionKey(right.VirtualSection);
    });
    std::sort(report.IncludeEdges.begin(), report.IncludeEdges.end(), [](const ScriptSourceIncludeEdge &left,
                                                                         const ScriptSourceIncludeEdge &right) {
        const std::string leftFrom = FoldVirtualSectionKey(left.FromSection);
        const std::string rightFrom = FoldVirtualSectionKey(right.FromSection);
        if (leftFrom != rightFrom)
            return leftFrom < rightFrom;
        if (left.Line != right.Line)
            return left.Line < right.Line;
        return FoldVirtualSectionKey(left.ToSection) < FoldVirtualSectionKey(right.ToSection);
    });
    report.Success = true;
    return true;
}

bool ParseScriptLibraryToolReportOptions(const std::vector<std::string> &args,
                                         size_t firstOption,
                                         ScriptLibraryToolReportOptions &options,
                                         std::string &diagnostic) {
    options = ScriptLibraryToolReportOptions();
    diagnostic.clear();
    for (size_t i = firstOption; i < args.size(); ++i) {
        if (args[i] == "--hashes")
            options.IncludeFileHashes = true;
        else if (args[i] == "--graph")
            options.IncludeIncludeGraph = true;
        else if (args[i] == "--compile")
            options.Compile = true;
        else {
            diagnostic = "unknown script library check option: " + args[i];
            return false;
        }
    }
    return true;
}

void AppendScriptLibraryPackageCheckLines(const ScriptLibraryPackageCheckReport &report,
                                          std::vector<std::string> &lines,
                                          const ScriptLibraryToolReportOptions &options) {
    if (!report.Success) {
        for (const std::string &error : report.Errors)
            lines.push_back("  error " + error);
        for (const std::string &detail : report.ErrorDetails)
            lines.push_back("    " + detail);
        if (report.Errors.empty())
            lines.push_back("  error package check failed: " + PackageKey(report.Package));
        return;
    }

    lines.push_back("  check=ok files=" + std::to_string(report.Files.size()) +
                    " includes=" + std::to_string(report.IncludeCount));

    if (options.IncludeFileHashes) {
        lines.push_back("  source hashes:");
        for (const ScriptLibraryToolFileReport &file : report.Files) {
            lines.push_back("    " + file.VirtualSection +
                            " hash=" + file.ContentHash +
                            " path=" + utils::Utf16ToUtf8(file.PhysicalPath));
        }
        if (report.Files.empty())
            lines.push_back("    none");
    }

    if (options.IncludeIncludeGraph) {
        lines.push_back("  include graph:");
        for (const ScriptSourceIncludeEdge &edge : report.IncludeEdges) {
            std::string line = "    " + edge.FromSection;
            if (edge.Line != 0)
                line += ":" + std::to_string(edge.Line);
            line += " -> " + edge.ToSection + " include=\"" + edge.Include + "\"";
            lines.push_back(std::move(line));
        }
        if (report.IncludeEdges.empty())
            lines.push_back("    none");
    }
}

std::vector<std::string> GetScriptLibraryCompileRoots(const ScriptLibraryPackageCheckReport &report) {
    std::set<std::string> includedSections;
    for (const ScriptSourceIncludeEdge &edge : report.IncludeEdges)
        includedSections.insert(FoldVirtualSectionKey(edge.ToSection));

    std::vector<std::string> roots;
    for (const ScriptLibraryToolFileReport &file : report.Files) {
        if (includedSections.find(FoldVirtualSectionKey(file.VirtualSection)) == includedSections.end())
            roots.push_back(file.VirtualSection);
    }

    if (!roots.empty())
        return roots;

    roots.reserve(report.Files.size());
    for (const ScriptLibraryToolFileReport &file : report.Files)
        roots.push_back(file.VirtualSection);
    return roots;
}

} // namespace BML
