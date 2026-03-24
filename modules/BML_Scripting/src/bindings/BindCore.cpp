#include "BindCore.h"

#include <cassert>
#include <string>

#include "../ModuleScope.h"

namespace BML::Scripting {

namespace {

static void ScriptLog(BML_LogSeverity severity, const std::string &message) {
    const BML_Mod owner = CurrentScriptOwner();
    if (!g_Services || !g_Services->Logging || !g_Services->Logging->Log || !owner) {
        return;
    }

    g_Services->Logging->Log(owner, severity, "script", "%s", message.c_str());
}

static void Script_Log(const std::string &message) { ScriptLog(BML_LOG_INFO, message); }
static void Script_LogWarning(const std::string &message) { ScriptLog(BML_LOG_WARN, message); }
static void Script_LogError(const std::string &message) { ScriptLog(BML_LOG_ERROR, message); }

} // namespace

void RegisterCoreBindings(asIScriptEngine *engine) {
    int r;
    r = engine->RegisterGlobalFunction("void bmlLog(const string &in)",
        asFUNCTION(Script_Log), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlLogWarning(const string &in)",
        asFUNCTION(Script_LogWarning), asCALL_CDECL); assert(r >= 0);
    r = engine->RegisterGlobalFunction("void bmlLogError(const string &in)",
        asFUNCTION(Script_LogError), asCALL_CDECL); assert(r >= 0);
}

} // namespace BML::Scripting
