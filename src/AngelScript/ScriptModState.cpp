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

void ScriptModState::ClearFailure() {
    m_Failed = false;
    m_LastDiagnostic = ScriptDiagnostic();
    m_LastDiagnosticText.clear();
}

} // namespace BML
