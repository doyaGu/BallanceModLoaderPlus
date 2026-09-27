#include "Behavior/Edit/Transaction.h"

#include "Behavior/Core/Hash.h"
#include "Behavior/Engine/Graph.h"
#include "Behavior/Engine/Message.h"
#include "Behavior/Runtime.h"

#include <algorithm>
#include <iterator>

namespace BML::Behavior::Internal {
namespace {

std::uint64_t HashSplice(const LinkBase &link, std::uint32_t ordinal,
                         std::uint32_t node) {
    std::uint64_t hash = Fnv::Offset;
    const auto add = [&](std::uint64_t value) { Fnv::Value(hash, value); };
    add(link.Anchor.Domain);
    add(link.Anchor.Slot);
    add(link.Anchor.Generation);
    add(ordinal);
    add(node);
    return hash;
}

std::uint64_t HashBind(const GraphEndpoint &pin, const CheckedBind &bind) {
    std::uint64_t hash = Fnv::Offset;
    const auto add = [&](std::uint64_t value) { Fnv::Value(hash, value); };
    add(pin.Node);
    add(static_cast<std::uint64_t>(pin.Kind));
    add(static_cast<std::uint32_t>(pin.Index));
    add(static_cast<std::uint64_t>(bind.Kind));
    add(bind.Ordinal);
    if (bind.Kind != BindKind::Literal) {
        add(bind.Source.Owner.Value);
        add(static_cast<std::uint64_t>(bind.Source.Slot.Kind));
        add(static_cast<std::uint32_t>(bind.Source.Slot.NativeIndex));
    }
    return hash;
}

} // namespace

CKEdit::Transaction::Transaction(CKEdit &editor, Patch::Journal &journal)
    : m_Editor(editor), m_Journal(journal), m_Context(editor.m_Context),
      m_Runtime(editor.m_Runtime), m_Source(editor.m_Graph) {}

Status CKEdit::Transaction::Apply(const Ops &edit) {
    m_Edit = &edit;
    Status status = Admit();
    if (!status)
        return status;
    // Each later phase journals what it changed before it continues, so a
    // failed phase reverts exactly the prefix that was applied.
    using Step = Status (Transaction::*)();
    static constexpr Step kSteps[] = {
        &Transaction::BorrowNodes,
        &Transaction::PinPorts,
        &Transaction::NoteDependencies,
        &Transaction::CheckValues,
        &Transaction::PrepareBlocks,
        &Transaction::CreateNodes,
        &Transaction::CreateTaps,
        &Transaction::AppendInterface,
        &Transaction::Reconcile,
        &Transaction::AddOperations,
        &Transaction::BindPorts,
        &Transaction::Replace,
        &Transaction::Bind,
        &Transaction::Push,
        &Transaction::NotifyAdded,
        &Transaction::Relink,
        &Transaction::Remove,
        &Transaction::Arrange,
        &Transaction::NotifyEdited,
        &Transaction::Reconnect,
        &Transaction::Flow,
        &Transaction::LinkTaps,
        &Transaction::PublishLayer,
        &Transaction::NotifyGraph,
        &Transaction::Set,
        &Transaction::Publish,
    };
    for (Step step : kSteps) {
        status = (this->*step)();
        if (!status)
            return Fail(std::move(status));
    }
    return {};
}

Status CKEdit::Transaction::Fail(Status failure) {
    Status reverted = Transaction(m_Editor, m_Journal).Revert();
    if (!reverted) {
        if (!failure.Message.empty())
            reverted.Message = failure.Message + " " + reverted.Message;
        return reverted;
    }
    return failure;
}

Status CKEdit::Transaction::Admit() {
    if (m_Edit->m_Nodes.empty())
        return Failure(Error::InvalidState, "The Edit has no graph.");

    m_Graph = ResolveBehavior(m_Context, m_Edit->m_Nodes.front().Native);
    if (!m_Graph || m_Graph->IsUsingFunction())
        return Failure(Error::InvalidGraphLocality,
                       "The Edit graph is stale or no longer graph-backed.",
                       CKERR_INVALIDOBJECT);
    m_GraphId = static_cast<std::uint32_t>(m_Graph->GetID());
    if (m_Editor.m_Active[m_GraphId].contains(m_Edit->Key()))
        return Failure(Error::InvalidState,
                       "The same owner and patch key is already active on this graph.");

    Status status = m_Source.Read(m_Edit->m_Nodes.front().Native,
                                  GraphView::Logical, m_Base);
    if (status)
        status = m_Edit->Validate(m_Base, m_Checked);
    if (!status)
        return status;
    if (m_Editor.m_StructuralEdits.contains(m_GraphId)) {
        return Failure(
            Error::SourceConflict,
            "This graph already has an active structural edit.");
    }
    if (!m_Checked.Replacements.empty() || !m_Checked.Removals.empty() ||
        !m_Checked.Reconnections.empty()) {
        const auto layered = m_Editor.m_Links->Patches.find(m_GraphId);
        if (layered != m_Editor.m_Links->Patches.end() && !layered->second.empty()) {
            return Failure(
                Error::SourceConflict,
                "A structural edit requires a graph without active Link overlays.");
        }
    }

    m_GraphTopology = &m_Editor.m_Topology[m_GraphId];
    m_GraphRelations = &m_Editor.m_Relations[m_GraphId];
    m_SpliceLayer.Patch = m_Edit->Key();
    m_SpliceLinks.reserve(m_Checked.Splices.size());
    for (const CheckedSplice &splice : m_Checked.Splices) {
        LinkId id;
        // Identify also re-checks a known anchor against its recorded base
        // endpoints and delay, so a foreign change to an idle spliced Link is
        // reported as GraphChanged here instead of as a RevertConflict later.
        status = m_GraphTopology->Identify(splice.Target, id);
        if (!status)
            return status;
        m_SpliceLinks.push_back(id);

        auto group = std::find_if(
            m_SpliceLayer.Links.begin(), m_SpliceLayer.Links.end(),
            [id](const LinkOverlays &candidate) { return candidate.Link == id; });
        if (group == m_SpliceLayer.Links.end()) {
            m_SpliceLayer.Links.push_back({id, splice.Ordering, {}});
            group = std::prev(m_SpliceLayer.Links.end());
        }
        group->Overlays.push_back(
            {OverlayKind::Splice, splice.Ordinal,
             HashSplice(splice.Target, splice.Ordinal,
                        splice.Input.Owner.Value)});
    }
    m_RedirectLinks.reserve(m_Checked.Redirects.size());
    for (const CheckedRedirect &redirect : m_Checked.Redirects) {
        LinkId id;
        status = m_GraphTopology->Identify(redirect.Target, id);
        if (!status)
            return status;
        m_RedirectLinks.push_back(id);

        auto group = std::find_if(
            m_SpliceLayer.Links.begin(), m_SpliceLayer.Links.end(),
            [id](const LinkOverlays &candidate) { return candidate.Link == id; });
        if (group == m_SpliceLayer.Links.end()) {
            m_SpliceLayer.Links.push_back({id, redirect.Ordering, {}});
            group = std::prev(m_SpliceLayer.Links.end());
        }
        // The new destination may be a Node this Edit has not created yet, so
        // the pre-check stands in the current sink and the authoritative
        // endpoint is written once the graph holds every Node.
        group->Overlays.push_back(
            {OverlayKind::Redirect, redirect.Ordinal,
             HashRedirect(redirect.Target, redirect.Ordinal,
                          redirect.Target.Sink),
             redirect.Target.Sink});
    }
    if (!m_SpliceLayer.Links.empty()) {
        status = m_GraphTopology->Validate(m_SpliceLayer);
        if (!status)
            return status;
    }

    m_RelationLayer.Patch = m_Edit->Key();
    for (const CheckedBind &bind : m_Checked.Binds) {
        if (bind.Target.Slot.Kind != SlotKind::InputParameter &&
            bind.Target.Slot.Kind != SlotKind::Target)
            continue;
        const Ops::EditNode *target = m_Edit->Find(bind.Target.Owner);
        if (!target || target->Authored() || bind.Target.Appended)
            continue;
        const GraphEndpoint pin{
            target->Native.Id, bind.Target.Slot.Kind,
            bind.Target.Slot.NativeIndex};
        m_RelationLayer.Pins.push_back(
            {pin, {}, {{RelationKind::Bind, bind.Ordinal,
                        HashBind(pin, bind)}}});
    }
    if (!m_RelationLayer.Pins.empty()) {
        status = m_GraphRelations->Validate(m_RelationLayer);
        if (!status)
            return status;
    }

    m_Journal.Editor = &m_Editor;
    m_Journal.Graph = Capture(m_Graph);
    m_Journal.GraphOwner = Capture(m_Graph->GetOwner());
    m_Journal.GraphParent = Capture(m_Graph->GetParent());
    m_Journal.Key = m_Edit->Key();
    status = CaptureOrder(m_Graph, m_Journal.BeforeOrder);
    if (!status)
        return status;
    m_Editor.AdoptGraph(m_Graph);
    if (!m_Checked.Replacements.empty() || !m_Checked.Removals.empty() ||
        !m_Checked.Reconnections.empty()) {
        m_Journal.StructuralEditClaim =
            m_Editor.m_StructuralEdits.insert(m_GraphId).second;
    }
    return {};
}

Status CKEdit::Transaction::BorrowNodes() {
    Status status;
    m_Handles.emplace(m_Edit->Graph().Value, m_Journal.Graph);
    for (std::size_t index = 1; index < m_Edit->m_Nodes.size(); ++index) {
        const Ops::EditNode &node = m_Edit->m_Nodes[index];
        if (node.Authored())
            continue;
        CKBehavior *native = ResolveBehavior(m_Context, node.Native);
        if (!native || native->GetParent() != m_Graph) {
            status = Failure(Error::InvalidGraphLocality,
                             "A borrowed Node left the target graph before Apply.",
                             CKERR_INVALIDOBJECT);
            break;
        }
        m_Handles.emplace(node.Handle.Value, Capture(native));
    }
    return status;
}

CKBehavior *CKEdit::Transaction::BehaviorFor(Node node) {
    const auto found = m_Handles.find(node.Value);
    return found == m_Handles.end()
        ? nullptr
        : Resolve<CKBehavior>(m_Context, found->second, CKCID_BEHAVIOR);
}

CKBehavior *CKEdit::Transaction::GraphFor() {
    return Resolve<CKBehavior>(m_Context, m_Journal.Graph, CKCID_BEHAVIOR);
}

Status CKEdit::Transaction::ValidateRemoved() {
    CKBehavior *currentGraph = GraphFor();
    if (!currentGraph)
        return Failure(Error::GraphChanged,
                       "The edited Behavior graph disappeared.",
                       CKERR_INVALIDOBJECT);
    for (const Patch::Journal::Removal &item : m_Journal.Removals) {
        if (!item.Removed)
            continue;
        CKBehavior *node = Resolve<CKBehavior>(
            m_Context, item.Node, CKCID_BEHAVIOR);
        if (!node || Engine::Contains(currentGraph, node)) {
            return Failure(
                Error::GraphChanged,
                "A removed Behavior Node changed identity during Apply.",
                CKERR_INVALIDOBJECT);
        }
    }
    for (const Patch::Journal::RemovedLink &item : m_Journal.RemovedLinks) {
        if (!item.Removed)
            continue;
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            m_Context, item.Value, CKCID_BEHAVIORLINK);
        CKBehaviorIO *source = ResolveIo(
            m_Context, item.SourceDetached
                ? m_Journal.DetachedSource : item.Source);
        CKBehaviorIO *sink = ResolveIo(
            m_Context, item.SinkDetached
                ? m_Journal.DetachedSink : item.Sink);
        if (!link || Engine::Contains(currentGraph, link) ||
            !source || !sink || link->GetInBehaviorIO() != source ||
            link->GetOutBehaviorIO() != sink ||
            link->GetInitialActivationDelay() != item.InitialDelay ||
            link->GetActivationDelay() != item.Delay) {
            return Failure(
                Error::GraphChanged,
                "A removed Behavior Link changed identity during Apply.",
                CKERR_INVALIDOBJECT);
        }
    }
    return {};
}

