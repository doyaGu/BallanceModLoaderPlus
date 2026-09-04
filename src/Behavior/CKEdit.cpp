#include "Behavior/CKEdit.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace BML::Behavior {
namespace {

Status Failure(Error error, std::string message,
               CKERROR ckError = CKERR_INVALIDPARAMETER) {
    Status status{error, ckError, CKBR_PARAMETERERROR, std::move(message)};
    status.Details.Stage = Phase::Edit;
    return status;
}

struct PublishingScope {
    int &Depth;
    explicit PublishingScope(int &depth) : Depth(depth) { ++Depth; }
    ~PublishingScope() { --Depth; }
};

struct Stamp {
    CK_ID Id = 0;
    CKObject *Address = nullptr;

    friend bool operator==(const Stamp &, const Stamp &) = default;
};

Stamp Capture(CKObject *object) {
    return {object ? object->GetID() : 0, object};
}

NativeRef Native(Stamp stamp) {
    return {static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(stamp.Id)),
            stamp.Address};
}

template <typename T>
T *Resolve(CKContext *context, Stamp stamp, CK_CLASSID type) {
    CKObject *object = context && stamp.Id ? context->GetObject(stamp.Id) : nullptr;
    if (object != stamp.Address || !object || object->IsToBeDeleted() ||
        !CKIsChildClassOf(object, type))
        return nullptr;
    return static_cast<T *>(object);
}

CKBehavior *ResolveBehavior(CKContext *context, NativeRef native) {
    if (!context || !native ||
        native.Id > static_cast<std::uint64_t>((std::numeric_limits<CK_ID>::max)()))
        return nullptr;
    CKObject *object = context->GetObject(static_cast<CK_ID>(native.Id));
    if (object != native.Address || !object || object->IsToBeDeleted() ||
        !CKIsChildClassOf(object, CKCID_BEHAVIOR))
        return nullptr;
    return static_cast<CKBehavior *>(object);
}

bool ContainsDestination(CKParameterOut *source, CKParameter *destination) {
    if (!source || !destination)
        return false;
    for (int index = 0; index < source->GetDestinationCount(); ++index) {
        if (source->GetDestination(index) == destination)
            return true;
    }
    return false;
}

using ParameterId = std::uint64_t;

constexpr ParameterId kPlannedParameter = 1ull << 63u;

ParameterId PlanParameter(const ResolvedPort &port) {
    return kPlannedParameter |
           (static_cast<ParameterId>(port.Owner.Value) << 32u) |
           (static_cast<ParameterId>(port.Slot.Kind) << 24u) |
           static_cast<std::uint32_t>(port.Slot.NativeIndex);
}

ParameterId NativeParameter(CKObject *parameter) {
    return parameter
        ? static_cast<std::uint32_t>(parameter->GetID()) : 0;
}

std::uint64_t HashTap(const GraphEndpoint &source, std::uint32_t ordinal) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto add = [&](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= static_cast<unsigned char>(value >> (byte * 8));
            hash *= 1099511628211ull;
        }
    };
    add(source.Node);
    add(static_cast<std::uint64_t>(source.Kind));
    add(static_cast<std::uint32_t>(source.Index));
    add(ordinal);
    return hash;
}

std::uint64_t HashSplice(const LinkBase &link, std::uint32_t ordinal,
                         std::uint32_t node) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto add = [&](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= static_cast<unsigned char>(value >> (byte * 8));
            hash *= 1099511628211ull;
        }
    };
    add(link.Anchor.Domain);
    add(link.Anchor.Slot);
    add(link.Anchor.Generation);
    add(ordinal);
    add(node);
    return hash;
}

std::uint64_t HashRedirect(const LinkBase &link, std::uint32_t ordinal,
                           const GraphEndpoint &sink) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto add = [&](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= static_cast<unsigned char>(value >> (byte * 8));
            hash *= 1099511628211ull;
        }
    };
    add(link.Anchor.Domain);
    add(link.Anchor.Slot);
    add(link.Anchor.Generation);
    add(ordinal);
    add(sink.Node);
    add(static_cast<std::uint64_t>(sink.Kind));
    add(static_cast<std::uint32_t>(sink.Index));
    return hash;
}

std::uint64_t HashBind(const GraphEndpoint &pin, const CheckedBind &bind) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto add = [&](std::uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= static_cast<unsigned char>(value >> (byte * 8));
            hash *= 1099511628211ull;
        }
    };
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

CKBehaviorIO *ResolveIo(CKContext *context, Stamp stamp) {
    CKObject *object = context && stamp.Id ? context->GetObject(stamp.Id) : nullptr;
    if (object != stamp.Address || !object || object->IsToBeDeleted())
        return nullptr;
    return static_cast<CKBehaviorIO *>(object);
}

bool Describe(CKBehaviorIO *io, GraphEndpoint &out) {
    out = {};
    CKBehavior *owner = io ? io->GetOwner() : nullptr;
    if (!owner)
        return false;
    out.Node = static_cast<std::uint32_t>(owner->GetID());
    out.Kind = SlotKind::Output;
    out.Index = owner->GetOutputPosition(io);
    if (out.Index < 0) {
        out.Kind = SlotKind::Input;
        out.Index = owner->GetInputPosition(io);
    }
    return out.Index >= 0;
}

bool Describe(CKBehaviorLink *link, GraphLinkShape &out) {
    out = {};
    return link && Describe(link->GetInBehaviorIO(), out.Source) &&
           Describe(link->GetOutBehaviorIO(), out.Target) &&
           (out.InitialDelay = link->GetInitialActivationDelay(), true);
}

PinSource DescribeSource(CKParameterIn *input) {
    if (!input)
        return {};
    if (CKParameterIn *shared = input->GetSharedSource()) {
        return {PinSourceKind::Shared,
                static_cast<std::uint32_t>(shared->GetID())};
    }
    if (CKParameter *direct = input->GetDirectSource()) {
        return {PinSourceKind::Direct,
                static_cast<std::uint32_t>(direct->GetID())};
    }
    return {};
}

GraphEndpoint DescribePin(CKParameterIn *input) {
    CKBehavior *owner = input
        ? CKBehavior::Cast(input->GetOwner()) : nullptr;
    if (!owner)
        return {};
    if (owner->GetTargetParameter() == input) {
        return {static_cast<std::uint32_t>(owner->GetID()),
                SlotKind::Target, 0};
    }
    const int index = owner->GetInputParameterPosition(input);
    return index < 0
        ? GraphEndpoint{}
        : GraphEndpoint{static_cast<std::uint32_t>(owner->GetID()),
                        SlotKind::InputParameter, index};
}

GraphEndpoint DescribeLocal(CKParameterLocal *local) {
    CKBehavior *owner = local ? CKBehavior::Cast(local->GetOwner()) : nullptr;
    if (!owner)
        return {};
    const int index = owner->GetLocalParameterPosition(local);
    if (index < 0)
        return {};
    return {static_cast<std::uint32_t>(owner->GetID()),
            owner->IsLocalParameterSetting(index) ? SlotKind::Setting
                                                  : SlotKind::Local,
            index};
}

// Copies the bytes CK2 keeps for one parameter. This is the whole value: the
// buffer is the parameter, so the copy round-trips through SetValue for every
// registered type without asking the type what it means.
std::vector<CKBYTE> Snapshot(CKParameter *parameter) {
    const int size = parameter ? parameter->GetDataSize() : 0;
    const auto *bytes = size > 0
        ? static_cast<const CKBYTE *>(parameter->GetReadDataPtr(FALSE))
        : nullptr;
    return bytes ? std::vector<CKBYTE>(bytes, bytes + size)
                 : std::vector<CKBYTE>{};
}

bool ContainsLink(CKBehavior *graph, CKBehaviorLink *link) {
    if (!graph || !link)
        return false;
    for (int index = 0; index < graph->GetSubBehaviorLinkCount(); ++index) {
        if (graph->GetSubBehaviorLink(index) == link)
            return true;
    }
    return false;
}

} // namespace

struct Patch::Journal {
    struct Link {
        Stamp Value;
    };

    struct Binding {
        Stamp Input;
        GraphEndpoint Pin;
        Stamp PreviousDirect;
        Stamp PreviousShared;
        Stamp InstalledDirect;
        Stamp InstalledShared;
        Stamp Literal;
        PinSource Before;
        PinSource Expected;
        // Set once this Pin has been handed back to its previous source. A
        // Conflicted Patch is closed again later, and a Pin that already
        // reverted must not be touched twice.
        bool Reverted = false;
    };

    struct Destination {
        Stamp Source;
        Stamp Target;
    };

    // A value written straight into a Setting or a Local. CK2 keeps the value
    // in the parameter's own buffer, so the bytes that were there are the only
    // thing a revert can hand back, and the bytes this Patch wrote are what
    // tells a revert whether anyone else has written since.
    struct Written {
        Stamp Parameter;
        GraphEndpoint Slot;
        std::vector<CKBYTE> Before;
        std::vector<CKBYTE> Expected;
        bool Reverted = false;
    };

