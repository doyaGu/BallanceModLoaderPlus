#include "Behavior/Edit/Transaction.h"

#include "Behavior/Engine/Message.h"
#include "Behavior/Runtime.h"

#include <algorithm>

namespace BML::Behavior::Internal {

Status CKEdit::Transaction::AppendInterface() {
    for (const InterfacePort &item : m_Edit->m_Interface) {
        if (item.InBlockSpec)
            continue;
        CKBehavior *behavior = BehaviorFor(item.Owner);
        if (!behavior)
            return Failure(Error::InvalidState,
                           "A dynamic interface Node is unavailable.");

        CKObject *created = nullptr;
        switch (item.Slot.Kind) {
        case SlotKind::Input:
            created = behavior->CreateInput(
                const_cast<CKSTRING>(item.Slot.Name.c_str()));
            break;
        case SlotKind::Output:
            created = behavior->CreateOutput(
                const_cast<CKSTRING>(item.Slot.Name.c_str()));
            break;
        case SlotKind::InputParameter:
            created = behavior->CreateInputParameter(
                const_cast<CKSTRING>(item.Slot.Name.c_str()), item.Slot.Type);
            break;
        case SlotKind::OutputParameter:
            created = behavior->CreateOutputParameter(
                const_cast<CKSTRING>(item.Slot.Name.c_str()), item.Slot.Type);
            break;
        case SlotKind::Local:
            created = behavior->CreateLocalParameter(
                const_cast<CKSTRING>(item.Slot.Name.c_str()), item.Slot.Type);
            break;
        default:
            return Failure(Error::InterfaceUnsupported,
                           "This dynamic interface kind is not supported.");
        }
        if (!created)
            return Failure(Error::CreateFailed,
                           "Virtools failed to append a Behavior port.",
                           CKERR_OUTOFMEMORY);
        m_Journal.Ports.push_back(
            {item.Identity, Capture(behavior), Capture(created),
             item.Slot.Kind});
        m_InterfacePorts.emplace(item.Identity, Capture(created));
        if (!IsAdded(item.Owner)) {
            RememberEdited(behavior);
        }
    }
    return {};
}

// Most interface edits need only the final EDITED callback after their
// relations are installed. A callback-owned port is different: it does
// not exist until the BB has reconciled a related interface family. Only
// nodes with a still-unresolved deferred endpoint receive this preliminary
// callback; the final callback below remains mandatory.
Status CKEdit::Transaction::Reconcile() {
    std::vector<Stamp> reconciliationNeeded;
    Status status = VisitPorts([&](ResolvedPort &port) -> Status {
        if (!port.Deferred)
            return {};
        CKBehavior *behavior = BehaviorFor(port.Owner);
        if (!behavior) {
            return Failure(
                Error::GraphChanged,
                "A Block with a deferred port disappeared before reconciliation.",
                CKERR_INVALIDOBJECT);
        }
        SlotInfo live;
        Status current = m_Runtime.Resolve(behavior, port.Selector, live);
        if (current)
            return {};
        if (current.Code != Error::SlotNotFound)
            return current;
        const Stamp stamp = Capture(behavior);
        if (std::find(reconciliationNeeded.begin(),
                      reconciliationNeeded.end(), stamp) ==
            reconciliationNeeded.end()) {
            reconciliationNeeded.push_back(stamp);
        }
        return {};
    });
    if (!status)
        return status;

    for (Stamp edited : reconciliationNeeded) {
        CKBehavior *behavior = Resolve<CKBehavior>(
            m_Context, edited, CKCID_BEHAVIOR);
        if (!behavior)
            return Failure(
                Error::GraphChanged,
                "A Block disappeared before reconciling its variable interface.",
                CKERR_INVALIDOBJECT);
        RememberObserved(edited);
        const int result = Engine::Send(m_Context, behavior, CKM_BEHAVIOREDITED);
        if (result != CK_OK) {
            return Failure(
                Error::CallbackFailed,
                "A Block rejected its variable interface.", result);
        }
        status = ValidateNodes();
        if (!status)
            return status;
        for (const Patch::Journal::Interface &item : m_Journal.Ports) {
            CKBehavior *owner = Resolve<CKBehavior>(
                m_Context, item.Behavior, CKCID_BEHAVIOR);
            SlotInfo live;
            status = ExactSlot(owner, item.Port, item.Kind, live);
            if (!status)
                return status;
        }
        m_Graph = GraphFor();
    }
    return {};
}

} // namespace BML::Behavior::Internal
