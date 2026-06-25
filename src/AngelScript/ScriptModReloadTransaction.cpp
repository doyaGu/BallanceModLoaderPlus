#include "ScriptModReloadTransaction.h"

#include <new>
#include <utility>

#include "Logger.h"
#include "ModContext.h"
#include "ScriptDevToolsService.h"
#include "ScriptStateMigration.h"
#include "Utils/PathUtils.h"
#include "Utils/StringUtils.h"

namespace BML {

ScriptModReloadTransaction::ScriptModReloadTransaction(ScriptMod &mod,
                                                       ScriptModReloadCandidate &candidate)
    : m_Mod(mod), m_State(*candidate.m_State) {
}

bool ScriptModReloadTransaction::CaptureStateForCommit(ScriptStateBagHandle &savedState,
                                                       Failure &failure) {
    failure = Failure();
    m_State.OldVersion = m_Mod.m_Definition.Version;
    m_State.StateSaved = false;
    m_State.RollbackState.Reset();
    savedState.Reset();

    if (!m_Mod.m_State.IsLoaded())
        return true;

    ScriptDiagnostic stateDiagnostic;
    bool currentSavesState = false;
    if (!ScriptStateMigration::HasSaveHook(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                           m_Mod.m_Runtime,
                                           currentSavesState,
                                           stateDiagnostic)) {
        RewriteReloadSnapshotDiagnosticPaths(m_State.Snapshot, stateDiagnostic);
        failure.Diagnostic = stateDiagnostic;
        failure.Fields = BuildStateMigrationFailureFields("SaveState", "lookup_current_save", false, false, true);
        failure.PublishDiagnostic = true;
        return false;
    }
    if (!currentSavesState)
        return true;

    savedState.Reset(new (std::nothrow) ScriptStateBag());
    if (!savedState) {
        failure.Diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                                  "Script hot reload could not allocate a StateBag.");
        failure.Fields = {
            {"boundary", "state_migration"},
            {"action", "retry_or_restart"},
        };
        return false;
    }
    savedState->SetScriptAccessEnabled(false);
    savedState->SetReloadState(true);

    bool stateSaved = false;
    {
        ScriptModReloadPhaseScope statePhase(m_Mod, ScriptModReloadPhase::SaveState);
        if (!ScriptStateMigration::Save(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                        m_Mod.m_Runtime,
                                        *savedState.Get(),
                                        stateSaved,
                                        stateDiagnostic)) {
            RewriteReloadSnapshotDiagnosticPaths(m_State.Snapshot, stateDiagnostic);
            failure.Diagnostic = stateDiagnostic;
            failure.Fields = BuildStateMigrationFailureFields("SaveState", "execute_current_save", true, true, true);
            failure.PublishDiagnostic = true;
            return false;
        }
    }

    if (!stateSaved) {
        savedState.Reset();
    } else {
        m_State.RollbackState.Reset(savedState->CloneNoThrow());
        if (!m_State.RollbackState) {
            failure.Diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                                      "Script hot reload could not clone StateBag for rollback.");
            failure.Fields = {
                {"boundary", "state_migration"},
                {"action", "retry_or_restart"},
            };
            return false;
        }
        m_State.RollbackState->SetScriptAccessEnabled(false);
    }

    m_State.StateSaved = stateSaved;
    if (stateSaved && savedState && m_Mod.m_Context && m_Mod.m_Context->GetScriptDevTools()) {
        m_Mod.m_Context->GetScriptDevTools()->PublishEvent(ScriptDevEventSeverity::Info,
                                                           "ScriptReloadStateSaved",
                                                           m_Mod.GetID() ? m_Mod.GetID() : "",
                                                           "reload",
                                                           m_State.Snapshot.CommitEntryPathUtf8,
                                                           "Script hot reload state saved.",
                                                           {{"keys", std::to_string(savedState->GetStoredCount())},
                                                            {"fromVersion", m_State.OldVersion}},
                                                           m_Mod.GetReloadAttemptId());
    }
    return true;
}

bool ScriptModReloadTransaction::PromoteFailedPlaceholder(Failure &failure) {
    failure = Failure();
    const std::string oldId = m_Mod.m_Definition.Id;
    if (!m_Mod.IsFailedPlaceholder())
        return true;

    if (!m_Mod.m_Context) {
        failure.Diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata,
                                                  "Script mod failed-load recovery requires a ModContext.");
        return false;
    }

    std::string recoveryDiagnostic;
    if (!m_Mod.m_Context->PromoteFailedScriptModPlaceholder(&m_Mod,
                                                            oldId,
                                                            m_State.CandidateDefinition,
                                                            recoveryDiagnostic)) {
        failure.Diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Metadata, recoveryDiagnostic);
        return false;
    }

    m_State.RecoveryPromoted = true;
    return true;
}

