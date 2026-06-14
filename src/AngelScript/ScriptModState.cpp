#include "ScriptModState.h"

namespace BML {

void ScriptModState::MarkLoaded(bool loaded) {
    m_Loaded = loaded;
}

void ScriptModState::Record(const ScriptDiagnostic &diagnostic) {
    m_LastDiagnostic = diagnostic;
    m_LastDiagnosticText = FormatScriptDiagnostic(diagnostic);
}

void ScriptModState::Fail(const ScriptDiagnostic &diagnostic) {
    m_Failed = true;
    Record(diagnostic);
}

} // namespace BML
