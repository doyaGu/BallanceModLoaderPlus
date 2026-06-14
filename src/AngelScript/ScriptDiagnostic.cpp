#include "ScriptDiagnostic.h"

#include <sstream>

namespace BML {

const char *ScriptDiagnosticPhaseName(ScriptDiagnosticPhase phase) {
    switch (phase) {
    case ScriptDiagnosticPhase::Entry:
        return "entry";
    case ScriptDiagnosticPhase::CkasHost:
        return "ckas-host";
    case ScriptDiagnosticPhase::Compile:
        return "compile";
    case ScriptDiagnosticPhase::Metadata:
        return "metadata";
    case ScriptDiagnosticPhase::CreateObject:
        return "create-object";
    case ScriptDiagnosticPhase::MethodLookup:
        return "method-lookup";
    case ScriptDiagnosticPhase::ExportLookup:
        return "export-lookup";
    case ScriptDiagnosticPhase::ExportCall:
        return "export-call";
    case ScriptDiagnosticPhase::Callback:
        return "callback";
    case ScriptDiagnosticPhase::Unload:
        return "unload";
    case ScriptDiagnosticPhase::Runtime:
    default:
        return "runtime";
    }
}

ScriptDiagnostic MakeScriptDiagnostic(ScriptDiagnosticPhase phase, const std::string &message) {
    ScriptDiagnostic diagnostic;
    diagnostic.Phase = phase;
    diagnostic.Status = CKAS_OK;
    diagnostic.Message = message;
    return diagnostic;
}

ScriptDiagnostic MakeScriptDiagnostic(ScriptDiagnosticPhase phase,
                                      CKAS_STATUS status,
                                      const CKAngelScriptResult &result,
                                      const std::string &prefix) {
    ScriptDiagnostic diagnostic;
    diagnostic.Phase = phase;
    diagnostic.Status = status;
    diagnostic.AngelScriptCode = result.AngelScriptCode;
    diagnostic.Message = prefix;
    if (!diagnostic.Message.empty())
        diagnostic.Message += ": ";
    diagnostic.Message += ::CKAngelScriptAdapter::FormatResult(status, result);
    if (result.StackTrace && result.StackTrace[0])
        diagnostic.StackTrace = result.StackTrace;
    return diagnostic;
}

std::string FormatScriptDiagnostic(const ScriptDiagnostic &diagnostic) {
    std::ostringstream text;
    text << "phase=" << ScriptDiagnosticPhaseName(diagnostic.Phase);
    text << " message=" << diagnostic.Message;
    if (!diagnostic.EntryPath.empty())
        text << " entry=" << diagnostic.EntryPath;
    if (!diagnostic.StackTrace.empty())
        text << "\n" << diagnostic.StackTrace;
    return text.str();
}

} // namespace BML
