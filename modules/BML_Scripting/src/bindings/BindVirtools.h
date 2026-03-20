#ifndef BML_SCRIPTING_BIND_VIRTOOLS_H
#define BML_SCRIPTING_BIND_VIRTOOLS_H

#include <angelscript.h>

namespace BML::Scripting {

void RegisterVirtoolsBindings(asIScriptEngine *engine);
void SetVirtoolsBindingsReady(bool ready);

} // namespace BML::Scripting

#endif
