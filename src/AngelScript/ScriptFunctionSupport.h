#ifndef BML_SCRIPTFUNCTIONSUPPORT_H
#define BML_SCRIPTFUNCTIONSUPPORT_H

#include <cstddef>

#include <angelscript.h>

#include "ScriptDiagnostic.h"

namespace BML {

class ScriptMod;

class ScriptHostCallScope {
public:
    explicit ScriptHostCallScope(ScriptMod *owner);
    ~ScriptHostCallScope();

    ScriptHostCallScope(const ScriptHostCallScope &) = delete;
    ScriptHostCallScope &operator=(const ScriptHostCallScope &) = delete;

    bool Entered() const { return m_Entered; }

private:
    ScriptMod *m_Owner = nullptr;
    bool m_Entered = false;
};

struct ScriptFunctionParam {
    const char *TypeDecl = nullptr;
    asDWORD RequiredFlags = 0;
};

using ScriptFunctionArgWriter = int (*)(asIScriptContext *context, void *userdata);
using ScriptFunctionResultReader = void (*)(asIScriptContext *context, void *userdata);

struct ScriptFunctionCall {
    asIScriptFunction *Function = nullptr;
    void *Object = nullptr;
    ScriptMod *Owner = nullptr;
    ScriptDiagnosticPhase Phase = ScriptDiagnosticPhase::Callback;
    const char *FailurePrefix = nullptr;
    const char *InvalidStateMessage = nullptr;
    const char *ContextFailureMessage = nullptr;
    const char *SuspendedMessage = nullptr;
    ScriptFunctionArgWriter WriteArgs = nullptr;
    ScriptFunctionResultReader ReadResult = nullptr;
    void *UserData = nullptr;
};

bool ScriptFunctionHasSignature(asIScriptFunction *function,
                                int returnTypeId,
                                const ScriptFunctionParam *params,
                                size_t paramCount);

bool ExecuteScriptFunction(const ScriptFunctionCall &call, ScriptDiagnostic &diagnostic);

} // namespace BML

#endif