Status CKEdit::Transaction::ValidateNodes() {
    CKBehavior *currentGraph = GraphFor();
    if (!currentGraph || currentGraph->IsUsingFunction()) {
        return Failure(
            Error::GraphChanged,
            "The edited Behavior graph changed identity during a callback.",
            CKERR_INVALIDOBJECT);
    }
    CKBehavior *currentParent = currentGraph->GetParent();
    if (!IsSameObject(m_Context, currentGraph->GetOwner(),
                      m_Journal.GraphOwner, CKCID_BEOBJECT) ||
        !IsSameObject(m_Context, currentParent, m_Journal.GraphParent,
                      CKCID_BEHAVIOR) ||
        (currentParent && !Engine::Contains(currentParent, currentGraph))) {
        return Failure(
            Error::GraphChanged,
            "The edited Behavior graph changed owner or parent during a callback.",
            CKERR_INVALIDOBJECT);
    }
    for (const auto &[handle, stamp] : m_Handles) {
        CKBehavior *current = Resolve<CKBehavior>(
            m_Context, stamp, CKCID_BEHAVIOR);
        if (!current || (handle != m_Edit->Graph().Value &&
                         (current->GetParent() != currentGraph ||
                          !Engine::Contains(currentGraph, current) ||
                          !IsSameObject(m_Context, current->GetOwner(),
                                        m_Journal.GraphOwner,
                                        CKCID_BEOBJECT)))) {
            return Failure(
                Error::GraphChanged,
                "An Edit Node changed owner, parent, or identity during a callback.",
                CKERR_INVALIDOBJECT);
        }
    }
    for (Stamp stamp : m_Journal.Nodes) {
        CKBehavior *current = Resolve<CKBehavior>(
            m_Context, stamp, CKCID_BEHAVIOR);
        if (!current || current->GetParent() != currentGraph ||
            !Engine::Contains(currentGraph, current) ||
            !IsSameObject(m_Context, current->GetOwner(),
                          m_Journal.GraphOwner, CKCID_BEOBJECT)) {
            return Failure(
                Error::GraphChanged,
                "An Edit-owned Block changed owner, parent, or identity during a callback.",
                CKERR_INVALIDOBJECT);
        }
    }
    for (Stamp stamp : m_Journal.GraphNodes) {
        CKBehavior *current = Resolve<CKBehavior>(
            m_Context, stamp, CKCID_BEHAVIOR);
        if (!current || current->GetParent() != currentGraph ||
            !Engine::Contains(currentGraph, current) ||
            !IsSameObject(m_Context, current->GetOwner(),
                          m_Journal.GraphOwner, CKCID_BEOBJECT) ||
            current->IsUsingFunction()) {
            return Failure(
                Error::GraphChanged,
                "An Edit-owned graph Node changed owner, parent, or identity.",
                CKERR_INVALIDOBJECT);
        }
    }
    for (const Patch::Journal::Operation &item : m_Journal.Operations) {
        auto *operation = Resolve<CKParameterOperation>(
            m_Context, item.Value, CKCID_PARAMETEROPERATION);
        if (!operation || operation->GetOwner() != currentGraph) {
            return Failure(
                Error::GraphChanged,
                "An Edit-owned Parameter Operation changed identity during a callback.",
                CKERR_INVALIDOBJECT);
        }
    }
    return {};
}

