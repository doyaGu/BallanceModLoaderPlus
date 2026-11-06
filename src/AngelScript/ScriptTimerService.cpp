#include "ScriptStringInterop.h"

#include "ScriptTimerService.h"

#include <algorithm>
#include <cstdint>
#include <new>
#include <utility>

#include "Logging/ILogger.h"
#include "Loader/ModContext.h"
#include "ScriptAngelScriptHandle.h"
#include "ScriptFunctionSupport.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"

namespace BML {

static constexpr const char *kTimerInterfaceDecl = "BML::Timer";
static constexpr const char *kTimerTickDecl =
    "bool Tick(const BML::ModContext &in, const BML::TimerEvent &in)";
struct ScriptTimerEntry {
    asIScriptObject *Object = nullptr;
    asIScriptFunction *Callback = nullptr;
    bool OwnsCallbackRef = false;
    bool Loop = false;
    bool ReturnsBool = true;
    std::string Name;
    int ActiveCalls = 0;
    bool PendingRetire = false;
    ScriptTimerEntry() = default;
    ScriptTimerEntry(const ScriptTimerEntry &) = delete;
    ScriptTimerEntry &operator=(const ScriptTimerEntry &) = delete;
    ScriptTimerEntry(ScriptTimerEntry &&other) noexcept
        : Object(std::exchange(other.Object, nullptr)), Callback(std::exchange(other.Callback, nullptr)),
          OwnsCallbackRef(std::exchange(other.OwnsCallbackRef, false)), Loop(other.Loop),
          ReturnsBool(other.ReturnsBool), Name(std::move(other.Name)) {}
    ~ScriptTimerEntry() {
        if (Object) Object->Release();
        if (OwnsCallbackRef && Callback) Callback->Release();
    }
};

static bool IsTimerCallbackSignature(asIScriptFunction *callback, bool returnsBool) {
    const ScriptFunctionParam params[] = {
        {"BML::ModContext", asTM_INREF | asTM_CONST},
        {"BML::TimerEvent", asTM_INREF | asTM_CONST},
    };
    return ScriptFunctionHasSignature(callback, returnsBool ? asTYPEID_BOOL : asTYPEID_VOID, params, 2);
}

struct ScriptTimerRegistration {
    std::string Name;
    bool Loop = false;
    bool StartPaused = false;
    int RepeatCount = 0;
    int Priority = 0;
    unsigned int DelayTicks = 0;
    bool HasDelayTicks = false;
    float DelayMs = 0.0f;
    bool HasDelayMs = false;
    asIScriptFunction *TickMethod = nullptr;
};

class ScriptTimerServiceState {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    ScriptModRuntime *Runtime = nullptr;
    ScriptModContextView *ContextView = nullptr;
    bool Active = false;
    TimerRegistry *Registry = nullptr;
    std::unordered_map<TimerScheduler::TimerId, ScriptTimerEntry> Timers;
};

static ScriptDiagnostic MakeAngelScriptDiagnostic(ScriptDiagnosticPhase phase,
                                                  int code,
                                                  asIScriptContext *context,
                                                  const std::string &prefix) {
    ScriptDiagnostic diagnostic;
    diagnostic.Phase = phase;
    diagnostic.AngelScriptCode = code;
    diagnostic.Message = prefix;
    if (!diagnostic.Message.empty())
        diagnostic.Message += ": ";

    if (code == asEXECUTION_EXCEPTION && context) {
        const char *exception = context->GetExceptionString();
        diagnostic.Message += exception ? exception : "AngelScript exception";
        asIScriptFunction *function = context->GetExceptionFunction();
        if (function) {
            diagnostic.StackTrace = function->GetDeclaration(true, true, true);
            int column = 0;
            const int line = context->GetExceptionLineNumber(&column);
            if (line > 0) {
                diagnostic.StackTrace += ":";
                diagnostic.StackTrace += std::to_string(line);
                if (column > 0) {
                    diagnostic.StackTrace += ":";
                    diagnostic.StackTrace += std::to_string(column);
                }
            }
        }
    } else if (code == asEXECUTION_SUSPENDED) {
        diagnostic.Message += "script timer callback suspended";
    } else {
        diagnostic.Message += "AngelScript call failed with code ";
        diagnostic.Message += std::to_string(code);
    }
    return diagnostic;
}

static bool ExecuteObjectMethod(asIScriptObject *object,
                                asIScriptFunction *method,
                                ScriptMod *owner,
                                ScriptModContextView *contextView,
                                ScriptTimerEventView *event,
                                const char *failurePrefix,
                                bool &result,
                                ScriptDiagnostic &diagnostic) {
    ScriptHostCallScope activeCall(owner);
    if (owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Callback, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    result = false;
    if (!object || !method || !contextView || !event) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Callback, "Timer callback has invalid runtime state.");
        return false;
    }