    struct Interface {
        Stamp Behavior;
        Stamp Port;
        SlotKind Kind = SlotKind::Input;
    };

    CKEdit *Editor = nullptr;
    mutable std::mutex Mutex;
    PatchState State = PatchState::Pending;
    Status LastStatus;
    bool Queued = false;
    Stamp Graph;
    PatchKey Key;
    std::vector<std::shared_ptr<CallbackResource>> Callbacks;
    std::vector<Stamp> Nodes;
    // Every Node this Edit named, borrowed or added, keyed by its Edit handle.
    // Filled once the Edit reaches the graph, which is what makes a Pending
    // Patch answer Busy instead of naming an object that does not exist yet.
    std::map<std::uint32_t, Stamp> Handles;
    std::vector<Stamp> InfrastructureNodes;
    std::vector<Stamp> InfrastructureLinks;
    std::vector<Link> Links;
    std::vector<Binding> Binds;
    std::vector<Written> Values;
    std::vector<Destination> Pushes;
    std::vector<Interface> Ports;
    PatchLayer Layer;
    RelationLayer Data;
    std::vector<std::pair<LinkId, std::uint32_t>> Splices;
    std::vector<std::pair<LinkId, std::uint32_t>> Redirects;
    std::vector<RevertConflict> Conflicts;
};

struct CKEdit::Request {
    enum class Kind {
        Apply,
        Close,
    };

    Kind Action = Kind::Apply;
    Edit Candidate;
    std::shared_ptr<Patch::Journal> Patch;
};

struct CKEdit::Links {
    struct Key {
        PatchKey Patch;
        std::uint32_t Ordinal = 0;

        friend auto operator<=>(const Key &, const Key &) = default;
    };

    struct Site {
        Stamp Input;
        Stamp Output;
    };

    struct Chain {
        LinkId Id;
        LinkBase Base;
        Stamp Anchor;
        Stamp Source;
        Stamp Sink;
        std::vector<Key> Order;
        std::vector<Stamp> Continuations;
        // Where the tail of this chain currently points. It is the native Sink
        // until a Patch redirects the Link, and it returns there when that
        // Patch closes.
        Stamp Terminal;
        bool Redirected = false;
    };

    struct Infrastructure {
        std::vector<Stamp> Nodes;
        std::vector<Stamp> Links;
    };

    std::map<std::uint64_t, std::map<LinkId, Chain>> Chains;
    std::map<std::uint64_t, std::map<Key, Site>> Sites;
    std::map<std::uint64_t, std::map<PatchKey, Infrastructure>> Patches;
    std::map<std::uint64_t, Stamp> Roots;
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
    std::lock_guard<std::mutex> lock(m_Journal->Mutex);
    return m_Journal->State;
}

Status Patch::Diagnostic() const {
    if (!m_Journal)
        return {};
    std::lock_guard<std::mutex> lock(m_Journal->Mutex);
    return m_Journal->LastStatus;
}

std::vector<RevertConflict> Patch::Conflicts() const {
    if (!m_Journal)
        return {};
    std::lock_guard<std::mutex> lock(m_Journal->Mutex);
    return m_Journal->Conflicts;
}

CKEdit::CKEdit(CKContext *context, Runtime &runtime,
               PrototypeCatalog *catalog, GraphSource &graph)
    : m_Context(context), m_Runtime(runtime), m_Catalog(catalog),
      m_Graph(graph), m_Thread(std::this_thread::get_id()),
      m_Links(std::make_unique<Links>()) {}

CKEdit::~CKEdit() = default;

void CKEdit::AdoptGraph(CKBehavior *graph) {
    if (!graph || !m_Links)
        return;
    const std::uint64_t graphId = static_cast<std::uint32_t>(graph->GetID());
    const Stamp current = Capture(graph);
    const auto known = m_Links->Roots.find(graphId);
    if (known != m_Links->Roots.end() && known->second != current) {
        m_Graph.SetLogicalGraph(Native(known->second), {});
        m_Topology.erase(graphId);
        m_Relations.erase(graphId);
        m_Active.erase(graphId);
        m_Links->Chains.erase(graphId);
        m_Links->Sites.erase(graphId);
        m_Links->Patches.erase(graphId);
    }
    m_Links->Roots[graphId] = current;
}

Status CKEdit::PublishLogicalGraph(std::uint64_t graphId) {
    if (!m_Links)
        return Failure(Error::InvalidState,
                       "The graph Link registry is unavailable.");
    const auto root = m_Links->Roots.find(graphId);
    if (root == m_Links->Roots.end())
        return Failure(Error::InvalidGraphLocality,
                       "The logical graph root is unavailable.");
    if (!Resolve<CKBehavior>(m_Context, root->second, CKCID_BEHAVIOR)) {
        m_Graph.SetLogicalGraph(Native(root->second), {});
        return {};
    }

    LogicalGraph logical;
    const auto patches = m_Links->Patches.find(graphId);
    if (patches != m_Links->Patches.end()) {
        for (const auto &[key, infrastructure] : patches->second) {
            (void) key;
            for (Stamp node : infrastructure.Nodes)
                logical.InfrastructureNodes.push_back(Native(node));
            for (Stamp linkStamp : infrastructure.Links) {
                CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, linkStamp, CKCID_BEHAVIORLINK);
                GraphLinkShape live;
                if (!link || !Describe(link, live))
                    return Failure(Error::GraphChanged,
                                   "A Tap infrastructure Link disappeared.");
                logical.Links.push_back({Native(linkStamp), live, std::nullopt});
            }
        }
    }

    const auto topology = m_Topology.find(graphId);
    const auto chains = m_Links->Chains.find(graphId);
    if (topology != m_Topology.end() && chains != m_Links->Chains.end()) {
        for (const auto &[id, chain] : chains->second) {
            if (chain.Order.empty() && !chain.Redirected)
                continue;
            const LogicalLink *link = topology->second.Find(id);
            CKBehaviorLink *anchor = Resolve<CKBehaviorLink>(
                m_Context, chain.Anchor, CKCID_BEHAVIORLINK);
            GraphLinkShape live;
            if (!link || !anchor || !Describe(anchor, live))
                return Failure(Error::GraphChanged,
                               "A Splice anchor disappeared.");
            // A Splice is infrastructure, so the Logical view keeps reporting
            // the original destination. A Redirect is a deliberate change of
            // destination, so the Logical view reports the new one.
            logical.Links.push_back(
                {Native(chain.Anchor), live,
                 GraphLinkShape{link->Base.Source, EffectiveSink(*link),
                                link->Base.Delay}});
            for (Stamp continuation : chain.Continuations) {
                CKBehaviorLink *native = Resolve<CKBehaviorLink>(
                    m_Context, continuation, CKCID_BEHAVIORLINK);
                if (!native || !Describe(native, live))
                    return Failure(Error::GraphChanged,
                                   "A Splice continuation disappeared.");
                logical.Links.push_back(
                    {Native(continuation), live, std::nullopt});
            }
        }
    }
    m_Graph.SetLogicalGraph(Native(root->second), std::move(logical));
    return {};
}

