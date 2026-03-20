#ifndef BML_SCRIPTING_BIND_TIMER_H
#define BML_SCRIPTING_BIND_TIMER_H

#include <angelscript.h>

namespace BML::Scripting {

class ScriptInstanceManager;

void RegisterTimerBindings(asIScriptEngine *engine, ScriptInstanceManager *manager);

} // namespace BML::Scripting

#endif
