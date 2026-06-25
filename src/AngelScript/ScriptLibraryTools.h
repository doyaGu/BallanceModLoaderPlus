#ifndef BML_SCRIPTLIBRARYTOOLS_H
#define BML_SCRIPTLIBRARYTOOLS_H

#include <cstddef>
#include <string>
#include <vector>

#include "ScriptLibraryRegistry.h"
#include "ScriptSourceSnapshot.h"

namespace BML {

struct ScriptLibraryToolFileReport {
    std::string VirtualSection;
    std::wstring PhysicalPath;
    std::string ContentHash;
};

struct ScriptLibraryPackageCheckReport {
    ScriptLibraryPackage Package;
    bool Success = false;
    size_t IncludeCount = 0;
    std::vector<std::string> Errors;
    std::vector<std::string> ErrorDetails;
    std::vector<ScriptLibraryToolFileReport> Files;
    std::vector<ScriptSourceIncludeEdge> IncludeEdges;
};

struct ScriptLibraryToolReportOptions {
    bool IncludeFileHashes = false;
    bool IncludeIncludeGraph = false;
};

bool BuildScriptLibraryPackageCheckReport(const ScriptLibraryRegistry &registry,
                                          const ScriptLibraryPackage &package,
                                          ScriptLibraryPackageCheckReport &report);

bool ParseScriptLibraryToolReportOptions(const std::vector<std::string> &args,
                                         size_t firstOption,
                                         ScriptLibraryToolReportOptions &options,
                                         std::string &diagnostic);

void AppendScriptLibraryPackageCheckLines(const ScriptLibraryPackageCheckReport &report,
                                          std::vector<std::string> &lines,
                                          const ScriptLibraryToolReportOptions &options = ScriptLibraryToolReportOptions());

} // namespace BML

#endif
