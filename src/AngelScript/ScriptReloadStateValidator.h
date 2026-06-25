#ifndef BML_SCRIPTRELOADSTATEVALIDATOR_H
#define BML_SCRIPTRELOADSTATEVALIDATOR_H

#include <string>
#include <vector>

#include "ScriptModReloadCandidateInternal.h"

namespace BML {

struct ScriptReloadStateValidation {
    bool CurrentSavesState = false;
    bool DryRunStateHooksExecuted = false;
    int DryRunStateKeyCount = 0;
};

class ScriptReloadStateValidator final {
public:
    struct Failure {
        ScriptDiagnostic Diagnostic;
        std::vector<ScriptModReloadDiagnosticField> Fields;
        std::string Message;
        bool PublishDiagnostic = true;
    };

    ScriptReloadStateValidator(ScriptMod &mod,
                               ScriptModReloadCandidate::State &state,
                               const ScriptModReloadOptions &options);

    bool Validate(ScriptReloadStateValidation &validation, Failure &failure);

private:
    bool FailWithDiagnostic(ScriptDiagnostic diagnostic,
                            std::vector<ScriptModReloadDiagnosticField> fields,
                            Failure &failure,
                            bool rewriteSnapshotPaths = false);
    bool FailWithMessage(const ScriptDiagnostic &diagnostic,
                         std::vector<ScriptModReloadDiagnosticField> fields,
                         Failure &failure);
    bool ValidateDeclarations(ScriptReloadStateValidation &validation, Failure &failure);
    bool ExecuteDryRunHooks(ScriptReloadStateValidation &validation, Failure &failure);

    ScriptMod &m_Mod;
    ScriptModReloadCandidate::State &m_State;
    const ScriptModReloadOptions &m_Options;
};

} // namespace BML

#endif
