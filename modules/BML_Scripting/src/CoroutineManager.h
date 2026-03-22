#ifndef BML_SCRIPTING_COROUTINE_MANAGER_H
#define BML_SCRIPTING_COROUTINE_MANAGER_H

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include <angelscript.h>

#include "bml_export.h"

namespace BML::Scripting {

struct ScriptInstance;
class ScriptInstanceManager;

// A suspended coroutine waiting to resume.
struct SuspendedCoroutine {
    asIScriptContext *context = nullptr; // Managed here; must Release
    ScriptInstance *instance = nullptr;
    uint32_t resume_frame = 0;          // 0 = resume next frame
    std::chrono::steady_clock::time_point resume_time{}; // zero = use frame count
    bool use_time = false;
};

// Manages coroutines across all script instances.
// Ticked once per frame from BML/Engine/PostProcess.
class CoroutineManager {
public:
    explicit CoroutineManager(asIScriptEngine *engine, ScriptInstanceManager *mgr);
    ~CoroutineManager();

    // Called every frame to resume ready coroutines.
    void Tick();

    // Suspend the currently executing context and schedule resumption.
    // Called from AS binding functions (yield/waitFrames/waitSeconds).
    void YieldCurrent();
    void WaitFrames(uint32_t frames);
    void WaitSeconds(float seconds);

    // Clean up coroutines for a specific instance (on reload/detach).
    void CleanupForInstance(ScriptInstance *inst);

    uint32_t GetFrame() const { return m_Frame; }

private:
    asIScriptEngine *m_Engine;
    ScriptInstanceManager *m_Manager;
    std::vector<std::unique_ptr<SuspendedCoroutine>> m_Suspended;
    uint32_t m_Frame = 0;
};

} // namespace BML::Scripting

#endif
