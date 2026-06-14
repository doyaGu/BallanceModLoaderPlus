#include "ScriptCallbackDispatcher.h"

namespace BML {

namespace {

static const ScriptCallbackContract &Descriptor(ScriptCallbackId id) {
    return ScriptApiContract::Callbacks().Data[static_cast<size_t>(id)];
}

struct ContextOnlyCallArgs {
    ScriptModContextView *ContextView = nullptr;
    const ::CKAngelScriptAdapter::Api *Api = nullptr;
};

struct EventCallArgs {
    ScriptModContextView *ContextView = nullptr;
    void *EventView = nullptr;
    const ::CKAngelScriptAdapter::Api *Api = nullptr;
};

static CKAS_STATUS WriteContextOnlyArgs(CKAngelScriptArgWriter *writer, void *userData) {
    auto *args = static_cast<ContextOnlyCallArgs *>(userData);
    if (!args || !args->ContextView || !args->Api || !args->Api->ArgSetBorrowedObject)
        return CKAS_INVALIDARGUMENT;
    return args->Api->ArgSetBorrowedObject(writer, 0, args->ContextView);
}

static CKAS_STATUS WriteGameEventArgs(CKAngelScriptArgWriter *writer, void *userData) {
    auto *args = static_cast<EventCallArgs *>(userData);
    if (!args || !args->ContextView || !args->Api || !args->Api->ArgSetBorrowedObject || !args->Api->ArgSetInt)
        return CKAS_INVALIDARGUMENT;

    CKAS_STATUS status = args->Api->ArgSetBorrowedObject(writer, 0, args->ContextView);
    if (status != CKAS_OK)
        return status;
    return args->Api->ArgSetInt(writer, 1, *static_cast<int *>(args->EventView));
}

static CKAS_STATUS WriteEventObjectArgs(CKAngelScriptArgWriter *writer, void *userData) {
    auto *args = static_cast<EventCallArgs *>(userData);
    if (!args || !args->ContextView || !args->EventView || !args->Api || !args->Api->ArgSetBorrowedObject)
        return CKAS_INVALIDARGUMENT;

    CKAS_STATUS status = args->Api->ArgSetBorrowedObject(writer, 0, args->ContextView);
    if (status != CKAS_OK)
        return status;
    return args->Api->ArgSetBorrowedObject(writer, 1, args->EventView);
}

} // namespace

bool ScriptCallbackDispatcher::CacheAll(CKContext *context, ScriptModRuntime &runtime, ScriptDiagnostic &diagnostic) {
    for (const ScriptCallbackContract &descriptor : ScriptApiContract::Callbacks()) {
        m_Methods[descriptor.Id] = runtime.FindMethod(context, descriptor.Declaration, diagnostic);
        if (!m_Methods[descriptor.Id] && diagnostic.Status != CKAS_OK && !diagnostic.Message.empty()) {
            diagnostic.Phase = ScriptDiagnosticPhase::MethodLookup;
            diagnostic.Message = std::string("Method lookup failed for ") + descriptor.Name + ": " + diagnostic.Message;
            return false;
        }
    }
    return true;
}

bool ScriptCallbackDispatcher::Release(CKContext *context, ScriptModRuntime &runtime, ScriptDiagnostic *diagnostic) {
    bool ok = true;
    ScriptDiagnostic firstFailure;
    for (CKAngelScriptMethod *&method : m_Methods) {
        ScriptDiagnostic releaseDiagnostic;
        if (!runtime.ReleaseMethod(context, method, &releaseDiagnostic)) {
            if (ok)
                firstFailure = releaseDiagnostic;
            ok = false;
        }
    }
    if (!ok && diagnostic)
        *diagnostic = firstFailure;
    return ok;
}

bool ScriptCallbackDispatcher::HasCallback(ScriptCallbackId id) const {
    return id >= 0 && id < ScriptCallbackCount && m_Methods[id] != nullptr;
}

bool ScriptCallbackDispatcher::CallContextOnly(CKContext *context,
                                               ScriptModRuntime &runtime,
                                               ScriptCallbackId id,
                                               ScriptModContextView &contextView,
                                               ScriptDiagnostic &diagnostic) {
    CKAngelScriptMethod *method = m_Methods[id];
    if (!method)
        return true;

    ContextOnlyCallArgs args = {&contextView, &runtime.GetApi()};
    const ScriptCallbackContract &descriptor = Descriptor(id);
    ScriptMethodCall call;
    call.Method = method;
    call.WriteArgs = WriteContextOnlyArgs;
    call.UserData = &args;
    call.Phase = ScriptDiagnosticPhase::Callback;
    call.FailurePrefix = descriptor.FailurePrefix;
    return runtime.CallMethod(context, call, diagnostic);
}

bool ScriptCallbackDispatcher::CallWithEvent(CKContext *context,
                                             ScriptModRuntime &runtime,
                                             ScriptCallbackId id,
                                             ScriptModContextView &contextView,
                                             void *eventView,
                                             ScriptDiagnostic &diagnostic) {
    CKAngelScriptMethod *method = m_Methods[id];
    if (!method)
        return true;

    EventCallArgs args = {&contextView, eventView, &runtime.GetApi()};
    const ScriptCallbackContract &descriptor = Descriptor(id);
    ScriptMethodCall call;
    call.Method = method;
    call.WriteArgs = descriptor.PayloadKind == ScriptCallbackPayloadKind::GameEventInt ? WriteGameEventArgs : WriteEventObjectArgs;
    call.UserData = &args;
    call.Phase = ScriptDiagnosticPhase::Callback;
    call.FailurePrefix = descriptor.FailurePrefix;
    return runtime.CallMethod(context, call, diagnostic);
}

bool ScriptCallbackDispatcher::CallOnLoad(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, ScriptDiagnostic &diagnostic) {
    return CallContextOnly(context, runtime, ScriptCallbackOnLoad, contextView, diagnostic);
}

bool ScriptCallbackDispatcher::CallOnUnload(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, ScriptDiagnostic &diagnostic) {
    return CallContextOnly(context, runtime, ScriptCallbackOnUnload, contextView, diagnostic);
}

bool ScriptCallbackDispatcher::CallOnProcess(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, ScriptDiagnostic &diagnostic) {
    return CallContextOnly(context, runtime, ScriptCallbackOnProcess, contextView, diagnostic);
}

bool ScriptCallbackDispatcher::CallGameEvent(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, size_t eventIndex, ScriptDiagnostic &diagnostic) {
    if (eventIndex >= ScriptGameEventCount) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Callback, "Script game event index is out of range.");
        return false;
    }
    int eventValue = static_cast<int>(eventIndex);
    return CallWithEvent(context, runtime, ScriptCallbackOnGameEvent, contextView, &eventValue, diagnostic);
}

