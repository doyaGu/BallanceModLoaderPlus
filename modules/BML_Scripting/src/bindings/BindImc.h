#ifndef BML_SCRIPTING_BIND_IMC_H
#define BML_SCRIPTING_BIND_IMC_H

#include <angelscript.h>

namespace BML::Scripting {

class ScriptInstanceManager;

void RegisterImcBindings(asIScriptEngine *engine, ScriptInstanceManager *manager);

} // namespace BML::Scripting

#endif
