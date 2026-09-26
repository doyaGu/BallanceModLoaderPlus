#ifndef BML_BEHAVIOR_EDIT_TRANSACTION_H
#define BML_BEHAVIOR_EDIT_TRANSACTION_H

#include <cstddef>
#include <functional>

#include "Behavior/Edit/Journal.h"

namespace BML::Behavior::Internal {

// One Apply or revert of a Patch journal. Apply runs the resolved Ops
// phase by phase and journals every native change before the next one,
// so a failed phase reverts exactly the applied prefix. Revert undoes
// the journal in reverse and records a RevertConflict instead of
// guessing when the graph changed under the Patch.
class CKEdit::Transaction final {
public:
    Transaction(CKEdit &editor, Patch::Journal &journal);

    Status Apply(const Ops &edit);
    Status Revert();

private:
    // Apply phases, in order.
    Status Admit();
    Status BorrowNodes();
    Status PinPorts();
    Status CheckValues();
    Status PrepareBlocks();
    Status CreateNodes();
    Status CreateTaps();
    Status AppendInterface();
    Status Reconcile();
    Status AddOperations();
    Status BindPorts();
    Status Replace();
    Status Bind();
    Status Push();
    Status NotifyAdded();
    Status Relink();
    Status Remove();
    Status Arrange();
    Status NotifyEdited();
    Status Reconnect();
    Status Flow();
    Status LinkTaps();
    Status PublishLayer();
    Status NotifyGraph();
    Status Set();
    Status Publish();
    Status Fail(Status failure);

    // Queries and checks shared by the Apply phases.
    CKBehavior *BehaviorFor(Node node);
    CKBehavior *GraphFor();
    Status ValidateRemoved();
    Status ValidateNodes();
    Status VisitPorts(const std::function<Status(ResolvedPort &)> &visitor);
    Status ParameterBeforeApply(const ResolvedPort &port, CKObject *&value);
    bool IsAdded(Node handle);
    void RememberEdited(CKBehavior *behavior);
    void RememberObserved(Stamp behavior);
    Status ExactSlot(CKBehavior *behavior, Stamp exact, SlotKind kind,
                     SlotInfo &slot);
    Status LiveSlot(const ResolvedPort &port, SlotInfo &slot);
    Status ControlPort(const ResolvedPort &port, CKBehaviorIO *&io);
    Status DataPort(const ResolvedPort &port, CKObject *&value);
    Status AddLink(CKBehaviorIO *source, CKBehaviorIO *sink, int delay,
                   Stamp *added = nullptr);
    Status ValidatePorts();
    Status CaptureNormalization(Stamp receiver);
    Status ValidateRelations();
    Status ValidateEdited(Stamp receiver, bool captureFinal);

    // Revert phases, in order.
    void RetireParked();
    Status CheckPublished();
    Status RetireLayer();
    void RevertPushes();
    void RevertValues();
    void RevertBinds();
    void RevertReconnections();
    void RestoreStructure();
    bool StructureRestored();
    void ReleaseDetached();
    void RetireOperations();
    void RetireLinks();
    void RetirePorts();
    void ReleaseClaims();
    void ReleaseLiterals();
    void RetireBlocks();
    void RetireGraphNodes();
    void Republish();
    void NotifyBlocksClosed();
    void NotifyGraphClosed();
    void ForgetGraph();
    bool RestoreOrder();
    void NoteConflict(RevertConflict conflict);
    void Remember(Status status);
    Status Conflict(RevertSubject subject, std::string message);

    CKEdit &m_Editor;
    Patch::Journal &m_Journal;
    CKContext *m_Context = nullptr;
    Runtime &m_Runtime;
    GraphSource &m_Source;

    // Apply state shared across phases.
    const Ops *m_Edit = nullptr;
    CKBehavior *m_Graph = nullptr;
    std::uint64_t m_GraphId = 0;
    GraphModel m_Base;
    CheckedOps m_Checked;
    Topology *m_GraphTopology = nullptr;
    Relations *m_GraphRelations = nullptr;
    PatchLayer m_SpliceLayer;
    std::vector<LinkId> m_SpliceLinks;
    std::vector<LinkId> m_RedirectLinks;
    RelationLayer m_RelationLayer;
    // Edit handles mapped to the native objects Apply borrowed or
    // created for them.
    std::unordered_map<std::uint32_t, Stamp> m_Handles;
    std::unordered_map<std::uint32_t, BlockSpec> m_AddedSpecs;
    std::unordered_map<std::uint32_t, Stamp> m_TapNodes;
    std::unordered_map<std::uint32_t, Stamp> m_InterfacePorts;
    std::unordered_map<std::uint32_t, Stamp> m_OperationObjects;
    std::unordered_map<CKObject *, CKObject *> m_ReplacementPorts;
    std::unordered_map<CKObject *, std::size_t> m_ReplacementOwners;

    // Revert keeps the first failure and still runs every later
    // inverse.
    Status m_First;
    bool m_OwnsLogicalGraph = false;
    bool m_PinsRetained = false;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_EDIT_TRANSACTION_H
