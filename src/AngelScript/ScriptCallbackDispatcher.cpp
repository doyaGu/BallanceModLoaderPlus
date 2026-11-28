#include "ScriptCallbackDispatcher.h"

#include "ScriptCallbackEvents.h"
#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"

namespace BML {

namespace {

static const char *kGameMethods[] = {
    "OnPreStartMenu",
    "OnPostStartMenu",
    "OnExitGame",
    "OnPreLoadLevel",
    "OnPostLoadLevel",
    "OnStartLevel",
    "OnPreResetLevel",
    "OnPostResetLevel",
    "OnPauseLevel",
    "OnUnpauseLevel",
    "OnPreExitLevel",
    "OnPostExitLevel",
    "OnPreNextLevel",
    "OnPostNextLevel",
    "OnDead",
    "OnPreEndLevel",
    "OnPostEndLevel",
    "OnCounterActive",
    "OnCounterInactive",
    "OnBallNavActive",
    "OnBallNavInactive",
    "OnCamNavActive",
    "OnCamNavInactive",
    "OnBallOff",
    "OnPreCheckpointReached",
    "OnPostCheckpointReached",
    "OnLevelFinish",
    "OnGameOver",
    "OnExtraPoint",
    "OnPreSubLife",
    "OnPostSubLife",
    "OnPreLifeUp",
    "OnPostLifeUp",
};

static const ScriptCallbackDescriptor &Descriptor(ScriptCallbackId id) {
    return ScriptApiSurface::Callbacks().Data[static_cast<size_t>(id)];
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

void ScriptCallbackDispatcher::Bind(CKContext *context,
                                    ScriptModRuntime &runtime,
                                    ScriptModContextView &contextView) {
    m_Context = context;
    m_Runtime = &runtime;
    m_ContextView = &contextView;
}

bool ScriptCallbackDispatcher::Cache(ScriptDiagnostic &diagnostic) {
    if (!RequireBound(diagnostic))
        return false;

    if (!Release(&diagnostic)) return false;

    for (const ScriptCallbackDescriptor &descriptor : ScriptApiSurface::Callbacks()) {
        m_Methods[descriptor.Id] = m_Runtime->FindMethod(m_Context, descriptor.Declaration, diagnostic);
        if (!m_Methods[descriptor.Id] && diagnostic.Status != CKAS_OK && !diagnostic.Message.empty()) {
            diagnostic.Phase = ScriptDiagnosticPhase::MethodLookup;
            diagnostic.Message = std::string("Method lookup failed for ") + descriptor.Name + ": " + diagnostic.Message;
            Release(nullptr);
            return false;
        }
    }
    for (size_t i = 0; i < std::size(kGameMethods); ++i) {
        const std::string declaration = std::string("void ") + kGameMethods[i] + "(const BML::ModContext &in)";
        m_GameMethods[i] = m_Runtime->FindMethod(m_Context, declaration.c_str(), diagnostic);
        if (!m_GameMethods[i] && diagnostic.Status != CKAS_OK && !diagnostic.Message.empty()) { Release(nullptr); return false; }
    }
    const char *commands[] = {"void OnPreCommandExecute(const BML::ModContext &in, const BML::CommandEvent &in)", "void OnPostCommandExecute(const BML::ModContext &in, const BML::CommandEvent &in)"};
    for (size_t i = 0; i < 2; ++i) {
        m_CommandMethods[i] = m_Runtime->FindMethod(m_Context, commands[i], diagnostic);
        if (!m_CommandMethods[i] && diagnostic.Status != CKAS_OK && !diagnostic.Message.empty()) { Release(nullptr); return false; }
    }
    return true;
}

bool ScriptCallbackDispatcher::Release(ScriptDiagnostic *diagnostic) {
    if (!m_Runtime)
        return true;

    bool ok = true;
    ScriptDiagnostic firstFailure;
    for (CKAngelScriptMethod *&method : m_Methods) {
        ScriptDiagnostic releaseDiagnostic;
        if (!m_Runtime->ReleaseMethod(m_Context, method, &releaseDiagnostic)) {
            if (ok)
                firstFailure = releaseDiagnostic;
            ok = false;
        }
    }
    const auto release = [&](CKAngelScriptMethod *&method) {
        ScriptDiagnostic failure;
        if (!m_Runtime->ReleaseMethod(m_Context, method, &failure)) {
            if (ok) firstFailure = failure;
            ok = false;
        }
    };
    for (auto &method : m_GameMethods) release(method);
    for (auto &method : m_CommandMethods) release(method);
    if (!ok && diagnostic)
        *diagnostic = firstFailure;
    return ok;
}

bool ScriptCallbackDispatcher::HasCallback(ScriptCallbackId id) const {
    return id >= 0 && id < ScriptCallbackCount && m_Methods[id] != nullptr;
}

bool ScriptCallbackDispatcher::HasGameEvent() const {
    if (HasCallback(ScriptCallbackOnGameEvent)) return true;
    for (auto *method : m_GameMethods) if (method) return true;
    return false;
}

void ScriptCallbackDispatcher::GetCallbackNames(std::vector<std::string> &out) const {
    for (const ScriptCallbackDescriptor &descriptor : ScriptApiSurface::Callbacks()) {
        if (HasCallback(descriptor.Id) && descriptor.Name)
            out.emplace_back(descriptor.Name);
    }
    for (size_t i = 0; i < std::size(kGameMethods); ++i)
        if (m_GameMethods[i]) out.emplace_back(kGameMethods[i]);
    if (m_CommandMethods[0]) out.emplace_back("OnPreCommandExecute");
    if (m_CommandMethods[1]) out.emplace_back("OnPostCommandExecute");
}

bool ScriptCallbackDispatcher::CallContextOnly(ScriptCallbackId id, ScriptDiagnostic &diagnostic) {
    ContextOnlyCallArgs args = {m_ContextView, &m_Runtime->GetApi()};
    const ScriptCallbackDescriptor &descriptor = Descriptor(id);
    ScriptMethodCall call;
    call.Method = m_Methods[id];
    call.WriteArgs = WriteContextOnlyArgs;
    call.UserData = &args;
    call.Phase = ScriptDiagnosticPhase::Callback;
    call.FailurePrefix = descriptor.FailurePrefix;
    return m_Runtime->CallMethod(m_Context, call, diagnostic);
}

bool ScriptCallbackDispatcher::CallWithEvent(ScriptCallbackId id,
                                             void *eventView,
                                             ScriptDiagnostic &diagnostic) {
    EventCallArgs args = {m_ContextView, eventView, &m_Runtime->GetApi()};
    const ScriptCallbackDescriptor &descriptor = Descriptor(id);
    ScriptMethodCall call;
    call.Method = m_Methods[id];
    call.WriteArgs = descriptor.PayloadKind == ScriptCallbackPayloadKind::GameEventInt
                         ? WriteGameEventArgs
                         : WriteEventObjectArgs;
    call.UserData = &args;
    call.Phase = ScriptDiagnosticPhase::Callback;
    call.FailurePrefix = descriptor.FailurePrefix;
    return m_Runtime->CallMethod(m_Context, call, diagnostic);
}

bool ScriptCallbackDispatcher::CallOnLoad(ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnLoad))
        return true;
    return RequireBound(diagnostic) && CallContextOnly(ScriptCallbackOnLoad, diagnostic);
}

bool ScriptCallbackDispatcher::CallOnUnload(ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnUnload))
        return true;
    return RequireBound(diagnostic) && CallContextOnly(ScriptCallbackOnUnload, diagnostic);
}