Status CKEdit::Transaction::VisitPorts(const std::function<Status(ResolvedPort &)> &visitor) {
    for (CheckedFlow &flow : m_Checked.Flows) {
        Status current = visitor(flow.Source);
        if (current)
            current = visitor(flow.Sink);
        if (!current)
            return current;
    }
    for (CheckedBind &bind : m_Checked.Binds) {
        Status current = visitor(bind.Target);
        if (current && bind.Kind != BindKind::Literal)
            current = visitor(bind.Source);
        if (!current)
            return current;
    }
    for (CheckedSet &set : m_Checked.Sets) {
        Status current = visitor(set.Target);
        if (!current)
            return current;
    }
    for (CheckedPush &push : m_Checked.Pushes) {
        Status current = visitor(push.Source);
        if (current)
            current = visitor(push.Destination);
        if (!current)
            return current;
    }
    for (CheckedTap &tap : m_Checked.Taps) {
        Status current = visitor(tap.Source);
        if (!current)
            return current;
    }
    for (CheckedSplice &splice : m_Checked.Splices) {
        Status current = visitor(splice.Input);
        if (current)
            current = visitor(splice.Output);
        if (!current)
            return current;
    }
    for (CheckedRedirect &redirect : m_Checked.Redirects) {
        Status current = visitor(redirect.Sink);
        if (!current)
            return current;
    }
    for (CheckedReconnect &reconnect : m_Checked.Reconnections) {
        Status current = visitor(reconnect.Source);
        if (current)
            current = visitor(reconnect.Sink);
        if (!current)
            return current;
    }
    return {};
}

