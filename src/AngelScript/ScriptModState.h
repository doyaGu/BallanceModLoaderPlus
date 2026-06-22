#ifndef BML_SCRIPTMODSTATE_H
#define BML_SCRIPTMODSTATE_H

#include <string>

#include "ScriptDiagnostic.h"

namespace BML {

class ScriptModState {
public:
    bool IsLoaded() const { return m_Loaded; }
    bool IsFailed() const { return m_Failed; }
    const ScriptDiagnostic &GetLastDiagnostic() const { return m_LastDiagnostic; }
    const std::string &GetLastDiagnosticText() const { return m_LastDiagnosticText; }

    void MarkLoaded(bool loaded);
    void Record(const ScriptDiagnostic &diagnostic);
    void Fail(const ScriptDiagnostic &diagnostic);
    void ClearFailure();

private:
    bool m_Loaded = false;
    bool m_Failed = false;
    ScriptDiagnostic m_LastDiagnostic;
    std::string m_LastDiagnosticText;
};

} // namespace BML

#endif
