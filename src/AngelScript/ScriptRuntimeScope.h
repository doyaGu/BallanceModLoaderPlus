#ifndef BML_SCRIPT_RUNTIME_SCOPE_H
#define BML_SCRIPT_RUNTIME_SCOPE_H

#include <memory>
#include <vector>

#include "Loader/ModContext.h"
#include "CKAngelScriptAdapter.h"

class asIScriptEngine;

namespace BML {
class ScriptMod;
class ScriptModContextView;
class ScriptConfigView;
class ScriptInputView;
class ScriptTimersView;
class ScriptCommandsView;
struct ScriptControlTicket;

// One physical Bind, independent of public diagnostic generation counters.
// Views and control tickets borrow it; only ScriptMod owns its lifetime.
struct ScriptRuntimeScope {
    ~ScriptRuntimeScope();
    BML_ModHandle Owner = 0;
    BML_ModContextHandle Context = 0;
    bool Active = true;
    bool Accepting = true;
    ScriptConfigView *Config = nullptr;
    ScriptInputView *Input = nullptr;
    ScriptTimersView *Timers = nullptr;
    ScriptCommandsView *Commands = nullptr;
    std::vector<std::pair<BML_InputBlockHandle, std::shared_ptr<ScriptControlTicket>>> Blocks;
};

struct ScriptControlTicket {
    explicit ScriptControlTicket(const std::shared_ptr<ScriptRuntimeScope> &scope)
        : Owner(scope ? scope->Owner : 0), Scope(scope) {}

    const BML_ModHandle Owner;
    bool Terminal = false;
    std::weak_ptr<ScriptRuntimeScope> Scope;
};

// Keeps both the context lease and invocation gate through the complete call.
class ScriptRuntimeAccess {
public:
    explicit ScriptRuntimeAccess(const std::weak_ptr<ScriptRuntimeScope> &scope);
    explicit ScriptRuntimeAccess(const ScriptModContextView &view);
    explicit ScriptRuntimeAccess(const ScriptControlTicket &ticket, bool ending = false);

    int Code() const { return m_Code; }

    ModContext &Context() const { return *m_Context; }

    ScriptMod &Owner() const { return *m_Owner; }

    BML_ModContextHandle Handle() const { return m_Scope->Context; }

    const std::shared_ptr<ScriptRuntimeScope> &Scope() const { return m_Scope; }

private:
    void Enter(BML_ModHandle owner);
    void Lock(const std::weak_ptr<ScriptRuntimeScope> &scope);
    ModContextLease m_Context;
    ModInvocationGate::CallLock m_Gate;
    std::shared_ptr<ScriptRuntimeScope> m_Scope;
    ScriptMod *m_Owner = nullptr;
    int m_Code = BML_ERROR_FROZEN;
};

bool BindScriptRuntime(ScriptMod &owner, std::shared_ptr<ScriptRuntimeScope> &scope);
bool RetireScriptRuntime(std::shared_ptr<ScriptRuntimeScope> &scope);
int RegisterScriptAuthorTypes(asIScriptEngine *engine, const CKAngelScriptAdapter::Api &api,
                              const char **errorMessage);
int RegisterScriptAuthorMethods(asIScriptEngine *engine, const char **errorMessage);
} // namespace BML
#endif