void ScriptModReloadTransaction::DeactivateCurrentRuntime() {
    ScriptDiagnostic unloadDiagnostic;
    if (m_Mod.m_State.IsLoaded()) {
        ScriptModReloadPhaseScope unloadPhase(m_Mod, ScriptModReloadPhase::Unload);
        if (!m_Mod.m_EventRouter.CallOnUnload(unloadDiagnostic))
            m_Mod.Record(unloadDiagnostic);
    }

    m_Mod.ReleaseScriptServices();
    m_Mod.ReleaseScriptMethodHandles();
    m_Mod.m_State.MarkLoaded(false);
}

void ScriptModReloadTransaction::ReleaseCurrentRuntime() {
    DeactivateCurrentRuntime();
    m_Mod.ReleaseRuntimeOnly(m_Mod.m_Runtime);
}

void ScriptModReloadTransaction::RetainCurrentRuntime() {
    m_State.OldDefinition = std::move(m_Mod.m_Definition);
    m_State.OldEntry = std::move(m_Mod.m_Entry);
    m_State.OldRuntime = std::move(m_Mod.m_Runtime);
    m_State.OldDiagnosticPathFrom = m_Mod.m_RuntimeDiagnosticPathFrom;
    m_State.OldDiagnosticPathTo = m_Mod.m_RuntimeDiagnosticPathTo;
    m_State.OldRuntimeRetained = true;
}

void ScriptModReloadTransaction::InstallPreparedRuntime() {
    m_Mod.m_Definition = std::move(m_State.CandidateDefinition);
    m_Mod.m_Entry = m_State.Snapshot.CommitEntry;
    m_Mod.m_Runtime = std::move(m_State.CandidateRuntime);
    m_Mod.m_Runtime.SetOwner(&m_Mod);
    m_Mod.SetRuntimeDiagnosticPathRewrite(m_State.Snapshot.DiagnosticStagedRootUtf8,
                                          m_State.Snapshot.DiagnosticDisplayRootUtf8);
    m_Mod.m_State.ClearFailure();
}

void ScriptModReloadTransaction::RestoreRetainedRuntime() {
    m_Mod.m_Definition = std::move(m_State.OldDefinition);
    m_Mod.m_Entry = std::move(m_State.OldEntry);
    m_Mod.m_Runtime = std::move(m_State.OldRuntime);
    m_Mod.m_Runtime.SetOwner(&m_Mod);
    m_State.OldRuntimeRetained = false;
    m_Mod.SetRuntimeDiagnosticPathRewrite(m_State.OldDiagnosticPathFrom, m_State.OldDiagnosticPathTo);
}

void ScriptModReloadTransaction::RestoreFailedPlaceholderAfterRejectedRecovery(const std::string &candidateId) {
    RestoreRetainedRuntime();
    if (m_State.RecoveryPromoted && m_Mod.m_Context)
        m_Mod.m_Context->RestoreFailedScriptModPlaceholder(&m_Mod, candidateId, m_Mod.m_Definition);
}

void ScriptModReloadTransaction::InstallPreparedRuntimeForCommit() {
    DeactivateCurrentRuntime();
    RetainCurrentRuntime();
    InstallPreparedRuntime();
}

void ScriptModReloadTransaction::RestoreRetainedRuntimeForRollback() {
    RestoreRetainedRuntime();
    m_Mod.m_State.ClearFailure();
}

void ScriptModReloadTransaction::RestoreCommittedRuntimeToRetained(const ScriptModEntry &committedEntry) {
    ReleaseCurrentRuntime();
    RemoveCommittedStaging(committedEntry);
    RestoreRetainedRuntimeForRollback();
}

bool ScriptModReloadTransaction::LoadRuntimeForPhase(ScriptModReloadPhase phase,
                                                     bool failedLoadRecovery,
                                                     ScriptStateBag *restoreState,
                                                     const std::string &restoreFromVersion,
                                                     bool migrateState,
                                                     std::vector<ScriptModReloadDiagnosticField> *failureFields) {
    ScriptModReloadPhaseScope phaseScope(m_Mod, phase);
    return m_Mod.LoadCurrentRuntime(true,
                                    failedLoadRecovery,
                                    restoreState,
                                    restoreFromVersion,
                                    migrateState,
                                    failureFields);
}

void ScriptModReloadTransaction::RemoveCommittedStaging(const ScriptModEntry &committedEntry) {
    if (committedEntry.SourceKind != ScriptModEntrySourceKind::ZipPackage ||
        committedEntry.RootDirectory.empty() ||
        committedEntry.RootDirectory == m_State.OldEntry.RootDirectory) {
        return;
    }

    if (!utils::DeleteDirectoryW(committedEntry.RootDirectory)) {
        const std::string committedRoot = utils::Utf16ToUtf8(committedEntry.RootDirectory);
        m_Mod.Record(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                          "Failed to remove rolled-back script package reload staging directory: " + committedRoot));
        if (m_Mod.m_Context && m_Mod.m_Context->GetLogger()) {
            m_Mod.m_Context->GetLogger()->Warn("Failed to remove rolled-back script package reload staging directory: %s",
                                               committedRoot.c_str());
        }
    }
}

} // namespace BML
