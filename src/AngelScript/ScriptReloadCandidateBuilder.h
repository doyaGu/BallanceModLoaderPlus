#ifndef BML_SCRIPTRELOADCANDIDATEBUILDER_H
#define BML_SCRIPTRELOADCANDIDATEBUILDER_H

#include <string>
#include <vector>

#include "ScriptExportDispatcher.h"
#include "ScriptModEventRouter.h"
#include "ScriptModReloadCandidateInternal.h"

namespace BML {

class ScriptReloadCandidateBuilder final {
public:
    struct Failure {
        ScriptDiagnostic Diagnostic;
        std::vector<ScriptModReloadDiagnosticField> Fields;
        std::string Message;
        bool PublishDiagnostic = true;
    };

    ScriptReloadCandidateBuilder(ScriptMod &mod,
                                 ScriptModReloadCandidate::State &state,
                                 const ScriptModReloadOptions &options);
    ~ScriptReloadCandidateBuilder();

    ScriptReloadCandidateBuilder(const ScriptReloadCandidateBuilder &) = delete;
    ScriptReloadCandidateBuilder &operator=(const ScriptReloadCandidateBuilder &) = delete;

    bool Build(ScriptModReloadResult &result, Failure &failure);
    void KeepPreparedRuntime();

private:
    bool FailWithDiagnostic(ScriptDiagnostic diagnostic, Failure &failure, bool rewriteSnapshotPaths = true);
    bool FailWithMessage(const std::string &message,
                         const ScriptDiagnostic &diagnostic,
                         const std::vector<ScriptModReloadDiagnosticField> &fields,
                         Failure &failure);
    void ReleasePreparedHandles();
    void DiscardPreparedCandidate();

    ScriptMod &m_Mod;
    ScriptModReloadCandidate::State &m_State;
    const ScriptModReloadOptions &m_Options;
    ScriptModEventRouter m_CandidateEvents;
    ScriptExportTable m_CandidateExports;
    bool m_EventsBound = false;
    bool m_ExportsTouched = false;
    bool m_RuntimeOwned = false;
};

} // namespace BML

#endif
