#include "ScriptReloadStateValidator.h"

#include <new>
#include <utility>

#include "ModContext.h"
#include "ScriptStateMigration.h"

namespace BML {

ScriptReloadStateValidator::ScriptReloadStateValidator(ScriptMod &mod,
                                                       ScriptModReloadCandidate::State &state,
                                                       const ScriptModReloadOptions &options)
    : m_Mod(mod), m_State(state), m_Options(options) {
}

bool ScriptReloadStateValidator::FailWithDiagnostic(ScriptDiagnostic diagnostic,
                                                    std::vector<ScriptModReloadDiagnosticField> fields,
                                                    Failure &failure,
                                                    bool rewriteSnapshotPaths) {
    if (rewriteSnapshotPaths)
        RewriteReloadSnapshotDiagnosticPaths(m_State.Snapshot, diagnostic);
    failure = Failure();
    failure.Diagnostic = diagnostic;
    failure.Fields = std::move(fields);
    failure.PublishDiagnostic = true;
    return false;
}

bool ScriptReloadStateValidator::FailWithMessage(const ScriptDiagnostic &diagnostic,
                                                 std::vector<ScriptModReloadDiagnosticField> fields,
                                                 Failure &failure) {
    failure = Failure();
    failure.Diagnostic = diagnostic;
    failure.Fields = std::move(fields);
    failure.Message = diagnostic.Message;
    failure.PublishDiagnostic = false;
    return false;
}

bool ScriptReloadStateValidator::ValidateDeclarations(ScriptReloadStateValidation &validation,
                                                      Failure &failure) {
    ScriptDiagnostic stateDiagnostic;
    if (!ScriptStateMigration::HasSaveHook(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                           m_Mod.m_Runtime,
                                           validation.CurrentSavesState,
                                           stateDiagnostic)) {
        return FailWithDiagnostic(
            stateDiagnostic,
            BuildStateMigrationFailureFields("SaveState", "lookup_current_save", false, false, true),
            failure);
    }
    if (!validation.CurrentSavesState)
        return true;

    bool oldCanRestore = false;
    if (!ScriptStateMigration::HasRestoreState(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                               m_Mod.m_Runtime,
                                               oldCanRestore,
                                               stateDiagnostic)) {
        return FailWithDiagnostic(
            stateDiagnostic,
            BuildStateMigrationFailureFields("RestoreState", "lookup_current_restore", false, false, true),
            failure);
    }
    if (!oldCanRestore) {
        const ScriptDiagnostic diagnostic = MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime,
            "Script hot reload state migration is unsafe: current runtime declares SaveState(StateBag@) "
            "but does not declare RestoreState(StateBag@); rollback could not restore saved state.");
        return FailWithMessage(
            diagnostic,
            {{"boundary", "state_migration"},
             {"required", "RestoreState(StateBag@)"},
             {"action", "add_restore_hook_or_remove_SaveState"}},
            failure);
    }

    bool candidateCanRestore = false;
    if (!ScriptStateMigration::HasRestoreHook(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                              m_State.CandidateRuntime,
                                              candidateCanRestore,
                                              stateDiagnostic)) {
        return FailWithDiagnostic(
            stateDiagnostic,
            BuildStateMigrationFailureFields("RestoreState|MigrateState", "lookup_candidate_restore", false, false, true),
            failure,
            true);
    }
    if (!candidateCanRestore) {
        const ScriptDiagnostic diagnostic = MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime,
            "Script hot reload state migration is unsafe: current runtime declares SaveState(StateBag@), "
            "but the candidate does not declare RestoreState(StateBag@) or MigrateState(fromVersion, StateBag@).");
        return FailWithMessage(
            diagnostic,
            {{"boundary", "state_migration"},
             {"required", "RestoreState(StateBag@) or MigrateState(fromVersion, StateBag@)"},
             {"action", "add_candidate_restore_hook"}},
            failure);
    }

    return true;
}

bool ScriptReloadStateValidator::ExecuteDryRunHooks(ScriptReloadStateValidation &validation,
                                                    Failure &failure) {
    if (!m_Options.DryRun || !m_Options.CheckStateHooks || !validation.CurrentSavesState)
        return true;

    ScriptStateBagHandle dryRunState(new (std::nothrow) ScriptStateBag());
    if (!dryRunState) {
        const ScriptDiagnostic diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                                                 "Reload dry-run could not allocate a StateBag.");
        return FailWithMessage(
            diagnostic,
            {{"boundary", "state_migration"},
             {"action", "retry_or_restart"}},
            failure);
    }
    dryRunState->SetScriptAccessEnabled(false);
    dryRunState->SetReloadState(true);

    ScriptDiagnostic stateDiagnostic;
    bool stateSaved = false;
    {
        ScriptModReloadPhaseScope statePhase(m_Mod, ScriptModReloadPhase::SaveState);
        if (!ScriptStateMigration::Save(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                        m_Mod.m_Runtime,
                                        *dryRunState.Get(),
                                        stateSaved,
                                        stateDiagnostic)) {
            return FailWithDiagnostic(
                stateDiagnostic,
                BuildStateMigrationFailureFields("SaveState", "execute_current_save", true, true, true),
                failure);
        }
    }

    if (!stateSaved)
        return true;

    bool migrated = false;
    {
        ScriptModReloadPhaseScope statePhase(m_Mod, ScriptModReloadPhase::MigrateState);
        if (!ScriptStateMigration::Migrate(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                           m_State.CandidateRuntime,
                                           m_Mod.m_Definition.Version,
                                           *dryRunState.Get(),
                                           migrated,
                                           stateDiagnostic)) {
            return FailWithDiagnostic(
                stateDiagnostic,
                BuildStateMigrationFailureFields("MigrateState", "execute_candidate_migrate", true, true, true),
                failure,
                true);
        }
    }

    bool restored = false;
    {
        ScriptModReloadPhaseScope statePhase(m_Mod, ScriptModReloadPhase::RestoreState);
        if (!ScriptStateMigration::Restore(m_Mod.m_Context ? m_Mod.m_Context->GetCKContext() : nullptr,
                                           m_State.CandidateRuntime,
                                           *dryRunState.Get(),
                                           restored,
                                           stateDiagnostic)) {
            return FailWithDiagnostic(
                stateDiagnostic,
                BuildStateMigrationFailureFields("RestoreState", "execute_candidate_restore", true, true, true),
                failure,
                true);
        }
    }

    validation.DryRunStateHooksExecuted = true;
    validation.DryRunStateKeyCount = dryRunState->GetStoredCount();
    if (!migrated && !restored) {
        const ScriptDiagnostic diagnostic = MakeScriptDiagnostic(
            ScriptDiagnosticPhase::Runtime,
            "Reload dry-run saved state, but the candidate did not execute RestoreState(StateBag@) "
            "or MigrateState(fromVersion, StateBag@).");
        return FailWithMessage(
            diagnostic,
            {{"boundary", "state_migration"},
             {"required", "RestoreState(StateBag@) or MigrateState(fromVersion, StateBag@)"},
             {"action", "add_candidate_restore_hook"}},
            failure);
    }

    return true;
}

bool ScriptReloadStateValidator::Validate(ScriptReloadStateValidation &validation,
                                          Failure &failure) {
    validation = ScriptReloadStateValidation();
    failure = Failure();

    if (!m_Mod.m_State.IsLoaded())
        return true;
    if (!ValidateDeclarations(validation, failure))
        return false;
    return ExecuteDryRunHooks(validation, failure);
}

} // namespace BML