// Borrowed graph Ports exist before any added Block can run a lifecycle
// callback. Pin their exact CK identities now so a callback cannot replace
// a peer Port with a same-shaped object and redirect this Edit silently.
Status CKEdit::Transaction::PinPorts() {
    Status status = VisitPorts([&](ResolvedPort &port) -> Status {
        const Ops::EditNode *node = m_Edit->Find(port.Owner);
        if (!node || node->Block || port.Interface != 0 || port.Deferred ||
            port.Native)
            return {};
        CKBehavior *behavior = BehaviorFor(port.Owner);
        SlotInfo live;
        Status current = m_Runtime.Resolve(behavior, port.Selector, live);
        CKObject *object = current
            ? m_Runtime.ResolveSlotObject(behavior, live) : nullptr;
        if (!current || !object)
            return current ? Failure(
                Error::GraphChanged,
                "A selected graph Port disappeared before Apply.",
                CKERR_INVALIDOBJECT) : current;
        port.Native = Native(Capture(object));
        return {};
    });
    return status;
}

// An Edit may build only on the host graph and on what it introduces
// itself. An object another active Patch introduced is destroyed when that
// Patch closes, and nothing would revert what this Edit attached to it.
Status CKEdit::Transaction::NoteDependencies() {
    std::vector<Stamp> named{m_Journal.Graph};
    for (const auto &entry : m_Handles)
        named.push_back(entry.second);
    Status status = VisitPorts([&](ResolvedPort &port) -> Status {
        if (port.Native)
            named.push_back(Capture(port.Native));
        return {};
    });
    if (!status)
        return status;
    const auto anchor = [&](const LinkBase &target) {
        const auto link = std::find_if(
            m_Base.Links.begin(), m_Base.Links.end(),
            [&](const GraphLink &candidate) {
                return candidate.Object == target.Anchor;
            });
        if (link != m_Base.Links.end())
            named.push_back(Capture(
                m_Context->GetObject(static_cast<CK_ID>(link->Id))));
    };
    for (const CheckedSplice &splice : m_Checked.Splices)
        anchor(splice.Target);
    for (const CheckedRedirect &redirect : m_Checked.Redirects)
        anchor(redirect.Target);
    for (const CheckedReconnect &reconnect : m_Checked.Reconnections)
        anchor(reconnect.Target);
    m_Journal.Named = std::move(named);
    return {};
}

Status CKEdit::Transaction::ParameterBeforeApply(const ResolvedPort &port,
                                                 CKObject *&value) {
    value = nullptr;
    if (port.Operation)
        return {};
    const Ops::EditNode *node = m_Edit->Find(port.Owner);
    if (!node)
        return Failure(Error::InvalidState,
                       "An Edit data port names an unknown Node.");
    if (node->Block || port.Appended)
        return {};

    CKBehavior *behavior = BehaviorFor(port.Owner);
    if (!behavior)
        return Failure(Error::InvalidGraphLocality,
                       "An Edit data port left the target graph.",
                       CKERR_INVALIDOBJECT);
    SlotInfo slot;
    Status result = m_Runtime.Resolve(behavior, port.Selector, slot);
    if (!result)
        return result;
    switch (slot.Kind) {
    case SlotKind::InputParameter:
        value = behavior->GetInputParameter(slot.NativeIndex);
        break;
    case SlotKind::OutputParameter:
        value = behavior->GetOutputParameter(slot.NativeIndex);
        break;
    case SlotKind::Setting:
    case SlotKind::Local:
        value = behavior->GetLocalParameter(slot.NativeIndex);
        break;
    case SlotKind::Target:
        value = behavior->GetTargetParameter();
        break;
    default:
        break;
    }
    return value
        ? Status{}
        : Failure(Error::GraphChanged,
                  "An Edit data port disappeared before Apply.",
                  CKERR_INVALIDOBJECT);
}

bool CKEdit::Transaction::IsAdded(Node handle) {
    const Ops::EditNode *node = m_Edit->Find(handle);
    return node && node->Authored();
}

void CKEdit::Transaction::RememberEdited(CKBehavior *behavior) {
    if (!behavior || behavior == m_Graph)
        return;
    const Stamp stamp = Capture(behavior);
    if (std::find(m_Journal.EditedNodes.begin(), m_Journal.EditedNodes.end(),
                  stamp) == m_Journal.EditedNodes.end())
        m_Journal.EditedNodes.push_back(stamp);
}

void CKEdit::Transaction::RememberObserved(Stamp behavior) {
    if (std::find(m_Journal.ObservedEditedNodes.begin(),
                  m_Journal.ObservedEditedNodes.end(), behavior) ==
        m_Journal.ObservedEditedNodes.end()) {
        m_Journal.ObservedEditedNodes.push_back(behavior);
    }
}

