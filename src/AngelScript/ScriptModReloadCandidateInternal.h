#ifndef BML_SCRIPTMODRELOADCANDIDATEINTERNAL_H
#define BML_SCRIPTMODRELOADCANDIDATEINTERNAL_H

#include <string>
#include <vector>

#include "ScriptMod.h"
#include "ScriptSourceSnapshot.h"
#include "ScriptStateBag.h"

namespace BML {

class ScriptModReloadPhaseScope {
public:
    ScriptModReloadPhaseScope(ScriptMod &mod, ScriptModReloadPhase phase)
        : m_Mod(mod), m_Previous(mod.GetReloadPhase()) {
        m_Mod.SetReloadPhase(phase);
    }

    ~ScriptModReloadPhaseScope() {
        m_Mod.SetReloadPhase(m_Previous);
    }

    ScriptModReloadPhaseScope(const ScriptModReloadPhaseScope &) = delete;
    ScriptModReloadPhaseScope &operator=(const ScriptModReloadPhaseScope &) = delete;

private:
    ScriptMod &m_Mod;
    ScriptModReloadPhase m_Previous;
};

struct ScriptModReloadSourceSnapshot {
    ScriptModLoadCandidate CompileCandidate;
    ScriptModEntry CompileEntry;
    ScriptModEntry CommitEntry;
    std::string CompileEntryPathUtf8;
    std::string CommitEntryPathUtf8;
    std::string EntrySectionNameUtf8;
    std::vector<ScriptSourceSection> SourceSections;
    std::vector<ScriptLibraryUse> SourceLibraries;
    std::vector<ScriptSourceDependency> SourceDependencies;
    std::wstring StagedRoot;
    std::string DiagnosticStagedRootUtf8;
    std::string DiagnosticDisplayRootUtf8;

    ScriptModReloadSourceSnapshot() = default;
    ScriptModReloadSourceSnapshot(const ScriptModReloadSourceSnapshot &) = delete;
    ScriptModReloadSourceSnapshot &operator=(const ScriptModReloadSourceSnapshot &) = delete;

    ~ScriptModReloadSourceSnapshot();

    void Reset();
    void Cleanup();
    void KeepStagedRoot();
};

struct ScriptModReloadCandidate::State {
    ScriptMod *Owner = nullptr;
    ScriptModReloadSourceSnapshot Snapshot;
    ScriptModDefinition CandidateDefinition;
    ScriptModRuntime CandidateRuntime;
    bool Prepared = false;
    bool Committed = false;
    bool RolledBack = false;
    bool OldRuntimeRetained = false;
    bool RecoveryPromoted = false;
    std::string OldVersion;
    ScriptStateBagHandle RollbackState;
    bool StateSaved = false;
    ScriptModDefinition OldDefinition;
    ScriptModEntry OldEntry;
    ScriptModRuntime OldRuntime;
    std::string OldDiagnosticPathFrom;
    std::string OldDiagnosticPathTo;
};

const char *ScriptReloadRollbackBoundary();

std::vector<ScriptModReloadDiagnosticField> BuildStateMigrationFailureFields(const char *hook,
                                                                             const char *operation,
                                                                             bool stateHookExecuted,
                                                                             bool oldSaveStateExecuted,
                                                                             bool oldRuntimeKept);

void RewriteReloadSnapshotDiagnosticPaths(const ScriptModReloadSourceSnapshot &snapshot,
                                          ScriptDiagnostic &diagnostic);

} // namespace BML

#endif
