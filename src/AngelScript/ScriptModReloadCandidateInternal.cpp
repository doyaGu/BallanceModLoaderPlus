#include "ScriptModReloadCandidateInternal.h"

#include <algorithm>

#include "Utils/PathUtils.h"

namespace BML {

static constexpr const char *kReloadRollbackBoundary =
    "Rollback restores only BML-managed script resources such as callbacks, exports, timers, commands, DataShare requests, and script runtime handles; it cannot undo game-world changes made by script code.";

ScriptModReloadSourceSnapshot::~ScriptModReloadSourceSnapshot() {
    Cleanup();
}

void ScriptModReloadSourceSnapshot::Reset() {
    Cleanup();
    CompileCandidate = ScriptModLoadCandidate();
    CompileEntry = ScriptModEntry();
    CommitEntry = ScriptModEntry();
    CompileEntryPathUtf8.clear();
    CommitEntryPathUtf8.clear();
    EntrySectionNameUtf8.clear();
    SourceSections.clear();
    SourceLibraries.clear();
    SourceDependencies.clear();
    DiagnosticStagedRootUtf8.clear();
    DiagnosticDisplayRootUtf8.clear();
}

void ScriptModReloadSourceSnapshot::Cleanup() {
    if (!StagedRoot.empty()) {
        utils::DeleteDirectoryW(StagedRoot);
        StagedRoot.clear();
    }
}

void ScriptModReloadSourceSnapshot::KeepStagedRoot() {
    StagedRoot.clear();
}

const char *ScriptReloadRollbackBoundary() {
    return kReloadRollbackBoundary;
}

std::vector<ScriptModReloadDiagnosticField> BuildStateMigrationFailureFields(const char *hook,
                                                                             const char *operation,
                                                                             bool stateHookExecuted,
                                                                             bool oldSaveStateExecuted,
                                                                             bool oldRuntimeKept) {
    return {
        {"boundary", "state_migration"},
        {"hook", hook ? hook : ""},
        {"operation", operation ? operation : ""},
        {"stateHookExecuted", stateHookExecuted ? "true" : "false"},
        {"oldSaveStateExecuted", oldSaveStateExecuted ? "true" : "false"},
        {"oldRuntimeKept", oldRuntimeKept ? "true" : "false"},
        {"rollbackScope", kReloadRollbackBoundary},
        {"stateHookContract", "state hooks may only copy primitive/string values through BML::StateBag and use read-only queries/logging"},
    };
}

static void ReplaceAllText(std::string &value, const std::string &from, const std::string &to) {
    if (from.empty())
        return;

    size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
}

static void ReplacePathPrefixText(std::string &value, const std::string &from, const std::string &to) {
    if (value.empty() || from.empty() || from == to)
        return;

    ReplaceAllText(value, from, to);
    std::string slashFrom = from;
    std::replace(slashFrom.begin(), slashFrom.end(), '\\', '/');
    if (slashFrom != from)
        ReplaceAllText(value, slashFrom, to);
}

void RewriteReloadSnapshotDiagnosticPaths(const ScriptModReloadSourceSnapshot &snapshot,
                                          ScriptDiagnostic &diagnostic) {
    if (snapshot.DiagnosticStagedRootUtf8.empty() ||
        snapshot.DiagnosticDisplayRootUtf8.empty()) {
        return;
    }

    ReplacePathPrefixText(diagnostic.EntryPath,
                          snapshot.DiagnosticStagedRootUtf8,
                          snapshot.DiagnosticDisplayRootUtf8);
    ReplacePathPrefixText(diagnostic.RawMessage,
                          snapshot.DiagnosticStagedRootUtf8,
                          snapshot.DiagnosticDisplayRootUtf8);
    ReplacePathPrefixText(diagnostic.StackTrace,
                          snapshot.DiagnosticStagedRootUtf8,
                          snapshot.DiagnosticDisplayRootUtf8);
    for (ScriptCompilerMessage &message : diagnostic.CompilerMessages) {
        ReplacePathPrefixText(message.Section,
                              snapshot.DiagnosticStagedRootUtf8,
                              snapshot.DiagnosticDisplayRootUtf8);
    }
}

} // namespace BML