    asIScriptContext *context = object->GetEngine()->CreateContext();
    if (!context) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Callback, "Unable to create AngelScript context for timer callback.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code >= 0)
        code = context->SetArgObject(0, contextView);
    if (code >= 0)
        code = context->SetArgObject(1, event);

    if (code < 0) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Callback, code, context, failurePrefix);
        context->Release();
        return false;
    }

    code = context->Execute();
    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Callback, code, context, failurePrefix);
        context->Release();
        return false;
    }

    result = context->GetReturnByte() != 0;
    context->Release();
    return true;
}

static bool ReadStringProperty(asIScriptObject *object,
                               ScriptMod *owner,
                               const char *declaration,
                               std::string &value,
                               ScriptDiagnostic &diagnostic) {
    asIScriptFunction *method = object && object->GetObjectType()
                                    ? object->GetObjectType()->GetMethodByDecl(declaration)
                                    : nullptr;
    if (!method) {
        value.clear();
        return true;
    }

    ScriptHostCallScope activeCall(owner);
    if (owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    asIScriptContext *context = object->GetEngine()->CreateContext();
    if (!context) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Unable to create AngelScript context for timer property.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code >= 0)
        code = context->Execute();

    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "Timer property read failed");
        context->Release();
        return false;
    }

    if (!ScriptStringInterop::ReadContextReturnString(context, value)) {
        diagnostic.Status = CKAS_TYPEMISMATCH;
        diagnostic.Message = std::string("Timer property returned an incompatible string for ") + declaration;
        context->Release();
        return false;
    }
    context->Release();
    return true;
}

static bool ReadBoolProperty(asIScriptObject *object,
                             ScriptMod *owner,
                             const char *declaration,
                             bool defaultValue,
                             bool &value,
                             ScriptDiagnostic &diagnostic) {
    asIScriptFunction *method = object && object->GetObjectType()
                                    ? object->GetObjectType()->GetMethodByDecl(declaration)
                                    : nullptr;
    if (!method) {
        value = defaultValue;
        return true;
    }

    ScriptHostCallScope activeCall(owner);
    if (owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    asIScriptContext *context = object->GetEngine()->CreateContext();
    if (!context) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Unable to create AngelScript context for timer property.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code >= 0)
        code = context->Execute();

    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "Timer property read failed");
        context->Release();
        return false;
    }

    value = context->GetReturnByte() != 0;
    context->Release();
    return true;
}

static bool ReadIntProperty(asIScriptObject *object,
                            ScriptMod *owner,
                            const char *declaration,
                            int defaultValue,
                            int &value,
                            ScriptDiagnostic &diagnostic) {
    asIScriptFunction *method = object && object->GetObjectType()
                                    ? object->GetObjectType()->GetMethodByDecl(declaration)
                                    : nullptr;
    if (!method) {
        value = defaultValue;
        return true;
    }

    ScriptHostCallScope activeCall(owner);
    if (owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    asIScriptContext *context = object->GetEngine()->CreateContext();
    if (!context) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Unable to create AngelScript context for timer property.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code >= 0)
        code = context->Execute();

    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "Timer property read failed");
        context->Release();
        return false;
    }

    value = context->GetReturnDWord();
    context->Release();
    return true;
}

