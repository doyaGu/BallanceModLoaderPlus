#include "ScriptStringInterop.h"
#include "Api/ModApi.h"
#include "BML/Detail/Api.hpp"

#include "ScriptTimerService.h"

#include <algorithm>
#include <cstdint>
#include <new>
#include <optional>
#include <utility>

#include "Logging/Logger.h"
#include "Loader/ModContext.h"
#include "ScriptAngelScriptHandle.h"
#include "ScriptFunctionSupport.h"
#include "ScriptMod.h"
#include "ScriptModContextView.h"
#include "ScriptModRuntime.h"

namespace BML {

struct ScriptTimerEntry {
    std::shared_ptr<ScriptControlTicket> Ticket;
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
        : Ticket(std::move(other.Ticket)), Callback(std::exchange(other.Callback, nullptr)),
          OwnsCallbackRef(std::exchange(other.OwnsCallbackRef, false)), Loop(other.Loop),
          ReturnsBool(other.ReturnsBool), Name(std::move(other.Name)) {}

    ~ScriptTimerEntry() {
        if (OwnsCallbackRef && Callback)
            Callback->Release();
    }
};

static bool IsTimerCallbackSignature(asIScriptFunction *callback, bool returnsBool) {
    const ScriptFunctionParam params[] = {
        {"BML::ModContext", asTM_INREF | asTM_CONST},
        {"BML::TimerEvent", asTM_INREF | asTM_CONST},
    };
    return ScriptFunctionHasSignature(callback, returnsBool ? asTYPEID_BOOL : asTYPEID_VOID, params,
                                      2);
}

class ScriptTimerServiceState {
public:
    ModContext *Context = nullptr;
    ScriptMod *Owner = nullptr;
    ScriptModRuntime *Runtime = nullptr;
    ScriptModContextView *ContextView = nullptr;
    bool Active = false;
    TimerRegistry *Registry = nullptr;
    std::unordered_map<BML_TimerHandle, ScriptTimerEntry> Timers;
};

static void ReleaseScriptTimerObject(ScriptTimerEntry &entry) {

    if (entry.OwnsCallbackRef && entry.Callback) {
        entry.Callback->Release();
        entry.Callback = nullptr;
        entry.OwnsCallbackRef = false;
    }
}

static void RetireTimerEntry(const std::shared_ptr<ScriptTimerServiceState> &state,
                             BML_TimerHandle id) {
    if (!state)
        return;

    auto it = state->Timers.find(id);
    if (it == state->Timers.end())
        return;

    if (it->second.Ticket)
        it->second.Ticket->Terminal = true;
    if (it->second.ActiveCalls > 0) {
        it->second.PendingRetire = true;
        return;
    }

    if (it->second.Ticket)
        it->second.Ticket->Terminal = true;
    ReleaseScriptTimerObject(it->second);
    state->Timers.erase(it);
}

static void RecordTimerDiagnostic(const std::shared_ptr<ScriptTimerServiceState> &state,
                                  const ScriptDiagnostic &diagnostic, bool fail) {
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
                                 BML_TimerHandle id, const BML_TimerEvent &timer) {
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
    const bool callSucceeded = [&]() {
        TimerFunctionCallArgs args = {state->ContextView, &event, it->second.ReturnsBool, false};
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

ScriptTimerEventView::ScriptTimerEventView(const BML_TimerEvent &event)
    : m_Valid(true), m_Info{event.Handle,
                            event.Name ? event.Name : "",
                            event.State,
                            event.Type,
                            event.TimeBase,
                            event.CompletedIterations,
                            event.RemainingIterations,
                            event.Progress} {}

int ScriptTimerEventView::Read(ScriptTimerInfo &out) const try {
    if (!m_Valid)
        return BML_ERROR_INVALID_HANDLE;
    ScriptTimerInfo value = m_Info;
    out = std::move(value);
    return BML_OK;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

ScriptTimerRef::ScriptTimerRef(std::weak_ptr<ScriptTimerServiceState> state, BML_TimerHandle id)
    : m_State(std::move(state)), m_Id(id) {}

void ScriptTimerRef::AddRef() {
    ++m_RefCount;
}

void ScriptTimerRef::Release() {
    if (--m_RefCount == 0)
        delete this;
}

TimerScheduler *ScriptTimerRef::ResolveTimer() const {
    std::shared_ptr<ScriptTimerServiceState> state = m_State.lock();
    if (!state || !state->Active || m_Id == 0)
        return nullptr;
    if (state->Timers.find(m_Id) == state->Timers.end())
        return nullptr;
    auto timer = state->Registry ? state->Registry->Find(state.get(), m_Id) : nullptr;
    return timer ? timer.get() : nullptr;
}

bool ScriptTimerRef::IsValid() const {
    std::optional<ScriptRuntimeAccess> access;
    if (Ticket) {
        access.emplace(*Ticket);
        if (access->Code() != BML_OK)
            return false;
    }
    return ResolveTimer() != nullptr;
}

int ScriptTimerRef::Read(ScriptTimerInfo &out) const try {
    if (!Ticket)
        return BML_ERROR_INVALID_HANDLE;
    ScriptRuntimeAccess access(*Ticket);
    if (access.Code() != BML_OK)
        return access.Code();
    auto state = m_State.lock();
    if (!state || !state->Active || !state->Registry)
        return BML_ERROR_INVALID_HANDLE;
    return state->Registry->Read(state.get(), m_Id, [&](const BML_TimerEvent &event) {
        return ScriptTimerEventView(event).Read(out);
    });
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

int ScriptTimerRef::Pause() try {
    if (!Ticket)
        return BML_ERROR_INVALID_HANDLE;
    ScriptRuntimeAccess access(*Ticket);
    if (access.Code() != BML_OK)
        return access.Code();
    auto state = m_State.lock();
    return state && state->Active && state->Registry
               ? state->Registry->Control(state.get(), m_Id, 0)
               : BML_ERROR_INVALID_HANDLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

int ScriptTimerRef::Resume() try {
    if (!Ticket)
        return BML_ERROR_INVALID_HANDLE;
    ScriptRuntimeAccess access(*Ticket);
    if (access.Code() != BML_OK)
        return access.Code();
    auto state = m_State.lock();
    return state && state->Active && state->Registry
               ? state->Registry->Control(state.get(), m_Id, 1)
               : BML_ERROR_INVALID_HANDLE;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

int ScriptTimerRef::Cancel() try {
    if (!Ticket)
        return BML_ERROR_INVALID_HANDLE;
    ScriptRuntimeAccess access(*Ticket, true);
    if (access.Code() != BML_OK || Ticket->Terminal)
        return access.Code();
    auto state = m_State.lock();
    if (!state || !state->Active || !state->Registry)
        return BML_ERROR_INVALID_HANDLE;
    int code = state->Registry->Control(state.get(), m_Id, 2);
    if (code == BML_OK)
        Ticket->Terminal = true;
    return code;
} catch (const std::bad_alloc &) {
    return BML_ERROR_OUT_OF_MEMORY;
} catch (...) {
    return BML_ERROR_FAIL;
}

ScriptTimerService::ScriptTimerService() : m_State(std::make_shared<ScriptTimerServiceState>()) {}

ScriptTimerService::~ScriptTimerService() {
    try {
        Release(nullptr);
    } catch (...) {
    }
}

bool ScriptTimerService::Bind(ModContext *context, ScriptMod *owner, ScriptModRuntime *runtime,
                              ScriptModContextView *contextView) {
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

static ScriptTimerRef *RegisterTimer(const std::shared_ptr<ScriptTimerServiceState> &state,
                                     const BML_TimerDefinition &definition, ScriptTimerEntry entry,
                                     int *status = nullptr) {
    if (!state || !state->Active || !state->Registry || !state->Context || !state->Owner)
        return nullptr;
    auto *clock = state->Context->GetTimeManager();
    if (!clock)
        return nullptr;
    auto ref = std::make_unique<ScriptTimerRef>(state, 0);
    auto scope = state->ContextView->RuntimeScope().lock();
    ref->Ticket = std::make_shared<ScriptControlTicket>(scope);
    entry.Ticket = ref->Ticket;
    auto identity = std::make_shared<BML_TimerHandle>(0);
    std::weak_ptr<ScriptTimerServiceState> weak = state;
    const int code = state->Registry->Register(
        state.get(), &definition,
        [weak](const BML_TimerEvent &event, uint32_t &keep) {
            keep = ExecuteTimerCallback(weak, event.Handle, event);
            return BML_OK;
        },
        [weak, identity] { RetireTimerEntry(weak.lock(), *identity); }, clock->GetMainTickCount(),
        clock->GetAbsoluteTime() / 1000.0f, identity.get());
    if (status)
        *status = code;
    if (code != BML_OK)
        return nullptr;
    const auto handle = *identity;
    try {
        state->Timers.emplace(handle, std::move(entry));
        ref->SetHandle(handle);
        return ref.release();
    } catch (...) {
        (void)state->Registry->Control(state.get(), handle, 2);
        throw;
    }
}

int ScriptTimerService::Register(const ScriptTimerDefinition &definition,
                                 asIScriptFunction *callback, bool returnsBool,
                                 ScriptTimerRef *&out) {
    out = nullptr;
    if (!m_State || !m_State->Active || !m_State->Context || !m_State->Owner)
        return BML_ERROR_INVALID_HANDLE;
    auto scope = m_State->ContextView->RuntimeScope().lock();
    if (!scope || !scope->Accepting ||
        !Api::AcceptsModWork(*m_State->Context, m_State->Owner->Registration()))
        return BML_ERROR_FROZEN;
    if (!callback || !IsTimerCallbackSignature(callback, returnsBool) ||
        !BML::Detail::IsText(definition.Name))
        return BML_ERROR_INVALID_PARAMETER;
    if (!m_State->Context->GetTimeManager())
        return BML_ERROR_UNAVAILABLE;
    BML_TimerDefinition wire{sizeof(wire),
                             definition.Name.c_str(),
                             definition.Type,
                             definition.TimeBase,
                             definition.StartPaused ? 1u : 0u,
                             definition.Priority,
                             definition.DelayMs,
                             definition.DelayTicks,
                             definition.RepeatCount};
    ScriptTimerEntry entry;
    callback->AddRef();
    entry.Callback = callback;
    entry.OwnsCallbackRef = true;
    entry.Loop = definition.Type != BML_TIMER_ONCE;
    entry.ReturnsBool = returnsBool;
    entry.Name = definition.Name;
    // Registry validation is shared with native registration, including zero delay and uint64
    // ticks.
    int code = BML_ERROR_UNAVAILABLE;
    out = RegisterTimer(m_State, wire, std::move(entry), &code);
    return code;
}

void ScriptTimerService::Release(ScriptDiagnostic *) {
    if (!m_State || !m_State->Active)
        return;

    std::shared_ptr<ScriptTimerServiceState> releasedState = m_State;
    releasedState->Active = false;
    if (releasedState->Registry)
        releasedState->Registry->CleanupOwner(releasedState.get());
    for (auto &[id, entry] : releasedState->Timers)
        if (entry.Ticket)
            entry.Ticket->Terminal = true;
    m_State.reset();
}

size_t ScriptTimerService::GetActiveCount() const {
    return m_State && m_State->Active ? m_State->Timers.size() : 0;
}

#ifdef BML_TEST
ScriptTimerRef *
ScriptTimerService::AddTestTimerForRelease(const std::string &name,
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
    if (registry.Register(
            m_State.get(), &definition,
            [fn = std::move(callback)](const BML_TimerEvent &, uint32_t &keep) {
                fn();
                keep = 0;
                return BML_OK;
            },
            [weak, identity] { RetireTimerEntry(weak.lock(), *identity); }, 0, 0.0f,
            identity.get()) != BML_OK)
        return nullptr;
    try {
        ScriptTimerEntry entry;
        entry.Name = name;
        m_State->Timers.emplace(*identity, std::move(entry));
        return new ScriptTimerRef(m_State, *identity);
    } catch (...) {
        (void)registry.Control(m_State.get(), *identity, 2);
        throw;
    }
}
#endif

} // namespace BML