bool ScriptCallbackDispatcher::CallOnProcess(ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnProcess))
        return true;
    return RequireBound(diagnostic) && CallContextOnly(ScriptCallbackOnProcess, diagnostic);
}

bool ScriptCallbackDispatcher::CallGameEvent(size_t eventIndex, ScriptDiagnostic &diagnostic) {
    if (!HasGameEvent()) return true;
    if (!RequireBound(diagnostic))
        return false;
    if (eventIndex >= ScriptGameEventCount) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Callback, "Script game event index is out of range.");
        return false;
    }
    if (!HasCallback(ScriptCallbackOnGameEvent)) {
        if (!m_GameMethods[eventIndex]) return true;
        ContextOnlyCallArgs args{m_ContextView, &m_Runtime->GetApi()};
        ScriptMethodCall call;
        call.Method = m_GameMethods[eventIndex]; call.WriteArgs = WriteContextOnlyArgs;
        call.UserData = &args; call.Phase = ScriptDiagnosticPhase::Callback;
        call.FailurePrefix = kGameMethods[eventIndex];
        return m_Runtime->CallMethod(m_Context, call, diagnostic);
    }
    int eventValue = static_cast<int>(eventIndex);
    return CallWithEvent(ScriptCallbackOnGameEvent, &eventValue, diagnostic);
}

