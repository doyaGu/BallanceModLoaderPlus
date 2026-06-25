#ifndef BML_SCRIPTSOURCESNAPSHOTBUILDER_H
#define BML_SCRIPTSOURCESNAPSHOTBUILDER_H

#include <string>
#include <unordered_map>
#include <unordered_set>
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

class ScriptLibrarySourceCache {
public:
    bool CapturePackage(const ScriptLibraryRegistry &registry,
                        const std::string &id,
                        const std::string &version,
                        ScriptDiagnostic &diagnostic);
    bool ReadFileUtf8(const std::wstring &physicalPath,
                      const std::string &virtualSection,
                      std::string &code,
                      ScriptDiagnostic &diagnostic);
    bool GetFileContentHash(const std::wstring &physicalPath, std::string &hash) const;
    size_t GetFileCount() const { return m_Files.size(); }

private:
    std::unordered_map<std::wstring, std::string> m_Files;
    std::unordered_set<std::string> m_CapturedPackages;
};

class ScriptSourceSnapshotBuilder {
public:
    ScriptSourceSnapshotBuilder() = default;
    explicit ScriptSourceSnapshotBuilder(ScriptLibraryRegistry registry);

    void SetLibraryRegistry(ScriptLibraryRegistry registry);
    void SetLibrarySourceCache(ScriptLibrarySourceCache *cache);
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
    ScriptLibrarySourceCache *m_LibrarySourceCache = nullptr;
};

} // namespace BML

#endif
