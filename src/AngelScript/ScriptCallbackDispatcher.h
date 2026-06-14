#ifndef BML_SCRIPTCALLBACKDISPATCHER_H
#define BML_SCRIPTCALLBACKDISPATCHER_H

#include <cstddef>
#include <vector>

#include "BML/ICommand.h"
#include "BML/IMod.h"
#include "ScriptApiContract.h"
#include "ScriptCallbackEvents.h"
#include "ScriptDiagnostic.h"
#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"

namespace BML {

class ScriptCallbackDispatcher {
public:
    bool CacheAll(CKContext *context, ScriptModRuntime &runtime, ScriptDiagnostic &diagnostic);
    bool Release(CKContext *context, ScriptModRuntime &runtime, ScriptDiagnostic *diagnostic = nullptr);
    bool HasCallback(ScriptCallbackId id) const;

    bool CallOnLoad(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, ScriptDiagnostic &diagnostic);
    bool CallOnUnload(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, ScriptDiagnostic &diagnostic);
    bool CallOnProcess(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, ScriptDiagnostic &diagnostic);
    bool CallGameEvent(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, size_t eventIndex, ScriptDiagnostic &diagnostic);
    bool CallRender(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, CK_RENDER_FLAGS flags, ScriptDiagnostic &diagnostic);
    bool CallLoadObject(CKContext *context,
                        ScriptModRuntime &runtime,
                        ScriptModContextView &contextView,
                        const char *filename,
                        CKBOOL isMap,
                        const char *masterName,
                        CK_CLASSID filterClass,
                        CKBOOL addToScene,
                        CKBOOL reuseMeshes,
                        CKBOOL reuseMaterials,
                        CKBOOL dynamic,
                        ScriptDiagnostic &diagnostic);
    bool CallLoadScript(CKContext *context,
                        ScriptModRuntime &runtime,
                        ScriptModContextView &contextView,
                        const char *filename,
                        CK_ID scriptId,
                        ScriptDiagnostic &diagnostic);
    bool CallCheatEnabled(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, bool enable, ScriptDiagnostic &diagnostic);
    bool CallCommandEvent(CKContext *context,
                          ScriptModRuntime &runtime,
                          ScriptModContextView &contextView,
                          bool beforeCommand,
                          ICommand *command,
                          const std::vector<std::string> &args,
                          ScriptDiagnostic &diagnostic);
    bool CallPhysicalize(CKContext *context,
                         ScriptModRuntime &runtime,
                         ScriptModContextView &contextView,
                         CK3dEntity *target,
                         CKBOOL fixed,
                         float friction,
                         float elasticity,
                         float mass,
                         const char *collGroup,
                         CKBOOL startFrozen,
                         CKBOOL enableColl,
                         CKBOOL calcMassCenter,
                         float linearDamp,
                         float rotDamp,
                         const char *collSurface,
                         VxVector massCenter,
                         int convexCnt,
                         int ballCnt,
                         int concaveCnt,
                         ScriptDiagnostic &diagnostic);
    bool CallUnphysicalize(CKContext *context,
                           ScriptModRuntime &runtime,
                           ScriptModContextView &contextView,
                           CK3dEntity *target,
                           ScriptDiagnostic &diagnostic);

private:
    bool CallContextOnly(CKContext *context,
                         ScriptModRuntime &runtime,
                         ScriptCallbackId id,
                         ScriptModContextView &contextView,
                         ScriptDiagnostic &diagnostic);
    bool CallWithEvent(CKContext *context,
                       ScriptModRuntime &runtime,
                       ScriptCallbackId id,
                       ScriptModContextView &contextView,
                       void *eventView,
                       ScriptDiagnostic &diagnostic);

    CKAngelScriptMethod *m_Methods[ScriptCallbackCount] = {};
};

} // namespace BML

#endif