void CKEdit::Queue(Request request) {
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_Queue.push_back(std::move(request));
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

Status CKEdit::Materialize(std::uint64_t graphId, CKBehavior *graph) {
    if (!graph || !m_Links)
        return Failure(Error::InvalidGraphLocality,
                       "The graph for Link materialization is unavailable.");
    const auto topology = m_Topology.find(graphId);
    const auto chains = m_Links->Chains.find(graphId);
    if (topology == m_Topology.end() || chains == m_Links->Chains.end())
        return {};
    auto &sites = m_Links->Sites[graphId];

    struct Prepared {
        Links::Chain *Chain = nullptr;
        CKBehaviorLink *Anchor = nullptr;
        CKBehaviorIO *OldHead = nullptr;
        CKBehaviorIO *NewHead = nullptr;
        CKBehaviorIO *Terminal = nullptr;
        bool Redirected = false;
        std::vector<Links::Key> Order;
        std::vector<Stamp> Continuations;
    };
    std::vector<Prepared> prepared;
    const auto siteInput = [&](const Links::Key &key) -> CKBehaviorIO * {
        const auto found = sites.find(key);
        return found == sites.end()
            ? nullptr : ResolveIo(m_Context, found->second.Input);
    };

    const auto discard = [&] {
        for (Prepared &change : prepared) {
            for (Stamp stamp : change.Continuations) {
                CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, stamp, CKCID_BEHAVIORLINK);
                if (!link)
                    continue;
                graph->RemoveSubBehaviorLink(link);
                m_Context->DestroyObject(link);
            }
            change.Continuations.clear();
        }
    };

    for (auto &[id, chain] : chains->second) {
        const LogicalLink *logical = topology->second.Find(id);
        if (!logical)
            return Failure(Error::GraphChanged,
                           "A materialized Link lost its logical identity.");
        std::vector<Links::Key> order;
        bool redirected = false;
        Links::Key redirect;
        for (const OrderedOverlay &overlay : logical->Overlays) {
            if (overlay.Kind == OverlayKind::Splice) {
                order.push_back({overlay.Patch, overlay.Ordinal});
            } else if (overlay.Kind == OverlayKind::Redirect) {
                redirected = true;
                redirect = {overlay.Patch, overlay.Ordinal};
            }
        }

        auto *anchor = Resolve<CKBehaviorLink>(
            m_Context, chain.Anchor, CKCID_BEHAVIORLINK);
        CKBehaviorIO *source = ResolveIo(m_Context, chain.Source);
        CKBehaviorIO *sink = ResolveIo(m_Context, chain.Terminal);
        CKBehaviorIO *desired = redirected
            ? siteInput(redirect) : ResolveIo(m_Context, chain.Sink);
        if (!desired) {
            discard();
            return Failure(Error::GraphChanged,
                           "A Link destination disappeared before publication.");
        }
        if (order == chain.Order && desired == sink)
            continue;
        CKBehaviorIO *oldHead = chain.Order.empty()
            ? sink : siteInput(chain.Order.front());
        if (!anchor || !source || !sink || !oldHead || !desired ||
            anchor->GetInBehaviorIO() != source ||
            anchor->GetOutBehaviorIO() != oldHead ||
            anchor->GetInitialActivationDelay() != chain.Base.Delay ||
            chain.Continuations.size() != chain.Order.size()) {
            discard();
            return Failure(Error::RevertConflict,
                           "A spliced Link changed outside its published Patch.");
        }
        for (std::size_t index = 0; index < chain.Order.size(); ++index) {
            const auto site = sites.find(chain.Order[index]);
            CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                m_Context, chain.Continuations[index], CKCID_BEHAVIORLINK);
            CKBehaviorIO *expectedSource = site == sites.end()
                ? nullptr : ResolveIo(m_Context, site->second.Output);
            CKBehaviorIO *expectedSink = index + 1 < chain.Order.size()
                ? siteInput(chain.Order[index + 1])
                : sink;
            if (!link || !expectedSource || !expectedSink ||
                link->GetInBehaviorIO() != expectedSource ||
                link->GetOutBehaviorIO() != expectedSink ||
                link->GetInitialActivationDelay() != 0) {
                discard();
                return Failure(Error::RevertConflict,
                               "A physical Splice chain changed outside its Patch.");
            }
        }

        Prepared change;
        change.Chain = &chain;
        change.Anchor = anchor;
        change.OldHead = oldHead;
        change.Order = order;
        change.Terminal = desired;
        change.Redirected = redirected;
        change.NewHead = order.empty()
            ? desired : siteInput(order.front());
        if (!change.NewHead) {
            discard();
            return Failure(Error::GraphChanged,
                           "A Splice node disappeared before publication.");
        }

        prepared.push_back(std::move(change));
        Prepared &pending = prepared.back();
        for (std::size_t index = 0; index < order.size(); ++index) {
            const auto site = sites.find(order[index]);
            CKBehaviorIO *from = site == sites.end()
                ? nullptr : ResolveIo(m_Context, site->second.Output);
            CKBehaviorIO *to = index + 1 < order.size()
                ? siteInput(order[index + 1])
                : desired;
            if (!from || !to) {
                discard();
                return Failure(Error::GraphChanged,
                               "A Splice endpoint disappeared before publication.");
            }
            auto *link = static_cast<CKBehaviorLink *>(m_Context->CreateObject(
                CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
            if (!link) {
                discard();
                return Failure(Error::CreateFailed,
                               "Virtools failed to create a Splice continuation.",
                               CKERR_OUTOFMEMORY);
            }
            CKERROR error = link->SetInBehaviorIO(from);
            if (error == CK_OK)
                error = link->SetOutBehaviorIO(to);
            if (error == CK_OK) {
                link->SetInitialActivationDelay(0);
                link->SetActivationDelay(0);
                error = graph->AddSubBehaviorLink(link);
            }
            if (error != CK_OK) {
                m_Context->DestroyObject(link);
                discard();
                return Failure(Error::GraphChanged,
                               "Virtools rejected a Splice continuation.", error);
            }
            pending.Continuations.push_back(Capture(link));
        }
    }

    std::size_t rewired = 0;
    for (; rewired < prepared.size(); ++rewired) {
        const CKERROR error =
            prepared[rewired].Anchor->SetOutBehaviorIO(prepared[rewired].NewHead);
        if (error == CK_OK)
            continue;
        while (rewired > 0) {
            --rewired;
            (void) prepared[rewired].Anchor->SetOutBehaviorIO(
                prepared[rewired].OldHead);
        }
        discard();
        return Failure(Error::GraphChanged,
                       "Virtools rejected the exact Link rewire.", error);
    }

    for (Prepared &change : prepared) {
        for (Stamp stamp : change.Chain->Continuations) {
            CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                m_Context, stamp, CKCID_BEHAVIORLINK);
            if (!link)
                continue;
            graph->RemoveSubBehaviorLink(link);
            m_Context->DestroyObject(link);
        }
        change.Chain->Order = std::move(change.Order);
        change.Chain->Continuations = std::move(change.Continuations);
        change.Chain->Terminal = Capture(change.Terminal);
        change.Chain->Redirected = change.Redirected;
    }
    return {};
}

Status CKEdit::Begin(CKBehavior *graph, PatchKey key, Edit &out) {
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
    out = Edit(std::move(key), native, std::move(layout));
    return {};
}

