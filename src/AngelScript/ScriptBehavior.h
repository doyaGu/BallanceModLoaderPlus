#ifndef BML_SCRIPT_BEHAVIOR_H
#define BML_SCRIPT_BEHAVIOR_H

#include <cstddef>
#include <memory>
#include <string>

#include "CKAll.h"
#include "ScriptDiagnostic.h"

class ModContext;
class asIScriptEngine;

namespace BML {

class ScriptMod;
class ScriptBehaviorBlock;
class ScriptBehaviorLayout;
class ScriptBehaviorGraph;
class ScriptBehaviorEdit;
class ScriptBehaviorPlan;
class ScriptBehaviorScript;
class ScriptBehaviorState;

// Owns the Behavior authoring objects created by one physical Script Mod
// runtime. Script handles only refer back to this state, so Release can close
// every native Behavior before CKAngelScript unloads or replaces the module.
class ScriptBehaviorService {
public:
    ScriptBehaviorService();
    ~ScriptBehaviorService();

    bool Bind(ModContext *context, ScriptMod *owner);
    void Release(ScriptDiagnostic *diagnostic = nullptr);
    [[nodiscard]] std::size_t GetActiveCount() const;

    ScriptBehaviorBlock *Use(CKGUID prototype);
    ScriptBehaviorBlock *Find(const std::string &name,
                              const std::string &category,
                              const std::string &provider);
    ScriptBehaviorLayout *Layout(CKGUID prototype);
    ScriptBehaviorEdit *Edit();
    ScriptBehaviorGraph *Inspect(CKBehavior *graph, bool live);
    ScriptBehaviorPlan *Plan(const std::string &name,
                             const std::string &script, bool each,
                             ScriptBehaviorEdit *edit);
    ScriptBehaviorScript *CreateScript(CKBeObject *owner,
                                       const std::string &name,
                                       ScriptBehaviorEdit *body,
                                       int priority);

private:
    std::shared_ptr<ScriptBehaviorState> m_State;
};

// Registers only BML's owner-scoped authoring model. CKAngelScript's raw
// Behavior/BB/Param API remains the low-level engine surface.
int RegisterScriptBehavior(asIScriptEngine *engine, const char **errorMessage);

} // namespace BML

#endif // BML_SCRIPT_BEHAVIOR_H
