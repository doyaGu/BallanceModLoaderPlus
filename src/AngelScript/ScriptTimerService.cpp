#include "ScriptTimerService.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include <angelscript.h>

#include "BML/ILogger.h"
#include "ModContext.h"
#include "ScriptAngelScriptHandle.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"

namespace BML {

static constexpr const char *kTimerInterfaceDecl = "BML::Timer";
static constexpr const char *kTimerTickDecl =
    "bool Tick(const BML::ModContext &in, const BML::TimerEvent &in)";

struct ScriptTimerEntry {
    asIScriptObject *Object = nullptr;
    asIScriptFunction *TickMethod = nullptr;
    bool Loop = false;
    std::string Name;
    int ActiveCalls = 0;
    bool PendingRetire = false;
};

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
    std::unordered_map<Timer::TimerId, ScriptTimerEntry> Timers;
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

    auto *result = static_cast<std::string *>(context->GetReturnObject());
    value = result ? *result : std::string();
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
}

static void RetireTimerEntry(const std::shared_ptr<ScriptTimerServiceState> &state,
                             Timer::TimerId id,
                             bool cancelNativeTimer) {
    if (!state)
        return;

    auto it = state->Timers.find(id);
    if (it == state->Timers.end())
        return;

    if (cancelNativeTimer) {
        if (std::shared_ptr<Timer> timer = Timer::FindById(id))
            timer->Cancel();
    }

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

static bool ExecuteTimerCallback(const std::weak_ptr<ScriptTimerServiceState> &weakState,
                                 Timer::TimerId id,
                                 Timer &timer) {
    std::shared_ptr<ScriptTimerServiceState> state = weakState.lock();
    if (!state || !state->Active)
        return false;

    auto it = state->Timers.find(id);
    if (it == state->Timers.end() || !it->second.Object || !it->second.TickMethod || !state->ContextView)
        return false;

    ScriptTimerEventView event(&timer);
    ScriptDiagnostic diagnostic;
    bool continueTimer = false;
    ++it->second.ActiveCalls;
    if (!ExecuteObjectMethod(it->second.Object,
                             it->second.TickMethod,
                             state->Owner,
                             state->ContextView,
                             &event,
                             "Timer callback failed",
                             continueTimer,
                             diagnostic)) {
        RecordTimerDiagnostic(state, diagnostic, true);
        auto afterCall = state->Timers.find(id);
        if (afterCall != state->Timers.end() && afterCall->second.ActiveCalls > 0)
            --afterCall->second.ActiveCalls;
        RetireTimerEntry(state, id, false);
        return false;
    }

    auto afterCall = state->Timers.find(id);
    bool keepTimer = false;
    if (afterCall != state->Timers.end()) {
        if (afterCall->second.ActiveCalls > 0)
            --afterCall->second.ActiveCalls;
        keepTimer = afterCall->second.Loop && continueTimer && !afterCall->second.PendingRetire;
    }
    if (!keepTimer)
        RetireTimerEntry(state, id, false);
    return keepTimer;
}

ScriptTimerEventView::ScriptTimerEventView(Timer *timer)
    : m_Valid(timer != nullptr),
      m_Id(timer ? static_cast<int>(timer->GetId()) : 0),
      m_Name(timer ? timer->GetName() : std::string()),
      m_State(timer ? static_cast<int>(timer->GetState()) : static_cast<int>(Timer::CANCELLED)),
      m_Type(timer ? static_cast<int>(timer->GetType()) : static_cast<int>(Timer::ONCE)),
      m_TimeBase(timer ? static_cast<int>(timer->GetTimeBase()) : static_cast<int>(Timer::TICK)),
      m_CompletedIterations(timer ? timer->GetCompletedIterations() : 0),
      m_RemainingIterations(timer ? timer->GetRemainingIterations() : 0),
      m_Progress(timer ? timer->GetProgress() : 0.0f) {}
int ScriptTimerEventView::GetId() const { return m_Id; }
std::string ScriptTimerEventView::GetName() const { return m_Name; }
int ScriptTimerEventView::GetState() const { return m_State; }
int ScriptTimerEventView::GetType() const { return m_Type; }
int ScriptTimerEventView::GetTimeBase() const { return m_TimeBase; }
int ScriptTimerEventView::GetCompletedIterations() const { return m_CompletedIterations; }
int ScriptTimerEventView::GetRemainingIterations() const { return m_RemainingIterations; }
float ScriptTimerEventView::GetProgress() const { return m_Progress; }

ScriptTimerRef::ScriptTimerRef(std::weak_ptr<ScriptTimerServiceState> state, Timer::TimerId id)
    : m_State(std::move(state)), m_Id(id) {}

void ScriptTimerRef::AddRef() { ++m_RefCount; }
void ScriptTimerRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

Timer *ScriptTimerRef::ResolveTimer() const {
    std::shared_ptr<ScriptTimerServiceState> state = m_State.lock();
    if (!state || !state->Active || m_Id == Timer::InvalidId)
        return nullptr;
    if (state->Timers.find(m_Id) == state->Timers.end())
        return nullptr;
    std::shared_ptr<Timer> timer = Timer::FindById(m_Id);
    return timer ? timer.get() : nullptr;
}

bool ScriptTimerRef::IsValid() const { return ResolveTimer() != nullptr; }
int ScriptTimerRef::GetId() const { return static_cast<int>(m_Id); }
std::string ScriptTimerRef::GetName() const {
    Timer *timer = ResolveTimer();
    return timer ? timer->GetName() : std::string();
}
int ScriptTimerRef::GetState() const {
    Timer *timer = ResolveTimer();
    return timer ? static_cast<int>(timer->GetState()) : static_cast<int>(Timer::CANCELLED);
}
int ScriptTimerRef::GetCompletedIterations() const {
    Timer *timer = ResolveTimer();
    return timer ? timer->GetCompletedIterations() : 0;
}
int ScriptTimerRef::GetRemainingIterations() const {
    Timer *timer = ResolveTimer();
    return timer ? timer->GetRemainingIterations() : 0;
}
float ScriptTimerRef::GetProgress() const {
    Timer *timer = ResolveTimer();
    return timer ? timer->GetProgress() : 0.0f;
}
void ScriptTimerRef::Pause() {
    if (Timer *timer = ResolveTimer())
        timer->Pause();
}
void ScriptTimerRef::Resume() {
    if (Timer *timer = ResolveTimer())
        timer->Resume();
}
void ScriptTimerRef::Cancel() {
    std::shared_ptr<ScriptTimerServiceState> state = m_State.lock();
    if (!state || !state->Active || m_Id == Timer::InvalidId)
        return;
    RetireTimerEntry(state, m_Id, true);
}

ScriptTimerService::ScriptTimerService() : m_State(std::make_shared<ScriptTimerServiceState>()) {}
ScriptTimerService::~ScriptTimerService() { Release(nullptr); }

void ScriptTimerService::Bind(ModContext *context, ScriptMod *owner, ScriptModRuntime *runtime, ScriptModContextView *contextView) {
    m_State->Context = context;
    m_State->Owner = owner;
    m_State->Runtime = runtime;
    m_State->ContextView = contextView;
    m_State->Active = true;
}

ScriptTimerRef *ScriptTimerService::Add(asIScriptObject *timer) {
    if (!timer || !m_State->Active || !m_State->Context || !m_State->Owner || !timer->GetObjectType())
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

    Timer::Builder builder;
    if (registration.HasDelayTicks && registration.DelayTicks > 0)
        builder.WithDelayTicks(registration.DelayTicks);
    else
        builder.WithDelaySeconds(registration.DelayMs / 1000.0f);

    if (registration.RepeatCount > 1)
        registration.Loop = true;
    builder.WithType(registration.Loop ? Timer::LOOP : Timer::ONCE)
        .WithPriority(static_cast<int8_t>(std::max(-128, std::min(127, registration.Priority))));
    if (registration.RepeatCount > 0)
        builder.WithRepeatCount(registration.RepeatCount);
    if (registration.StartPaused)
        builder.StartPaused();

    return AddTimer(builder,
                    timer,
                    registration,
                    time->GetMainTickCount(),
                    time->GetAbsoluteTime() / 1000.0f);
}

ScriptTimerRef *ScriptTimerService::AddTimer(Timer::Builder &builder,
                                             asIScriptObject *timer,
                                             const ScriptTimerRegistration &registration,
                                             size_t tick,
                                             float time) {
    if (!m_State->Active || !m_State->Owner || !timer || !timer->GetObjectType())
        return nullptr;

    std::weak_ptr<ScriptTimerServiceState> weakState = m_State;
    builder.WithName(registration.Name)
        .AddToGroup(std::string("script:") + (m_State->Owner ? m_State->Owner->GetID() : "unknown"));
    if (registration.Loop) {
        builder.WithLoopCallback([weakState](Timer &nativeTimer) {
            return ExecuteTimerCallback(weakState, nativeTimer.GetId(), nativeTimer);
        });
    } else {
        builder.WithOnceCallback([weakState](Timer &nativeTimer) {
            ExecuteTimerCallback(weakState, nativeTimer.GetId(), nativeTimer);
        });
    }

    std::shared_ptr<Timer> nativeTimer = builder.Build(tick, time);
    if (!nativeTimer)
        return nullptr;

    ScriptObjectHandle retained = ScriptObjectHandle::Retain(timer);
    ScriptTimerEntry entry;
    entry.Object = retained.Detach();
    entry.TickMethod = registration.TickMethod;
    entry.Loop = registration.Loop;
    entry.Name = registration.Name;
    m_State->Timers.emplace(nativeTimer->GetId(), std::move(entry));
    return new ScriptTimerRef(m_State, nativeTimer->GetId());
}

void ScriptTimerService::Release(ScriptDiagnostic *) {
    if (!m_State || !m_State->Active)
        return;

    m_State->Active = false;
    for (auto &entry : m_State->Timers) {
        if (std::shared_ptr<Timer> timer = Timer::FindById(entry.first))
            timer->Cancel();
        ReleaseScriptTimerObject(entry.second);
    }
    m_State->Timers.clear();
}

} // namespace BML
