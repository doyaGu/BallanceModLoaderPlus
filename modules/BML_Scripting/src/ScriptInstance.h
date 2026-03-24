#ifndef BML_SCRIPTING_SCRIPT_INSTANCE_H
#define BML_SCRIPTING_SCRIPT_INSTANCE_H

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <angelscript.h>

#include "bml_export.h"

namespace BML::Scripting {

struct ScriptInstance {
    // Core association (managed by Core)
    BML_Mod mod_handle = nullptr;
    const BML_Services *services = nullptr;
    std::filesystem::path entry_file;
    std::filesystem::path module_dir;
    std::string mod_id;

    // AngelScript state (managed by BML_Scripting)
    asIScriptModule *as_module = nullptr;

    // Cached lifecycle callback pointers
    asIScriptFunction *fn_OnAttach = nullptr;
    asIScriptFunction *fn_OnDetach = nullptr;
    asIScriptFunction *fn_OnInit = nullptr;
    asIScriptFunction *fn_OnPrepareReload = nullptr;

    // Script-registered event callbacks: topic -> [EventHandler]
    struct EventHandler {
        asIScriptFunction *fn = nullptr;
        int param_type = -1;  // cached AS type ID for multi-type dispatch
    };
    std::unordered_map<std::string,
        std::vector<EventHandler>> event_handlers;

    // Per-topic IMC subscription (one subscription per topic, shared by all
    // handlers on that topic). The SubContext struct is defined in BindImc.cpp.
    struct SubContextDeleter { void operator()(void *p) const; };
    struct TopicSubscription {
        std::unique_ptr<void, SubContextDeleter> ctx;  // SubContext* (type-erased)
        BML_Subscription handle = nullptr;              // for explicit unsubscribe
    };
    std::unordered_map<std::string, TopicSubscription> topic_subs;

    // Per-timer context objects (owned here, pointers passed as
    // user_data to Timer callbacks). Cleaned up when instance is destroyed.
    struct TimerContextDeleter { void operator()(void *p) const; };
    std::vector<std::unique_ptr<void, TimerContextDeleter>> timer_contexts;

    // Timeout from [scripting].timeout_ms (default 100)
    uint32_t timeout_ms = 100;

    enum class State { Unloaded, Compiled, Attached, InitDone, Error };
    State state = State::Unloaded;
};

// Thread-local pointer to the currently executing script instance.
// Set by DispatchToScript / InvokeLifecycle before entering AS code,
// cleared after. Binding functions use this to find their context.
inline thread_local ScriptInstance *t_CurrentScript = nullptr;

// Timeout protection for AS execution.
struct TimeoutGuard {
    std::chrono::steady_clock::time_point start;
    uint32_t limit_ms;

    explicit TimeoutGuard(uint32_t ms)
        : start(std::chrono::steady_clock::now()), limit_ms(ms) {}

    static void LineCallback(asIScriptContext *c, void *param) {
        auto *g = static_cast<TimeoutGuard *>(param);
        auto elapsed = std::chrono::steady_clock::now() - g->start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
            > static_cast<long long>(g->limit_ms))
            c->Abort();
    }
};

} // namespace BML::Scripting

#endif // BML_SCRIPTING_SCRIPT_INSTANCE_H