Status CKEdit::Transaction::ExactSlot(CKBehavior *behavior, Stamp exact,
                                      SlotKind kind, SlotInfo &slot) {
    CKObject *object = m_Context && exact.Id
        ? m_Context->GetObject(exact.Id) : nullptr;
    if (!behavior || !object || object != exact.Address ||
        object->IsToBeDeleted()) {
        return Failure(Error::GraphChanged,
                       "A selected Behavior Port changed identity during Apply.",
                       CKERR_INVALIDOBJECT);
    }
    const Layout layout = m_Runtime.Describe(behavior);
    for (const SlotInfo &candidate : layout.Slots) {
        if (candidate.Kind == kind &&
            m_Runtime.ResolveSlotObject(behavior, candidate) == object) {
            slot = candidate;
            return {};
        }
    }
    return Failure(Error::GraphChanged,
                   "A selected Behavior Port left its owning Block during Apply.",
                   CKERR_INVALIDOBJECT);
}

Status CKEdit::Transaction::LiveSlot(const ResolvedPort &port, SlotInfo &slot) {
    CKBehavior *behavior = BehaviorFor(port.Owner);
    if (!behavior)
        return Failure(Error::GraphChanged,
                       "An Edit Node disappeared during Apply.",
                       CKERR_INVALIDOBJECT);
    Status resolved;
    if (port.Native) {
        resolved = ExactSlot(
            behavior, Capture(port.Native), port.Slot.Kind, slot);
    } else if (port.Interface != 0) {
        const auto found = m_InterfacePorts.find(port.Interface);
        resolved = found == m_InterfacePorts.end()
            ? Failure(Error::GraphChanged,
                      "An Edit interface Port was not created.",
                      CKERR_INVALIDOBJECT)
            : ExactSlot(behavior, found->second, port.Slot.Kind, slot);
    } else {
        resolved = m_Runtime.Resolve(behavior, port.Selector, slot);
    }
    if (!resolved)
        return resolved;
    const bool changedKind = slot.Kind != port.Slot.Kind;
    const bool changedType = port.Slot.Type.IsValid() &&
                             slot.Type != port.Slot.Type;
    const bool changedIndexedSlot = !port.Native && !port.Deferred &&
        port.Interface == 0 && !port.Selector.UsesName() &&
        !port.Selector.RequireOnly &&
        (slot.Name != port.Slot.Name ||
         slot.Occurrence != port.Slot.Occurrence);
    if (changedKind || changedType || changedIndexedSlot) {
        return Failure(
            Error::GraphChanged,
            "A selected Behavior port changed identity during Apply.");
    }
    return Status{};
}

// Added Blocks and Edit-declared interface Ports now exist. Bind every
// remaining symbolic endpoint to the exact CK object that it denotes.
Status CKEdit::Transaction::BindPorts() {
    Status status = VisitPorts([&](ResolvedPort &port) -> Status {
        if (port.Native)
            return {};
        if (port.Operation) {
            const auto found = m_OperationObjects.find(port.Owner.Value);
            auto *operation = found == m_OperationObjects.end()
                ? nullptr : Resolve<CKParameterOperation>(
                    m_Context, found->second, CKCID_PARAMETEROPERATION);
            CKObject *parameter = nullptr;
            CKGUID type;
            if (operation && port.Slot.Kind == SlotKind::InputParameter) {
                CKParameterIn *input = port.Slot.NativeIndex == 0
                    ? operation->GetInParameter1()
                    : operation->GetInParameter2();
                parameter = input;
                if (input)
                    type = input->GetGUID();
            } else if (operation &&
                       port.Slot.Kind == SlotKind::OutputParameter) {
                CKParameterOut *output = operation->GetOutParameter();
                parameter = output;
                if (output)
                    type = output->GetGUID();
            }
            if (!parameter || type != port.Slot.Type) {
                return Failure(
                    Error::GraphChanged,
                    "A Parameter Operation port does not match its declared type.",
                    CKERR_INVALIDOBJECT);
            }
            port.Native = Native(Capture(parameter));
            return {};
        }
        CKBehavior *behavior = BehaviorFor(port.Owner);
        SlotInfo live;
        Status current = LiveSlot(port, live);
        if (!current)
            return current;
        CKObject *object = m_Runtime.ResolveSlotObject(behavior, live);
        if (!object)
            return Failure(Error::GraphChanged,
                           "A selected Behavior Port disappeared before publication.",
                           CKERR_INVALIDOBJECT);
        port.Native = Native(Capture(object));
        if (port.Deferred)
            port.Slot = std::move(live);
        return {};
    });
    return status;
}

Status CKEdit::Transaction::ControlPort(const ResolvedPort &port,
                                        CKBehaviorIO *&io) {
    io = nullptr;
    CKBehavior *behavior = BehaviorFor(port.Owner);
    SlotInfo slot;
    Status result = LiveSlot(port, slot);
    if (!result)
        return result;
    io = slot.Kind == SlotKind::Input
        ? behavior->GetInput(slot.NativeIndex)
        : slot.Kind == SlotKind::Output
            ? behavior->GetOutput(slot.NativeIndex) : nullptr;
    return io ? Status{}
              : Failure(Error::GraphChanged,
                        "A control port disappeared during Apply.",
                        CKERR_INVALIDOBJECT);
}

