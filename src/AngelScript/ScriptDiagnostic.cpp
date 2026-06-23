#include "ScriptDiagnostic.h"

#include <sstream>
#include <utility>

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

const char *ScriptCompilerMessageTypeName(CKAS_MESSAGETYPE type) {
    switch (type) {
    case CKAS_MESSAGE_ERROR:
        return "ERROR";
    case CKAS_MESSAGE_WARNING:
        return "WARN";
    case CKAS_MESSAGE_INFORMATION:
    default:
        return "INFO";
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
    diagnostic.Message = prefix.empty() ? ::CKAngelScriptAdapter::StatusName(status) : prefix;
    if (result.ErrorMessage && result.ErrorMessage[0])
        diagnostic.RawMessage = result.ErrorMessage;
    if (result.StackTrace && result.StackTrace[0])
        diagnostic.StackTrace = result.StackTrace;
    if (result.CompilerMessages && result.CompilerMessageCount != 0) {
        diagnostic.CompilerMessages.reserve(result.CompilerMessageCount);
        for (size_t i = 0; i < result.CompilerMessageCount; ++i) {
            const CKAngelScriptCompilerMessage &source = result.CompilerMessages[i];
            if (source.Size < sizeof(CKAngelScriptCompilerMessage))
                continue;
            ScriptCompilerMessage message;
            message.Section = source.Section ? source.Section : "";
            message.Row = source.Row;
            message.Column = source.Column;
            message.Type = source.Type;
            message.Message = source.Message ? source.Message : "";
            diagnostic.CompilerMessages.push_back(std::move(message));
        }
    }
    return diagnostic;
}

std::string FormatScriptDiagnostic(const ScriptDiagnostic &diagnostic) {
    std::ostringstream text;
    text << "phase=" << ScriptDiagnosticPhaseName(diagnostic.Phase);
    if (diagnostic.Status != CKAS_OK)
        text << " status=" << ::CKAngelScriptAdapter::StatusName(diagnostic.Status);
    if (diagnostic.AngelScriptCode != 0)
        text << " code=" << diagnostic.AngelScriptCode;
    text << " message=" << diagnostic.Message;
    if (!diagnostic.EntryPath.empty())
        text << " entry=" << diagnostic.EntryPath;
    if (!diagnostic.RawMessage.empty())
        text << "\n" << diagnostic.RawMessage;
    if (diagnostic.RawMessage.empty()) {
        for (const ScriptCompilerMessage &message : diagnostic.CompilerMessages) {
            text << "\ncompiler " << ScriptCompilerMessageTypeName(message.Type);
            if (!message.Section.empty())
                text << " section=" << message.Section;
            if (message.Row != 0 || message.Column != 0)
                text << " line=" << message.Row << " column=" << message.Column;
            if (!message.Message.empty())
                text << " message=" << message.Message;
        }
    }
    if (!diagnostic.StackTrace.empty())
        text << "\n" << diagnostic.StackTrace;
    return text.str();
}

} // namespace BML
