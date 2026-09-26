#ifndef BML_BEHAVIOR_ENGINE_SCOPE_H
#define BML_BEHAVIOR_ENGINE_SCOPE_H

#include "CKAll.h"

namespace BML::Behavior::Internal::Engine {

// CKBehavior::CallCallbackFunction overwrites CKContext::m_BehaviorContext
// and does not restore it. BML-driven Execute and callback calls therefore
// bind the addressed Behavior for the duration of the call and restore the
// interrupted CK dispatch state afterwards.
class Scope final {
public:
    Scope(CKContext *context, CKBehavior *behavior,
          const CKBehaviorContext *frame = nullptr)
        : m_Context(context), m_SavedContext(context->m_BehaviorContext),
          m_Manager(context->GetBehaviorManager()),
          m_SavedCurrent(m_Manager ? m_Manager->m_CurrentBehavior : nullptr) {
        if (frame)
            m_Context->m_BehaviorContext = *frame;
        m_Context->m_BehaviorContext.Context = m_Context;
        m_Context->m_BehaviorContext.Behavior = behavior;
        if (m_Manager)
            m_Manager->m_CurrentBehavior = behavior;
    }

    ~Scope() {
        m_Context->m_BehaviorContext = m_SavedContext;
        if (m_Manager)
            m_Manager->m_CurrentBehavior = m_SavedCurrent;
    }

    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

private:
    CKContext *m_Context;
    CKBehaviorContext m_SavedContext;
    CKBehaviorManager *m_Manager;
    CKBehavior *m_SavedCurrent;
};

// CKBehaviorManager::Execute defers object destruction for the whole
// behavior pass. A Block that BML executes outside that pass sees the same
// deferral, so an object it destroys stays valid until its function returns.
class ExecutionScope final {
public:
    ExecutionScope(CKContext *context, CKBehavior *behavior,
                   const CKBehaviorContext *frame)
        : m_Scope(context, behavior, frame), m_Context(context),
          m_SavedDeferDestroy(context->m_DeferDestroyObjects) {
        m_Context->m_DeferDestroyObjects = TRUE;
    }

    ~ExecutionScope() {
        m_Context->m_DeferDestroyObjects = m_SavedDeferDestroy;
    }

    ExecutionScope(const ExecutionScope &) = delete;
    ExecutionScope &operator=(const ExecutionScope &) = delete;

private:
    Scope m_Scope;
    CKContext *m_Context;
    CKDWORD m_SavedDeferDestroy;
};

} // namespace BML::Behavior::Internal::Engine

#endif // BML_BEHAVIOR_ENGINE_SCOPE_H
