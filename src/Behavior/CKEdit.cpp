#include "Behavior/CKEdit.h"

#include "Behavior/Edit/Journal.h"
#include "Behavior/Edit/Transaction.h"
#include "Behavior/Runtime.h"

#include <algorithm>
#include <string>
#include <utility>

namespace BML::Behavior::Internal {
namespace {

struct PublishingScope {
    int &Depth;
    explicit PublishingScope(int &depth) : Depth(depth) { ++Depth; }
    ~PublishingScope() { --Depth; }
};

} // namespace

struct CKEdit::Request {
    enum class Kind {
        Apply,
        Close,
    };

    Kind Action = Kind::Apply;
    Ops Candidate;
    std::shared_ptr<Patch::Journal> Patch;
};

Patch::Patch() = default;
Patch::~Patch() = default;
Patch::Patch(Patch &&) noexcept = default;
Patch &Patch::operator=(Patch &&) noexcept = default;

Patch::operator bool() const noexcept {
    const PatchState state = State();
    return state == PatchState::Pending || state == PatchState::Active ||
           state == PatchState::Closing ||
           state == PatchState::Conflicted;
}

PatchState Patch::State() const noexcept {
    if (!m_Journal)
        return PatchState::Closed;
    return m_Journal->State;
}

Status Patch::Diagnostic() const {
    if (!m_Journal)
        return {};
    return m_Journal->LastStatus;
}

std::vector<RevertConflict> Patch::Conflicts() const {
    if (!m_Journal)
        return {};
    return m_Journal->Conflicts;
}

CKEdit::CKEdit(CKContext *context, Runtime &runtime,
               PrototypeCatalog *catalog, GraphSource &graph)
    : m_Context(context), m_Runtime(runtime), m_Catalog(catalog),
      m_Graph(graph), m_Thread(std::this_thread::get_id()),
      m_Links(std::make_unique<Links>()) {}

CKEdit::~CKEdit() = default;

void CKEdit::Queue(Request request) {
    m_Queue.push_back(std::move(request));
}

bool CKEdit::NeedsFrameProcessing() const noexcept {
    return !m_LostOverlays.empty() || !m_Queue.empty();
}

Status CKEdit::Ready() const {
    if (!m_Context)
        return Failure(Error::ContextExpired,
                       "Virtools context is unavailable.", CKERR_INVALIDOBJECT);
    if (m_Thread != std::this_thread::get_id())
        return Failure(Error::WrongThread,
                       "Behavior graph Edits require the game thread.");
    return {};
}

bool CKEdit::InDispatch() const noexcept {
    if (CallbackInvocation::Active())
        return true;
    CKBehaviorManager *manager = m_Context
        ? m_Context->GetBehaviorManager() : nullptr;
    return manager && manager->m_CurrentBehavior != nullptr;
}

bool CKEdit::Deferred() const noexcept {
    return InDispatch() || m_Processing || m_Publishing > 0;
}

bool CKEdit::CanPublish() const noexcept {
    return std::this_thread::get_id() == m_Thread && !Deferred();
}

Status CKEdit::Begin(CKBehavior *graph, PatchKey key, Ops &out) {
    Status status = Ready();
    if (!status)
        return status;
    NativeRef native;
    status = m_Graph.Refer(graph, native);
    if (status)
        AdoptGraph(graph);
    Layout layout;
    if (status)
        status = m_Graph.ReadLayout(native, layout);
    if (!status)
        return status;
    if (!graph || graph->IsUsingFunction())
        return Failure(Error::InvalidGraphLocality,
                       "Behavior Edit requires a graph-backed Behavior.");
    out = Ops(std::move(key), native, std::move(layout));
    return {};
}

Status CKEdit::Use(Ops &edit, CKBehavior *behavior, Node &out) {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    NativeRef native;
    status = m_Graph.Refer(behavior, native);
    Layout layout;
    if (status)
        status = m_Graph.ReadLayout(native, layout);
    if (!status)
        return status;
    out = edit.Use(native, std::move(layout));
    return {};
}

Status CKEdit::Use(Ops &edit, CKBehaviorLink *link, Link &out) {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    if (!link || edit.m_Nodes.empty())
        return Failure(Error::LinkNotFound,
                       "A native Link and Edit graph are required.");
    NativeRef native;
    status = m_Graph.Refer(link, native);
    GraphModel graph;
    if (status)
        status = m_Graph.Read(edit.m_Nodes.front().Native,
                              GraphView::Logical, graph);
    if (!status)
        return status;
    const auto found = std::find_if(
        graph.Links.begin(), graph.Links.end(),
        [&](const GraphLink &candidate) { return candidate.Id == native.Id; });
    if (found == graph.Links.end())
        return Failure(Error::LinkNotFound,
                       "The native Link does not belong to the Edit graph.");
    out = edit.Use(found->Object);
    return {};
}

Status CKEdit::Add(Ops &edit, BlockSpec block, Node &out, NodeRole role) {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    if (!m_Catalog || !m_Catalog->TracksRetirement())
        return Failure(Error::Unavailable,
                       "Behavior Prototype identity is unavailable.");
    Layout declared;
    status = m_Catalog->DeclaredLayout(
        {block.Prototype(), block.PrototypeGeneration()}, declared);
    if (!status)
        return status;
    if (!block.PrototypeGeneration())
        block.PrototypeGeneration(declared.ProviderGeneration);
    out = edit.Add(std::move(block), std::move(declared), role);
    return {};
}

Status CKEdit::AddGraph(Ops &edit, std::string name, int priority,
                        Node &out, NodeRole role) {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    if (name.empty())
        return Failure(Error::InvalidState,
                       "A graph-backed Node requires a name.");
    out = edit.AddGraph(std::move(name), priority, role);
    return {};
}


Status CKEdit::ResolveNode(const Patch &patch, Node handle,
                           CKBehavior *&out) const {
    out = nullptr;
    if (!handle)
        return Failure(Error::InvalidState, "An Edit handle names no Node.");
    if (!patch.m_Journal)
        return Failure(Error::InvalidState, "The Patch is closed.");
    const Patch::Journal &journal = *patch.m_Journal;
    if (journal.State == PatchState::Pending)
        return Failure(Error::Busy,
                       "The Patch reaches its graph at the next safe point.",
                       CK_OK);
    if (journal.State != PatchState::Active)
        return Failure(Error::InvalidState,
                       "Only an active Patch names live Nodes.");
    const auto found = journal.Handles.find(handle.Value);
    if (found == journal.Handles.end())
        return Failure(Error::QueryNotFound,
                       "This Edit handle names no Node of the Patch.");
    CKBehavior *native = Resolve<CKBehavior>(
        m_Context, found->second, CKCID_BEHAVIOR);
    if (!native)
        return Failure(Error::GraphChanged,
                       "The Node this Edit handle named is gone.");
    out = native;
    return {};
}

Status CKEdit::ResolvePort(const Patch &patch, Port port,
                           CKBehavior *&behavior, SlotInfo &slot) const {
    behavior = nullptr;
    slot = SlotInfo();
    if (!port)
        return Failure(Error::InvalidState, "An Edit handle names no Port.");

    Status status = ResolveNode(patch, Node{port.Owner}, behavior);
    if (!status)
        return status;

    if (port.Interface == 0)
        return m_Runtime.Resolve(behavior, port.Selector, slot);

    if (!patch.m_Journal)
        return Failure(Error::InvalidState, "The Patch is closed.");
    const Patch::Journal &journal = *patch.m_Journal;
    const auto found = std::find_if(
        journal.Ports.begin(), journal.Ports.end(),
        [&](const Patch::Journal::Interface &item) {
            return item.Identity == port.Interface &&
                item.Behavior.Id == behavior->GetID() &&
                item.Behavior.Address == behavior;
        });
    if (found == journal.Ports.end())
        return Failure(Error::GraphChanged,
                       "The appended Port this Edit handle named is gone.");
    CKObject *object = Resolve<CKObject>(m_Context, found->Port, CKCID_OBJECT);
    if (!object)
        return Failure(Error::GraphChanged,
                       "The appended Port this Edit handle named is gone.");

    int index = -1;
    CKGUID type;
    switch (found->Kind) {
    case SlotKind::InputParameter: {
        auto *parameter = CKParameterIn::Cast(object);
        index = behavior->GetInputParameterPosition(parameter);
        if (parameter)
            type = parameter->GetGUID();
        break;
    }
    case SlotKind::OutputParameter: {
        auto *parameter = CKParameterOut::Cast(object);
        index = behavior->GetOutputParameterPosition(parameter);
        if (parameter)
            type = parameter->GetGUID();
        break;
    }
    case SlotKind::Local: {
        auto *parameter = CKParameterLocal::Cast(object);
        index = behavior->GetLocalParameterPosition(parameter);
        if (parameter)
            type = parameter->GetGUID();
        break;
    }
    default:
        return Failure(Error::SlotNotFound,
                       "Only parameter Ports expose installation values.");
    }
    if (index < 0)
        return Failure(Error::GraphChanged,
                       "The appended parameter changed identity.");
    slot.Kind = found->Kind;
    slot.Index = index;
    slot.NativeIndex = index;
    slot.Name = object->GetName() ? object->GetName() : "";
    slot.Type = type;
    return {};
}

Status CKEdit::ReadValue(const Patch &patch, Port port, GraphValue &out) const {
    out = {};
    CKBehavior *behavior = nullptr;
    SlotInfo slot;
    Status status = ResolvePort(patch, std::move(port), behavior, slot);
    if (!status)
        return status;
    return m_Graph.ReadValue(
        {static_cast<std::uint64_t>(static_cast<std::uint32_t>(behavior->GetID())),
         behavior},
        0, Slot::At(slot.Kind, slot.NativeIndex, slot.Type),
        ReadMode::NonForcing, out);
}

Status CKEdit::WriteValue(const Patch &patch, Port port,
                          const Parameter::Binding &value) const {
    CKBehavior *behavior = nullptr;
    SlotInfo slot;
    Status status = ResolvePort(patch, std::move(port), behavior, slot);
    if (!status)
        return status;

    CKParameter *parameter = nullptr;
    switch (slot.Kind) {
    case SlotKind::InputParameter: {
        CKParameterIn *input = behavior->GetInputParameter(slot.NativeIndex);
        parameter = input ? input->GetRealSource() : nullptr;
        break;
    }
    case SlotKind::Target: {
        CKParameterIn *input = behavior->GetTargetParameter();
        parameter = input ? input->GetRealSource() : nullptr;
        break;
    }
    case SlotKind::OutputParameter:
        parameter = behavior->GetOutputParameter(slot.NativeIndex);
        break;
    case SlotKind::Setting:
    case SlotKind::Local:
        parameter = behavior->GetLocalParameter(slot.NativeIndex);
        break;
    default:
        return Failure(Error::SlotNotFound,
                       "Only Target, Pin, Pout, Setting, or Local values can be written.");
    }
    if (!parameter || CKParameterOperation::Cast(parameter->GetOwner())) {
        return Failure(
            Error::SourceInvalid,
            "An installation value requires a stored parameter behind the selected Port.");
    }
    return Parameter::Write(m_Context, parameter, value);
}

Status CKEdit::ApplyNow(const Ops &edit,
                        const std::shared_ptr<Patch::Journal> &patch) {
    Status status = Ready();
    if (!status)
        return status;
    if (!patch)
        return Failure(Error::InvalidState,
                       "The Patch journal is unavailable.");
    PublishingScope publishing(m_Publishing);
    return Transaction(*this, *patch).Apply(edit);
}

void CKEdit::CloseAdmission(Patch &patch) noexcept {
    if (patch.m_Journal)
        CloseAdmission(*patch.m_Journal);
}

void CKEdit::CloseAdmission(Patch::Journal &patch) noexcept {
    for (const std::shared_ptr<CallbackResource> &callback : patch.Callbacks) {
        if (callback)
            callback->CloseAdmission();
    }
}

Status CKEdit::Apply(const Ops &edit, Patch &out,
                     std::shared_ptr<const CallbackAdmission> admission) {
    Status status = Ready();
    if (!status)
        return status;
    if (out)
        return Failure(Error::InvalidState,
                       "The destination Patch is already live.");

    auto patch = std::make_shared<Patch::Journal>();
    patch->Editor = this;
    patch->Key = edit.Key();
    for (const Ops::EditNode &node : edit.m_Nodes) {
        if (!node.Block)
            continue;
        patch->Callbacks.insert(patch->Callbacks.end(),
                                node.Block->Callbacks().begin(),
                                node.Block->Callbacks().end());
    }
    for (const EditTap &tap : edit.m_Taps) {
        if (tap.Callback)
            patch->Callbacks.push_back(tap.Callback);
    }
    // Attach before ApplyNow can run native lifecycle callbacks. This also
    // guards journals not yet visible in their aggregate owner's registry.
    for (const auto &callback : patch->Callbacks) {
        if (callback)
            callback->AdmitThrough(admission);
    }
    if (admission && !admission->IsOpen())
        return Failure(Error::InvalidState, "Behavior Patch admission is closed.");
    if (Deferred()) {
        if (edit.m_Nodes.empty())
            return Failure(Error::InvalidState, "The Edit has no graph.");
        CKBehavior *graph = ResolveBehavior(
            m_Context, edit.m_Nodes.front().Native);
        if (!graph || graph->IsUsingFunction())
            return Failure(Error::InvalidGraphLocality,
                           "The Edit graph is stale or no longer graph-backed.",
                           CKERR_INVALIDOBJECT);
        const std::uint64_t graphId =
            static_cast<std::uint32_t>(graph->GetID());
        if (m_Active[graphId].contains(edit.Key()))
            return Failure(
                Error::InvalidState,
                "The same owner and patch key is already active on this graph.");
        GraphModel base;
        status = m_Graph.Read(edit.m_Nodes.front().Native,
                              GraphView::Logical, base);
        CheckedOps checked;
        if (status)
            status = edit.Validate(base, checked);
        if (!status)
            return status;
        patch->Queued = true;
        out.m_Journal = patch;
        Queue({Request::Kind::Apply, edit, patch});
        Status queued;
        queued.Message = "Behavior Patch application is queued for the game-thread safe point.";
        queued.Details.Stage = Phase::Edit;
        return queued;
    }

    status = ApplyNow(edit, patch);
    patch->LastStatus = status;
    patch->State = status
        ? PatchState::Active
        : status.Code == Error::RevertConflict
            ? PatchState::Conflicted : PatchState::Failed;
    if (!status && status.Code != Error::RevertConflict)
        patch->Callbacks.clear();
    if (status || status.Code == Error::RevertConflict)
        out.m_Journal = std::move(patch);
    return status;
}

Status CKEdit::CloseNow(const std::shared_ptr<Patch::Journal> &patch) {
    if (!patch)
        return {};
    CloseAdmission(*patch);
    // The Patch is Closing for the whole inverse, so a native teardown
    // callback that closes it again is answered Busy instead of starting a
    // second revert under the first.
    patch->State = PatchState::Closing;
    Status status;
    {
        PublishingScope publishing(m_Publishing);
        status = Transaction(*this, *patch).Revert();
    }
    patch->LastStatus = status;
    patch->State = status.Code == Error::RevertConflict
        ? PatchState::Conflicted : PatchState::Closed;
    if (status.Code != Error::RevertConflict)
        patch->Callbacks.clear();
    return status;
}

Status CKEdit::Close(Patch &patch) {
    if (!patch.m_Journal)
        return {};
    const std::shared_ptr<Patch::Journal> data = patch.m_Journal;
    if (data->Editor != this)
        return Failure(Error::InvalidState,
                       "The Patch belongs to another Behavior editor.");

    const PatchState state = data->State;
    if (state == PatchState::Closed || state == PatchState::Failed) {
        patch.m_Journal.reset();
        return {};
    }

    Status status = Ready();
    if (!status)
        return status;
    CloseAdmission(*data);
    if (Deferred() || state == PatchState::Pending) {
        data->State = PatchState::Closing;
        if (!data->Queued) {
            data->Queued = true;
            Queue({Request::Kind::Close, {}, data});
        }
        Status queued;
        queued.Message = "Behavior Patch close is queued for the game-thread safe point.";
        queued.Details.Stage = Phase::Teardown;
        return queued;
    }

    status = CloseNow(data);
    if (status.Code != Error::RevertConflict)
        patch.m_Journal.reset();
    return status;
}

void CKEdit::GraphDeleted(Patch &patch) {
    const std::shared_ptr<Patch::Journal> journal = patch.m_Journal;
    if (!journal)
        return;
    CloseAdmission(*journal);
    journal->State = PatchState::Closed;
    journal->Queued = false;
    journal->Callbacks.clear();
    patch.m_Journal.reset();
}

bool CKEdit::OwnsAny(const Patch &patch,
                     const std::set<CK_ID> &objects) const {
    const std::shared_ptr<Patch::Journal> journal = patch.m_Journal;
    if (!journal || objects.empty())
        return false;
    const auto contains = [&](Stamp object) {
        return object.Id != 0 && objects.contains(object.Id);
    };
    if (std::any_of(journal->Nodes.begin(), journal->Nodes.end(), contains) ||
        std::any_of(journal->GraphNodes.begin(), journal->GraphNodes.end(),
                    contains) ||
        std::any_of(journal->InfrastructureNodes.begin(),
                    journal->InfrastructureNodes.end(), contains) ||
        std::any_of(journal->InfrastructureLinks.begin(),
                    journal->InfrastructureLinks.end(), contains) ||
        std::any_of(journal->Links.begin(), journal->Links.end(),
                    [&](const Patch::Journal::Link &link) {
                        return contains(link.Value);
                    }) ||
        std::any_of(journal->Ports.begin(), journal->Ports.end(),
                    [&](const Patch::Journal::Interface &port) {
                        return contains(port.Port);
                    }) ||
        std::any_of(journal->Operations.begin(), journal->Operations.end(),
                    [&](const Patch::Journal::Operation &operation) {
                        return contains(operation.Value);
                    }) ||
        contains(journal->DetachedSource) || contains(journal->DetachedSink)) {
        return true;
    }
    return std::any_of(
        journal->Binds.begin(), journal->Binds.end(),
        [&](const Patch::Journal::Binding &binding) {
            return contains(binding.Literal);
        });
}

void CKEdit::InstallationDeleted(Patch &patch) {
    const std::shared_ptr<Patch::Journal> journal = patch.m_Journal;
    if (!journal)
        return;
    CloseAdmission(*journal);
    const std::uint64_t graphId =
        static_cast<std::uint32_t>(journal->Graph.Id);
    const auto topology = m_Topology.find(graphId);
    if (topology != m_Topology.end())
        (void) topology->second.Remove(journal->Key);
    if (m_Links) {
        auto &lost = m_LostOverlays[graphId];
        for (const auto &[link, ordinal] : journal->Splices) {
            (void) link;
            lost.emplace(journal->Key, ordinal);
        }
        for (const auto &[link, ordinal] : journal->Redirects) {
            (void) link;
            lost.emplace(journal->Key, ordinal);
        }
        const auto infrastructure = m_Links->Patches.find(graphId);
        if (infrastructure != m_Links->Patches.end())
            infrastructure->second.erase(journal->Key);
    }
    const auto relations = m_Relations.find(graphId);
    if (relations != m_Relations.end())
        (void) relations->second.Remove(journal->Key);
    const auto active = m_Active.find(graphId);
    if (active != m_Active.end())
        active->second.erase(journal->Key);
    if (journal->StructuralEditClaim)
        m_StructuralEdits.erase(graphId);
    journal->State = PatchState::Closed;
    journal->Queued = false;
    journal->Callbacks.clear();
    patch.m_Journal.reset();
}

void CKEdit::ObjectsToBeDeleted(const CK_ID *ids, int count) {
    if (!ids || count <= 0 || !Ready())
        return;
    const std::set<CK_ID> deleting(ids, ids + count);

    for (auto request = m_Queue.begin(); request != m_Queue.end();) {
        const auto &journal = request->Patch;
        if (!journal || !deleting.contains(journal->Graph.Id)) {
            ++request;
            continue;
        }
        CloseAdmission(*journal);
        journal->State = PatchState::Closed;
        journal->Queued = false;
        journal->Callbacks.clear();
        request = m_Queue.erase(request);
    }

    for (CK_ID id : deleting) {
        const std::uint64_t graphId = static_cast<std::uint32_t>(id);
        if (m_Links) {
            const auto root = m_Links->Roots.find(graphId);
            if (root != m_Links->Roots.end())
                m_Graph.SetLogicalGraph(Native(root->second), {});
        }
        m_Topology.erase(graphId);
        m_Relations.erase(graphId);
        m_Active.erase(graphId);
        m_LostOverlays.erase(graphId);
        m_StructuralEdits.erase(graphId);
        if (m_Links) {
            m_Links->Chains.erase(graphId);
            m_Links->Sites.erase(graphId);
            m_Links->Patches.erase(graphId);
            m_Links->Roots.erase(graphId);
        }
    }
}

void CKEdit::ProcessFrame() {
    if (!Ready() || Deferred())
        return;
    struct ProcessingScope {
        bool &Flag;
        explicit ProcessingScope(bool &flag) : Flag(flag) { Flag = true; }
        ~ProcessingScope() { Flag = false; }
    } processing(m_Processing);

    if (!m_Links)
        return;
    for (auto graph = m_LostOverlays.begin();
         graph != m_LostOverlays.end();) {
        const std::uint64_t graphId = graph->first;
        const auto root = m_Links->Roots.find(graphId);
        CKBehavior *native = root != m_Links->Roots.end()
            ? Resolve<CKBehavior>(m_Context, root->second, CKCID_BEHAVIOR)
            : nullptr;
        if (!native) {
            graph = m_LostOverlays.erase(graph);
            continue;
        }
        Status repaired = Materialize(graphId, native);
        if (!repaired) {
            ++graph;
            continue;
        }
        auto sites = m_Links->Sites.find(graphId);
        if (sites != m_Links->Sites.end()) {
            for (const auto &key : graph->second)
                sites->second.erase({key.first, key.second});
        }
        (void) PublishLogicalGraph(graphId);
        graph = m_LostOverlays.erase(graph);
    }

    // Requests queued while this batch runs wait for the next safe point.
    std::vector<Request> requests;
    requests.swap(m_Queue);
    for (Request &request : requests) {
        const std::shared_ptr<Patch::Journal> &patch = request.Patch;
        if (!patch)
            continue;
        patch->Queued = false;
        const PatchState state = patch->State;

        if (request.Action == Request::Kind::Apply) {
            if (state == PatchState::Closing || patch.use_count() == 1) {
                patch->State = PatchState::Closed;
                continue;
            }
            if (state != PatchState::Pending)
                continue;
            Status status = ApplyNow(request.Candidate, patch);
            bool close = false;
            patch->LastStatus = status;
            close = patch->State == PatchState::Closing;
            if (!close) {
                patch->State = status
                    ? PatchState::Active
                    : status.Code == Error::RevertConflict
                        ? PatchState::Conflicted : PatchState::Failed;
                if (!status && status.Code != Error::RevertConflict)
                    patch->Callbacks.clear();
            }
            if (close) {
                if (status)
                    (void) CloseNow(patch);
                else {
                    patch->State = PatchState::Closed;
                    patch->Callbacks.clear();
                }
            }
            continue;
        }

        if (state == PatchState::Closing)
            (void) CloseNow(patch);
    }
}

std::uint64_t CKEdit::TopologyFingerprint(CKBehavior *graph) const {
    if (!graph)
        return 0;
    const std::uint64_t graphId =
        static_cast<std::uint32_t>(graph->GetID());
    const auto active = m_Active.find(graphId);
    if (active == m_Active.end() || active->second.empty())
        return 0;
    const auto found = m_Topology.find(graphId);
    return found == m_Topology.end() ? 0 : found->second.Fingerprint();
}

} // namespace BML::Behavior::Internal