static bool ReadUIntProperty(asIScriptObject *object,
                             ScriptMod *owner,
                             const char *declaration,
                             unsigned int &value,
                             bool &present,
                             ScriptDiagnostic &diagnostic) {
    asIScriptFunction *method = object && object->GetObjectType()
                                    ? object->GetObjectType()->GetMethodByDecl(declaration)
                                    : nullptr;
    if (!method) {
        value = 0;
        present = false;
        return true;
    }

    ScriptHostCallScope activeCall(owner);
    if (owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    asIScriptContext *context = object->GetEngine()->CreateContext();
    if (!context) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Unable to create AngelScript context for timer property.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code >= 0)
        code = context->Execute();

    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "Timer property read failed");
        context->Release();
        return false;
    }

    value = context->GetReturnDWord();
    present = true;
    context->Release();
    return true;
}

static bool ReadFloatProperty(asIScriptObject *object,
                              ScriptMod *owner,
                              const char *declaration,
                              float &value,
                              bool &present,
                              ScriptDiagnostic &diagnostic) {
    asIScriptFunction *method = object && object->GetObjectType()
                                    ? object->GetObjectType()->GetMethodByDecl(declaration)
                                    : nullptr;
    if (!method) {
        value = 0.0f;
        present = false;
        return true;
    }

    ScriptHostCallScope activeCall(owner);
    if (owner && !activeCall.Entered()) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Script mod reload is in progress.");
        diagnostic.Status = CKAS_INUSE;
        return false;
    }

    asIScriptContext *context = object->GetEngine()->CreateContext();
    if (!context) {
        diagnostic = MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime, "Unable to create AngelScript context for timer property.");
        return false;
    }

    ScriptCurrentModScope callScope(owner);
    int code = context->Prepare(method);
    if (code >= 0)
        code = context->SetObject(object);
    if (code >= 0)
        code = context->Execute();

    if (code == asEXECUTION_SUSPENDED)
        context->Abort();
    if (code != asEXECUTION_FINISHED) {
        diagnostic = MakeAngelScriptDiagnostic(ScriptDiagnosticPhase::Runtime, code, context, "Timer property read failed");
        context->Release();
        return false;
    }

    value = context->GetReturnFloat();
    present = true;
    context->Release();
    return true;
}

static void ReleaseScriptTimerObject(ScriptTimerEntry &entry) {
    if (entry.Object) {
        entry.Object->Release();
        entry.Object = nullptr;
    }
    if (entry.OwnsCallbackRef && entry.Callback) {
        entry.Callback->Release();
        entry.Callback = nullptr;
        entry.OwnsCallbackRef = false;
    }
}

static void RetireTimerEntry(const std::shared_ptr<ScriptTimerServiceState> &state,
                             TimerScheduler::TimerId id) {
    if (!state)
        return;

    auto it = state->Timers.find(id);
    if (it == state->Timers.end())
        return;

    if (it->second.ActiveCalls > 0) {
        it->second.PendingRetire = true;
        return;
    }

    ReleaseScriptTimerObject(it->second);
    state->Timers.erase(it);
}

static void RecordTimerDiagnostic(const std::shared_ptr<ScriptTimerServiceState> &state,
                                  const ScriptDiagnostic &diagnostic,
                                  bool fail) {
    if (!state || !state->Owner)
        return;
    if (fail)
        state->Owner->SetLoadFailure(diagnostic);
    else
        state->Owner->RecordScriptDiagnostic(diagnostic);
}

struct TimerFunctionCallArgs {
    ScriptModContextView *ContextView = nullptr;
    ScriptTimerEventView *Event = nullptr;
    bool ReturnsBool = false;
    bool Result = false;
};

static int WriteTimerFunctionArgs(asIScriptContext *context, void *userdata) {
    auto *args = static_cast<TimerFunctionCallArgs *>(userdata);
    int code = context->SetArgObject(0, args ? args->ContextView : nullptr);
    if (code >= 0)
        code = context->SetArgObject(1, args ? args->Event : nullptr);
    return code;
}

