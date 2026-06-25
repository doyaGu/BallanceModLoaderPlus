#ifndef BML_SCRIPTSOURCESNAPSHOTBUILDER_H
#define BML_SCRIPTSOURCESNAPSHOTBUILDER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "ScriptDiagnostic.h"
#include "ScriptLibraryRegistry.h"
#include "ScriptSourceSnapshot.h"

namespace BML {

struct ScriptIncludeDirective {
    std::string Include;
    bool Quoted = false;
    size_t Offset = 0;
};

class ScriptSourceSnapshotBuilder {
public:
    ScriptSourceSnapshotBuilder() = default;
    explicit ScriptSourceSnapshotBuilder(ScriptLibraryRegistry registry);

    void SetLibraryRegistry(ScriptLibraryRegistry registry);
    bool Build(const ScriptModEntry &entry,
               ScriptSourceSnapshot &snapshot,
               ScriptDiagnostic &diagnostic) const;

    static std::vector<ScriptIncludeDirective> ScanIncludeDirectives(const std::string &code);
    static bool ContainsBmlMetadata(const std::string &code, std::string *metadata = nullptr);

private:
    bool AddLocalSources(const ScriptModEntry &entry,
                         ScriptSourceSnapshot &snapshot,
                         std::unordered_map<std::string, size_t> &sectionIndex,
                         ScriptDiagnostic &diagnostic) const;
    bool ResolveLibraryClosure(ScriptSourceSnapshot &snapshot,
                               std::unordered_map<std::string, size_t> &sectionIndex,
                               ScriptDiagnostic &diagnostic) const;
    bool AddLibrarySection(const ScriptLibraryInclude &include,
                           ScriptSourceSnapshot &snapshot,
                           std::unordered_map<std::string, size_t> &sectionIndex,
                           std::vector<size_t> &pending,
                           ScriptDiagnostic &diagnostic) const;

    ScriptLibraryRegistry m_LibraryRegistry;
};

} // namespace BML

#endif