Status CKEdit::Use(Edit &edit, CKBehavior *behavior, Node &out) {
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

Status CKEdit::Use(Edit &edit, CKBehaviorLink *link, Link &out) {
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

Status CKEdit::Add(Edit &edit, Spec block, Node &out, NodeRole role) {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    if (!m_Catalog)
        return Failure(Error::Unavailable,
                       "Behavior Prototype discovery is unavailable.");
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

Status CKEdit::ApplyNow(const Edit &edit,
                        const std::shared_ptr<Patch::Journal> &patch) {
    Status status = Ready();
    if (!status)
        return status;
    if (!patch)
        return Failure(Error::InvalidState,
                       "The Patch journal is unavailable.");
    PublishingScope publishing(m_Publishing);
    if (edit.m_Nodes.empty())
        return Failure(Error::InvalidState, "The Edit has no graph.");

    CKBehavior *graph = ResolveBehavior(m_Context, edit.m_Nodes.front().Native);
    if (!graph || graph->IsUsingFunction())
        return Failure(Error::InvalidGraphLocality,
                       "The Edit graph is stale or no longer graph-backed.",
                       CKERR_INVALIDOBJECT);
    const std::uint64_t graphId = static_cast<std::uint32_t>(graph->GetID());
    if (m_Active[graphId].contains(edit.Key()))
        return Failure(Error::InvalidState,
                       "The same owner and patch key is already active on this graph.");

    GraphModel base;
    status = m_Graph.Read(edit.m_Nodes.front().Native,
                          GraphView::Logical, base);
    CheckedEdit checked;
    if (status)
        status = edit.Validate(base, checked);
    if (!status)
        return status;

    Topology &topology = m_Topology[graphId];
    Relations &relations = m_Relations[graphId];
    PatchLayer spliceLayer;
    spliceLayer.Patch = edit.Key();
    std::vector<LinkId> spliceLinks;
    spliceLinks.reserve(checked.Splices.size());
    for (const CheckedSplice &splice : checked.Splices) {
        LinkId id;
        // Identify also re-checks a known anchor against its recorded base
        // endpoints and delay, so a foreign change to an idle spliced Link is
        // reported as GraphChanged here instead of as a RevertConflict later.
        status = topology.Identify(splice.Target, id);
        if (!status)
            return status;
        spliceLinks.push_back(id);

        auto group = std::find_if(
            spliceLayer.Links.begin(), spliceLayer.Links.end(),
            [id](const LinkOverlays &candidate) { return candidate.Link == id; });
        if (group == spliceLayer.Links.end()) {
            spliceLayer.Links.push_back({id, splice.Ordering, {}});
            group = std::prev(spliceLayer.Links.end());
        }
        group->Overlays.push_back(
            {OverlayKind::Splice, splice.Ordinal,
             HashSplice(splice.Target, splice.Ordinal,
                        splice.Input.Owner.Value)});
    }
    std::vector<LinkId> redirectLinks;
    redirectLinks.reserve(checked.Redirects.size());
    for (const CheckedRedirect &redirect : checked.Redirects) {
        LinkId id;
        status = topology.Identify(redirect.Target, id);
        if (!status)
            return status;
        redirectLinks.push_back(id);

        auto group = std::find_if(
            spliceLayer.Links.begin(), spliceLayer.Links.end(),
            [id](const LinkOverlays &candidate) { return candidate.Link == id; });
        if (group == spliceLayer.Links.end()) {
            spliceLayer.Links.push_back({id, redirect.Ordering, {}});
            group = std::prev(spliceLayer.Links.end());
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
    if (!spliceLayer.Links.empty()) {
        status = topology.Validate(spliceLayer);
        if (!status)
            return status;
    }

    RelationLayer relationLayer;
    relationLayer.Patch = edit.Key();
    for (const CheckedBind &bind : checked.Binds) {
        const Edit::EditNode *target = edit.Find(bind.Target.Owner);
        if (!target || target->Block || bind.Target.Appended)
            continue;
        const GraphEndpoint pin{
            target->Native.Id, bind.Target.Slot.Kind,
            bind.Target.Slot.NativeIndex};
        relationLayer.Pins.push_back(
            {pin, {}, {{RelationKind::Bind, bind.Ordinal,
                        HashBind(pin, bind)}}});
    }
    if (!relationLayer.Pins.empty()) {
        status = relations.Validate(relationLayer);
        if (!status)
            return status;
    }

    patch->Editor = this;
    patch->Graph = Capture(graph);
    patch->Key = edit.Key();
    AdoptGraph(graph);

    std::unordered_map<std::uint32_t, CKBehavior *> nodes;
    nodes.emplace(edit.Graph().Value, graph);
    for (std::size_t index = 1; index < edit.m_Nodes.size(); ++index) {
        const Edit::EditNode &node = edit.m_Nodes[index];
        if (node.Block)
            continue;
        CKBehavior *native = ResolveBehavior(m_Context, node.Native);
        if (!native || native->GetParent() != graph) {
            status = Failure(Error::InvalidGraphLocality,
                             "A borrowed Node left the target graph before Apply.",
                             CKERR_INVALIDOBJECT);
            break;
        }
        nodes.emplace(node.Handle.Value, native);
    }

    const auto fail = [&](Status failure) {
        Status reverted = Undo(*patch, false);
        if (!reverted) {
            if (!failure.Message.empty())
                reverted.Message = failure.Message + " " + reverted.Message;
            return reverted;
        }
        return failure;
    };
    if (!status)
        return fail(std::move(status));

    const auto behaviorFor = [&](Node node) -> CKBehavior * {
        const auto found = nodes.find(node.Value);
        return found == nodes.end() ? nullptr : found->second;
    };

    const auto parameterBeforeApply = [&](const ResolvedPort &port,
                                          CKObject *&value) -> Status {
        value = nullptr;
        const Edit::EditNode *node = edit.Find(port.Owner);
        if (!node)
            return Failure(Error::InvalidState,
                           "An Edit data port names an unknown Node.");
        if (node->Block || port.Appended)
            return {};

        CKBehavior *behavior = behaviorFor(port.Owner);
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
    };

    CKParameterManager *manager = m_Context->GetParameterManager();
    const auto compatible = [&](CKGUID destination, CKGUID source,
                                const char *relation) -> Status {
        if (!destination.IsValid() || !source.IsValid())
            return {};
        if (!manager)
            return Failure(Error::ParameterTypeUnavailable,
                           "Virtools parameter types are unavailable.");
        return Parameter::Compatible(manager, destination, source)
            ? Status{}
            : Failure(Error::TypeMismatch,
                      std::string("Virtools rejected the ") + relation +
                          " parameter types.");
    };
    const auto selectorType = [&](const ResolvedPort &port) {
        return compatible(port.Slot.Type, port.Selector.ExpectedType,
                          "selected");
    };
    for (const CheckedBind &bind : checked.Binds) {
        status = selectorType(bind.Target);
        if (status && bind.Kind != BindKind::Literal)
            status = selectorType(bind.Source);
        if (status) {
            const CKGUID source = bind.Kind == BindKind::Literal
                ? bind.Literal.Type() : bind.Source.Slot.Type;
            status = compatible(bind.Target.Slot.Type, source, "Bind");
        }
        if (!status)
            return fail(std::move(status));
    }
    for (const CheckedPush &push : checked.Pushes) {
        status = selectorType(push.Source);
        if (status)
            status = selectorType(push.Destination);
        if (status)
            status = compatible(push.Destination.Slot.Type,
                                push.Source.Slot.Type, "Push");
        if (!status)
            return fail(std::move(status));
    }
    std::unordered_map<ParameterId, CKObject *> nativeParameters;
    const auto parameterId = [&](const ResolvedPort &port,
                                 ParameterId &id) -> Status {
        CKObject *parameter = nullptr;
        Status result = parameterBeforeApply(port, parameter);
        if (!result)
            return result;
        if (!parameter) {
            id = PlanParameter(port);
            return {};
        }
        id = NativeParameter(parameter);
        nativeParameters[id] = parameter;
        return {};
    };

    std::unordered_map<ParameterId, std::optional<ParameterId>> shared;
    std::vector<std::pair<ParameterId, ParameterId>> sharedEdges;
    for (const CheckedBind &bind : checked.Binds) {
        ParameterId target = 0;
        status = parameterId(bind.Target, target);
        if (!status)
            return fail(std::move(status));
        if (bind.Kind != BindKind::Shared) {
            shared[target] = std::nullopt;
            continue;
        }
        ParameterId source = 0;
        status = parameterId(bind.Source, source);
        if (!status)
            return fail(std::move(status));
        shared[target] = source;
        sharedEdges.emplace_back(target, source);
    }
    const auto sharedSource = [&](ParameterId input,
                                  std::optional<ParameterId> &source) -> Status {
        const auto replacement = shared.find(input);
        if (replacement != shared.end()) {
            source = replacement->second;
            return {};
        }
        const auto found = nativeParameters.find(input);
        auto *native = found == nativeParameters.end()
            ? nullptr : CKParameterIn::Cast(found->second);
        CKParameterIn *next = native ? native->GetSharedSource() : nullptr;
        if (!next) {
            source.reset();
            return {};
        }
        CKObject *live = m_Context->GetObject(next->GetID());
        if (live != next || next->IsToBeDeleted())
            return Failure(Error::GraphChanged,
                           "A shared Pin source disappeared before Apply.",
                           CKERR_INVALIDOBJECT);
        const ParameterId id = NativeParameter(next);
        nativeParameters[id] = next;
        source = id;
        return {};
    };
    for (const auto &[target, source] : sharedEdges) {
        ParameterId current = source;
        std::unordered_set<ParameterId> seen;
        while (seen.insert(current).second) {
            if (current == target)
                return fail(Failure(
                    Error::SharedSourceCycle,
                    "The final shared-source graph contains a cycle."));
            std::optional<ParameterId> next;
            status = sharedSource(current, next);
            if (!status)
                return fail(std::move(status));
            if (!next)
                break;
            current = *next;
        }
    }

    std::unordered_map<ParameterId, std::vector<ParameterId>> pushes;
    std::vector<std::pair<ParameterId, ParameterId>> pushEdges;
    for (const CheckedPush &push : checked.Pushes) {
        ParameterId source = 0;
        ParameterId destination = 0;
        status = parameterId(push.Source, source);
        if (status)
            status = parameterId(push.Destination, destination);
        if (!status)
            return fail(std::move(status));
        auto *nativeSource = source & kPlannedParameter
            ? nullptr : CKParameterOut::Cast(nativeParameters[source]);
        auto *nativeDestination = destination & kPlannedParameter
            ? nullptr : CKParameter::Cast(nativeParameters[destination]);
        if (nativeSource && nativeDestination &&
            ContainsDestination(nativeSource, nativeDestination)) {
            return fail(Failure(
                Error::InvalidState,
                "The Pout already has this destination."));
        }
        pushes[source].push_back(destination);
        pushEdges.emplace_back(source, destination);
    }
    const auto pushDestinations = [&](ParameterId source,
                                      std::vector<ParameterId> &destinations)
        -> Status {
        const auto added = pushes.find(source);
        if (added != pushes.end())
            destinations.insert(destinations.end(), added->second.begin(),
                                added->second.end());
        const auto found = nativeParameters.find(source);
        auto *native = found == nativeParameters.end()
            ? nullptr : CKParameterOut::Cast(found->second);
        if (!native)
            return {};
        for (int index = 0; index < native->GetDestinationCount(); ++index) {
            CKParameter *destination = native->GetDestination(index);
            if (!destination)
                return Failure(Error::GraphChanged,
                               "A Pout destination disappeared before Apply.",
                               CKERR_INVALIDOBJECT);
            CKObject *live = m_Context->GetObject(destination->GetID());
            if (live != destination || destination->IsToBeDeleted())
                return Failure(Error::GraphChanged,
                               "A Pout destination disappeared before Apply.",
                               CKERR_INVALIDOBJECT);
            const ParameterId id = NativeParameter(destination);
            nativeParameters[id] = destination;
            destinations.push_back(id);
        }
        return {};
    };
    for (const auto &[source, destination] : pushEdges) {
        std::vector<ParameterId> pending{destination};
        std::unordered_set<ParameterId> seen;
        while (!pending.empty()) {
            const ParameterId current = pending.back();
            pending.pop_back();
            if (current == source)
                return fail(Failure(
                    Error::PushCycle,
                    "The final Pout destination graph contains a cycle."));
            if (!seen.insert(current).second)
                continue;
            std::vector<ParameterId> destinations;
            status = pushDestinations(current, destinations);
            if (!status)
                return fail(std::move(status));
            pending.insert(pending.end(), destinations.begin(),
                           destinations.end());
        }
    }

    // Added Nodes enter their complete Runtime lifecycle before any control
    // path can reach them.
    for (const Edit::EditNode &node : edit.m_Nodes) {
        if (!node.Block)
            continue;
        AttachResult added = m_Runtime.AddToGraph(graph, *node.Block);
        if (!added || !added.Block)
            return fail(added.Detail);
        nodes.emplace(node.Handle.Value, added.Block);
        const Stamp stamp = Capture(added.Block);
        patch->Nodes.push_back(stamp);
        if (node.Role == NodeRole::Infrastructure)
            patch->InfrastructureNodes.push_back(stamp);
    }

    std::unordered_map<std::uint32_t, CKBehavior *> tapNodes;
    for (const CheckedTap &tap : checked.Taps) {
        AttachResult added = m_Runtime.AddToGraph(
            graph, HookBlock::Make(tap.Callback, 1, 0));
        if (!added || !added.Block)
            return fail(added.Detail);
        tapNodes.emplace(tap.Ordinal, added.Block);
        const Stamp node = Capture(added.Block);
        patch->Nodes.push_back(node);
        patch->InfrastructureNodes.push_back(node);
    }

    std::unordered_set<CKBehavior *> changed;
    for (const InterfacePort &item : edit.m_Interface) {
        if (item.InBlockSpec)
            continue;
        const auto owner = nodes.find(item.Owner.Value);
        CKBehavior *behavior = owner == nodes.end() ? nullptr : owner->second;
        if (!behavior)
            return fail(Failure(Error::InvalidState,
                                "A dynamic interface Node is unavailable."));

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
        default:
            return fail(Failure(Error::InterfaceUnsupported,
                                "This dynamic interface kind is not supported."));
        }
        if (!created)
            return fail(Failure(Error::CreateFailed,
                                "Virtools failed to append a Behavior port.",
                                CKERR_OUTOFMEMORY));
        patch->Ports.push_back(
            {Capture(behavior), Capture(created), item.Slot.Kind});
        changed.insert(behavior);
    }

    for (CKBehavior *behavior : changed) {
        const int result = behavior->CallCallbackFunction(CKM_BEHAVIOREDITED);
        if (result != CK_OK)
            return fail(Failure(Error::CallbackFailed,
                                "A dynamic interface EDITED callback failed.",
                                result));
    }

    const auto liveSlot = [&](const ResolvedPort &port, SlotInfo &slot) {
        CKBehavior *behavior = behaviorFor(port.Owner);
        if (!behavior)
            return Failure(Error::InvalidState,
                           "An Edit Node disappeared during Apply.",
                           CKERR_INVALIDOBJECT);
        return m_Runtime.Resolve(behavior, port.Selector, slot);
    };

    const auto control = [&](const ResolvedPort &port,
                             CKBehaviorIO *&io) -> Status {
        io = nullptr;
        CKBehavior *behavior = behaviorFor(port.Owner);
        SlotInfo slot;
        Status result = liveSlot(port, slot);
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
    };

    const auto parameter = [&](const ResolvedPort &port,
                               CKObject *&value) -> Status {
        value = nullptr;
        CKBehavior *behavior = behaviorFor(port.Owner);
        SlotInfo slot;
        Status result = liveSlot(port, slot);
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
    };

    const auto addLink = [&](CKBehaviorIO *source, CKBehaviorIO *sink,
                             int delay, Stamp *added = nullptr) -> Status {
        auto *link = static_cast<CKBehaviorLink *>(m_Context->CreateObject(
            CKCID_BEHAVIORLINK, nullptr, CK_OBJECTCREATION_DYNAMIC));
        if (!link)
            return Failure(Error::CreateFailed,
                           "Virtools failed to create a Behavior Link.",
                           CKERR_OUTOFMEMORY);
        CKERROR error = link->SetInBehaviorIO(source);
        if (error == CK_OK)
            error = link->SetOutBehaviorIO(sink);
        if (error == CK_OK) {
            link->SetInitialActivationDelay(delay);
            link->SetActivationDelay(delay);
            error = graph->AddSubBehaviorLink(link);
        }
        if (error != CK_OK) {
            m_Context->DestroyObject(link);
            return Failure(Error::GraphChanged,
                           "Virtools rejected a Behavior Link.", error);
        }
        const Stamp value = Capture(link);
        patch->Links.push_back({value});
        if (added)
            *added = value;
        return {};
    };

    for (const CheckedFlow &flow : checked.Flows) {
        CKBehaviorIO *source = nullptr;
        CKBehaviorIO *sink = nullptr;
        status = control(flow.Source, source);
        if (status)
            status = control(flow.Sink, sink);
        if (status)
            status = addLink(source, sink, flow.Delay);
        if (!status)
            return fail(std::move(status));
    }

    for (const CheckedBind &bind : checked.Binds) {
        CKObject *targetParameter = nullptr;
        status = parameter(bind.Target, targetParameter);
        if (!status)
            return fail(std::move(status));
        // A Local holds its value itself, so there is no source to install and
        // nothing to create. The bytes go into the parameter and the bytes that
        // were there go into the journal.
        if (bind.Target.Slot.Kind == SlotKind::Local) {
            auto *stored = CKParameterLocal::Cast(targetParameter);
            if (!stored)
                return fail(Failure(
                    Error::GraphChanged,
                    "A written value names a slot that is not a stored parameter."));
            Patch::Journal::Written change;
            change.Parameter = Capture(stored);
            change.Slot = DescribeLocal(stored);
            change.Before = Snapshot(stored);
            status = Parameter::Write(m_Context, stored, bind.Literal);
            if (!status)
                return fail(std::move(status));
            change.Expected = Snapshot(stored);
            patch->Values.push_back(std::move(change));
            continue;
        }
        auto *target = CKParameterIn::Cast(targetParameter);
        if (!target)
            return fail(Failure(Error::GraphChanged,
                                "A Bind destination is not a Pin."));

        Patch::Journal::Binding change;
        change.Input = Capture(target);
        change.Pin = DescribePin(target);
        change.PreviousDirect = Capture(target->GetDirectSource());
        change.PreviousShared = Capture(target->GetSharedSource());
        change.Before = DescribeSource(target);

        CKERROR error = CK_OK;
        if (bind.Kind == BindKind::Direct) {
            CKObject *sourceObject = nullptr;
            status = parameter(bind.Source, sourceObject);
            CKParameter *source = status
                ? CKParameter::Cast(sourceObject) : nullptr;
            if (status && !source)
                status = Failure(Error::TypeMismatch,
                                 "A direct Bind source is not a stored parameter.");
            if (status && !Parameter::Compatible(
                    m_Context->GetParameterManager(), target->GetGUID(),
                    source->GetGUID())) {
                status = Failure(Error::TypeMismatch,
                                 "Virtools rejected the direct Bind type.");
            }
            if (status)
                error = target->SetDirectSource(source);
            change.InstalledDirect = Capture(source);
        } else if (bind.Kind == BindKind::Shared) {
            CKObject *sourceParameter = nullptr;
            status = parameter(bind.Source, sourceParameter);
            auto *source = status ? CKParameterIn::Cast(sourceParameter) : nullptr;
            if (status && !source)
                status = Failure(Error::TypeMismatch,
                                 "A shared Bind source is not a Pin.");
            if (status)
                error = target->ShareSourceWith(source);
            change.InstalledShared = Capture(source);
        } else {
            std::ostringstream name;
            name << "__BML_Edit_" << graph->GetID() << '_'
                 << bind.Ordinal;
            CKParameterLocal *literal = m_Context->CreateCKParameterLocal(
                const_cast<CKSTRING>(name.str().c_str()),
                target->GetGUID(), TRUE);
            if (!literal)
                status = Failure(Error::CreateFailed,
                                 "Virtools failed to create a Bind value.",
                                 CKERR_OUTOFMEMORY);
            if (status)
                status = Parameter::Write(m_Context, literal, bind.Literal);
            if (status)
                error = target->SetDirectSource(literal);
            if (!status || error != CK_OK) {
                if (literal)
                    m_Context->DestroyObject(literal);
                return fail(status ? Failure(Error::TypeMismatch,
                                             "Virtools rejected a Bind value.",
                                             error)
                                   : std::move(status));
            }
            change.Literal = Capture(literal);
            change.InstalledDirect = Capture(literal);
        }
        if (!status)
            return fail(std::move(status));
        if (error != CK_OK)
            return fail(Failure(Error::TypeMismatch,
                                "Virtools rejected a Bind relation.", error));
        change.Expected = DescribeSource(target);
        patch->Binds.push_back(std::move(change));
    }

    patch->Data = relationLayer;
    if (!relationLayer.Pins.empty()) {
        status = relations.Set(relationLayer);
        if (!status)
            return fail(std::move(status));
    }

    for (const CheckedPush &push : checked.Pushes) {
        CKObject *sourceParameter = nullptr;
        CKObject *destinationObject = nullptr;
        status = parameter(push.Source, sourceParameter);
        if (status)
            status = parameter(push.Destination, destinationObject);
        auto *source = status ? CKParameterOut::Cast(sourceParameter) : nullptr;
        auto *destination = status
            ? CKParameter::Cast(destinationObject) : nullptr;
        if (!status || !source || !destination)
            return fail(status ? Failure(Error::GraphChanged,
                                         "Push requires a Pout and a stored parameter.")
                               : std::move(status));
        const CKERROR error = source->AddDestination(destination, TRUE);
        if (error != CK_OK)
            return fail(Failure(Error::TypeMismatch,
                                "Virtools rejected a Push relation.", error));
        patch->Pushes.push_back({Capture(source), Capture(destination)});
    }

    for (const CheckedTap &tap : checked.Taps) {
        CKBehaviorIO *source = nullptr;
        status = control(tap.Source, source);
        const auto observer = tapNodes.find(tap.Ordinal);
        CKBehavior *block = observer == tapNodes.end() ? nullptr : observer->second;
        if (status && (!block || !block->GetInput(0)))
            status = Failure(Error::GraphChanged,
                             "A Tap observer disappeared during Apply.");
        Stamp link;
        if (status)
            status = addLink(source, block->GetInput(0), 0, &link);
        if (!status)
            return fail(std::move(status));
        patch->InfrastructureLinks.push_back(link);
    }

    PatchLayer layer = spliceLayer;
    layer.Patch = edit.Key();
    for (const CheckedTap &tap : checked.Taps) {
        CKBehavior *owner = behaviorFor(tap.Source.Owner);
        SlotInfo slot;
        status = liveSlot(tap.Source, slot);
        if (!status)
            return fail(std::move(status));
        GraphEndpoint source{
            static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(owner->GetID())),
            slot.Kind, slot.NativeIndex};
        auto found = std::find_if(
            layer.Outs.begin(), layer.Outs.end(),
            [&](const OutTaps &candidate) {
                return candidate.Out == source;
            });
        if (found == layer.Outs.end()) {
            layer.Outs.push_back({source, {}});
            found = std::prev(layer.Outs.end());
        }
        found->Taps.push_back({tap.Ordinal, HashTap(source, tap.Ordinal)});
    }

    // Splice and Redirect both work on one bookkeeping chain per logical Link,
    // so both open it the same way.
    const auto openChain = [&](const LinkBase &target, LinkId id) -> Status {
        const auto nativeRecord = std::find_if(
            base.Links.begin(), base.Links.end(), [&](const GraphLink &candidate) {
                return candidate.Object == target.Anchor;
            });
        CKBehaviorLink *anchor = nativeRecord == base.Links.end()
            ? nullptr
            : Resolve<CKBehaviorLink>(
                  m_Context,
                  {static_cast<CK_ID>(nativeRecord->Id),
                   m_Context->GetObject(static_cast<CK_ID>(nativeRecord->Id))},
                  CKCID_BEHAVIORLINK);
        if (!anchor)
            return Failure(Error::LinkNotFound,
                           "The exact native Link disappeared before Apply.");

        auto &chains = m_Links->Chains[graphId];
        const auto chain = chains.find(id);
        if (chain == chains.end()) {
            CKBehaviorIO *source = anchor->GetInBehaviorIO();
            CKBehaviorIO *sink = anchor->GetOutBehaviorIO();
            if (!source || !sink)
                return Failure(Error::GraphChanged,
                               "The selected Link lost an endpoint.");
            const LogicalLink *logical = topology.Find(id);
            if (!logical)
                return Failure(Error::GraphChanged,
                               "The selected Link lost its logical identity.");
            Links::Chain value;
            value.Id = id;
            value.Base = logical->Base;
            value.Anchor = Capture(anchor);
            value.Source = Capture(source);
            value.Sink = Capture(sink);
            value.Terminal = value.Sink;
            chains.emplace(id, std::move(value));
            return {};
        }
        if (chain->second.Anchor.Address != anchor)
            return Failure(Error::GraphChanged,
                           "The selected Link anchor changed identity.");
        return {};
    };

    for (std::size_t spliceIndex = 0;
         spliceIndex < checked.Splices.size(); ++spliceIndex) {
        const CheckedSplice &splice = checked.Splices[spliceIndex];
        const LinkId id = spliceLinks[spliceIndex];
        status = openChain(splice.Target, id);
        if (!status)
            return fail(std::move(status));

        CKBehaviorIO *input = nullptr;
        CKBehaviorIO *output = nullptr;
        status = control(splice.Input, input);
        if (status)
            status = control(splice.Output, output);
        if (!status)
            return fail(std::move(status));

        const Links::Key key{edit.Key(), splice.Ordinal};
        auto &sites = m_Links->Sites[graphId];
        if (sites.contains(key))
            return fail(Failure(Error::InvalidState,
                                "The Splice action is already installed."));
        sites.emplace(key, Links::Site{Capture(input), Capture(output)});
        patch->Splices.emplace_back(id, splice.Ordinal);
        // Keep the candidate in the journal as it is assembled so any later
        // validation or materialization failure can remove every admitted
        // Splice site, including sites recorded before the failure.
        patch->Layer = layer;
    }

    for (std::size_t index = 0; index < checked.Redirects.size(); ++index) {
        const CheckedRedirect &redirect = checked.Redirects[index];
        const LinkId id = redirectLinks[index];
        CKBehaviorIO *sink = nullptr;
        status = control(redirect.Sink, sink);
        if (!status)
            return fail(std::move(status));
        CKBehavior *owner = behaviorFor(redirect.Sink.Owner);
        SlotInfo slot;
        status = liveSlot(redirect.Sink, slot);
        if (!status)
            return fail(std::move(status));
        const GraphEndpoint target{
            static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(owner->GetID())),
            slot.Kind, slot.NativeIndex};

        const auto group = std::find_if(
            layer.Links.begin(), layer.Links.end(),
            [id](const LinkOverlays &candidate) { return candidate.Link == id; });
        if (group == layer.Links.end())
            return fail(Failure(Error::InvalidState,
                                "A Redirect lost its Link group."));
        const auto overlay = std::find_if(
            group->Overlays.begin(), group->Overlays.end(),
            [&](const Overlay &candidate) {
                return candidate.Kind == OverlayKind::Redirect &&
                    candidate.Ordinal == redirect.Ordinal;
            });
        if (overlay == group->Overlays.end())
            return fail(Failure(Error::InvalidState,
                                "A Redirect lost its Link overlay."));
        overlay->Target = target;
        overlay->Fingerprint =
            HashRedirect(redirect.Target, redirect.Ordinal, target);

        status = openChain(redirect.Target, id);
        if (!status)
            return fail(std::move(status));

        const Links::Key key{edit.Key(), redirect.Ordinal};
        auto &sites = m_Links->Sites[graphId];
        if (sites.contains(key))
            return fail(Failure(Error::InvalidState,
                                "The Redirect action is already installed."));
        sites.emplace(key, Links::Site{Capture(sink), {}});
        patch->Redirects.emplace_back(id, redirect.Ordinal);
        patch->Layer = layer;
    }

    patch->Layer = layer;
    if (!layer.Links.empty() || !layer.Outs.empty()) {
        status = topology.Set(layer);
        if (status) {
            m_Active[graphId].insert(edit.Key());
            status = Materialize(graphId, graph);
        }
        if (!status)
            return fail(std::move(status));
        m_Links->Patches[graphId][patch->Key] = {
            patch->InfrastructureNodes, patch->InfrastructureLinks};
        status = PublishLogicalGraph(graphId);
        if (!status)
            return fail(std::move(status));
    }

    const int edited = graph->CallCallbackFunction(CKM_BEHAVIOREDITED);
    if (edited != CK_OK)
        return fail(Failure(Error::CallbackFailed,
                            "The graph EDITED callback failed.", edited));

    for (const Patch::Journal::Link &item : patch->Links) {
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            m_Context, item.Value, CKCID_BEHAVIORLINK);
        if (!link || link->GetInBehaviorIO() == nullptr ||
            link->GetOutBehaviorIO() == nullptr) {
            return fail(Failure(Error::GraphChanged,
                                "The graph changed while the Edit was published."));
        }
    }

    for (const auto &entry : nodes)
        patch->Handles.emplace(entry.first, Capture(entry.second));

    m_Active[graphId].insert(edit.Key());
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
    std::lock_guard<std::mutex> lock(journal.Mutex);
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

Status CKEdit::Undo(Patch::Journal &patch, bool notify) {
    Status first;
    {
        std::lock_guard<std::mutex> lock(patch.Mutex);
        patch.Conflicts.clear();
    }
    const auto noteConflict = [&](RevertConflict conflict) {
        std::lock_guard<std::mutex> lock(patch.Mutex);
        patch.Conflicts.push_back(std::move(conflict));
    };
    const auto remember = [&](Status status) {
        if (!status && first)
            first = std::move(status);
    };
    auto *graph = Resolve<CKBehavior>(m_Context, patch.Graph, CKCID_BEHAVIOR);
    const std::uint64_t graphId = patch.Graph.Id
        ? static_cast<std::uint32_t>(patch.Graph.Id) : 0;
    const auto registeredRoot = m_Links->Roots.find(graphId);
    const bool ownsLogicalGraph =
        registeredRoot != m_Links->Roots.end() &&
        registeredRoot->second == patch.Graph;

    if (ownsLogicalGraph &&
        (!patch.Layer.Links.empty() || !patch.Layer.Outs.empty())) {
        const auto topology = m_Topology.find(graphId);
        auto graphSites = m_Links->Sites.find(graphId);
        const bool removed = topology != m_Topology.end() &&
                             topology->second.Remove(patch.Key);
        if (graph && removed) {
            Status materialized = Materialize(graphId, graph);
            if (!materialized) {
                (void) topology->second.Set(patch.Layer);
                for (const LinkOverlays &group : patch.Layer.Links) {
                    const LogicalLink *link = topology->second.Find(group.Link);
                    RevertConflict conflict;
                    conflict.Subject = RevertSubject::Link;
                    if (link)
                        conflict.Link = link->Base.Anchor;
                    conflict.Diagnostic = materialized;
                    noteConflict(std::move(conflict));
                }
                // The layer is back in the Topology and every native resource
                // is still installed. Only a RevertConflict keeps the journal
                // for a later retry; any other code would let CloseNow report
                // Closed over a Patch that is still physically present.
                if (materialized.Code == Error::RevertConflict)
                    return materialized;
                Status conflict = Failure(
                    Error::RevertConflict,
                    "The spliced Links could not be restored: " +
                        materialized.Message,
                    materialized.CkError);
                conflict.Details = materialized.Details;
                return conflict;
            }
        }
        // Materialize needs both the old and desired chains to remain
        // resolvable. The retiring Patch's endpoints stop being part of the
        // graph only after the exact Link has been published successfully.
        if (graphSites != m_Links->Sites.end()) {
            for (const auto &[link, ordinal] : patch.Splices) {
                (void) link;
                graphSites->second.erase(Links::Key{patch.Key, ordinal});
            }
            for (const auto &[link, ordinal] : patch.Redirects) {
                (void) link;
                graphSites->second.erase(Links::Key{patch.Key, ordinal});
            }
        }
    }

    for (auto item = patch.Pushes.rbegin(); item != patch.Pushes.rend(); ++item) {
        auto *source = Resolve<CKParameterOut>(
            m_Context, item->Source, CKCID_PARAMETEROUT);
        auto *target = Resolve<CKParameter>(
            m_Context, item->Target, CKCID_PARAMETER);
        if (source && target && ContainsDestination(source, target))
            source->RemoveDestination(target);
    }

    for (auto item = patch.Values.rbegin(); item != patch.Values.rend(); ++item) {
        if (item->Reverted)
            continue;
        auto *stored = Resolve<CKParameterLocal>(
            m_Context, item->Parameter, CKCID_PARAMETERLOCAL);
        if (!stored || item->Before.empty()) {
            // The parameter went with its block, or never held a buffer, so
            // there is nothing left to hand back.
            item->Reverted = true;
            continue;
        }
        if (Snapshot(stored) != item->Expected) {
            Status conflict = Failure(
                Error::RevertConflict,
                "A written Local changed after the Patch was published.");
            conflict.Details.Stage = Phase::Teardown;
            noteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            remember(std::move(conflict));
            continue;
        }
        const CKERROR error = stored->SetValue(
            item->Before.data(), static_cast<int>(item->Before.size()));
        if (error != CK_OK) {
            Status conflict = Failure(
                Error::RevertConflict,
                "Virtools rejected the previous Local value.", error);
            conflict.Details.Stage = Phase::Teardown;
            noteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            remember(std::move(conflict));
        } else {
            item->Reverted = true;
        }
    }

    for (auto item = patch.Binds.rbegin(); item != patch.Binds.rend(); ++item) {
        if (item->Reverted)
            continue;
        auto *input = Resolve<CKParameterIn>(
            m_Context, item->Input, CKCID_PARAMETERIN);
        if (!input) {
            // Nothing is left to hand back, so the revert is settled even
            // though the owner still hears about the missing Pin.
            item->Reverted = true;
            if (graph) {
                Status conflict = Failure(
                    Error::RevertConflict,
                    "A Pin disappeared after the Patch was published.");
                conflict.Details.Stage = Phase::Teardown;
                noteConflict({RevertSubject::PinSource, item->Pin, {},
                              item->Before, item->Expected, {}, conflict});
                remember(std::move(conflict));
            }
            continue;
        }
        CKParameter *installedDirect = Resolve<CKParameter>(
            m_Context, item->InstalledDirect, CKCID_PARAMETER);
        CKParameterIn *installedShared = Resolve<CKParameterIn>(
            m_Context, item->InstalledShared, CKCID_PARAMETERIN);
        const bool unchanged = item->InstalledShared.Id
            ? input->GetSharedSource() == installedShared
            : input->GetDirectSource() == installedDirect;
        if (!unchanged) {
            Status conflict = Failure(
                Error::RevertConflict,
                "A Pin source changed after the Patch was published.");
            conflict.Details.Stage = Phase::Teardown;
            noteConflict({RevertSubject::PinSource, item->Pin, {},
                          item->Before, item->Expected, DescribeSource(input),
                          conflict});
            remember(std::move(conflict));
            continue;
        }
        CKParameterIn *previousShared = Resolve<CKParameterIn>(
            m_Context, item->PreviousShared, CKCID_PARAMETERIN);
        CKParameter *previousDirect = Resolve<CKParameter>(
            m_Context, item->PreviousDirect, CKCID_PARAMETER);
        const bool previousMissing =
            (item->PreviousShared.Id && !previousShared) ||
            (item->PreviousDirect.Id && !previousDirect);
        if (previousMissing) {
            Status conflict = Failure(
                Error::RevertConflict,
                "The previous Pin source no longer exists.");
            conflict.Details.Stage = Phase::Teardown;
            noteConflict({RevertSubject::PinSource, item->Pin, {},
                          item->Before, item->Expected, DescribeSource(input),
                          conflict});
            remember(std::move(conflict));
            continue;
        }
        const CKERROR error = item->PreviousShared.Id
            ? input->ShareSourceWith(previousShared)
            : input->SetDirectSource(previousDirect);
        if (error != CK_OK) {
            Status conflict = Failure(
                Error::RevertConflict,
                "Virtools rejected the previous Pin source.", error);
            conflict.Details.Stage = Phase::Teardown;
            noteConflict({RevertSubject::PinSource, item->Pin, {},
                          item->Before, item->Expected, DescribeSource(input),
                          conflict});
            remember(std::move(conflict));
        } else {
            item->Reverted = true;
        }
    }

    for (auto item = patch.Links.rbegin(); item != patch.Links.rend(); ++item) {
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            m_Context, item->Value, CKCID_BEHAVIORLINK);
        if (!link)
            continue;
        if (graph)
            graph->RemoveSubBehaviorLink(link);
        m_Context->DestroyObject(link);
    }

    // Apply notified each block that received a dynamic port; Close notifies
    // the same blocks once their ports are gone.
    std::vector<Stamp> portOwners;
    for (auto item = patch.Ports.rbegin(); item != patch.Ports.rend(); ++item) {
        CKBehavior *behavior = Resolve<CKBehavior>(
            m_Context, item->Behavior, CKCID_BEHAVIOR);
        CKObject *port = item->Port.Id
            ? m_Context->GetObject(item->Port.Id) : nullptr;
        if (!behavior || port != item->Port.Address ||
            port->IsToBeDeleted())
            continue;
        CKObject *removed = nullptr;
        switch (item->Kind) {
        case SlotKind::Input: {
            auto *io = static_cast<CKBehaviorIO *>(port);
            const int index = behavior->GetInputPosition(io);
            removed = index >= 0 ? behavior->RemoveInput(index) : nullptr;
            break;
        }
        case SlotKind::Output: {
            auto *io = static_cast<CKBehaviorIO *>(port);
            const int index = behavior->GetOutputPosition(io);
            removed = index >= 0 ? behavior->RemoveOutput(index) : nullptr;
            break;
        }
        case SlotKind::InputParameter: {
            auto *parameter = static_cast<CKParameterIn *>(port);
            const int index = behavior->GetInputParameterPosition(parameter);
            removed = index >= 0 ? behavior->RemoveInputParameter(index) : nullptr;
            break;
        }
        case SlotKind::OutputParameter: {
            auto *parameter = static_cast<CKParameterOut *>(port);
            const int index = behavior->GetOutputParameterPosition(parameter);
            removed = index >= 0 ? behavior->RemoveOutputParameter(index) : nullptr;
            break;
        }
        default:
            break;
        }
        if (removed == port) {
            m_Context->DestroyObject(removed);
            if (std::find(portOwners.begin(), portOwners.end(),
                          item->Behavior) == portOwners.end())
                portOwners.push_back(item->Behavior);
        }
    }

    // A Pin removed with the ports above has nothing left to hand back.
    for (Patch::Journal::Binding &item : patch.Binds) {
        if (!item.Reverted &&
            !Resolve<CKParameterIn>(m_Context, item.Input, CKCID_PARAMETERIN))
            item.Reverted = true;
    }

    // A Pin this Patch could not hand back still points at a source the Patch
    // owns. Until that clears, the Patch keeps its relation claim so no other
    // Patch can take the Pin, and keeps its key in the graph's active set so
    // the same key cannot be applied on top of the unfinished teardown.
    const bool pinsRetained = std::any_of(
            patch.Binds.begin(), patch.Binds.end(),
            [](const Patch::Journal::Binding &item) { return !item.Reverted; }) ||
        std::any_of(
            patch.Values.begin(), patch.Values.end(),
            [](const Patch::Journal::Written &item) { return !item.Reverted; });
    if (ownsLogicalGraph && !pinsRetained) {
        if (!patch.Data.Pins.empty()) {
            const auto relations = m_Relations.find(graphId);
            if (relations != m_Relations.end())
                (void) relations->second.Remove(patch.Key);
        }
        const auto active = m_Active.find(graphId);
        if (active != m_Active.end())
            active->second.erase(patch.Key);
    }

    for (const Patch::Journal::Binding &item : patch.Binds) {
        CKParameterLocal *literal = Resolve<CKParameterLocal>(
            m_Context, item.Literal, CKCID_PARAMETERLOCAL);
        CKParameterIn *input = Resolve<CKParameterIn>(
            m_Context, item.Input, CKCID_PARAMETERIN);
        // An unreverted Bind may still read this value, so the literal outlives
        // the failed teardown and is destroyed on a later close.
        if (literal && item.Reverted &&
            (!input || input->GetDirectSource() != literal))
            m_Context->DestroyObject(literal);
    }

    for (auto item = patch.Nodes.rbegin(); item != patch.Nodes.rend(); ++item) {
        CKBehavior *node = Resolve<CKBehavior>(
            m_Context, *item, CKCID_BEHAVIOR);
        if (!node)
            continue;
        Status closed = m_Runtime.Close(node);
        // Forget the Node once the Runtime disowns it. Asking again on a retry
        // would report a Block this Behavior Runtime no longer owns.
        if (closed)
            *item = {};
        remember(std::move(closed));
    }

    if (ownsLogicalGraph) {
        const auto infrastructure = m_Links->Patches.find(graphId);
        if (infrastructure != m_Links->Patches.end())
            infrastructure->second.erase(patch.Key);
        remember(PublishLogicalGraph(graphId));
    }

    if (notify) {
        for (Stamp owner : portOwners) {
            CKBehavior *block = Resolve<CKBehavior>(
                m_Context, owner, CKCID_BEHAVIOR);
            if (!block)
                continue;
            const int result = block->CallCallbackFunction(CKM_BEHAVIOREDITED);
            if (result != CK_OK)
                remember(Failure(Error::CallbackFailed,
                                 "A block EDITED callback failed while the Patch closed.",
                                 result));
        }
    }
    if (notify && graph) {
        const int result = graph->CallCallbackFunction(CKM_BEHAVIOREDITED);
        if (result != CK_OK)
            remember(Failure(Error::CallbackFailed,
                             "The graph EDITED callback failed while the Patch closed.",
                             result));
    }
    if (ownsLogicalGraph && !pinsRetained) {
        const auto active = m_Active.find(graphId);
        if (active == m_Active.end() || active->second.empty()) {
            if (active != m_Active.end())
                m_Active.erase(active);
            m_Topology.erase(graphId);
            m_Relations.erase(graphId);
            if (m_Links) {
                m_Links->Chains.erase(graphId);
                m_Links->Sites.erase(graphId);
                m_Links->Patches.erase(graphId);
                m_Links->Roots.erase(graphId);
            }
        }
    }
    return first;
}

void CKEdit::CloseAdmission(Patch::Journal &patch) noexcept {
    std::vector<std::shared_ptr<CallbackResource>> callbacks;
    {
        std::lock_guard<std::mutex> lock(patch.Mutex);
        callbacks = patch.Callbacks;
    }
    for (const std::shared_ptr<CallbackResource> &callback : callbacks) {
        if (callback)
            callback->CloseAdmission();
    }
}

Status CKEdit::Apply(const Edit &edit, Patch &out) {
    Status status = Ready();
    if (!status)
        return status;
    if (out)
        return Failure(Error::InvalidState,
                       "The destination Patch is already live.");

    auto patch = std::make_shared<Patch::Journal>();
    patch->Editor = this;
    patch->Key = edit.Key();
    for (const Edit::EditNode &node : edit.m_Nodes) {
        if (!node.Block)
            continue;
        patch->Callbacks.insert(patch->Callbacks.end(),
                                node.Block->m_KeepAlive.begin(),
                                node.Block->m_KeepAlive.end());
    }
    for (const EditTap &tap : edit.m_Taps) {
        if (tap.Callback)
            patch->Callbacks.push_back(tap.Callback);
    }
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
        CheckedEdit checked;
        if (status)
            status = edit.Validate(base, checked);
        if (!status)
            return status;
        {
            std::lock_guard<std::mutex> lock(patch->Mutex);
            patch->Queued = true;
        }
        out.m_Journal = patch;
        Queue({Request::Kind::Apply, edit, patch});
        Status queued;
        queued.Message = "Behavior Patch application is queued for the game-thread safe point.";
        queued.Details.Stage = Phase::Edit;
        return queued;
    }

    status = ApplyNow(edit, patch);
    {
        std::lock_guard<std::mutex> lock(patch->Mutex);
        patch->LastStatus = status;
        patch->State = status
            ? PatchState::Active
            : status.Code == Error::RevertConflict
                ? PatchState::Conflicted : PatchState::Failed;
        if (!status && status.Code != Error::RevertConflict)
            patch->Callbacks.clear();
    }
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
    // second Undo under the first.
    {
        std::lock_guard<std::mutex> lock(patch->Mutex);
        patch->State = PatchState::Closing;
    }
    Status status;
    {
        PublishingScope publishing(m_Publishing);
        status = Undo(*patch, true);
    }
    {
        std::lock_guard<std::mutex> lock(patch->Mutex);
        patch->LastStatus = status;
        patch->State = status.Code == Error::RevertConflict
            ? PatchState::Conflicted : PatchState::Closed;
        if (status.Code != Error::RevertConflict)
            patch->Callbacks.clear();
    }
    return status;
}