static int ReadTimerFunctionResult(asIScriptContext *context, void *userdata) {
    auto *args = static_cast<TimerFunctionCallArgs *>(userdata);
    if (!context || !args)
        return asERROR;
    args->Result = args->ReturnsBool ? context->GetReturnByte() != 0 : false;
    return asSUCCESS;
}

static bool ExecuteTimerCallback(const std::weak_ptr<ScriptTimerServiceState> &weakState,
                                 TimerScheduler::TimerId id,
                                 const BML_TimerEvent &timer) {
    std::shared_ptr<ScriptTimerServiceState> state = weakState.lock();
    if (!state || !state->Active)
        return false;
    if (state->Owner && !state->Owner->CanDispatchScriptServiceCallback())
        return false;

    auto it = state->Timers.find(id);
    if (it == state->Timers.end() || !it->second.Callback || !state->ContextView)
        return false;

    ScriptTimerEventView event(timer);
    ScriptDiagnostic diagnostic;
    bool continueTimer = false;
    ++it->second.ActiveCalls;
    const bool callSucceeded = it->second.Object
                                   ? ExecuteObjectMethod(it->second.Object,
                                                         it->second.Callback,
                                                         state->Owner,
                                                         state->ContextView,
                                                         &event,
                                                         "Timer callback failed",
                                                         continueTimer,
                                                         diagnostic)
                                   : [&]() {
                                         TimerFunctionCallArgs args = {state->ContextView,
                                                                       &event,
                                                                       it->second.ReturnsBool,
                                                                       false};
                                         ScriptFunctionCall call;
                                         call.Function = it->second.Callback;
                                         call.Owner = state->Owner;
                                         call.Phase = ScriptDiagnosticPhase::Callback;
                                         call.FailurePrefix = "Timer callback failed";
                                         call.InvalidStateMessage = "Timer callback has invalid runtime state.";
                                         call.ContextFailureMessage = "Unable to create AngelScript context for timer callback.";
                                         call.SuspendedMessage = "script timer callback suspended";
                                         call.WriteArgs = WriteTimerFunctionArgs;
                                         call.ReadResult = ReadTimerFunctionResult;
                                         call.UserData = &args;
                                         const bool ok = ExecuteScriptFunction(call, diagnostic);
                                         continueTimer = args.Result;
                                         return ok;
                                     }();
    if (!callSucceeded) {
        RecordTimerDiagnostic(state, diagnostic, true);
        auto afterCall = state->Timers.find(id);
        if (afterCall != state->Timers.end() && afterCall->second.ActiveCalls > 0)
            --afterCall->second.ActiveCalls;
        return false;
    }

    auto afterCall = state->Timers.find(id);
    bool keepTimer = false;
    if (afterCall != state->Timers.end()) {
        if (afterCall->second.ActiveCalls > 0)
            --afterCall->second.ActiveCalls;
        keepTimer = afterCall->second.Loop && continueTimer && !afterCall->second.PendingRetire;
    }
    return keepTimer;
}

ScriptTimerEventView::ScriptTimerEventView(TimerScheduler *timer)
    : m_Valid(timer != nullptr),
      m_Id(timer ? static_cast<int>(timer->GetId()) : 0),
      m_Name(timer ? timer->GetName() : std::string()),
      m_State(timer ? static_cast<int>(timer->GetState()) : static_cast<int>(TimerScheduler::CANCELLED)),
      m_Type(timer ? static_cast<int>(timer->GetType()) : static_cast<int>(TimerScheduler::ONCE)),
      m_TimeBase(timer ? static_cast<int>(timer->GetTimeBase()) : static_cast<int>(TimerScheduler::TICK)),
      m_CompletedIterations(timer ? timer->GetCompletedIterations() : 0),
      m_RemainingIterations(timer ? timer->GetRemainingIterations() : 0),
      m_Progress(timer ? timer->GetProgress() : 0.0f) {}
