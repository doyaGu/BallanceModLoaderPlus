#ifndef BML_SCRIPTTIMERSERVICE_H
#define BML_SCRIPTTIMERSERVICE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "Timer/TimerRegistry.h"
#include "ScriptRuntimeScope.h"
#include "ScriptDiagnostic.h"
#include "ScriptModRuntime.h"

class ModContext;
class asIScriptObject;
class asIScriptFunction;

namespace BML {

class ScriptMod;
class ScriptModContextView;

struct ScriptTimerDefinition {
    std::string Name;
    uint32_t Type = BML_TIMER_ONCE;
    uint32_t TimeBase = BML_TIMER_TIMEBASE_TIME;
    bool StartPaused = false;
    int Priority = 0;
    double DelayMs = 0;
    uint64_t DelayTicks = 0, RepeatCount = 0;
};

struct ScriptTimerInfo {
    uint64_t Handle = 0;
    std::string Name;
    uint32_t State = 0, Type = 0, TimeBase = 0;
    uint64_t CompletedIterations = 0;
    int64_t RemainingIterations = 0;
    float Progress = 0;
};

class ScriptTimerEventView {
public:
    ScriptTimerEventView() = default;
    explicit ScriptTimerEventView(const BML_TimerEvent &event);

    bool IsValid() const { return m_Valid; }

    int Read(ScriptTimerInfo &out) const;

private:
    bool m_Valid = false;
    ScriptTimerInfo m_Info;
};

class ScriptTimerServiceState;

class ScriptTimerRef {
public:
    ScriptTimerRef(std::weak_ptr<ScriptTimerServiceState> state, BML_TimerHandle id);

    void AddRef();
    void Release();

    bool IsValid() const;
    int Read(ScriptTimerInfo &out) const;
    int Pause();
    int Resume();
    int Cancel();
    std::shared_ptr<ScriptControlTicket> Ticket;

    void SetHandle(BML_TimerHandle handle) { m_Id = handle; }

private:
    TimerScheduler *ResolveTimer() const;

    int m_RefCount = 1;
    std::weak_ptr<ScriptTimerServiceState> m_State;
    BML_TimerHandle m_Id = 0;
};

class ScriptTimerService {
public:
    ScriptTimerService();
    ~ScriptTimerService();

    bool Bind(ModContext *context, ScriptMod *owner, ScriptModRuntime *runtime,
              ScriptModContextView *contextView);
    int Register(const ScriptTimerDefinition &definition, asIScriptFunction *callback,
                 bool returnsBool, ScriptTimerRef *&out);
    void Release(ScriptDiagnostic *diagnostic = nullptr);
    size_t GetActiveCount() const;

#ifdef BML_TEST
    ScriptTimerRef *AddTestTimerForRelease(
        const std::string &name,
        TimerScheduler::SimpleCallback callback = TimerScheduler::SimpleCallback());
#endif

private:
    std::shared_ptr<ScriptTimerServiceState> m_State;
};

} // namespace BML

#endif
