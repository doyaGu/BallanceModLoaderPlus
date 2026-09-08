#ifndef BML_BEHAVIOR_CKBEHAVIORCONTEXT_H
#define BML_BEHAVIOR_CKBEHAVIORCONTEXT_H

#include "CKAll.h"

namespace BML::Behavior::Internal {

// CKBehavior::CallCallbackFunction overwrites CKContext::m_BehaviorContext
// and does not restore it. BML-driven Execute and callback calls therefore
// bind the addressed Behavior for the duration of the call and restore the
// interrupted CK dispatch state afterwards.
class CKBehaviorContextScope final {
public:
    CKBehaviorContextScope(CKContext *context, CKBehavior *behavior,
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

    ~CKBehaviorContextScope() {
        m_Context->m_BehaviorContext = m_SavedContext;
        if (m_Manager)
            m_Manager->m_CurrentBehavior = m_SavedCurrent;
    }

    CKBehaviorContextScope(const CKBehaviorContextScope &) = delete;
    CKBehaviorContextScope &operator=(const CKBehaviorContextScope &) = delete;

private:
    CKContext *m_Context;
    CKBehaviorContext m_SavedContext;
    CKBehaviorManager *m_Manager;
    CKBehavior *m_SavedCurrent;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_CKBEHAVIORCONTEXT_H
