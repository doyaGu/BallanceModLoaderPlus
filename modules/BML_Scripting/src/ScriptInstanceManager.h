#ifndef BML_SCRIPTING_SCRIPT_INSTANCE_MANAGER_H
#define BML_SCRIPTING_SCRIPT_INSTANCE_MANAGER_H

#include <memory>
#include <unordered_map>

#include <angelscript.h>

#include "bml_export.h"
#include "bml_errors.h"

#include "ScriptInstance.h"

namespace BML::Scripting {

class CoroutineManager;

class ScriptInstanceManager {
public:
    explicit ScriptInstanceManager(asIScriptEngine *engine);
    ~ScriptInstanceManager();

    void SetCoroutineManager(CoroutineManager *mgr) { m_Coroutines = mgr; }

    // Context pool (re-entrant safe, one context per invocation)
    asIScriptContext *AcquireContext();
    void ReleaseContext(asIScriptContext *ctx);

    // RuntimeProvider callbacks
    BML_Result CompileAndAttach(BML_Mod mod, PFN_BML_GetProcAddress get_proc,
                                const char *entry_path, const char *module_dir);
    BML_Result PrepareDetach(BML_Mod mod);
    BML_Result Detach(BML_Mod mod);
    BML_Result Reload(BML_Mod mod);

    // Engine lifecycle
    void SetVirtoolsReady(bool ready) { m_VirtoolsReady = ready; }
    void InitAll();

    // Reload helpers
    BML_Result ReloadById(const std::string &mod_id);
    void ReloadAll();

    // Public dispatch (used by BML_ScriptEngineInterface)
    BML_Result InvokeByName(BML_Mod mod, const char *func_name);
    BML_Result InvokeByNameInt(BML_Mod mod, const char *func_name, int arg);
    BML_Result InvokeByNameString(BML_Mod mod, const char *func_name, const char *arg);
    void *GetModulePtr(BML_Mod mod);
    bool IsScript(BML_Mod mod);

    // Query
    ScriptInstance *FindByMod(BML_Mod mod);
    size_t GetInstanceCount() const { return m_Instances.size(); }

    template <typename Fn>
    void ForEachInstance(Fn &&fn) const {
        for (const auto &[mod, inst] : m_Instances) {
            fn(*inst);
        }
    }

private:
    bool CompileScript(ScriptInstance &inst);
    void ResolveCallbacks(ScriptInstance &inst);
    BML_Result InvokeCallback(ScriptInstance &inst, asIScriptFunction *fn);
    BML_Result InvokeLifecycle(ScriptInstance &inst, asIScriptFunction *fn);
    BML_Result RunInitCallback(ScriptInstance &inst, bool fail_if_ready);
    void LogInitFailure(const ScriptInstance &inst, BML_Result result) const;

    asIScriptEngine *m_Engine;
    CoroutineManager *m_Coroutines = nullptr;
    std::unordered_map<BML_Mod, std::unique_ptr<ScriptInstance>> m_Instances;
    std::vector<asIScriptContext *> m_ContextPool;
    bool m_VirtoolsReady = false;
};

} // namespace BML::Scripting

#endif // BML_SCRIPTING_SCRIPT_INSTANCE_MANAGER_H
