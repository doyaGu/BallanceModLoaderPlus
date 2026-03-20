#ifndef BML_SCRIPTING_BIND_CORE_H
#define BML_SCRIPTING_BIND_CORE_H

#include <angelscript.h>

namespace BML::Scripting {

void RegisterCoreBindings(asIScriptEngine *engine);

} // namespace BML::Scripting

#endif
