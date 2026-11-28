#include "ScriptModContextView.h"
#include "ScriptRuntimeScope.h"
#include "ScriptMod.h"

namespace BML {
bool ScriptModContextView::IsReloading() const {
    ScriptRuntimeAccess access(*this);
    return access.Code() == BML_OK && access.Owner().GetReloadPhase() != ScriptModReloadPhase::None;
}

ScriptModReloadPhase ScriptModContextView::GetReloadPhase() const {
    ScriptRuntimeAccess access(*this);
    return access.Code() == BML_OK ? access.Owner().GetReloadPhase() : ScriptModReloadPhase::None;
}

unsigned int ScriptModContextView::GetReloadAttemptId() const {
    ScriptRuntimeAccess access(*this);
    return access.Code() == BML_OK ? access.Owner().GetReloadAttemptId() : 0;
}

unsigned int ScriptModContextView::GetModGeneration() const {
    ScriptRuntimeAccess access(*this);
    return access.Code() == BML_OK ? access.Owner().GetModGeneration() : 0;
}

unsigned int ScriptModContextView::GetRuntimeGeneration() const {
    ScriptRuntimeAccess access(*this);
    return access.Code() == BML_OK ? access.Owner().GetRuntimeGeneration() : 0;
}

ScriptDataShareRequestRef *ScriptModContextView::RequestDataShare(asIScriptObject *request) const {
    ScriptRuntimeAccess access(*this);
    if (access.Code() == BML_OK)
        return access.Owner().RequestScriptDataShare(request);
    return nullptr;
}

ScriptDataShareRequestRef *ScriptModContextView::RequestDataShare(const std::string &key, int type,
                                                                  asIScriptFunction *callback,
                                                                  const std::string &name) const {
    ScriptRuntimeAccess access(*this);
    return access.Code() == BML_OK
               ? access.Owner().RequestScriptDataShare(key, type, callback, name)
               : nullptr;
}

int ScriptModContextView::IsImcRpcAvailable(const std::string &route, bool &available) const {
    ScriptRuntimeAccess access(*this);
    available = false;
    return access.Code() == BML_OK ? access.Owner().IsScriptImcRpcAvailable(route, available)
                                   : access.Code();
}

ScriptImcRequestRef *
ScriptModContextView::CallImc(const std::string &route, const std::string &requestPayload,
                              const std::string &responsePayload, const ScriptImcRecord &request,
                              asIScriptFunction *callback, unsigned int timeoutMs) const {
    ScriptRuntimeAccess access(*this);
    return access.Code() == BML_OK
               ? access.Owner().CallScriptImc(route, requestPayload, responsePayload, request,
                                              callback, timeoutMs)
               : nullptr;
}

ScriptImcSubscriptionRef *ScriptModContextView::SubscribeImc(const std::string &topic,
                                                             const std::string &payload,
                                                             asIScriptFunction *callback,
                                                             unsigned int capacity) const {
    ScriptRuntimeAccess access(*this);
    return access.Code() == BML_OK
               ? access.Owner().SubscribeScriptImc(topic, payload, callback, capacity)
               : nullptr;
}

int ScriptModContextView::PublishImc(const std::string &topic, const std::string &payload,
                                     const ScriptImcRecord &message,
                                     std::uint64_t &delivered) const {
    ScriptRuntimeAccess access(*this);
    if (access.Code() != BML_OK) {
        delivered = 0;
        return access.Code();
    }
    return access.Owner().PublishScriptImc(topic, payload, message, delivered);
}

int ScriptModContextView::GetImcSubscriberCount(const std::string &topic,
                                                std::uint64_t &count) const {
    ScriptRuntimeAccess access(*this);
    if (access.Code() != BML_OK) {
        count = 0;
        return access.Code();
    }
    return access.Owner().GetScriptImcSubscriberCount(topic, count);
}

ScriptImcProviderRef *ScriptModContextView::OpenImcProvider() const {
    ScriptRuntimeAccess access(*this);
    return access.Code() == BML_OK ? access.Owner().OpenScriptImcProvider() : nullptr;
}

} // namespace BML
