#ifndef BML_SCRIPTMODDEFINITIONBUILDER_H
#define BML_SCRIPTMODDEFINITIONBUILDER_H

#include "ScriptDiagnostic.h"
#include "ScriptModDefinition.h"
#include "ScriptModEntryScanner.h"
#include "ScriptModRuntime.h"

class CKContext;

namespace BML {

class ScriptModDefinitionBuilder {
public:
    bool Build(CKContext *context,
               const ScriptModEntry &entry,
               ScriptModRuntime &runtime,
               ScriptModDefinition &definition,
               ScriptDiagnostic &diagnostic) const;
};

} // namespace BML

#endif
