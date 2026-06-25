#include "ScriptLibraryValidator.h"

#include "ScriptLibraryTools.h"

namespace BML {

bool ValidateScriptLibraryPackage(const ScriptLibraryRegistry &registry,
                                  const ScriptLibraryPackage &package,
                                  std::vector<std::string> &lines) {
    ScriptLibraryPackageCheckReport report;
    const bool success = BuildScriptLibraryPackageCheckReport(registry, package, report);
    AppendScriptLibraryPackageCheckLines(report, lines);
    return success;
}

} // namespace BML
