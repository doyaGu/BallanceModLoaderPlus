#include "Behavior/Runtime.h"

#include <utility>

namespace BML::Behavior {

Instance::~Instance() = default;

Instance::Instance(Instance &&other) noexcept
    : m_Access(std::move(other.m_Access)),
      m_Id(std::exchange(other.m_Id, 0)) {}

Instance &Instance::operator=(Instance &&other) noexcept {
    if (this != &other) {
        m_Access = std::move(other.m_Access);
        m_Id = std::exchange(other.m_Id, 0);
    }
    return *this;
}

Instance::operator bool() const noexcept {
    return m_Id != 0;
}

void Instance::Reset() {
    m_Access.reset();
    m_Id = 0;
}

Runtime::Runtime(CKContext *context,
                 std::function<ObjectRef(const void *)> issueObjectRef)
    : m_Context(context), m_IssueObjectRef(std::move(issueObjectRef)) {}

Runtime::~Runtime() = default;

CreateResult Runtime::Instantiate(CKBeObject *, const Spec &,
                                  const CKBehaviorContext *) {
    return {{Error::ContextExpired, CKERR_INVALIDOBJECT, CKBR_BEHAVIORERROR,
             "No CKContext is present in the Authoring golden test."},
            {}, {}};
}

CallResult Runtime::Call(CKBeObject *, const Spec &, const Slot &,
                         const CKBehaviorContext *) {
    return {{Error::ContextExpired, CKERR_INVALIDOBJECT, CKBR_BEHAVIORERROR,
             "No CKContext is present in the Authoring golden test."},
            {}, {}, {}};
}

RunResult Runtime::StartTask(Instance &, const Slot &,
                             const CKBehaviorContext *) {
    return {{Error::ContextExpired, CKERR_INVALIDOBJECT, CKBR_BEHAVIORERROR,
             "No CKContext is present in the Authoring golden test."},
            RunState::Failed, CKBR_BEHAVIORERROR, {}};
}

Status Runtime::Continue(Instance &) {
    return {Error::ContextExpired, CKERR_INVALIDOBJECT, CKBR_BEHAVIORERROR,
            "No CKContext is present in the Authoring golden test."};
}

RunResult Runtime::Pulse(Instance &, const Slot &,
                         const CKBehaviorContext *) {
    return {{Error::ContextExpired, CKERR_INVALIDOBJECT, CKBR_BEHAVIORERROR,
             "No CKContext is present in the Authoring golden test."},
            RunState::Failed, CKBR_BEHAVIORERROR, {}};
}

ExecutionState Runtime::State(const Instance &) const {
    return ExecutionState::Closed;
}

std::shared_ptr<OutcomeStore> Runtime::Outcomes(const Instance &) const {
    return {};
}

void Runtime::ClosePending() {}

} // namespace BML::Behavior
