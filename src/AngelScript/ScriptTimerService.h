#ifndef BML_SCRIPTTIMERSERVICE_H
#define BML_SCRIPTTIMERSERVICE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "Timer/TimerRegistry.h"
#include "ScriptDiagnostic.h"
#include "ScriptModRuntime.h"

class ModContext;
class asIScriptObject;
class asIScriptFunction;

namespace BML {

class ScriptMod;
class ScriptModContextView;

class ScriptTimerEventView {
public:
    explicit ScriptTimerEventView(TimerScheduler *timer = nullptr);
    explicit ScriptTimerEventView(const BML_TimerEvent &event);

    bool IsValid() const { return m_Valid; }
    int GetId() const;
    std::string GetName() const;
    int GetState() const;
    int GetType() const;
    int GetTimeBase() const;
    int GetCompletedIterations() const;
    int GetRemainingIterations() const;
    float GetProgress() const;

private:
    bool m_Valid = false;
    int m_Id = 0;
    std::string m_Name;
    int m_State = 0;
    int m_Type = 0;
    int m_TimeBase = 0;
    int m_CompletedIterations = 0;
    int m_RemainingIterations = 0;
    float m_Progress = 0.0f;
};

class ScriptTimerServiceState;
struct ScriptTimerRegistration;

class ScriptTimerRef {
public:
    ScriptTimerRef(std::weak_ptr<ScriptTimerServiceState> state, TimerScheduler::TimerId id);

    void AddRef();
    void Release();

    bool IsValid() const;
    int GetId() const;
    std::string GetName() const;
    int GetState() const;
    int GetCompletedIterations() const;
    int GetRemainingIterations() const;
    float GetProgress() const;
    void Pause();
    void Resume();
    void Cancel();

private:
    TimerScheduler *ResolveTimer() const;

    int m_RefCount = 1;
    std::weak_ptr<ScriptTimerServiceState> m_State;
    TimerScheduler::TimerId m_Id = TimerScheduler::InvalidId;
};

class ScriptTimerService {
public:
    ScriptTimerService();
    ~ScriptTimerService();

    bool Bind(ModContext *context, ScriptMod *owner, ScriptModRuntime *runtime, ScriptModContextView *contextView);
    ScriptTimerRef *Add(asIScriptObject *timer);
    ScriptTimerRef *AddTimeoutTicks(unsigned int delayTicks, asIScriptFunction *callback, const std::string &name = std::string());
    ScriptTimerRef *AddTimeoutMs(float delayMs, asIScriptFunction *callback, const std::string &name = std::string());
    ScriptTimerRef *AddIntervalTicks(unsigned int delayTicks, asIScriptFunction *callback, const std::string &name = std::string());
    ScriptTimerRef *AddIntervalMs(float delayMs, asIScriptFunction *callback, const std::string &name = std::string());
    void Release(ScriptDiagnostic *diagnostic = nullptr);
    size_t GetActiveCount() const;

#ifdef BML_TEST
    ScriptTimerRef *AddTestTimerForRelease(const std::string &name,
                                           TimerScheduler::SimpleCallback callback = TimerScheduler::SimpleCallback());
#endif

private:
    ScriptTimerRef *AddTimer(const BML_TimerDefinition &, asIScriptObject *,
                             const ScriptTimerRegistration &);
    ScriptTimerRef *AddCallbackTimer(const BML_TimerDefinition &, asIScriptFunction *, bool);

    std::shared_ptr<ScriptTimerServiceState> m_State;
};

} // namespace BML

#endif
