#ifndef BML_SCRIPTMODRELOADTRANSACTION_H
#define BML_SCRIPTMODRELOADTRANSACTION_H

#include <string>
#include <vector>

#include "ScriptModReloadCandidateInternal.h"

namespace BML {

class ScriptModReloadTransaction final {
public:
    struct Failure {
        ScriptDiagnostic Diagnostic;
        std::vector<ScriptModReloadDiagnosticField> Fields;
        bool PublishDiagnostic = false;
    };

    ScriptModReloadTransaction(ScriptMod &mod, ScriptModReloadCandidate &candidate);

    bool CaptureStateForCommit(ScriptStateBagHandle &savedState, Failure &failure);
    bool PromoteFailedPlaceholder(Failure &failure);
    void InstallPreparedRuntimeForCommit();
    void RestoreFailedPlaceholderAfterRejectedRecovery(const std::string &candidateId);
    void RestoreRetainedRuntimeForRollback();
    void RestoreCommittedRuntimeToRetained(const ScriptModEntry &committedEntry);
    bool LoadRuntimeForPhase(ScriptModReloadPhase phase,
                             bool failedLoadRecovery,
                             ScriptStateBag *restoreState,
                             const std::string &restoreFromVersion,
                             bool migrateState,
                             std::vector<ScriptModReloadDiagnosticField> *failureFields);

private:
    void DeactivateCurrentRuntime();
    void ReleaseCurrentRuntime();
    void RetainCurrentRuntime();
    void InstallPreparedRuntime();
    void RestoreRetainedRuntime();
    void RemoveCommittedStaging(const ScriptModEntry &committedEntry);

    ScriptMod &m_Mod;
    ScriptModReloadCandidate::State &m_State;
};

} // namespace BML

#endif
