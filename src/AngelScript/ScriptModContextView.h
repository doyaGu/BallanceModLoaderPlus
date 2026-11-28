#ifndef BML_SCRIPTMODCONTEXTVIEW_H
#define BML_SCRIPTMODCONTEXTVIEW_H

#include <memory>
#include <string>

#include "BML/Mod.h"
#include "ScriptModLifecycle.h"

class asIScriptFunction;
class asIScriptObject;

namespace BML {
struct ScriptRuntimeScope;
class ScriptDataShareRequestRef;
class ScriptImcRecord;
class ScriptImcRequestRef;
class ScriptImcSubscriptionRef;
class ScriptImcProviderRef;

class ScriptModContextView {
public:
    void BindRuntime(const std::shared_ptr<ScriptRuntimeScope> &scope, BML_ModHandle owner) {
        m_Runtime = scope;
        m_RuntimeOwner = owner;
    }
    const std::weak_ptr<ScriptRuntimeScope> &RuntimeScope() const {
        return m_Runtime;
    }
    BML_ModHandle RuntimeOwner() const {
        return m_RuntimeOwner;
    }

    bool IsReloading() const;
    ScriptModReloadPhase GetReloadPhase() const;
    unsigned int GetReloadAttemptId() const;
    unsigned int GetModGeneration() const;
    unsigned int GetRuntimeGeneration() const;
    ScriptDataShareRequestRef *RequestDataShare(asIScriptObject *request) const;
    ScriptDataShareRequestRef *RequestDataShare(const std::string &key, int type,
                                                asIScriptFunction *callback,
                                                const std::string &name) const;
    int IsImcRpcAvailable(const std::string &route, bool &available) const;
    ScriptImcRequestRef *CallImc(const std::string &route, const std::string &requestPayload,
                                 const std::string &responsePayload, const ScriptImcRecord &request,
                                 asIScriptFunction *callback, unsigned int timeoutMs) const;
    ScriptImcSubscriptionRef *SubscribeImc(const std::string &topic, const std::string &payload,
                                           asIScriptFunction *callback,
                                           unsigned int capacity) const;
    int PublishImc(const std::string &topic, const std::string &payload,
                   const ScriptImcRecord &message, std::uint64_t &delivered) const;
    int GetImcSubscriberCount(const std::string &topic, std::uint64_t &count) const;
    ScriptImcProviderRef *OpenImcProvider() const;

private:
    std::weak_ptr<ScriptRuntimeScope> m_Runtime;
    BML_ModHandle m_RuntimeOwner = 0;
};
} // namespace BML
#endif
