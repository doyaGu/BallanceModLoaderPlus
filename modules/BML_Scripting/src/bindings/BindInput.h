#ifndef BML_SCRIPTING_BIND_INPUT_H
#define BML_SCRIPTING_BIND_INPUT_H

#include <angelscript.h>

namespace BML::Scripting {

void RegisterInputBindings(asIScriptEngine *engine);
void SetInputBindingsReady(bool ready);

} // namespace BML::Scripting

#endif
