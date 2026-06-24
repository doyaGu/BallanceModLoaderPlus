#include "ScriptFunctionSupport.h"

#include <string>

#include "ScriptMod.h"
#include "ScriptModRuntime.h"

namespace BML {

ScriptHostCallScope::ScriptHostCallScope(ScriptMod *owner) : m_Owner(owner) {
    if (m_Owner)
        m_Entered = m_Owner->EnterScriptCall();
}

ScriptHostCallScope::~ScriptHostCallScope() {
    if (m_Owner && m_Entered)
        m_Owner->LeaveScriptCall();
}

bool ScriptFunctionHasSignature(asIScriptFunction *function,
                                int returnTypeId,
                                const ScriptFunctionParam *params,
                                size_t paramCount) {
    if (!function || !function->GetEngine() || function->GetParamCount() != static_cast<asUINT>(paramCount))
        return false;

    if (function->GetReturnTypeId() != returnTypeId)
        return false;

    asIScriptEngine *engine = function->GetEngine();
    for (size_t i = 0; i < paramCount; ++i) {
        const int expectedType = engine->GetTypeIdByDecl(params[i].TypeDecl);
        int actualType = 0;
        asDWORD flags = 0;
        if (expectedType < 0 ||
            function->GetParam(static_cast<asUINT>(i), &actualType, &flags) < 0 ||
            actualType != expectedType ||
            (flags & params[i].RequiredFlags) != params[i].RequiredFlags) {
            return false;
        }
    }

    return true;
}

static ScriptDiagnostic MakeScriptFunctionDiagnostic(const ScriptFunctionCall &call,
                                                     int code,
                                                     asIScriptContext *context) {
    ScriptDiagnostic diagnostic;
    diagnostic.Phase = call.Phase;
    diagnostic.AngelScriptCode = code;
    diagnostic.Message = call.FailurePrefix ? call.FailurePrefix : "AngelScript callback failed";
    if (!diagnostic.Message.empty())
        diagnostic.Message += ": ";

    if (code == asEXECUTION_EXCEPTION && context) {
        const char *exception = context->GetExceptionString();
        diagnostic.Message += exception ? exception : "AngelScript exception";
        asIScriptFunction *function = context->GetExceptionFunction();
        if (function) {
            diagnostic.StackTrace = function->GetDeclaration(true, true, true);
            int column = 0;
            const int line = context->GetExceptionLineNumber(&column);
            if (line > 0) {
                diagnostic.StackTrace += ":";
                diagnostic.StackTrace += std::to_string(line);
                if (column > 0) {
                    diagnostic.StackTrace += ":";
                    diagnostic.StackTrace += std::to_string(column);
                }
            }
        }
    } else if (code == asEXECUTION_SUSPENDED) {
        diagnostic.Message += call.SuspendedMessage ? call.SuspendedMessage : "script callback suspended";
    } else {
        diagnostic.Message += "AngelScript call failed with code ";
        diagnostic.Message += std::to_string(code);
    }
    return diagnostic;
}

bool ExecuteScriptFunction(const ScriptFunctionCall &call, ScriptDiagnostic &diagnostic) {
    ScriptHostCallScope activeCall(call.Owner);
    if (call.Owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(call.Phase, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    if (!call.Function) {
        diagnostic = MakeScriptDiagnostic(call.Phase,
                                          call.InvalidStateMessage ? call.InvalidStateMessage : "Script callback has invalid runtime state.");
        return false;
    }

    asIScriptEngine *engine = call.Function->GetEngine();
    asIScriptContext *context = engine ? engine->CreateContext() : nullptr;
    if (!context) {
        diagnostic = MakeScriptDiagnostic(call.Phase,
                                          call.ContextFailureMessage ? call.ContextFailureMessage : "Unable to create AngelScript context for script callback.");
        return false;
    }

    asIScriptFunction *callable = call.Function;
    void *object = call.Object;
    if (!object && call.Function->GetFuncType() == asFUNC_DELEGATE) {
        callable = call.Function->GetDelegateFunction();
        object = call.Function->GetDelegateObject();
    }
    if (!callable) {
        diagnostic = MakeScriptDiagnostic(call.Phase,
                                          call.InvalidStateMessage ? call.InvalidStateMessage : "Script callback has invalid runtime state.");
        context->Release();
        return false;
    }

    ScriptCurrentModScope callScope(call.Owner);
    int code = context->Prepare(callable);
    if (code >= 0 && object)
        code = context->SetObject(object);
    if (code >= 0 && call.WriteArgs)
        code = call.WriteArgs(context, call.UserData);

    if (code < 0) {
        diagnostic = MakeScriptFunctionDiagnostic(call, code, context);
        context->Release();
        return false;
    }

    code = context->Execute();
    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeScriptFunctionDiagnostic(call, code, context);
        context->Release();
        return false;
    }

    if (call.ReadResult)
        call.ReadResult(context, call.UserData);

    context->Release();
    return true;
}

bool RejectScriptRestrictedHostCall(const char *apiName) {
    return ScriptModRuntime::RecordConstructionHostCallViolation(apiName) ||
           ScriptModRuntime::RecordStateHookHostCallViolation(apiName);
}

} // namespace BML
