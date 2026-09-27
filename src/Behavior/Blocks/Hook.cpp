#include "Behavior/Blocks/Hook.h"

#include <unordered_map>

namespace BML::Behavior::Internal::HookBlock {
namespace {

class Table final : public Placements {
public:
    Binding *Find(CKBehavior *block) const noexcept override {
        if (!block)
            return nullptr;
        const auto it = m_Entries.find(block->GetID());
        // CK_IDs are reused as soon as they are freed, so the address must
        // match too.
        return it != m_Entries.end() && it->second.Block == block
            ? it->second.Owner : nullptr;
    }

    void Add(CKBehavior *block, Binding *owner) {
        m_Entries[block->GetID()] = {block, owner};
    }

    void Remove(const Binding *owner) noexcept {
        std::erase_if(m_Entries, [owner](const auto &entry) {
            return entry.second.Owner == owner;
        });
    }

private:
    struct Entry {
        CKBehavior *Block = nullptr;
        Binding *Owner = nullptr;
    };

    std::unordered_map<CK_ID, Entry> m_Entries;
};

Table &Placed() {
    // Never destroyed: a Block's AppData may still point here while the
    // world is torn down at exit.
    static Table *table = new Table;
    return *table;
}

} // namespace

Binding::Binding(PlanCallbackState state, Callback callback, void *argument)
    : Binding(std::move(state), callback, argument, true) {}

Binding::Binding(PlanCallbackState state, Callback callback, void *argument,
                 bool ownsState)
    : m_State(std::move(state)), m_Lease(m_State.OpenLease()),
      m_Callback(callback), m_Argument(argument),
      m_OwnsState(ownsState) {}

Binding::~Binding() {
    CloseAdmission();
    (void) RetireAtSafePoint();
}

CallbackCall Binding::Invoke(const CKBehaviorContext *context) noexcept {
    if (!m_Callback)
        return {false, CKBR_OK, {}};
    CallbackCall call = InvokeCallback(
        m_Lease, CKBR_BEHAVIORERROR,
        [&] { return m_Callback(context, m_Argument); });
    if (call.Invoked && call.ReturnCode == CallbackFaulted) {
        call.Fault = {CallbackError::Exception,
                      "Behavior Hook callback reported a fault."};
    }
    if (call.Fault && !m_Diagnostic)
        m_Diagnostic = call.Fault;
    // A callback that ran but did not complete is a bug, not a decision. Keep
    // its first diagnostic and stop invoking this occurrence; the Hook Block
    // treats the faulted call as if no callback ran, the same way CK2 swallows
    // a native BB's exception instead of stopping the graph.
    if (call.Invoked && call.Fault)
        CloseAdmission();
    return call;
}

void Binding::CloseAdmission() noexcept {
    (void) m_Lease.CloseAdmission();
}

bool Binding::RetireAtSafePoint() noexcept {
    CloseAdmission();
    if (std::exchange(m_Placed, false))
        Placed().Remove(this);
    (void) m_Lease.Close();
    m_Lease = {};
    if (m_OwnsState)
        m_State.Retire();
    return !m_State.Retired() || m_State.Collect();
}

void *Binding::Place(CKBehavior *block) noexcept {
    if (!block)
        return nullptr;
    try {
        Placed().Add(block, this);
    } catch (...) {
        // Without an entry the Block stays transparent, as if closed.
        return nullptr;
    }
    m_Placed = true;
    return static_cast<Placements *>(&Placed());
}

CallbackLeaseState Binding::State() const noexcept {
    return m_Lease.State();
}

CallbackFault Binding::Diagnostic() const {
    return m_Diagnostic;
}

std::shared_ptr<Binding> Bind(Callback callback, void *argument) {
    return Bind(PlanCallbackState::Static(argument), callback, argument);
}

std::shared_ptr<Binding> Bind(PlanCallbackState state, Callback callback,
                              void *argument) {
    if (!callback)
        return {};
    return std::make_shared<Binding>(std::move(state), callback, argument);
}

std::shared_ptr<Binding> Hook::Bind() const {
    if (!m_Occurrence)
        return {};
    auto binding = std::shared_ptr<Binding>(new Binding(
        m_Occurrence->State, m_Occurrence->Function,
        m_Occurrence->Argument, false));
    return binding->State() == CallbackLeaseState::Open
        ? std::move(binding) : nullptr;
}

} // namespace BML::Behavior::Internal::HookBlock
