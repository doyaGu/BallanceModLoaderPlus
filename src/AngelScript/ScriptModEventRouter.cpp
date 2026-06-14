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
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallOnLoad(m_Context, *m_Runtime, *m_ContextView, diagnostic);
}

bool ScriptModEventRouter::CallOnUnload(ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnUnload))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallOnUnload(m_Context, *m_Runtime, *m_ContextView, diagnostic);
}

bool ScriptModEventRouter::CallOnProcess(ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnProcess))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallOnProcess(m_Context, *m_Runtime, *m_ContextView, diagnostic);
}

bool ScriptModEventRouter::CallGameEvent(size_t eventIndex, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnGameEvent))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallGameEvent(m_Context, *m_Runtime, *m_ContextView, eventIndex, diagnostic);
}

bool ScriptModEventRouter::CallRender(CK_RENDER_FLAGS flags, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnRender))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallRender(m_Context, *m_Runtime, *m_ContextView, flags, diagnostic);
}

bool ScriptModEventRouter::CallLoadObject(const char *filename,
                                          CKBOOL isMap,
                                          const char *masterName,
                                          CK_CLASSID filterClass,
                                          CKBOOL addToScene,
                                          CKBOOL reuseMeshes,
                                          CKBOOL reuseMaterials,
                                          CKBOOL dynamic,
                                          XObjectArray *objectArray,
                                          CKObject *masterObject,
                                          ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnLoadObject))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallLoadObject(m_Context, *m_Runtime, *m_ContextView, filename, isMap, masterName,
                                       filterClass, addToScene, reuseMeshes, reuseMaterials, dynamic,
                                       objectArray, masterObject, diagnostic);
}

bool ScriptModEventRouter::CallLoadScript(const char *filename, CKBehavior *script, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnLoadScript))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallLoadScript(m_Context, *m_Runtime, *m_ContextView, filename, script, diagnostic);
}

bool ScriptModEventRouter::CallCheatEnabled(bool enable, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnCheatEnabled))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallCheatEnabled(m_Context, *m_Runtime, *m_ContextView, enable, diagnostic);
}

bool ScriptModEventRouter::CallCommandEvent(bool beforeCommand,
                                            ICommand *command,
                                            const std::vector<std::string> &args,
                                            ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnCommandEvent))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallCommandEvent(m_Context, *m_Runtime, *m_ContextView, beforeCommand, command, args, diagnostic);
}

bool ScriptModEventRouter::CallModifyConfig(const char *modId,
                                            const char *category,
                                            const char *key,
                                            IProperty *property,
                                            ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnModifyConfig))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallModifyConfig(m_Context, *m_Runtime, *m_ContextView, modId, category, key, property, diagnostic);
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
                                           CKMesh **convexMesh,
                                           int ballCnt,
                                           VxVector *ballCenter,
                                           float *ballRadius,
                                           int concaveCnt,
                                           CKMesh **concaveMesh,
                                           ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnPhysicalize))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallPhysicalize(m_Context, *m_Runtime, *m_ContextView, target, fixed, friction,
                                        elasticity, mass, collGroup, startFrozen, enableColl,
                                        calcMassCenter, linearDamp, rotDamp, collSurface, massCenter,
                                        convexCnt, convexMesh, ballCnt, ballCenter, ballRadius,
                                        concaveCnt, concaveMesh, diagnostic);
}

bool ScriptModEventRouter::CallUnphysicalize(CK3dEntity *target, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnUnphysicalize))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    return m_Dispatcher.CallUnphysicalize(m_Context, *m_Runtime, *m_ContextView, target, diagnostic);
}

bool ScriptModEventRouter::IsBound() const {
    return m_Runtime && m_ContextView;
}

bool ScriptModEventRouter::RequireBound(ScriptDiagnostic &diagnostic) const {
    if (IsBound())
        return true;
    diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script event router is not bound.");
    return false;
}

} // namespace BML