ScriptTimerEventView::ScriptTimerEventView(const BML_TimerEvent &event)
    : m_Valid(true), m_Id(static_cast<int>(event.Handle)), m_Name(event.Name ? event.Name : ""),
      m_State(event.State), m_Type(event.Type), m_TimeBase(event.TimeBase),
      m_CompletedIterations(static_cast<int>(event.CompletedIterations)),
      m_RemainingIterations(static_cast<int>(event.RemainingIterations)), m_Progress(event.Progress) {}
int ScriptTimerEventView::GetId() const { return m_Id; }
std::string ScriptTimerEventView::GetName() const { return m_Name; }
int ScriptTimerEventView::GetState() const { return m_State; }
int ScriptTimerEventView::GetType() const { return m_Type; }
int ScriptTimerEventView::GetTimeBase() const { return m_TimeBase; }
int ScriptTimerEventView::GetCompletedIterations() const { return m_CompletedIterations; }
int ScriptTimerEventView::GetRemainingIterations() const { return m_RemainingIterations; }
float ScriptTimerEventView::GetProgress() const { return m_Progress; }

ScriptTimerRef::ScriptTimerRef(std::weak_ptr<ScriptTimerServiceState> state, TimerScheduler::TimerId id)
    : m_State(std::move(state)), m_Id(id) {}

void ScriptTimerRef::AddRef() { ++m_RefCount; }
void ScriptTimerRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

TimerScheduler *ScriptTimerRef::ResolveTimer() const {
    std::shared_ptr<ScriptTimerServiceState> state = m_State.lock();
    if (!state || !state->Active || m_Id == TimerScheduler::InvalidId)
        return nullptr;
    if (state->Timers.find(m_Id) == state->Timers.end())
        return nullptr;
    auto timer = state->Registry ? state->Registry->Find(state.get(), m_Id) : nullptr;
    return timer ? timer.get() : nullptr;
}

bool ScriptTimerRef::IsValid() const { return ResolveTimer() != nullptr; }
int ScriptTimerRef::GetId() const { return static_cast<int>(m_Id); }
std::string ScriptTimerRef::GetName() const {
    TimerScheduler *timer = ResolveTimer();
    return timer ? timer->GetName() : std::string();
}
int ScriptTimerRef::GetState() const {
    TimerScheduler *timer = ResolveTimer();
    return timer ? static_cast<int>(timer->GetState()) : static_cast<int>(TimerScheduler::CANCELLED);
}
int ScriptTimerRef::GetCompletedIterations() const {
    TimerScheduler *timer = ResolveTimer();
    return timer ? timer->GetCompletedIterations() : 0;
}
int ScriptTimerRef::GetRemainingIterations() const {
    TimerScheduler *timer = ResolveTimer();
    return timer ? timer->GetRemainingIterations() : 0;
}
float ScriptTimerRef::GetProgress() const {
    TimerScheduler *timer = ResolveTimer();
    return timer ? timer->GetProgress() : 0.0f;
}
void ScriptTimerRef::Pause() {
    if (auto state = m_State.lock(); state && state->Active && state->Registry)
        (void) state->Registry->Control(state.get(), m_Id, 0);
}
void ScriptTimerRef::Resume() {
    if (auto state = m_State.lock(); state && state->Active && state->Registry)
        (void) state->Registry->Control(state.get(), m_Id, 1);
}
void ScriptTimerRef::Cancel() {
    std::shared_ptr<ScriptTimerServiceState> state = m_State.lock();
    if (!state || !state->Active || m_Id == TimerScheduler::InvalidId)
        return;
    if (state->Registry) (void) state->Registry->Control(state.get(), m_Id, 2);
}

ScriptTimerService::ScriptTimerService() : m_State(std::make_shared<ScriptTimerServiceState>()) {}
ScriptTimerService::~ScriptTimerService() {
    try {
        Release(nullptr);
    } catch (...) {
    }
}

