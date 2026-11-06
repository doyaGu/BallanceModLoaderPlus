#include "ScriptModState.h"

namespace BML {

void ScriptModState::MarkLoaded(bool loaded) {
    m_State.MarkLoaded(loaded);
}

void ScriptModState::Record(const ScriptDiagnostic &diagnostic) {
    m_LastDiagnostic = diagnostic;
    m_LastDiagnosticText = FormatScriptDiagnostic(diagnostic);
}

void ScriptModState::Fail(const ScriptDiagnostic &diagnostic) {
    m_State.Fail();
    Record(diagnostic);
}

void ScriptModState::ClearFailure() {
    m_State.ClearFailure();
    m_LastDiagnostic = ScriptDiagnostic();
    m_LastDiagnosticText.clear();
}

} // namespace BML