Status CKEdit::Transaction::DataPort(const ResolvedPort &port,
                                     CKObject *&value) {
    value = nullptr;
    if (port.Native) {
        CKObject *native = m_Context->GetObject(
            static_cast<CK_ID>(port.Native.Id));
        if (!native || native != port.Native.Address || native->IsToBeDeleted() ||
            (!CKIsChildClassOf(native, CKCID_PARAMETER) &&
             !CKIsChildClassOf(native, CKCID_PARAMETERIN))) {
            return Failure(
                Error::GraphChanged,
                "An Edit data port changed identity during Apply.",
                CKERR_INVALIDOBJECT);
        }
        value = native;
        return {};
    }
    CKBehavior *behavior = BehaviorFor(port.Owner);
    SlotInfo slot;
    Status result = LiveSlot(port, slot);
    if (!result)
        return result;
    switch (slot.Kind) {
    case SlotKind::InputParameter:
        value = behavior->GetInputParameter(slot.NativeIndex);
        break;
    case SlotKind::OutputParameter:
        value = behavior->GetOutputParameter(slot.NativeIndex);
        break;
    case SlotKind::Setting:
    case SlotKind::Local:
        value = behavior->GetLocalParameter(slot.NativeIndex);
        break;
    case SlotKind::Target:
        value = behavior->GetTargetParameter();
        break;
    default:
        break;
    }
    return value ? Status{}
                 : Failure(Error::GraphChanged,
                           "A data port disappeared during Apply.",
                           CKERR_INVALIDOBJECT);
}

Status CKEdit::Transaction::ValidatePorts() {
    Status current = ValidateNodes();
    if (!current)
        return current;
    current = VisitPorts([&](ResolvedPort &port) -> Status {
        if (port.Operation) {
            CKObject *native = nullptr;
            return DataPort(port, native);
        }
        SlotInfo live;
        return LiveSlot(port, live);
    });
    if (!current)
        return current;
    for (const Patch::Journal::Interface &item : m_Journal.Ports) {
        CKBehavior *owner = Resolve<CKBehavior>(
            m_Context, item.Behavior, CKCID_BEHAVIOR);
        SlotInfo live;
        current = ExactSlot(owner, item.Port, item.Kind, live);
        if (!current)
            return current;
    }
    for (const Patch::Journal::Replacement &replacement :
         m_Journal.Replacements) {
        CKBehavior *currentGraph = GraphFor();
        CKBehavior *original = Resolve<CKBehavior>(
            m_Context, replacement.Original, CKCID_BEHAVIOR);
        CKBehavior *installed = Resolve<CKBehavior>(
            m_Context, replacement.Installed, CKCID_BEHAVIOR);
        if (!currentGraph || !original || !installed ||
            !IsSameObject(m_Context, original->GetOwner(),
                          m_Journal.GraphOwner, CKCID_BEOBJECT) ||
            !IsSameObject(m_Context, installed->GetOwner(),
                          m_Journal.GraphOwner, CKCID_BEOBJECT))
            return Failure(
                Error::GraphChanged,
                "A replacement Node changed owner or identity during Apply.",
                CKERR_INVALIDOBJECT);
        for (const auto &pair : replacement.Ports) {
            CKObject *oldPort = m_Context->GetObject(pair.Original.Id);
            CKObject *newPort = m_Context->GetObject(pair.Installed.Id);
            if (oldPort != pair.Original.Address ||
                newPort != pair.Installed.Address || !oldPort || !newPort ||
                oldPort->IsToBeDeleted() || newPort->IsToBeDeleted()) {
                return Failure(
                    Error::GraphChanged,
                    "A replacement public port changed identity during Apply.",
                    CKERR_INVALIDOBJECT);
            }
            const auto owns = [&](CKBehavior *behavior,
                                  CKObject *port) {
                const Layout layout = m_Runtime.Describe(behavior);
                return std::any_of(
                    layout.Slots.begin(), layout.Slots.end(),
                    [&](const SlotInfo &slot) {
                        return slot.Kind == pair.Kind &&
                            slot.Index == pair.Index &&
                            m_Runtime.ResolveSlotObject(behavior, slot) ==
                                port;
                    });
            };
            if (!owns(original, oldPort) || !owns(installed, newPort)) {
                return Failure(
                    Error::GraphChanged,
                    "A replacement public port left its Node during Apply.",
                    CKERR_INVALIDOBJECT);
            }
        }
    }
    for (const Patch::Journal::Reconnection &change :
         m_Journal.Reconnections) {
        if (!change.Applied || change.Reverted)
            continue;
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            m_Context, change.Link, CKCID_BEHAVIORLINK);
        if (!link || !Engine::Contains(GraphFor(), link) ||
            Capture(link->GetInBehaviorIO()) != change.InstalledSource ||
            Capture(link->GetOutBehaviorIO()) != change.InstalledSink ||
            link->GetInitialActivationDelay() != change.InitialDelay ||
            link->GetActivationDelay() != change.Delay) {
            return Failure(
                Error::GraphChanged,
                "A reconnected Link changed identity, endpoints, or delay during Apply.",
                CKERR_INVALIDOBJECT);
        }
    }
    return {};
}