bool ScriptTimerService::Bind(ModContext *context, ScriptMod *owner, ScriptModRuntime *runtime, ScriptModContextView *contextView) {
    if (!m_State) {
        try {
            m_State = std::make_shared<ScriptTimerServiceState>();
        } catch (const std::bad_alloc &) {
            return false;
        }
    }
    m_State->Context = context;
    m_State->Owner = owner;
    m_State->Runtime = runtime;
    m_State->ContextView = contextView;
    m_State->Registry = context ? &context->GetTimerApi().Registry() : nullptr;
    m_State->Active = true;
    return true;
}

ScriptTimerRef *ScriptTimerService::Add(asIScriptObject *timer) {
    if (!m_State || !timer || !m_State->Active || !m_State->Context || !m_State->Owner || !timer->GetObjectType())
        return nullptr;

    asITypeInfo *timerInterface = timer->GetEngine()->GetTypeInfoByDecl(kTimerInterfaceDecl);
    if (!timerInterface || !timer->GetObjectType()->Implements(timerInterface)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "AddTimer requires an object implementing BML::Timer."));
        return nullptr;
    }

    CKTimeManager *time = m_State->Context->GetTimeManager();
    if (!time)
        return nullptr;

    ScriptDiagnostic diagnostic;
    ScriptTimerRegistration registration;
    if (!ReadStringProperty(timer, m_State->Owner, "string get_Name() const", registration.Name, diagnostic) ||
        !ReadBoolProperty(timer, m_State->Owner, "bool get_Loop() const", false, registration.Loop, diagnostic) ||
        !ReadBoolProperty(timer, m_State->Owner, "bool get_StartPaused() const", false, registration.StartPaused, diagnostic) ||
        !ReadIntProperty(timer, m_State->Owner, "int get_RepeatCount() const", 0, registration.RepeatCount, diagnostic) ||
        !ReadIntProperty(timer, m_State->Owner, "int get_Priority() const", 0, registration.Priority, diagnostic) ||
        !ReadUIntProperty(timer, m_State->Owner, "uint get_DelayTicks() const", registration.DelayTicks, registration.HasDelayTicks, diagnostic) ||
        !ReadFloatProperty(timer, m_State->Owner, "float get_DelayMs() const", registration.DelayMs, registration.HasDelayMs, diagnostic)) {
        m_State->Owner->RecordScriptDiagnostic(diagnostic);
        return nullptr;
    }

    if ((!registration.HasDelayTicks || registration.DelayTicks == 0) &&
        (!registration.HasDelayMs || registration.DelayMs <= 0.0f)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Timer object must provide positive DelayTicks or DelayMs."));
        return nullptr;
    }

    registration.TickMethod = timer->GetObjectType()->GetMethodByDecl(kTimerTickDecl);
    if (!registration.TickMethod) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "Timer object is missing Tick implementation."));
        return nullptr;
    }

    BML_TimerDefinition definition = BML_TIMER_DEFINITION_INIT;
    definition.Name = registration.Name.c_str();
    definition.StartPaused = registration.StartPaused;
    definition.Priority = registration.Priority;
    if (registration.HasDelayTicks && registration.DelayTicks > 0) {
        definition.TimeBase = BML_TIMER_TIMEBASE_TICK;
        definition.DelayTicks = registration.DelayTicks;
    } else {
        definition.TimeBase = BML_TIMER_TIMEBASE_TIME;
        definition.DelayMs = registration.DelayMs;
    }
    definition.Type = registration.RepeatCount > 0 ? BML_TIMER_REPEAT
        : registration.Loop ? BML_TIMER_LOOP : BML_TIMER_ONCE;
    if (definition.Type == BML_TIMER_REPEAT) definition.RepeatCount = registration.RepeatCount;
    return AddTimer(definition, timer, registration);
}

