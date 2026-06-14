#ifndef BML_SCRIPTMODLOADER_H
#define BML_SCRIPTMODLOADER_H

#include <memory>
#include <string>

#include "ScriptMod.h"
#include "ScriptModEntryScanner.h"

namespace BML {

struct ScriptModLoadResult {
    std::unique_ptr<ScriptMod> Mod;
    ScriptModDefinition Definition;
    bool Failed = false;
};

class ScriptModLoader {
public:
    ScriptModLoadResult Load(ModContext *owner, CKContext *context, const ScriptModLoadCandidate &candidate) const;

private:
    static ScriptModDefinition MakePlaceholderDefinition(const ScriptModLoadCandidate &candidate);
};

} // namespace BML

#endif
