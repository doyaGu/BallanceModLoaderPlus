#include "ScriptRuntimeScope.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"

namespace BML {
void ScriptRuntimeAccess::Enter(BML_ModHandle owner) {
    if (!m_Context)
        return;
    if (!m_Context->IsMainThread()) {
        m_Code = BML_ERROR_WRONG_THREAD;
        return;
    }
    m_Gate = m_Context->GetModLoader().LockModInvocation();
    m_Owner = ScriptModRuntime::GetCurrentScriptMod();
    if (!m_Owner || m_Context->GetModLoader().GetModGeneration(&m_Owner->Registration()) != owner) {
        m_Code = BML_ERROR_ACCESS_DENIED;
        return;
    }
    m_Code = BML_OK;
}

void ScriptRuntimeAccess::Lock(const std::weak_ptr<ScriptRuntimeScope> &scope) {
    if (m_Code != BML_OK)
        return;
    m_Scope = scope.lock();
    if (!m_Scope || !m_Scope->Active ||
        m_Owner->BorrowContextView()->RuntimeScope().lock() != m_Scope)
        m_Code = BML_ERROR_INVALID_HANDLE;
}

ScriptRuntimeAccess::ScriptRuntimeAccess(const std::weak_ptr<ScriptRuntimeScope> &scope) {
    // Immutable owner identity must remain available when a weak state expires.
    // Ordinary views keep it separately via their control ticket.
    const auto state = scope.lock();
    Enter(state ? state->Owner : 0);
    if (!state && m_Code == BML_ERROR_ACCESS_DENIED && m_Owner)
        m_Code = BML_ERROR_INVALID_HANDLE;
    Lock(scope);
}

ScriptRuntimeAccess::ScriptRuntimeAccess(const ScriptModContextView &view) {
    Enter(view.RuntimeOwner());
    Lock(view.RuntimeScope());
}

ScriptRuntimeAccess::ScriptRuntimeAccess(const ScriptControlTicket &ticket, bool ending) {
    Enter(ticket.Owner);
    if (m_Code == BML_OK && ending && ticket.Terminal)
        return;
    Lock(ticket.Scope);
}

bool RetireScriptRuntime(std::shared_ptr<ScriptRuntimeScope> &scope) {
    if (!scope)
        return true;
    scope->Active = false;
    ModContextLease context;
    if (context) {
        if (auto *owner = context->GetModLoader().ResolveModHandle(scope->Owner)) {
            context->GetConfigApi().CleanupOwner(*owner);
            auto &blocks = scope->Blocks;
            for (auto it = blocks.begin(); it != blocks.end();) {
                const int code = context->GetInputApi().ReleaseBlock(*context, *owner, it->first);
                if (code == BML_OK || code == BML_ERROR_INVALID_HANDLE) {
                    it->second->Terminal = true;
                    it = blocks.erase(it);
                } else
                    ++it;
            }
        }
    }
    if (!scope->Blocks.empty())
        return false;
    scope.reset();
    return true;
}
} // namespace BML
