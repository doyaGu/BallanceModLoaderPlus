#ifndef BML_SCRIPTING_MODULE_SCOPE_H
#define BML_SCRIPTING_MODULE_SCOPE_H

#include "bml_builtin_interfaces.h"
#include "bml_services.hpp"
#include "ScriptInstance.h"

namespace BML::Scripting {

// Pointer to the runtime's builtin services. Set once during OnAttach,
// used by all binding functions and dispatch code to access Core APIs.
inline const bml::BuiltinServices *g_Builtins = nullptr;

// RAII guard: sets g_CurrentModule + t_CurrentScript, restores on destruction.
// Uses BuiltinServices->Module for Set/GetCurrentModule.
struct ScriptScope {
    BML_Mod previousModule = nullptr;
    ScriptInstance *previousScript = nullptr;

    explicit ScriptScope(ScriptInstance *inst) {
        previousScript = t_CurrentScript;
        if (g_Builtins && g_Builtins->Module) {
            previousModule = g_Builtins->Module->GetCurrentModule();
            g_Builtins->Module->SetCurrentModule(inst ? inst->mod_handle : nullptr);
        }
        t_CurrentScript = inst;
    }

    ~ScriptScope() {
        t_CurrentScript = previousScript;
        if (g_Builtins && g_Builtins->Module) {
            g_Builtins->Module->SetCurrentModule(previousModule);
        }
    }

    ScriptScope(const ScriptScope &) = delete;
    ScriptScope &operator=(const ScriptScope &) = delete;
};

} // namespace BML::Scripting

#endif
