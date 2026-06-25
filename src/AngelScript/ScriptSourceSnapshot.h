#ifndef BML_SCRIPTSOURCESNAPSHOT_H
#define BML_SCRIPTSOURCESNAPSHOT_H

#include <string>
#include <vector>

#include "ScriptModEntryScanner.h"
#include "ScriptModRuntime.h"

namespace BML {

struct ScriptLibraryUse {
    std::string Id;
    std::string Version;
    std::wstring RootDirectory;
    std::string VirtualRoot;
};

struct ScriptSourceDependency {
    std::wstring PhysicalPath;
    std::string VirtualSection;
    std::string ContentHash;
    bool LibraryOwned = false;
    std::string LibraryId;
    std::string LibraryVersion;
};

struct ScriptSourceSnapshot {
    ScriptModEntry CompileEntry;
    std::vector<ScriptSourceSection> Sections;
    std::vector<ScriptLibraryUse> Libraries;
    std::vector<ScriptSourceDependency> Dependencies;
    std::string EntrySectionName;
};

} // namespace BML

#endif
