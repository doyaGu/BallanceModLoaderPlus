#include "ScriptModEventRouter.h"

#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"

namespace BML {

void ScriptModEventRouter::Bind(CKContext *context, ScriptModRuntime *runtime, ScriptModContextView *contextView) {
    m_Context = context;
    m_Runtime = runtime;
    m_ContextView = contextView;
}

bool ScriptModEventRouter::Cache(ScriptDiagnostic &diagnostic) {
    if (!IsBound()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script event router is not bound.");
        return false;
    }
    return m_Dispatcher.CacheAll(m_Context, *m_Runtime, diagnostic);
}

bool ScriptModEventRouter::Release(ScriptDiagnostic *diagnostic) {
    if (!m_Runtime)
        return true;
    return m_Dispatcher.Release(m_Context, *m_Runtime, diagnostic);
}

bool ScriptModEventRouter::HasCallback(ScriptCallbackId id) const {
    return m_Dispatcher.HasCallback(id);
}

bool ScriptModEventRouter::CallOnLoad(ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnLoad))
        return true;
    return IsBound() && m_Dispatcher.CallOnLoad(m_Context, *m_Runtime, *m_ContextView, diagnostic);
}

bool ScriptModEventRouter::CallOnUnload(ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnUnload))
        return true;
    return IsBound() && m_Dispatcher.CallOnUnload(m_Context, *m_Runtime, *m_ContextView, diagnostic);
}

bool ScriptModEventRouter::CallOnProcess(ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnProcess))
        return true;
    return IsBound() && m_Dispatcher.CallOnProcess(m_Context, *m_Runtime, *m_ContextView, diagnostic);
}

bool ScriptModEventRouter::CallGameEvent(size_t eventIndex, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnGameEvent))
        return true;
    return IsBound() && m_Dispatcher.CallGameEvent(m_Context, *m_Runtime, *m_ContextView, eventIndex, diagnostic);
}

bool ScriptModEventRouter::CallRender(CK_RENDER_FLAGS flags, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnRender))
        return true;
    return IsBound() && m_Dispatcher.CallRender(m_Context, *m_Runtime, *m_ContextView, flags, diagnostic);
}

bool ScriptModEventRouter::CallLoadObject(const char *filename,
                                          CKBOOL isMap,
                                          const char *masterName,
                                          CK_CLASSID filterClass,
                                          CKBOOL addToScene,
                                          CKBOOL reuseMeshes,
                                          CKBOOL reuseMaterials,
                                          CKBOOL dynamic,
                                          ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnLoadObject))
        return true;
    return IsBound() && m_Dispatcher.CallLoadObject(m_Context, *m_Runtime, *m_ContextView, filename, isMap, masterName,
                                                    filterClass, addToScene, reuseMeshes, reuseMaterials, dynamic,
                                                    diagnostic);
}

bool ScriptModEventRouter::CallLoadScript(const char *filename, CK_ID scriptId, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnLoadScript))
        return true;
    return IsBound() && m_Dispatcher.CallLoadScript(m_Context, *m_Runtime, *m_ContextView, filename, scriptId, diagnostic);
}

bool ScriptModEventRouter::CallCheatEnabled(bool enable, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnCheatEnabled))
        return true;
    return IsBound() && m_Dispatcher.CallCheatEnabled(m_Context, *m_Runtime, *m_ContextView, enable, diagnostic);
}

bool ScriptModEventRouter::CallCommandEvent(bool beforeCommand,
                                            ICommand *command,
                                            const std::vector<std::string> &args,
                                            ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnCommandEvent))
        return true;
    return IsBound() && m_Dispatcher.CallCommandEvent(m_Context, *m_Runtime, *m_ContextView, beforeCommand, command, args, diagnostic);
}

bool ScriptModEventRouter::CallPhysicalize(CK3dEntity *target,
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
                                           ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnPhysicalize))
        return true;
    return IsBound() && m_Dispatcher.CallPhysicalize(m_Context, *m_Runtime, *m_ContextView, target, fixed, friction,
                                                     elasticity, mass, collGroup, startFrozen, enableColl,
                                                     calcMassCenter, linearDamp, rotDamp, collSurface, massCenter,
                                                     convexCnt, ballCnt, concaveCnt, diagnostic);
}

bool ScriptModEventRouter::CallUnphysicalize(CK3dEntity *target, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnUnphysicalize))
        return true;
    return IsBound() && m_Dispatcher.CallUnphysicalize(m_Context, *m_Runtime, *m_ContextView, target, diagnostic);
}

bool ScriptModEventRouter::IsBound() const {
    return m_Runtime && m_ContextView;
}

} // namespace BML