bool ScriptCallbackDispatcher::CallRender(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, CK_RENDER_FLAGS flags, ScriptDiagnostic &diagnostic) {
    ScriptRenderEventView event(flags);
    ScriptRenderCallbackScope renderScope;
    return CallWithEvent(context, runtime, ScriptCallbackOnRender, contextView, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallLoadObject(CKContext *context,
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
                                              XObjectArray *objectArray,
                                              CKObject *masterObject,
                                              ScriptDiagnostic &diagnostic) {
    ScriptLoadObjectEventView event(filename,
                                    isMap,
                                    masterName,
                                    filterClass,
                                    addToScene,
                                    reuseMeshes,
                                    reuseMaterials,
                                    dynamic,
                                    context,
                                    objectArray,
                                    masterObject);
    return CallWithEvent(context, runtime, ScriptCallbackOnLoadObject, contextView, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallLoadScript(CKContext *context,
                                              ScriptModRuntime &runtime,
                                              ScriptModContextView &contextView,
                                              const char *filename,
                                              CKBehavior *script,
                                              ScriptDiagnostic &diagnostic) {
    ScriptLoadScriptEventView event(context, filename, script);
    return CallWithEvent(context, runtime, ScriptCallbackOnLoadScript, contextView, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallCheatEnabled(CKContext *context, ScriptModRuntime &runtime, ScriptModContextView &contextView, bool enable, ScriptDiagnostic &diagnostic) {
    ScriptCheatEventView event(enable);
    return CallWithEvent(context, runtime, ScriptCallbackOnCheatEnabled, contextView, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallCommandEvent(CKContext *context,
                                                ScriptModRuntime &runtime,
                                                ScriptModContextView &contextView,
                                                bool beforeCommand,
                                                ICommand *command,
                                                const std::vector<std::string> &args,
                                                ScriptDiagnostic &diagnostic) {
    ScriptCommandEventView event(beforeCommand ? ScriptCommandEventPre : ScriptCommandEventPost, command, &args);
    return CallWithEvent(context, runtime, ScriptCallbackOnCommandEvent, contextView, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallModifyConfig(CKContext *context,
                                                ScriptModRuntime &runtime,
                                                ScriptModContextView &contextView,
                                                const char *modId,
                                                const char *category,
                                                const char *key,
                                                IProperty *property,
                                                ScriptDiagnostic &diagnostic) {
    ScriptConfigEventView event(modId, category, key, property);
    return CallWithEvent(context, runtime, ScriptCallbackOnModifyConfig, contextView, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallPhysicalize(CKContext *context,
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
                                               CKMesh **convexMesh,
                                               int ballCnt,
                                               VxVector *ballCenter,
                                               float *ballRadius,
                                               int concaveCnt,
                                               CKMesh **concaveMesh,
                                               ScriptDiagnostic &diagnostic) {
    ScriptPhysicalizeEventView event(context, target, fixed, friction, elasticity, mass, collGroup, startFrozen, enableColl,
                                     calcMassCenter, linearDamp, rotDamp, collSurface, massCenter, convexCnt,
                                     convexMesh, ballCnt, ballCenter, ballRadius, concaveCnt, concaveMesh);
    return CallWithEvent(context, runtime, ScriptCallbackOnPhysicalize, contextView, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallUnphysicalize(CKContext *context,
                                                 ScriptModRuntime &runtime,
                                                 ScriptModContextView &contextView,
                                                 CK3dEntity *target,
                                                 ScriptDiagnostic &diagnostic) {
    ScriptObjectEventView event(context, target);
    return CallWithEvent(context, runtime, ScriptCallbackOnUnphysicalize, contextView, &event, diagnostic);
}

} // namespace BML
