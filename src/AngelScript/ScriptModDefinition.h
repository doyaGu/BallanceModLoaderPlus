#ifndef BML_SCRIPTMODDEFINITION_H
#define BML_SCRIPTMODDEFINITION_H

#include <string>
#include <unordered_map>
#include <vector>

#include "BML/IMod.h"
#include "ScriptSourceSnapshot.h"

namespace BML {

struct ScriptModDependency {
    std::string Id;
    BMLVersion MinVersion = BMLVersion(0, 0, 0);
    bool Optional = false;
};

struct ScriptModExportDefinition {
    std::string Name;
    std::string Signature;
    std::string Method;
};

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
    std::vector<ScriptModDependency> Dependencies;
    std::vector<ScriptModExportDefinition> Exports;
    std::vector<ScriptLibraryUse> SourceLibraries;
    std::vector<ScriptSourceDependency> SourceDependencies;
    std::unordered_map<std::string, std::string> Metadata;
};

enum class ScriptModReloadPolicy {
    Default,
    Auto,
    Manual,
};

ScriptModReloadPolicy ParseScriptModReloadPolicy(const std::string &value);
const char *ToString(ScriptModReloadPolicy policy);

struct ScriptMetadataTag {
    std::string Name;
    std::unordered_map<std::string, std::string> Args;
};

BMLVersion ParseBmlVersion(const std::string &value);
bool ParseScriptMetadataTag(const std::string &metadata,
                            ScriptMetadataTag &tag,
                            std::string &diagnostic);

} // namespace BML

#endif
