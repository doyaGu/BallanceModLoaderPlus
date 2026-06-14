#ifndef BML_SCRIPTTIMERSERVICE_H
#define BML_SCRIPTTIMERSERVICE_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "BML/Timer.h"
#include "ScriptDiagnostic.h"
#include "ScriptModRuntime.h"

class ModContext;
class asIScriptObject;

namespace BML {

class ScriptMod;
class ScriptModContextView;

class ScriptTimerEventView {
public:
    explicit ScriptTimerEventView(Timer *timer = nullptr);

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
    ScriptTimerRef(std::weak_ptr<ScriptTimerServiceState> state, Timer::TimerId id);

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
    Timer *ResolveTimer() const;

    int m_RefCount = 1;
    std::weak_ptr<ScriptTimerServiceState> m_State;
    Timer::TimerId m_Id = Timer::InvalidId;
};

class ScriptTimerService {
public:
    ScriptTimerService();
    ~ScriptTimerService();

    void Bind(ModContext *context, ScriptMod *owner, ScriptModRuntime *runtime, ScriptModContextView *contextView);
    ScriptTimerRef *Add(asIScriptObject *timer);
    void Release(ScriptDiagnostic *diagnostic = nullptr);

private:
    ScriptTimerRef *AddTimer(BML::Timer::Builder &builder,
                             asIScriptObject *timer,
                             const ScriptTimerRegistration &registration,
                             size_t tick,
                             float time);

    std::shared_ptr<ScriptTimerServiceState> m_State;
};

} // namespace BML

#endif