Status CKEdit::Close(Patch &patch) {
    if (!patch.m_Journal)
        return {};
    const std::shared_ptr<Patch::Journal> data = patch.m_Journal;
    if (data->Editor != this)
        return Failure(Error::InvalidState,
                       "The Patch belongs to another Behavior editor.");

    PatchState state;
    {
        std::lock_guard<std::mutex> lock(data->Mutex);
        state = data->State;
    }
    if (state == PatchState::Closed || state == PatchState::Failed) {
        patch.m_Journal.reset();
        return {};
    }

    CloseAdmission(*data);
    if (m_Thread != std::this_thread::get_id() || Deferred() ||
        state == PatchState::Pending) {
        bool queue = false;
        {
            std::lock_guard<std::mutex> lock(data->Mutex);
            data->State = PatchState::Closing;
            if (!data->Queued) {
                data->Queued = true;
                queue = true;
            }
        }
        if (queue)
            Queue({Request::Kind::Close, {}, data});
        Status queued;
        queued.Message = "Behavior Patch close is queued for the game-thread safe point.";
        queued.Details.Stage = Phase::Teardown;
        return queued;
    }

    Status status = Ready();
    if (status)
        status = CloseNow(data);
    if (status.Code != Error::RevertConflict)
        patch.m_Journal.reset();
    return status;
}