Status CKEdit::Transaction::CaptureNormalization(Stamp receiver) {
    CKBehavior *behavior = Resolve<CKBehavior>(
        m_Context, receiver, CKCID_BEHAVIOR);
    if (!behavior || behavior->GetParent() != GraphFor()) {
        return Failure(
            Error::GraphChanged,
            "A Block changed identity during its EDITED callback.",
            CKERR_INVALIDOBJECT);
    }
    const std::uint64_t behaviorId = static_cast<std::uint32_t>(
        behavior->GetID());
    for (Patch::Journal::Written &change : m_Journal.Values) {
        if (change.Slot.Node != behaviorId)
            continue;
        CKParameter *stored = Resolve<CKParameter>(
            m_Context, change.Parameter, CKCID_PARAMETER);
        if (!stored) {
            return Failure(
                Error::GraphChanged,
                "An EDITED callback replaced a value written by the Patch.",
                CKERR_INVALIDOBJECT);
        }
        CKParameterLocal *expected = nullptr;
        Status copied = Parameter::Clone(m_Context, stored, expected);
        if (!copied)
            return copied;
        CKParameterLocal *previous = Resolve<CKParameterLocal>(
            m_Context, change.Expected, CKCID_PARAMETERLOCAL);
        change.Expected = Capture(expected);
        if (previous)
            m_Context->DestroyObject(previous);
    }
    for (Patch::Journal::Binding &change : m_Journal.Binds) {
        if (change.Pin.Node != behaviorId)
            continue;
        CKParameterIn *input = Resolve<CKParameterIn>(
            m_Context, change.Input, CKCID_PARAMETERIN);
        if (!input) {
            return Failure(
                Error::GraphChanged,
                "An EDITED callback replaced a Pin owned by the Patch.",
                CKERR_INVALIDOBJECT);
        }
        CKParameterIn *shared = input->GetSharedSource();
        CKParameter *direct = shared ? nullptr : input->GetDirectSource();
        change.InstalledShared = Capture(shared);
        change.InstalledDirect = Capture(direct);
        change.Expected = DescribeSource(input);
    }
    return {};
}

Status CKEdit::Transaction::ValidateRelations() {
    for (const Patch::Journal::Written &change : m_Journal.Values) {
        CKParameter *stored = Resolve<CKParameter>(
            m_Context, change.Parameter, CKCID_PARAMETER);
        CKParameterLocal *expected = Resolve<CKParameterLocal>(
            m_Context, change.Expected, CKCID_PARAMETERLOCAL);
        if (!stored || !expected) {
            return Failure(
                Error::GraphChanged,
                "A written value changed identity during an EDITED callback.",
                CKERR_INVALIDOBJECT);
        }
        bool equal = false;
        Status compared = Parameter::Equal(
            m_Context, stored, expected, equal);
        if (!compared)
            return compared;
        if (!equal) {
            return Failure(
                Error::GraphChanged,
                "A different Block changed a written value during an EDITED callback.");
        }
    }
    for (const Patch::Journal::Binding &change : m_Journal.Binds) {
        CKParameterIn *input = Resolve<CKParameterIn>(
            m_Context, change.Input, CKCID_PARAMETERIN);
        if (!input || Capture(input->GetSharedSource()) !=
                          change.InstalledShared ||
            Capture(input->GetSharedSource()
                        ? nullptr : input->GetDirectSource()) !=
                change.InstalledDirect) {
            return Failure(
                Error::GraphChanged,
                "A different Block changed a published Pin relation during an EDITED callback.",
                CKERR_INVALIDOBJECT);
        }
    }
    for (const Patch::Journal::Destination &change : m_Journal.Pushes) {
        CKParameterOut *source = Resolve<CKParameterOut>(
            m_Context, change.Source, CKCID_PARAMETEROUT);
        CKParameter *target = Resolve<CKParameter>(
            m_Context, change.Target, CKCID_PARAMETER);
        if (!source || !target || !ContainsDestination(source, target)) {
            return Failure(
                Error::GraphChanged,
                "A Pout destination changed during an EDITED callback.",
                CKERR_INVALIDOBJECT);
        }
    }
    for (const Patch::Journal::Replacement &replacement :
         m_Journal.Replacements) {
        for (const auto &change : replacement.Inputs) {
            if (!change.Applied)
                continue;
            CKParameterIn *input = Resolve<CKParameterIn>(
                m_Context, change.Input, CKCID_PARAMETERIN);
            CKParameterIn *shared = Resolve<CKParameterIn>(
                m_Context, change.InstalledShared, CKCID_PARAMETERIN);
            CKParameter *direct = Resolve<CKParameter>(
                m_Context, change.InstalledDirect, CKCID_PARAMETER);
            const bool unchanged = change.InstalledShared.Id
                ? input && input->GetSharedSource() == shared
                : input && input->GetSharedSource() == nullptr &&
                    input->GetDirectSource() == direct;
            if (!unchanged)
                return Failure(
                    Error::GraphChanged,
                    "A replacement Pin source changed during Apply.",
                    CKERR_INVALIDOBJECT);
        }
        for (const auto &change : replacement.Destinations) {
            if (!change.Applied)
                continue;
            CKParameterOut *oldOutput = Resolve<CKParameterOut>(
                m_Context, change.OriginalSource, CKCID_PARAMETEROUT);
            CKParameterOut *newOutput = Resolve<CKParameterOut>(
                m_Context, change.InstalledSource, CKCID_PARAMETEROUT);
            CKParameter *originalDestination = Resolve<CKParameter>(
                m_Context, change.OriginalParameter, CKCID_PARAMETER);
            CKParameter *installedDestination = Resolve<CKParameter>(
                m_Context, change.InstalledParameter, CKCID_PARAMETER);
            if (!oldOutput || !newOutput || !originalDestination ||
                !installedDestination ||
                ContainsDestination(oldOutput, originalDestination) ||
                !ContainsDestination(newOutput, installedDestination)) {
                return Failure(
                    Error::GraphChanged,
                    "A replacement Pout destination changed during Apply.",
                    CKERR_INVALIDOBJECT);
            }
        }
        for (const auto &change : replacement.Links) {
            if (!change.Applied)
                continue;
            CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                m_Context, change.Link, CKCID_BEHAVIORLINK);
            const auto removed = std::find_if(
                m_Journal.RemovedLinks.begin(), m_Journal.RemovedLinks.end(),
                [&](const Patch::Journal::RemovedLink &item) {
                    return !item.Restored && item.Value == change.Link;
                });
            const bool parked = removed != m_Journal.RemovedLinks.end();
            if (!link ||
                (parked &&
                 (removed->Source != change.InstalledSource ||
                  removed->Sink != change.InstalledSink)) ||
                (!parked &&
                 (Capture(link->GetInBehaviorIO()) !=
                      change.InstalledSource ||
                  Capture(link->GetOutBehaviorIO()) !=
                      change.InstalledSink))) {
                return Failure(
                    Error::GraphChanged,
                    "A replacement Link endpoint changed during Apply.",
                    CKERR_INVALIDOBJECT);
            }
        }
    }
    return {};
}

