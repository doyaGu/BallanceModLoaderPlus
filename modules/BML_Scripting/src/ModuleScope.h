#ifndef BML_SCRIPTING_MODULE_SCOPE_H
#define BML_SCRIPTING_MODULE_SCOPE_H

#include "bml_services.hpp"
#include "ScriptInstance.h"

namespace BML::Scripting {

// Pointer to the runtime's service bundle. Set once during OnAttach,
// used by all binding functions and dispatch code to access Core APIs.
inline const BML_Services *g_Services = nullptr;

inline BML_Mod CurrentScriptOwner() {
    return t_CurrentScript ? t_CurrentScript->mod_handle : nullptr;
}

// RAII guard for the currently executing script instance.
struct ScriptScope {
    ScriptInstance *previousScript = nullptr;

    explicit ScriptScope(ScriptInstance *inst) {
        previousScript = t_CurrentScript;
        t_CurrentScript = inst;
    }

    ~ScriptScope() {
        t_CurrentScript = previousScript;
    }

    ScriptScope(const ScriptScope &) = delete;
    ScriptScope &operator=(const ScriptScope &) = delete;
};

} // namespace BML::Scripting

#endif