void CKEdit::ProcessFrame() {
    if (!Ready() || Deferred())
        return;
    struct ProcessingScope {
        bool &Flag;
        explicit ProcessingScope(bool &flag) : Flag(flag) { Flag = true; }
        ~ProcessingScope() { Flag = false; }
    } processing(m_Processing);

    std::vector<Request> requests;
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        requests.swap(m_Queue);
    }
    for (Request &request : requests) {
        const std::shared_ptr<Patch::Journal> &patch = request.Patch;
        if (!patch)
            continue;
        PatchState state;
        {
            std::lock_guard<std::mutex> lock(patch->Mutex);
            patch->Queued = false;
            state = patch->State;
        }

        if (request.Action == Request::Kind::Apply) {
            if (state == PatchState::Closing || patch.use_count() == 1) {
                std::lock_guard<std::mutex> lock(patch->Mutex);
                patch->State = PatchState::Closed;
                continue;
            }
            if (state != PatchState::Pending)
                continue;
            Status status = ApplyNow(request.Candidate, patch);
            bool close = false;
            {
                std::lock_guard<std::mutex> lock(patch->Mutex);
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
            }
            if (close) {
                if (status)
                    (void) CloseNow(patch);
                else {
                    std::lock_guard<std::mutex> lock(patch->Mutex);
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

} // namespace BML::Behavior
