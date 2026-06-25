#ifndef BML_SCRIPTLIBRARYREGISTRY_H
#define BML_SCRIPTLIBRARYREGISTRY_H

#include <string>
#include <unordered_map>
#include <vector>

#include "ScriptSourceSnapshot.h"

namespace BML {

struct ScriptLibraryPackage {
    std::string Id;
    std::string Version;
    std::wstring RootDirectory;
    std::string VirtualRoot;
};

struct ScriptLibraryInclude {
    std::string Id;
    std::string Version;
    std::string RelativePath;
    std::string VirtualSection;
};

class ScriptLibraryRegistry {
public:
    ScriptLibraryRegistry() = default;
    explicit ScriptLibraryRegistry(std::wstring rootDirectory);

    void SetRootDirectory(std::wstring rootDirectory);
    const std::wstring &GetRootDirectory() const { return m_RootDirectory; }

    bool Scan(std::string &diagnostic);
    bool FindPackage(const std::string &id,
                     const std::string &version,
                     ScriptLibraryPackage &package) const;
    bool ResolveInclude(const ScriptLibraryInclude &include,
                        std::wstring &physicalPath,
                        std::string &diagnostic) const;

    const std::vector<ScriptLibraryPackage> &GetPackages() const { return m_Packages; }

    static bool IsValidLibraryId(const std::string &id);
    static bool IsValidLibraryVersion(const std::string &version);
    static bool TryParseVirtualInclude(const std::string &include,
                                       ScriptLibraryInclude &parsed,
                                       std::string &diagnostic);
    static bool ResolveRelativeInclude(const std::string &includingVirtualSection,
                                       const std::string &relativeInclude,
                                       std::string &resolvedVirtualSection,
                                       std::string &diagnostic);
    static bool IsLibraryVirtualPath(const std::string &path);

private:
    static std::string MakePackageKey(const std::string &id, const std::string &version);
    void RebuildIndex();

    std::wstring m_RootDirectory;
    std::vector<ScriptLibraryPackage> m_Packages;
    std::unordered_map<std::string, size_t> m_Index;
};

} // namespace BML

#endif
