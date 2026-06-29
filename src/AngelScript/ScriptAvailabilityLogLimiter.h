#ifndef BML_SCRIPTAVAILABILITYLOGLIMITER_H
#define BML_SCRIPTAVAILABILITYLOGLIMITER_H

#include <string>

#include "CKAngelScriptAdapter.h"

namespace BML {

class ScriptAvailabilityLogLimiter {
public:
    bool ShouldLog(CKAngelScriptAdapter::State state, const std::string &diagnostic) {
        if (state == m_LastState && diagnostic == m_LastDiagnostic)
            return false;

        m_LastState = state;
        m_LastDiagnostic = diagnostic;
        return true;
    }

    void Reset() {
        m_LastState = CKAngelScriptAdapter::State::Unchecked;
        m_LastDiagnostic.clear();
    }

private:
    CKAngelScriptAdapter::State m_LastState = CKAngelScriptAdapter::State::Unchecked;
    std::string m_LastDiagnostic;
};

} // namespace BML

#endif
