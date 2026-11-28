#ifndef BML_SCRIPTMODSTATE_H
#define BML_SCRIPTMODSTATE_H

#include <string>

#include "ScriptDiagnostic.h"
#include "Loader/ModState.h"

namespace BML {

class ScriptModState {
public:
    bool IsLoaded() const { return m_State.IsLoaded(); }
    bool IsFailed() const { return m_State.IsFailed(); }
    const ScriptDiagnostic &GetLastDiagnostic() const { return m_LastDiagnostic; }
    const std::string &GetLastDiagnosticText() const { return m_LastDiagnosticText; }

    void MarkLoaded(bool loaded);
    void Record(const ScriptDiagnostic &diagnostic);
    void Fail(const ScriptDiagnostic &diagnostic);
    void ClearFailure();

private:
    ModState m_State;
    ScriptDiagnostic m_LastDiagnostic;
    std::string m_LastDiagnosticText;
};

} // namespace BML

#endif