ScriptTimerRef *ScriptTimerService::AddTimeoutTicks(unsigned int delayTicks,
                                                    asIScriptFunction *callback,
                                                    const std::string &name) {
    if (!m_State || !callback || delayTicks == 0 || !m_State->Active || !m_State->Context || !m_State->Owner)
        return nullptr;
    if (!IsTimerCallbackSignature(callback, false)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "SetTimeoutTicks requires BML::TimerCallback."));
        return nullptr;
    }
    CKTimeManager *time = m_State->Context->GetTimeManager();
    if (!time)
        return nullptr;

    BML_TimerDefinition definition = BML_TIMER_DEFINITION_INIT;
    definition.Name = name.c_str();
    definition.Type = BML_TIMER_ONCE;
    definition.TimeBase = BML_TIMER_TIMEBASE_TICK;
    definition.DelayTicks = delayTicks;
    return AddCallbackTimer(definition, callback, false);
}

ScriptTimerRef *ScriptTimerService::AddTimeoutMs(float delayMs,
                                                 asIScriptFunction *callback,
                                                 const std::string &name) {
    if (!m_State || !callback || delayMs <= 0.0f || !m_State->Active || !m_State->Context || !m_State->Owner)
        return nullptr;
    if (!IsTimerCallbackSignature(callback, false)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "SetTimeout requires BML::TimerCallback."));
        return nullptr;
    }
    CKTimeManager *time = m_State->Context->GetTimeManager();
    if (!time)
        return nullptr;

    BML_TimerDefinition definition = BML_TIMER_DEFINITION_INIT;
    definition.Name = name.c_str();
    definition.Type = BML_TIMER_ONCE;
    definition.TimeBase = BML_TIMER_TIMEBASE_TIME;
    definition.DelayMs = delayMs;
    return AddCallbackTimer(definition, callback, false);
}

ScriptTimerRef *ScriptTimerService::AddIntervalTicks(unsigned int delayTicks,
                                                     asIScriptFunction *callback,
                                                     const std::string &name) {
    if (!m_State || !callback || delayTicks == 0 || !m_State->Active || !m_State->Context || !m_State->Owner)
        return nullptr;
    if (!IsTimerCallbackSignature(callback, true)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "SetIntervalTicks requires BML::TimerLoopCallback."));
        return nullptr;
    }
    CKTimeManager *time = m_State->Context->GetTimeManager();
    if (!time)
        return nullptr;

    BML_TimerDefinition definition = BML_TIMER_DEFINITION_INIT;
    definition.Name = name.c_str();
    definition.Type = BML_TIMER_INTERVAL;
    definition.TimeBase = BML_TIMER_TIMEBASE_TICK;
    definition.DelayTicks = delayTicks;
    return AddCallbackTimer(definition, callback, true);
}

ScriptTimerRef *ScriptTimerService::AddIntervalMs(float delayMs,
                                                  asIScriptFunction *callback,
                                                  const std::string &name) {
    if (!m_State || !callback || delayMs <= 0.0f || !m_State->Active || !m_State->Context || !m_State->Owner)
        return nullptr;
    if (!IsTimerCallbackSignature(callback, true)) {
        m_State->Owner->RecordScriptDiagnostic(MakeScriptDiagnostic(ScriptDiagnosticPhase::Runtime,
            "SetInterval requires BML::TimerLoopCallback."));
        return nullptr;
    }
    CKTimeManager *time = m_State->Context->GetTimeManager();
    if (!time)
        return nullptr;

    BML_TimerDefinition definition = BML_TIMER_DEFINITION_INIT;
    definition.Name = name.c_str();
    definition.Type = BML_TIMER_INTERVAL;
    definition.TimeBase = BML_TIMER_TIMEBASE_TIME;
    definition.DelayMs = delayMs;
    return AddCallbackTimer(definition, callback, true);
}