Status CKEdit::Transaction::ValidateEdited(Stamp receiver, bool captureFinal) {
    Status current = ValidatePorts();
    if (current && captureFinal)
        current = CaptureNormalization(receiver);
    if (current)
        current = ValidateRelations();
    return current;
}

// Finish each authored Block only after every graph parameter relation is
// present. Runtime sends the Block's one lifecycle EDITED callback and
// reflects the resulting layout before any control Flow can reach it.
Status CKEdit::Transaction::NotifyAdded() {
    Status status;
    for (const Ops::EditNode &node : m_Edit->m_Nodes) {
        if (!node.Block)
            continue;
        const auto behavior = m_Handles.find(node.Handle.Value);
        const auto spec = m_AddedSpecs.find(node.Handle.Value);
        if (behavior == m_Handles.end() || spec == m_AddedSpecs.end())
            return Failure(Error::InvalidState,
                           "An added Block disappeared before EDITED.");
        CKBehavior *live = Resolve<CKBehavior>(
            m_Context, behavior->second, CKCID_BEHAVIOR);
        if (!live)
            return Failure(Error::GraphChanged,
                           "An added Block disappeared before EDITED.",
                           CKERR_INVALIDOBJECT);
        status = m_Runtime.EditInGraph(live, spec->second);
        if (status)
            status = ValidateEdited(behavior->second, false);
        if (!status)
            return status;
        m_Graph = GraphFor();
    }
    return {};
}

Status CKEdit::Transaction::Arrange() {
    OrderAliases nodeAliases;
    OrderAliases portAliases;
    for (const Patch::Journal::Replacement &replacement :
         m_Journal.Replacements) {
        nodeAliases.emplace(replacement.Original.Id, replacement.Installed);
        for (const auto &port : replacement.Ports) {
            if (port.Kind == SlotKind::Output)
                portAliases.emplace(port.Original.Id, port.Installed);
        }
    }
    Status status = ArrangeOrder(m_Context, GraphFor(), m_Journal.BeforeOrder,
                                 nodeAliases, portAliases, false);
    if (!status)
        return status;
    status = ValidatePorts();
    if (status)
        status = ValidateRelations();
    if (status)
        status = ValidateRemoved();
    return status;
}

Status CKEdit::Transaction::NotifyEdited() {
    Status status;
    for (Stamp edited : m_Journal.EditedNodes) {
        CKBehavior *behavior = Resolve<CKBehavior>(
            m_Context, edited, CKCID_BEHAVIOR);
        if (!behavior)
            return Failure(Error::GraphChanged,
                           "A changed Block disappeared before EDITED.");
        RememberObserved(edited);
        const int result = Engine::Send(m_Context, behavior, CKM_BEHAVIOREDITED);
        if (result != CK_OK)
            return Failure(Error::CallbackFailed,
                           "A Block EDITED callback failed.", result);
        status = ValidateEdited(edited, true);
        if (!status)
            return status;
        m_Graph = GraphFor();
    }
    return {};
}

Status CKEdit::Transaction::NotifyGraph() {
    m_Journal.GraphObserved = true;
    const int edited = Engine::Send(m_Context, m_Graph, CKM_BEHAVIOREDITED);
    if (edited != CK_OK)
        return Failure(Error::CallbackFailed,
                       "The graph EDITED callback failed.", edited);
    Status status = ValidatePorts();
    if (status)
        status = ValidateRelations();
    if (status)
        status = ValidateRemoved();
    if (!status)
        return status;
    m_Graph = GraphFor();
    if (!m_Graph)
        return Failure(
            Error::GraphChanged,
            "The graph changed identity during its EDITED callback.",
            CKERR_INVALIDOBJECT);
    return {};
}

Status CKEdit::Transaction::Publish() {
    for (const Patch::Journal::Link &item : m_Journal.Links) {
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            m_Context, item.Value, CKCID_BEHAVIORLINK);
        if (!link || link->GetInBehaviorIO() == nullptr ||
            link->GetOutBehaviorIO() == nullptr) {
            return Failure(Error::GraphChanged,
                           "The graph changed while the Edit was published.");
        }
    }

    for (const auto &entry : m_Handles)
        m_Journal.Handles.emplace(entry.first, entry.second);

    Status status = CaptureOrder(m_Graph, m_Journal.AfterOrder);
    if (!status)
        return status;

    m_Editor.m_Active[m_GraphId].insert(m_Edit->Key());
    m_Journal.Published = true;
    return {};
}

} // namespace BML::Behavior::Internal
