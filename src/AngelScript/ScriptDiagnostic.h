#ifndef BML_SCRIPTDIAGNOSTIC_H
#define BML_SCRIPTDIAGNOSTIC_H

#include <string>
#include <vector>

#include "CKAngelScriptAdapter.h"

namespace BML {

enum class ScriptDiagnosticPhase {
    Entry,
    CkasHost,
    Compile,
    Metadata,
    CreateObject,
    MethodLookup,
    ExportLookup,
    ExportCall,
    Callback,
    Runtime,
    Unload,
};

struct ScriptCompilerMessage {
    std::string Section;
    int Row = 0;
    int Column = 0;
    CKAS_MESSAGETYPE Type = CKAS_MESSAGE_INFORMATION;
    std::string Message;
};

struct ScriptDiagnostic {
    ScriptDiagnosticPhase Phase = ScriptDiagnosticPhase::Runtime;
    CKAS_STATUS Status = CKAS_OK;
    int AngelScriptCode = 0;
    std::string Message;
    std::string RawMessage;
    std::string StackTrace;
    std::string EntryPath;
    std::vector<ScriptCompilerMessage> CompilerMessages;
};

const char *ScriptDiagnosticPhaseName(ScriptDiagnosticPhase phase);
const char *ScriptCompilerMessageTypeName(CKAS_MESSAGETYPE type);
ScriptDiagnostic MakeScriptDiagnostic(ScriptDiagnosticPhase phase, const std::string &message);
ScriptDiagnostic MakeScriptDiagnostic(ScriptDiagnosticPhase phase,
                                      CKAS_STATUS status,
                                      const CKAngelScriptResult &result,
                                      const std::string &prefix = std::string());
std::string FormatScriptDiagnostic(const ScriptDiagnostic &diagnostic);

} // namespace BML

#endif