bool ScriptCallbackDispatcher::CallRender(CK_RENDER_FLAGS flags, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnRender))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    ScriptRenderEventView event(flags);
    return CallWithEvent(ScriptCallbackOnRender, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallLoadObject(const char *filename,
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
    ScriptLoadObjectEventView event(filename,
                                    isMap,
                                    masterName,
                                    filterClass,
                                    addToScene,
                                    reuseMeshes,
                                    reuseMaterials,
                                    dynamic,
                                    m_Context,
                                    objectArray,
                                    masterObject);
    return CallWithEvent(ScriptCallbackOnLoadObject, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallLoadScript(const char *filename,
                                              CKBehavior *script,
                                              ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnLoadScript))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    ScriptLoadScriptEventView event(m_Context, filename, script);
    return CallWithEvent(ScriptCallbackOnLoadScript, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallCheatEnabled(bool enable, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnCheatEnabled))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    ScriptCheatEventView event(enable);
    return CallWithEvent(ScriptCallbackOnCheatEnabled, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallCommandEvent(bool beforeCommand, BML::CommandRecord *command,
                                                const std::vector<std::string> &args,
                                                ScriptDiagnostic &diagnostic) {
    const size_t index = beforeCommand ? 0 : 1;
    if (!HasCallback(ScriptCallbackOnCommandEvent) && !m_CommandMethods[index]) return true;
    if (!RequireBound(diagnostic))
        return false;
    ScriptCommandEventView event(beforeCommand ? ScriptCommandEventPre : ScriptCommandEventPost, command, &args);
    if (!HasCallback(ScriptCallbackOnCommandEvent)) {
        EventCallArgs args{m_ContextView, &event, &m_Runtime->GetApi()};
        ScriptMethodCall call;
        call.Method = m_CommandMethods[index]; call.WriteArgs = WriteEventObjectArgs;
        call.UserData = &args; call.Phase = ScriptDiagnosticPhase::Callback;
        call.FailurePrefix = beforeCommand ? "OnPreCommandExecute failed" : "OnPostCommandExecute failed";
        return m_Runtime->CallMethod(m_Context, call, diagnostic);
    }
    return CallWithEvent(ScriptCallbackOnCommandEvent, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallModifyConfig(const char *modId, const char *category,
                                                const char *key, Property *property,
                                                ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnModifyConfig))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    ScriptConfigEventView event(modId, category, key, property);
    return CallWithEvent(ScriptCallbackOnModifyConfig, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallPhysicalize(CK3dEntity *target,
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
    ScriptPhysicalizeEventView event(m_Context, target, fixed, friction, elasticity, mass, collGroup, startFrozen,
                                     enableColl, calcMassCenter, linearDamp, rotDamp, collSurface, massCenter,
                                     convexCnt, convexMesh, ballCnt, ballCenter, ballRadius, concaveCnt, concaveMesh);
    return CallWithEvent(ScriptCallbackOnPhysicalize, &event, diagnostic);
}

bool ScriptCallbackDispatcher::CallUnphysicalize(CK3dEntity *target, ScriptDiagnostic &diagnostic) {
    if (!HasCallback(ScriptCallbackOnUnphysicalize))
        return true;
    if (!RequireBound(diagnostic))
        return false;
    ScriptObjectEventView event(m_Context, target);
    return CallWithEvent(ScriptCallbackOnUnphysicalize, &event, diagnostic);
}

bool ScriptCallbackDispatcher::IsBound() const {
    return m_Runtime && m_ContextView;
}

bool ScriptCallbackDispatcher::RequireBound(ScriptDiagnostic &diagnostic) const {
    if (IsBound())
        return true;
    diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
                                      "Script callback dispatcher is not bound.");
    return false;
}

} // namespace BML
