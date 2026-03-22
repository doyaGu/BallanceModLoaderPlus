#ifndef BML_SCRIPTING_H
#define BML_SCRIPTING_H

/**
 * @file bml_scripting.h
 * @brief Public interface for the BML_Scripting module.
 *
 * Other modules acquire this interface to register AS bindings and
 * dispatch callbacks into script mods. Consumers must include
 * <angelscript.h> for the AS types.
 *
 * Usage:
 * @code
 * #include <bml_scripting.h>
 * #include <angelscript.h>
 *
 * auto *scripting = Services().Acquire<BML_ScriptEngineInterface>();
 *
 * // Register a new AS function
 * asIScriptEngine *engine = scripting->GetEngine();
 * engine->RegisterGlobalFunction(...);
 *
 * // Later, dispatch a callback into a script mod
 * scripting->InvokeFunction(scriptMod, "OnMyEvent");
 * @endcode
 */

#include "bml_interface.h"

// Forward declarations (full definitions in angelscript.h)
class asIScriptEngine;
class asIScriptModule;
class asIScriptContext;

BML_BEGIN_CDECLS

#define BML_SCRIPT_ENGINE_INTERFACE_ID "bml.scripting.engine"

typedef struct BML_ScriptEngineInterface {
    BML_InterfaceHeader header;

    // ----------------------------------------------------------------
    // Engine access
    // ----------------------------------------------------------------

    /** Get the shared asIScriptEngine. Do NOT ShutDownAndRelease(). */
    asIScriptEngine *(*GetEngine)(void);

    // ----------------------------------------------------------------
    // Per-script-mod queries
    // ----------------------------------------------------------------

    /** Get the asIScriptModule for a script mod. NULL if not a script. */
    asIScriptModule *(*GetModule)(BML_Mod mod);

    /** Check if a mod is a script mod managed by this runtime. */
    BML_Bool (*IsScriptMod)(BML_Mod mod);

    // ----------------------------------------------------------------
    // Safe dispatch into script functions
    //
    // These maintain thread-local script state and timeout protection while
    // runtime attribution continues to flow through the explicit BML_Mod
    // parameter. Safe to call from any native module's callback.
    // ----------------------------------------------------------------

    /** Invoke a zero-arg function by name. */
    BML_Result (*InvokeFunction)(BML_Mod mod, const char *func_name);

    /** Invoke a function with a single int argument. */
    BML_Result (*InvokeFunctionInt)(BML_Mod mod, const char *func_name,
                                     int arg);

    /** Invoke a function with a string argument (const string &in). */
    BML_Result (*InvokeFunctionString)(BML_Mod mod, const char *func_name,
                                        const char *arg);

} BML_ScriptEngineInterface;

BML_END_CDECLS

#endif /* BML_SCRIPTING_H */
