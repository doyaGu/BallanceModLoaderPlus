#ifndef BML_SCRIPTING_BIND_COROUTINE_H
#define BML_SCRIPTING_BIND_COROUTINE_H

#include <angelscript.h>

namespace BML::Scripting {

class CoroutineManager;

void RegisterCoroutineBindings(asIScriptEngine *engine, CoroutineManager *mgr);

} // namespace BML::Scripting

#endif
