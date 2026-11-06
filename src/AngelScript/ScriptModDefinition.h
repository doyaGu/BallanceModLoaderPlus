#ifndef BML_SCRIPTMODDEFINITION_H
#define BML_SCRIPTMODDEFINITION_H

#include <string>
#include <unordered_map>
#include <vector>

#include "Loader/ModVersion.h"
#include "ScriptSourceSnapshot.h"

namespace BML {

struct ScriptModDefinition {
    std::string Id;
    std::string Name;
    std::string Version;
    std::string Author;
    std::string Description;
    BMLVersion MinBmlVersion;
    std::string Entry;
    std::string ClassName;
    std::string ClassNamespace;
    bool Enabled = true;
    std::string ReloadPolicy;
    std::vector<ModDependency> Dependencies;
    std::vector<ScriptLibraryUse> SourceLibraries;
    std::vector<ScriptSourceDependency> SourceDependencies;
    std::vector<ScriptSourceIncludeEdge> SourceIncludeEdges;
    std::unordered_map<std::string, std::string> Metadata;
};

enum class ScriptModReloadPolicy {
    Default,
    Auto,
    Manual,
};

ScriptModReloadPolicy ParseScriptModReloadPolicy(const std::string &value);
const char *ToString(ScriptModReloadPolicy policy);
bool ParseScriptModVersion(const std::string &value, BMLVersion &version);

struct ScriptMetadataTag {
    std::string Name;
    std::unordered_map<std::string, std::string> Args;
};

bool ParseScriptMetadataTag(const std::string &metadata,
                            ScriptMetadataTag &tag,
                            std::string &diagnostic);

} // namespace BML

#endif