static ScriptTimerRef *RegisterTimer(const std::shared_ptr<ScriptTimerServiceState> &state,
                                     const BML_TimerDefinition &definition, ScriptTimerEntry entry) {
    if (!state || !state->Active || !state->Registry || !state->Context || !state->Owner)
        return nullptr;
    auto *clock = state->Context->GetTimeManager();
    if (!clock) return nullptr;
    auto identity = std::make_shared<BML_TimerHandle>(0);
    std::weak_ptr<ScriptTimerServiceState> weak = state;
    const int code = state->Registry->Register(state.get(), &definition,
        [weak](const BML_TimerEvent &event, uint32_t &keep) {
            keep = ExecuteTimerCallback(weak, event.Handle, event);
            return BML_OK;
        }, [weak, identity] { RetireTimerEntry(weak.lock(), *identity); },
        clock->GetMainTickCount(), clock->GetAbsoluteTime() / 1000.0f, identity.get());
    if (code != BML_OK) return nullptr;
    const auto handle = *identity;
    try {
        state->Timers.emplace(handle, std::move(entry));
        auto ref = std::make_unique<ScriptTimerRef>(state, handle);
        return ref.release();
    } catch (...) {
        (void) state->Registry->Control(state.get(), handle, 2);
        throw;
    }
}

ScriptTimerRef *ScriptTimerService::AddTimer(const BML_TimerDefinition &definition,
                                             asIScriptObject *timer,
                                             const ScriptTimerRegistration &registration) {
    if (!timer) return nullptr;
    ScriptTimerEntry entry;
    auto retained = ScriptObjectHandle::Retain(timer);
    entry.Object = retained.Detach();
    entry.Callback = registration.TickMethod;
    entry.Loop = definition.Type != BML_TIMER_ONCE;
    entry.ReturnsBool = true;
    entry.Name = registration.Name;
    return RegisterTimer(m_State, definition, std::move(entry));
}

ScriptTimerRef *ScriptTimerService::AddCallbackTimer(const BML_TimerDefinition &definition,
                                                     asIScriptFunction *callback, bool loop) {
    if (!callback) return nullptr;
    ScriptTimerEntry entry;
    callback->AddRef();
    entry.Callback = callback;
    entry.OwnsCallbackRef = true;
    entry.Loop = loop;
    entry.ReturnsBool = loop;
    entry.Name = definition.Name ? definition.Name : "";
    return RegisterTimer(m_State, definition, std::move(entry));
}

void ScriptTimerService::Release(ScriptDiagnostic *) {
    if (!m_State || !m_State->Active)
        return;

    std::shared_ptr<ScriptTimerServiceState> releasedState = m_State;
    releasedState->Active = false;
    if (releasedState->Registry) releasedState->Registry->CleanupOwner(releasedState.get());
    m_State.reset();
}

size_t ScriptTimerService::GetActiveCount() const {
    return m_State && m_State->Active ? m_State->Timers.size() : 0;
}

#ifdef BML_TEST
ScriptTimerRef *ScriptTimerService::AddTestTimerForRelease(const std::string &name,
                                                           TimerScheduler::SimpleCallback callback) {
    if (!m_State)
        return nullptr;
    m_State->Active = true;
    if (!callback)
        callback = []() {};

    static TimerRegistry registry;
    m_State->Registry = &registry;
    BML_TimerDefinition definition = BML_TIMER_DEFINITION_INIT;
    definition.Name = name.c_str();
    definition.TimeBase = BML_TIMER_TIMEBASE_TICK;
    definition.DelayTicks = 1000;
    auto identity = std::make_shared<BML_TimerHandle>(0);
    std::weak_ptr<ScriptTimerServiceState> weak = m_State;
    if (registry.Register(m_State.get(), &definition,
        [fn = std::move(callback)](const BML_TimerEvent &, uint32_t &keep) { fn(); keep = 0; return BML_OK; },
        [weak, identity] { RetireTimerEntry(weak.lock(), *identity); }, 0, 0.0f, identity.get()) != BML_OK)
        return nullptr;
    try {
        ScriptTimerEntry entry;
        entry.Name = name;
        m_State->Timers.emplace(*identity, std::move(entry));
        return new ScriptTimerRef(m_State, *identity);
    } catch (...) {
        (void) registry.Control(m_State.get(), *identity, 2);
        throw;
    }
}
#endif

} // namespace BML
