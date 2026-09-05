#include "Behavior/CKEdit.h"

#include <algorithm>
#include <array>
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

Stamp Capture(NativeRef native) {
    return {static_cast<CK_ID>(native.Id),
            const_cast<CKObject *>(
                static_cast<const CKObject *>(native.Address))};
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
    if (port.Interface != 0) {
        return kPlannedParameter |
               (static_cast<ParameterId>(port.Owner.Value) << 32u) |
               0x80000000u | port.Interface;
    }
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

bool ContainsLink(CKBehavior *graph, CKBehaviorLink *link) {
    if (!graph || !link)
        return false;
    for (int index = 0; index < graph->GetSubBehaviorLinkCount(); ++index) {
        if (graph->GetSubBehaviorLink(index) == link)
            return true;
    }
    return false;
}

bool ContainsNode(CKBehavior *graph, CKBehavior *node) {
    // Membership is the graph fact used by scheduling. Retail CK2 may retain
    // a cached GetParent() value after removal, so parent identity alone does
    // not prove that a Behavior is still a sub-behavior.
    if (!graph || !node)
        return false;
    for (int index = 0; index < graph->GetSubBehaviorCount(); ++index) {
        if (graph->GetSubBehavior(index) == node)
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
        // Inputs owned by a Parameter Operation disappear with the Operation;
        // teardown never restores their previous source.
        bool OwnedTarget = false;
        // Set once this Pin has been handed back to its previous source. A
        // Conflicted Patch is closed again later, and a Pin that already
        // reverted must not be touched twice.
        bool Reverted = false;
    };

    struct Destination {
        Stamp Source;
        Stamp Target;
    };

    // A value written straight into a Local. Before and Expected are ordinary
    // CKParameterLocal objects so the registered Virtools value semantics own
    // every non-trivial representation kept by the journal.
    struct Written {
        Stamp Parameter;
        GraphEndpoint Slot;
        Stamp Before;
        Stamp Expected;
        bool Reverted = false;
    };

    struct Interface {
        std::uint32_t Identity = 0;
        Stamp Behavior;
        Stamp Port;
        SlotKind Kind = SlotKind::Input;
    };

    struct Operation {
        Stamp Owner;
        Stamp Value;
    };

    struct Replacement {
        struct PortPair {
            Stamp Original;
            Stamp Installed;
            SlotKind Kind = SlotKind::Input;
            int Index = -1;
        };

        struct LinkEndpoint {
            Stamp Link;
            Stamp OriginalSource;
            Stamp OriginalSink;
            Stamp InstalledSource;
            Stamp InstalledSink;
            bool Applied = false;
        };

        struct InputSource {
            Stamp Input;
            Stamp OriginalDirect;
            Stamp OriginalShared;
            Stamp InstalledDirect;
            Stamp InstalledShared;
            bool Applied = false;
        };

        struct Destination {
            Stamp OriginalSource;
            Stamp InstalledSource;
            Stamp OriginalParameter;
            Stamp InstalledParameter;
            bool Applied = false;
        };

        Stamp Original;
        Stamp Installed;
        std::vector<PortPair> Ports;
        std::vector<LinkEndpoint> Links;
        std::vector<InputSource> Inputs;
        std::vector<Destination> Destinations;
        bool OriginalRemoved = false;
        bool Restored = false;
    };

    CKEdit *Editor = nullptr;
    mutable std::mutex Mutex;
    PatchState State = PatchState::Pending;
    Status LastStatus;
    bool Queued = false;
    bool Published = false;
    bool GraphObserved = false;
    bool RestoredEdited = false;
    bool RestoredGraph = false;
    bool ReplacementClaim = false;
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
    // Existing Blocks whose parameter state or relations this Patch changes.
    // Apply and successful teardown each send exactly one EDITED callback.
    std::vector<Stamp> EditedNodes;
    // Apply can fail after only part of EditedNodes has observed the proposed
    // graph. Those Blocks alone observe the checked inverse.
    std::vector<Stamp> ObservedEditedNodes;
    std::vector<Link> Links;
    std::vector<Binding> Binds;
    std::vector<Written> Values;
    std::vector<Destination> Pushes;
    std::vector<Interface> Ports;
    std::vector<Operation> Operations;
    std::vector<Replacement> Replacements;
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
        m_Replacing.erase(graphId);
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

Status CKEdit::Add(Edit &edit, BlockSpec block, Node &out, NodeRole role) {
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
    if (m_Replacing.contains(graphId)) {
        return Failure(
            Error::SourceConflict,
            "This graph already has an active Node replacement.");
    }

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
        if (bind.Target.Slot.Kind != SlotKind::InputParameter &&
            bind.Target.Slot.Kind != SlotKind::Target)
            continue;
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
    if (!checked.Replacements.empty())
        patch->ReplacementClaim = m_Replacing.insert(graphId).second;

    std::unordered_map<std::uint32_t, Stamp> nodes;
    nodes.emplace(edit.Graph().Value, patch->Graph);
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
        nodes.emplace(node.Handle.Value, Capture(native));
    }

    const auto fail = [&](Status failure) {
        Status reverted = Undo(*patch);
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
        return found == nodes.end()
            ? nullptr
            : Resolve<CKBehavior>(m_Context, found->second, CKCID_BEHAVIOR);
    };
    const auto graphFor = [&]() -> CKBehavior * {
        return Resolve<CKBehavior>(m_Context, patch->Graph, CKCID_BEHAVIOR);
    };
    const auto validateNodes = [&]() -> Status {
        CKBehavior *currentGraph = graphFor();
        if (!currentGraph || currentGraph->IsUsingFunction()) {
            return Failure(
                Error::GraphChanged,
                "The edited Behavior graph changed identity during a callback.",
                CKERR_INVALIDOBJECT);
        }
        for (const auto &[handle, stamp] : nodes) {
            CKBehavior *current = Resolve<CKBehavior>(
                m_Context, stamp, CKCID_BEHAVIOR);
            if (!current || (handle != edit.Graph().Value &&
                             current->GetParent() != currentGraph)) {
                return Failure(
                    Error::GraphChanged,
                    "An Edit Node changed identity during a callback.",
                    CKERR_INVALIDOBJECT);
            }
        }
        for (Stamp stamp : patch->Nodes) {
            CKBehavior *current = Resolve<CKBehavior>(
                m_Context, stamp, CKCID_BEHAVIOR);
            if (!current || current->GetParent() != currentGraph) {
                return Failure(
                    Error::GraphChanged,
                    "An Edit-owned Block changed identity during a callback.",
                    CKERR_INVALIDOBJECT);
            }
        }
        for (const Patch::Journal::Operation &item : patch->Operations) {
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
    };

    const auto visitPorts = [&](auto &&visitor) -> Status {
        for (CheckedFlow &flow : checked.Flows) {
            Status current = visitor(flow.Source);
            if (current)
                current = visitor(flow.Sink);
            if (!current)
                return current;
        }
        for (CheckedBind &bind : checked.Binds) {
            Status current = visitor(bind.Target);
            if (current && bind.Kind != BindKind::Literal)
                current = visitor(bind.Source);
            if (!current)
                return current;
        }
        for (CheckedPush &push : checked.Pushes) {
            Status current = visitor(push.Source);
            if (current)
                current = visitor(push.Destination);
            if (!current)
                return current;
        }
        for (CheckedTap &tap : checked.Taps) {
            Status current = visitor(tap.Source);
            if (!current)
                return current;
        }
        for (CheckedSplice &splice : checked.Splices) {
            Status current = visitor(splice.Input);
            if (current)
                current = visitor(splice.Output);
            if (!current)
                return current;
        }
        for (CheckedRedirect &redirect : checked.Redirects) {
            Status current = visitor(redirect.Sink);
            if (!current)
                return current;
        }
        return {};
    };

    // Borrowed graph Ports exist before any added Block can run a lifecycle
    // callback. Pin their exact CK identities now so a callback cannot replace
    // a peer Port with a same-shaped object and redirect this Edit silently.
    status = visitPorts([&](ResolvedPort &port) -> Status {
        const Edit::EditNode *node = edit.Find(port.Owner);
        if (!node || node->Block || port.Interface != 0 || port.Native)
            return {};
        CKBehavior *behavior = behaviorFor(port.Owner);
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
    if (!status)
        return fail(std::move(status));

    const auto parameterBeforeApply = [&](const ResolvedPort &port,
                                          CKObject *&value) -> Status {
        value = nullptr;
        if (port.Operation)
            return {};
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

    // CREATE every authored Block first. Their Block-defined control
    // interface is already present, but parameter relations and the single
    // final EDITED callback wait until all Nodes exist.
    std::unordered_map<std::uint32_t, BlockSpec> addedSpecs;
    for (const Edit::EditNode &node : edit.m_Nodes) {
        if (!node.Block)
            continue;
        auto [spec, inserted] = addedSpecs.emplace(
            node.Handle.Value, *node.Block);
        if (!inserted)
            return fail(Failure(Error::InvalidState,
                                "An added Node is declared more than once."));
        AttachResult added = m_Runtime.CreateInGraph(graph, spec->second);
        if (!added || !added.Block)
            return fail(added.Detail);
        const Stamp stamp = Capture(added.Block);
        nodes.emplace(node.Handle.Value, stamp);
        patch->Nodes.push_back(stamp);
        if (node.Role == NodeRole::Infrastructure)
            patch->InfrastructureNodes.push_back(stamp);
        status = validateNodes();
        if (!status)
            return fail(std::move(status));
        graph = graphFor();
    }

    std::unordered_map<std::uint32_t, Stamp> tapNodes;
    for (const CheckedTap &tap : checked.Taps) {
        BlockSpec observerSpec = HookBlock::Make(tap.Callback, 1, 0);
        AttachResult added = m_Runtime.CreateInGraph(graph, observerSpec);
        if (!added || !added.Block)
            return fail(added.Detail);
        status = m_Runtime.EditInGraph(added.Block, observerSpec);
        if (!status)
            return fail(std::move(status));
        const Stamp node = Capture(added.Block);
        tapNodes.emplace(tap.Ordinal, node);
        patch->Nodes.push_back(node);
        patch->InfrastructureNodes.push_back(node);
        status = validateNodes();
        if (!status)
            return fail(std::move(status));
        graph = graphFor();
    }

    const auto isAdded = [&](Node handle) {
        const Edit::EditNode *node = edit.Find(handle);
        return node && node->Block.has_value();
    };
    const auto rememberEdited = [&](CKBehavior *behavior) {
        if (!behavior || behavior == graph)
            return;
        const Stamp stamp = Capture(behavior);
        if (std::find(patch->EditedNodes.begin(), patch->EditedNodes.end(),
                      stamp) == patch->EditedNodes.end())
            patch->EditedNodes.push_back(stamp);
    };
    std::unordered_map<std::uint32_t, Stamp> interfacePorts;
    for (const InterfacePort &item : edit.m_Interface) {
        if (item.InBlockSpec)
            continue;
        CKBehavior *behavior = behaviorFor(item.Owner);
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
        case SlotKind::Local:
            created = behavior->CreateLocalParameter(
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
            {item.Identity, Capture(behavior), Capture(created),
             item.Slot.Kind});
        interfacePorts.emplace(item.Identity, Capture(created));
        if (!isAdded(item.Owner))
            rememberEdited(behavior);
    }

    std::unordered_map<std::uint32_t, Stamp> operations;
    for (const EditOperation &item : edit.m_Operations) {
        std::ostringstream name;
        name << "__BML_Operation_" << graph->GetID() << '_'
             << item.Handle.Value;
        CKParameterOperation *operation = m_Context->CreateCKParameterOperation(
            const_cast<CKSTRING>(name.str().c_str()), item.Operation,
            item.Result, item.Input1, item.Input2);
        if (!operation) {
            Status failed = Failure(
                Error::CreateFailed,
                "Virtools failed to create a Parameter Operation.",
                CKERR_OUTOFMEMORY);
            failed.Details.OperationGuid = item.Operation;
            return fail(std::move(failed));
        }
        if (!operation->GetOutParameter() ||
            !operation->GetOperationFunction()) {
            m_Context->DestroyObject(operation);
            Status failed = Failure(
                Error::OperationInvalid,
                "No native Parameter Operation function matches the requested type tuple.",
                CKERR_INVALIDPARAMETER);
            failed.Details.OperationGuid = item.Operation;
            return fail(std::move(failed));
        }
        const CKERROR added = graph->AddParameterOperation(operation);
        if (added != CK_OK) {
            m_Context->DestroyObject(operation);
            Status failed = Failure(
                Error::OperationInvalid,
                "Virtools rejected the Parameter Operation graph ownership.",
                added);
            failed.Details.OperationGuid = item.Operation;
            return fail(std::move(failed));
        }
        const Stamp stamp = Capture(operation);
        patch->Operations.push_back({Capture(graph), stamp});
        operations.emplace(item.Handle.Value, stamp);
    }

    const auto exactSlot = [&](CKBehavior *behavior, Stamp exact,
                               SlotKind kind, SlotInfo &slot) -> Status {
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
    };

    const auto liveSlot = [&](const ResolvedPort &port, SlotInfo &slot) {
        CKBehavior *behavior = behaviorFor(port.Owner);
        if (!behavior)
            return Failure(Error::GraphChanged,
                           "An Edit Node disappeared during Apply.",
                           CKERR_INVALIDOBJECT);
        Status resolved;
        if (port.Native) {
            resolved = exactSlot(
                behavior, Capture(port.Native), port.Slot.Kind, slot);
        } else if (port.Interface != 0) {
            const auto found = interfacePorts.find(port.Interface);
            resolved = found == interfacePorts.end()
                ? Failure(Error::GraphChanged,
                          "An Edit interface Port was not created.",
                          CKERR_INVALIDOBJECT)
                : exactSlot(behavior, found->second, port.Slot.Kind, slot);
        } else {
            resolved = m_Runtime.Resolve(behavior, port.Selector, slot);
        }
        if (!resolved)
            return resolved;
        const bool changedKind = slot.Kind != port.Slot.Kind;
        const bool changedType = port.Slot.Type.IsValid() &&
                                 slot.Type != port.Slot.Type;
        const bool changedIndexedSlot = !port.Native &&
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
    };

    // Added Blocks and Edit-declared interface Ports now exist. Bind every
    // remaining symbolic endpoint to the exact CK object that it denotes.
    status = visitPorts([&](ResolvedPort &port) -> Status {
        if (port.Native)
            return {};
        if (port.Operation) {
            const auto found = operations.find(port.Owner.Value);
            auto *operation = found == operations.end()
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
        CKBehavior *behavior = behaviorFor(port.Owner);
        SlotInfo live;
        Status current = liveSlot(port, live);
        if (!current)
            return current;
        CKObject *object = m_Runtime.ResolveSlotObject(behavior, live);
        if (!object)
            return Failure(Error::GraphChanged,
                           "A selected Behavior Port disappeared before publication.",
                           CKERR_INVALIDOBJECT);
        port.Native = Native(Capture(object));
        return {};
    });
    if (!status)
        return fail(std::move(status));

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

    // A replacement preserves the public graph role of an idle Node. CK2's
    // RemoveSubBehavior only removes parent membership; it deliberately keeps
    // the owner and native lifecycle state. That lets the Patch park the exact
    // original object and restore it without inventing DETACH/DELETE callbacks.
    std::unordered_map<CKObject *, CKObject *> replacementPorts;
    std::unordered_map<CKObject *, std::size_t> replacementOwners;
    std::unordered_set<CKObject *> installedPorts;
    std::unordered_set<CKBehavior *> originalNodes;
    std::unordered_set<CKParameter *> privateState;
    const auto slots = [](const Layout &layout, SlotKind kind) {
        std::vector<const SlotInfo *> result;
        for (const SlotInfo &slot : layout.Slots) {
            if (slot.Kind == kind)
                result.push_back(&slot);
        }
        std::sort(result.begin(), result.end(),
                  [](const SlotInfo *left, const SlotInfo *right) {
                      return left->Index < right->Index;
                  });
        return result;
    };
    const std::array<SlotKind, 5> publicKinds{
        SlotKind::Input, SlotKind::Output, SlotKind::Target,
        SlotKind::InputParameter, SlotKind::OutputParameter};

    for (const CheckedReplace &item : checked.Replacements) {
        CKBehavior *original = behaviorFor(item.Target);
        CKBehavior *installed = behaviorFor(item.Replacement);
        if (!original || !installed || original == installed ||
            original->GetParent() != graph || installed->GetParent() != graph) {
            return fail(Failure(
                Error::InvalidGraphLocality,
                "A replacement Node left the target graph before Apply.",
                CKERR_INVALIDOBJECT));
        }
        if (original->IsActive()) {
            return fail(Failure(
                Error::Busy,
                "A running Behavior Node cannot be replaced."));
        }
        for (int index = 0; index < original->GetInputCount(); ++index) {
            CKBehaviorIO *io = original->GetInput(index);
            if (io && io->IsActive())
                return fail(Failure(
                    Error::Busy,
                    "A Behavior Node with an active In cannot be replaced."));
        }
        for (int index = 0; index < original->GetOutputCount(); ++index) {
            CKBehaviorIO *io = original->GetOutput(index);
            if (io && io->IsActive())
                return fail(Failure(
                    Error::Busy,
                    "A Behavior Node with an active Out cannot be replaced."));
        }

        Patch::Journal::Replacement replacement;
        replacement.Original = Capture(original);
        replacement.Installed = Capture(installed);
        originalNodes.insert(original);
        const std::size_t ownerIndex = patch->Replacements.size();
        const Layout originalLayout = m_Runtime.Describe(original);
        const Layout installedLayout = m_Runtime.Describe(installed);
        for (SlotKind kind : publicKinds) {
            const auto before = slots(originalLayout, kind);
            const auto after = slots(installedLayout, kind);
            if (before.size() != after.size()) {
                return fail(Failure(
                    Error::InterfaceUnsupported,
                    "A replacement Block has a different public Behavior interface."));
            }
            for (std::size_t index = 0; index < before.size(); ++index) {
                const SlotInfo &oldSlot = *before[index];
                const SlotInfo &newSlot = *after[index];
                if (oldSlot.Index != newSlot.Index ||
                    oldSlot.Name != newSlot.Name ||
                    oldSlot.Occurrence != newSlot.Occurrence ||
                    oldSlot.Type != newSlot.Type) {
                    return fail(Failure(
                        Error::InterfaceUnsupported,
                        "A replacement Block changed a public Behavior port."));
                }
                CKObject *oldPort = m_Runtime.ResolveSlotObject(
                    original, oldSlot);
                CKObject *newPort = m_Runtime.ResolveSlotObject(
                    installed, newSlot);
                if (!oldPort || !newPort) {
                    return fail(Failure(
                        Error::GraphChanged,
                        "A replacement public port disappeared before Apply.",
                        CKERR_INVALIDOBJECT));
                }
                replacement.Ports.push_back(
                    {Capture(oldPort), Capture(newPort), kind, oldSlot.Index});
                replacementPorts.emplace(oldPort, newPort);
                replacementOwners.emplace(oldPort, ownerIndex);
                installedPorts.insert(newPort);
            }
        }
        for (const SlotInfo &slot : originalLayout.Slots) {
            if (slot.Kind != SlotKind::Setting &&
                slot.Kind != SlotKind::Local)
                continue;
            if (auto *state = CKParameter::Cast(
                    m_Runtime.ResolveSlotObject(original, slot)))
                privateState.insert(state);
        }

        if (const char *name = original->GetName())
            installed->SetName(const_cast<CKSTRING>(name));
        installed->SetPriority(original->GetPriority());
        patch->Replacements.push_back(std::move(replacement));
    }

    const auto isPrivateParameter = [&](CKParameter *parameter) {
        if (!parameter)
            return false;
        if (privateState.contains(parameter))
            return true;
        CKBehavior *owner = CKBehavior::Cast(parameter->GetOwner());
        return owner && originalNodes.contains(owner) &&
            !replacementPorts.contains(parameter);
    };

    // Preserve the original Pin and Target source relations in the
    // replacement BlockSpec. Explicit Bind actions that follow this step may
    // deliberately override them before the Block's single EDITED callback.
    for (std::size_t index = 0; index < checked.Replacements.size(); ++index) {
        const CheckedReplace &item = checked.Replacements[index];
        const auto spec = addedSpecs.find(item.Replacement.Value);
        if (spec == addedSpecs.end())
            return fail(Failure(Error::InvalidState,
                                "A replacement Block lost its configuration."));
        BlockSpec &block = spec->second;
        for (const Patch::Journal::Replacement::PortPair &pair :
             patch->Replacements[index].Ports) {
            if (pair.Kind != SlotKind::InputParameter &&
                pair.Kind != SlotKind::Target)
                continue;
            auto *oldInput = Resolve<CKParameterIn>(
                m_Context, pair.Original, CKCID_PARAMETERIN);
            auto *newInput = Resolve<CKParameterIn>(
                m_Context, pair.Installed, CKCID_PARAMETERIN);
            if (!oldInput || !newInput)
                return fail(Failure(
                    Error::GraphChanged,
                    "A replacement Pin changed identity before Apply.",
                    CKERR_INVALIDOBJECT));

            CKParameterIn *shared = oldInput->GetSharedSource();
            CKParameter *direct = shared ? nullptr : oldInput->GetDirectSource();
            if (shared) {
                const auto mapped = replacementPorts.find(shared);
                if (mapped != replacementPorts.end())
                    shared = CKParameterIn::Cast(mapped->second);
                if (!shared)
                    return fail(Failure(
                        Error::TypeMismatch,
                        "A replacement could not preserve a shared Pin source."));
                if (pair.Kind == SlotKind::Target)
                    block.TargetShared(newInput->GetGUID(), shared);
                else
                    block.Input(Slot::At(pair.Kind, pair.Index,
                                         newInput->GetGUID()),
                                Parameter::Binding::Shared(shared));
            } else if (direct) {
                if (isPrivateParameter(direct))
                    return fail(Failure(
                        Error::InterfaceUnsupported,
                        "A replacement public Pin depends on the original Block's private state."));
                const auto mapped = replacementPorts.find(direct);
                if (mapped != replacementPorts.end())
                    direct = CKParameter::Cast(mapped->second);
                if (!direct)
                    return fail(Failure(
                        Error::TypeMismatch,
                        "A replacement could not preserve a direct Pin source."));
                if (pair.Kind == SlotKind::Target)
                    block.TargetSource(newInput->GetGUID(), direct);
                else
                    block.Input(Slot::At(pair.Kind, pair.Index,
                                         newInput->GetGUID()),
                                Parameter::Binding::Direct(direct));
            }
        }
    }

    const auto relationOwnerInGraph = [&](CKObject *parameter) {
        CKObject *owner = nullptr;
        if (auto *input = CKParameterIn::Cast(parameter))
            owner = input->GetOwner();
        else if (auto *value = CKParameter::Cast(parameter))
            owner = value->GetOwner();
        if (CKBehavior *behavior = CKBehavior::Cast(owner))
            return behavior == graph || behavior->GetParent() == graph;
        if (auto *operation = CKParameterOperation::Cast(owner))
            return operation->GetOwner() == graph;
        return false;
    };

    // Find every graph-local Pin that reads a public parameter of a parked
    // Node. CKParameterIn does not expose reverse users, so CK2's exact
    // ParameterIn object list is the authoritative relation inventory.
    const int inputCount = m_Context->GetObjectsCountByClassID(
        CKCID_PARAMETERIN);
    CK_ID *inputIds = m_Context->GetObjectsListByClassID(CKCID_PARAMETERIN);
    for (int index = 0; index < inputCount; ++index) {
        auto *input = CKParameterIn::Cast(m_Context->GetObject(inputIds[index]));
        if (!input || input->IsToBeDeleted() ||
            replacementPorts.contains(input) || installedPorts.contains(input))
            continue;
        CKParameterIn *oldShared = input->GetSharedSource();
        CKParameter *oldDirect = oldShared ? nullptr : input->GetDirectSource();
        CKObject *oldSource = oldShared
            ? static_cast<CKObject *>(oldShared)
            : static_cast<CKObject *>(oldDirect);
        const auto mapped = replacementPorts.find(oldSource);
        if (mapped == replacementPorts.end()) {
            if (oldDirect && isPrivateParameter(oldDirect)) {
                return fail(Failure(
                    Error::InvalidGraphLocality,
                    "A parameter outside the replaced Node reads its private state."));
            }
            continue;
        }
        if (!relationOwnerInGraph(input)) {
            return fail(Failure(
                Error::InvalidGraphLocality,
                "A parameter outside the target graph reads the replaced Node."));
        }
        const std::size_t owner = replacementOwners.at(oldSource);
        auto &change = patch->Replacements[owner].Inputs.emplace_back();
        change.Input = Capture(input);
        change.OriginalDirect = Capture(oldDirect);
        change.OriginalShared = Capture(oldShared);
        if (oldShared) {
            auto *newShared = CKParameterIn::Cast(mapped->second);
            if (!newShared)
                return fail(Failure(
                    Error::TypeMismatch,
                    "A shared Pin source does not map to a replacement Pin."));
            change.InstalledShared = Capture(newShared);
        } else {
            auto *newDirect = CKParameter::Cast(mapped->second);
            if (!newDirect)
                return fail(Failure(
                    Error::TypeMismatch,
                    "A direct Pin source does not map to a replacement parameter."));
            change.InstalledDirect = Capture(newDirect);
        }
    }

    // Settings, Locals and other non-public parameters stay with the parked
    // implementation. A graph relation that writes one of them cannot be
    // transferred to the replacement without pretending it is public state.
    const int outputCount = m_Context->GetObjectsCountByClassID(
        CKCID_PARAMETEROUT);
    CK_ID *outputIds = m_Context->GetObjectsListByClassID(
        CKCID_PARAMETEROUT);
    for (int index = 0; index < outputCount; ++index) {
        auto *source = CKParameterOut::Cast(
            m_Context->GetObject(outputIds[index]));
        if (!source || source->IsToBeDeleted() ||
            installedPorts.contains(source))
            continue;
        for (int destinationIndex = 0;
             destinationIndex < source->GetDestinationCount();
             ++destinationIndex) {
            if (isPrivateParameter(source->GetDestination(destinationIndex)))
                return fail(Failure(
                    Error::InterfaceUnsupported,
                    "A graph Pout writes the original Block's private state."));
        }
    }

    for (const auto &[oldObject, newObject] : replacementPorts) {
        auto *oldOutput = CKParameterOut::Cast(oldObject);
        auto *newOutput = CKParameterOut::Cast(newObject);
        if (!oldOutput || !newOutput)
            continue;
        std::vector<CKParameter *> destinations;
        for (int index = 0; index < oldOutput->GetDestinationCount(); ++index)
            destinations.push_back(oldOutput->GetDestination(index));
        for (CKParameter *destination : destinations) {
            if (!destination || destination->IsToBeDeleted())
                return fail(Failure(
                    Error::GraphChanged,
                    "A Pout destination disappeared before replacement.",
                    CKERR_INVALIDOBJECT));
            if (privateState.contains(destination))
                return fail(Failure(
                    Error::InvalidGraphLocality,
                    "A Pout destination belongs to the original Block's private state."));
            CKParameter *originalDestination = destination;
            const auto mapped = replacementPorts.find(destination);
            if (mapped != replacementPorts.end())
                destination = CKParameter::Cast(mapped->second);
            if (!destination || !relationOwnerInGraph(destination))
                return fail(Failure(
                    Error::InvalidGraphLocality,
                    "A Pout destination lies outside the replacement graph."));
            if (ContainsDestination(newOutput, destination))
                return fail(Failure(
                    Error::SourceConflict,
                    "The replacement Pout already owns an original destination."));
            const std::size_t owner = replacementOwners.at(oldObject);
            patch->Replacements[owner].Destinations.push_back(
                {Capture(oldOutput), Capture(newOutput),
                 Capture(originalDestination), Capture(destination)});
        }
    }

    for (auto &replacement : patch->Replacements) {
        for (auto &change : replacement.Inputs) {
            CKParameterIn *input = Resolve<CKParameterIn>(
                m_Context, change.Input, CKCID_PARAMETERIN);
            CKERROR error = CKERR_INVALIDOBJECT;
            if (input && change.InstalledShared.Id) {
                error = input->ShareSourceWith(Resolve<CKParameterIn>(
                    m_Context, change.InstalledShared, CKCID_PARAMETERIN));
            } else if (input) {
                error = input->SetDirectSource(Resolve<CKParameter>(
                    m_Context, change.InstalledDirect, CKCID_PARAMETER));
            }
            if (error != CK_OK)
                return fail(Failure(
                    Error::TypeMismatch,
                    "Virtools rejected a replacement Pin source.", error));
            change.Applied = true;
            if (CKBehavior *owner = CKBehavior::Cast(input->GetOwner()))
                rememberEdited(owner);
        }
        for (auto &change : replacement.Destinations) {
            CKParameterOut *oldOutput = Resolve<CKParameterOut>(
                m_Context, change.OriginalSource, CKCID_PARAMETEROUT);
            CKParameterOut *newOutput = Resolve<CKParameterOut>(
                m_Context, change.InstalledSource, CKCID_PARAMETEROUT);
            CKParameter *originalDestination = Resolve<CKParameter>(
                m_Context, change.OriginalParameter, CKCID_PARAMETER);
            CKParameter *installedDestination = Resolve<CKParameter>(
                m_Context, change.InstalledParameter, CKCID_PARAMETER);
            if (!oldOutput || !newOutput || !originalDestination ||
                !installedDestination)
                return fail(Failure(
                    Error::GraphChanged,
                    "A replacement Pout relation changed before Apply.",
                    CKERR_INVALIDOBJECT));
            const CKERROR error = newOutput->AddDestination(
                installedDestination, TRUE);
            if (error != CK_OK)
                return fail(Failure(
                    Error::TypeMismatch,
                    "Virtools rejected a replacement Pout destination.", error));
            oldOutput->RemoveDestination(originalDestination);
            change.Applied = true;
        }
    }

    for (const CheckedBind &bind : checked.Binds) {
        CKObject *targetParameter = nullptr;
        status = parameter(bind.Target, targetParameter);
        if (!status)
            return fail(std::move(status));

        const Edit::EditNode *targetNode = edit.Find(bind.Target.Owner);
        if (targetNode && targetNode->Block) {
            const auto found = addedSpecs.find(bind.Target.Owner.Value);
            if (found == addedSpecs.end())
                return fail(Failure(Error::InvalidState,
                                    "An added Block lost its configuration."));
            BlockSpec &spec = found->second;
            SlotInfo liveTarget;
            status = liveSlot(bind.Target, liveTarget);
            if (!status)
                return fail(std::move(status));
            Slot target = Slot::At(
                liveTarget.Kind, liveTarget.Index, liveTarget.Type);

            if (bind.Kind == BindKind::Literal) {
                if (target.Kind == SlotKind::InputParameter) {
                    spec.Input(std::move(target), bind.Literal);
                } else if (target.Kind == SlotKind::Local) {
                    spec.Local(std::move(target), bind.Literal);
                } else if (target.Kind == SlotKind::Target) {
                    spec.m_TargetMode = bind.Literal.IsNull()
                        ? TargetMode::ExplicitNull : TargetMode::Explicit;
                    spec.m_TargetType = bind.Target.Slot.Type;
                    spec.m_TargetValue = Parameter::Binding(bind.Literal);
                } else {
                    return fail(Failure(
                        Error::TypeMismatch,
                        "A Block value requires a Pin, Local, or Target."));
                }
                continue;
            }

            CKObject *sourceObject = nullptr;
            status = parameter(bind.Source, sourceObject);
            if (!status)
                return fail(std::move(status));
            if (bind.Kind == BindKind::Direct) {
                CKParameter *source = CKParameter::Cast(sourceObject);
                if (!source)
                    return fail(Failure(
                        Error::TypeMismatch,
                        "A direct Bind source is not a stored parameter."));
                if (target.Kind == SlotKind::Target)
                    spec.TargetSource(bind.Target.Slot.Type, source);
                else
                    spec.Input(std::move(target),
                               Parameter::Binding::Direct(source));
            } else {
                CKParameterIn *source = CKParameterIn::Cast(sourceObject);
                if (!source)
                    return fail(Failure(Error::TypeMismatch,
                                        "A shared Bind source is not a Pin."));
                if (target.Kind == SlotKind::Target)
                    spec.TargetShared(bind.Target.Slot.Type, source);
                else
                    spec.Input(std::move(target),
                               Parameter::Binding::Shared(source));
            }
            continue;
        }

        if (!bind.Target.Operation)
            rememberEdited(behaviorFor(bind.Target.Owner));
        // A Local holds its value itself, so there is no source relation to
        // install. Keep both journal values in ordinary CK parameters so the
        // registered Virtools copy and destruction semantics remain in force.
        if (bind.Target.Slot.Kind == SlotKind::Local) {
            auto *stored = CKParameterLocal::Cast(targetParameter);
            if (!stored)
                return fail(Failure(
                    Error::GraphChanged,
                    "A written value names a slot that is not a stored parameter."));
            Patch::Journal::Written change;
            change.Parameter = Capture(stored);
            change.Slot = DescribeLocal(stored);
            CKParameterLocal *before = nullptr;
            status = Parameter::Clone(m_Context, stored, before);
            if (!status)
                return fail(std::move(status));
            change.Before = Capture(before);
            patch->Values.push_back(std::move(change));
            status = Parameter::Write(m_Context, stored, bind.Literal);
            if (!status)
                return fail(std::move(status));
            CKParameterLocal *expected = nullptr;
            status = Parameter::Clone(m_Context, stored, expected);
            if (!status)
                return fail(std::move(status));
            patch->Values.back().Expected = Capture(expected);
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
        change.OwnedTarget = bind.Target.Operation;

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
        if (!isAdded(push.Source.Owner))
            rememberEdited(behaviorFor(push.Source.Owner));
    }

    const auto validatePorts = [&]() -> Status {
        Status current = validateNodes();
        if (!current)
            return current;
        current = visitPorts([&](ResolvedPort &port) -> Status {
            if (port.Operation) {
                CKObject *native = nullptr;
                return parameter(port, native);
            }
            SlotInfo live;
            return liveSlot(port, live);
        });
        if (!current)
            return current;
        for (const Patch::Journal::Interface &item : patch->Ports) {
            CKBehavior *owner = Resolve<CKBehavior>(
                m_Context, item.Behavior, CKCID_BEHAVIOR);
            SlotInfo live;
            current = exactSlot(owner, item.Port, item.Kind, live);
            if (!current)
                return current;
        }
        for (const Patch::Journal::Replacement &replacement :
             patch->Replacements) {
            CKBehavior *original = Resolve<CKBehavior>(
                m_Context, replacement.Original, CKCID_BEHAVIOR);
            CKBehavior *installed = Resolve<CKBehavior>(
                m_Context, replacement.Installed, CKCID_BEHAVIOR);
            if (!original || !installed)
                return Failure(
                    Error::GraphChanged,
                    "A replacement Node changed identity during Apply.",
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
        return {};
    };

    const auto captureNormalization = [&](Stamp receiver) -> Status {
        CKBehavior *behavior = Resolve<CKBehavior>(
            m_Context, receiver, CKCID_BEHAVIOR);
        if (!behavior || behavior->GetParent() != graphFor()) {
            return Failure(
                Error::GraphChanged,
                "A Block changed identity during its EDITED callback.",
                CKERR_INVALIDOBJECT);
        }
        const std::uint64_t behaviorId = static_cast<std::uint32_t>(
            behavior->GetID());
        for (Patch::Journal::Written &change : patch->Values) {
            if (change.Slot.Node != behaviorId)
                continue;
            CKParameterLocal *stored = Resolve<CKParameterLocal>(
                m_Context, change.Parameter, CKCID_PARAMETERLOCAL);
            if (!stored) {
                return Failure(
                    Error::GraphChanged,
                    "An EDITED callback replaced a Local owned by the Patch.",
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
        for (Patch::Journal::Binding &change : patch->Binds) {
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
    };

    const auto validateRelations = [&]() -> Status {
        for (const Patch::Journal::Written &change : patch->Values) {
            CKParameterLocal *stored = Resolve<CKParameterLocal>(
                m_Context, change.Parameter, CKCID_PARAMETERLOCAL);
            CKParameterLocal *expected = Resolve<CKParameterLocal>(
                m_Context, change.Expected, CKCID_PARAMETERLOCAL);
            if (!stored || !expected) {
                return Failure(
                    Error::GraphChanged,
                    "A written Local changed identity during an EDITED callback.",
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
                    "A different Block changed a written Local during an EDITED callback.");
            }
        }
        for (const Patch::Journal::Binding &change : patch->Binds) {
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
        for (const Patch::Journal::Destination &change : patch->Pushes) {
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
             patch->Replacements) {
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
                if (!link ||
                    Capture(link->GetInBehaviorIO()) != change.InstalledSource ||
                    Capture(link->GetOutBehaviorIO()) != change.InstalledSink) {
                    return Failure(
                        Error::GraphChanged,
                        "A replacement Link endpoint changed during Apply.",
                        CKERR_INVALIDOBJECT);
                }
            }
        }
        return {};
    };

    const auto validateEdited = [&](Stamp receiver,
                                    bool captureFinal) -> Status {
        Status current = validatePorts();
        if (current && captureFinal)
            current = captureNormalization(receiver);
        if (current)
            current = validateRelations();
        return current;
    };

    // Finish each authored Block only after every graph parameter relation is
    // present. Runtime sends the Block's one lifecycle EDITED callback and
    // reflects the resulting layout before any control Flow can reach it.
    for (const Edit::EditNode &node : edit.m_Nodes) {
        if (!node.Block)
            continue;
        const auto behavior = nodes.find(node.Handle.Value);
        const auto spec = addedSpecs.find(node.Handle.Value);
        if (behavior == nodes.end() || spec == addedSpecs.end())
            return fail(Failure(Error::InvalidState,
                                "An added Block disappeared before EDITED."));
        CKBehavior *live = Resolve<CKBehavior>(
            m_Context, behavior->second, CKCID_BEHAVIOR);
        if (!live)
            return fail(Failure(Error::GraphChanged,
                                "An added Block disappeared before EDITED.",
                                CKERR_INVALIDOBJECT));
        status = m_Runtime.EditInGraph(live, spec->second);
        if (status)
            status = validateEdited(behavior->second, false);
        if (!status)
            return fail(std::move(status));
        graph = graphFor();
    }

    // Keep each native Link object and its current delay state. Only its exact
    // endpoint objects change, so a delayed activation remains the same Link
    // in CK2's scheduler rather than being recreated or guessed from fields.
    std::vector<CKBehaviorLink *> graphLinks;
    graphLinks.reserve(static_cast<std::size_t>(graph->GetSubBehaviorLinkCount()));
    for (int index = 0; index < graph->GetSubBehaviorLinkCount(); ++index)
        graphLinks.push_back(graph->GetSubBehaviorLink(index));
    for (CKBehaviorLink *link : graphLinks) {
        if (!link)
            return fail(Failure(Error::GraphChanged,
                                "A graph Link disappeared during replacement."));
        CKBehaviorIO *oldSource = link->GetInBehaviorIO();
        CKBehaviorIO *oldSink = link->GetOutBehaviorIO();
        const auto source = replacementPorts.find(oldSource);
        const auto sink = replacementPorts.find(oldSink);
        if (source == replacementPorts.end() &&
            sink == replacementPorts.end())
            continue;
        auto *newSource = source == replacementPorts.end()
            ? oldSource : static_cast<CKBehaviorIO *>(source->second);
        auto *newSink = sink == replacementPorts.end()
            ? oldSink : static_cast<CKBehaviorIO *>(sink->second);
        const std::size_t owner = source != replacementPorts.end()
            ? replacementOwners.at(oldSource)
            : replacementOwners.at(oldSink);
        auto &change = patch->Replacements[owner].Links.emplace_back();
        change.Link = Capture(link);
        change.OriginalSource = Capture(oldSource);
        change.OriginalSink = Capture(oldSink);
        change.InstalledSource = Capture(newSource);
        change.InstalledSink = Capture(newSink);
        CKERROR error = CK_OK;
        if (newSource != oldSource)
            error = link->SetInBehaviorIO(newSource);
        if (error == CK_OK && newSink != oldSink)
            error = link->SetOutBehaviorIO(newSink);
        if (error != CK_OK) {
            if (newSource != oldSource)
                (void) link->SetInBehaviorIO(oldSource);
            return fail(Failure(
                Error::GraphChanged,
                "Virtools rejected a replacement Link endpoint.", error));
        }
        change.Applied = true;
    }

    for (std::size_t index = 0; index < checked.Replacements.size(); ++index) {
        const CheckedReplace &item = checked.Replacements[index];
        auto &replacement = patch->Replacements[index];
        CKBehavior *original = Resolve<CKBehavior>(
            m_Context, replacement.Original, CKCID_BEHAVIOR);
        graph = graphFor();
        if (!graph || !original || graph->RemoveSubBehavior(original) != original)
            return fail(Failure(
                Error::GraphChanged,
                "Virtools could not park the original Behavior Node.",
                CKERR_INVALIDOBJECT));
        if (ContainsNode(graph, original))
            return fail(Failure(
                Error::GraphChanged,
                "Virtools kept the parked Behavior Node in its graph.",
                CKERR_INVALIDOBJECT));
        replacement.OriginalRemoved = true;
        nodes.erase(item.Target.Value);
    }
    status = validatePorts();
    if (status)
        status = validateRelations();
    if (!status)
        return fail(std::move(status));

    for (Stamp edited : patch->EditedNodes) {
        CKBehavior *behavior = Resolve<CKBehavior>(
            m_Context, edited, CKCID_BEHAVIOR);
        if (!behavior)
            return fail(Failure(Error::GraphChanged,
                                "A changed Block disappeared before EDITED."));
        patch->ObservedEditedNodes.push_back(edited);
        const int result = behavior->CallCallbackFunction(CKM_BEHAVIOREDITED);
        if (result != CK_OK)
            return fail(Failure(Error::CallbackFailed,
                                "A Block EDITED callback failed.", result));
        status = validateEdited(edited, true);
        if (!status)
            return fail(std::move(status));
        graph = graphFor();
    }

    // Control Flow is installed last, after every Block has observed and
    // validated its final parameter relations.
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

    for (const CheckedTap &tap : checked.Taps) {
        CKBehaviorIO *source = nullptr;
        status = control(tap.Source, source);
        const auto observer = tapNodes.find(tap.Ordinal);
        CKBehavior *block = observer == tapNodes.end()
            ? nullptr
            : Resolve<CKBehavior>(m_Context, observer->second,
                                  CKCID_BEHAVIOR);
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

    patch->GraphObserved = true;
    const int edited = graph->CallCallbackFunction(CKM_BEHAVIOREDITED);
    if (edited != CK_OK)
        return fail(Failure(Error::CallbackFailed,
                            "The graph EDITED callback failed.", edited));
    status = validatePorts();
    if (status)
        status = validateRelations();
    if (!status)
        return fail(std::move(status));
    graph = graphFor();
    if (!graph)
        return fail(Failure(
            Error::GraphChanged,
            "The graph changed identity during its EDITED callback.",
            CKERR_INVALIDOBJECT));

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
        patch->Handles.emplace(entry.first, entry.second);

    m_Active[graphId].insert(edit.Key());
    patch->Published = true;
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

Status CKEdit::Undo(Patch::Journal &patch) {
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
    const std::uint64_t graphId = patch.Graph.Id
        ? static_cast<std::uint32_t>(patch.Graph.Id) : 0;

    auto *graph = Resolve<CKBehavior>(m_Context, patch.Graph, CKCID_BEHAVIOR);
    const auto replacementConflict = [&](RevertSubject subject,
                                         std::string message) {
        Status conflict = Failure(Error::RevertConflict, std::move(message),
                                  CKERR_INVALIDOBJECT);
        conflict.Details.Stage = Phase::Teardown;
        noteConflict({subject, {}, {}, {}, {}, {}, conflict});
        return conflict;
    };
    if (graph && m_Replacing.contains(graphId) &&
        !patch.ReplacementClaim) {
        return replacementConflict(
            RevertSubject::Node,
            "This Patch cannot close while the graph contains an active Node replacement.");
    }
    if (graph) {
        for (const Patch::Journal::Replacement &replacement :
             patch.Replacements) {
            if (replacement.Restored)
                continue;
            CKBehavior *original = Resolve<CKBehavior>(
                m_Context, replacement.Original, CKCID_BEHAVIOR);
            CKBehavior *installed = Resolve<CKBehavior>(
                m_Context, replacement.Installed, CKCID_BEHAVIOR);
            if (!original || !installed || installed->GetParent() != graph ||
                ContainsNode(graph, original) ==
                    replacement.OriginalRemoved) {
                return replacementConflict(
                    RevertSubject::Node,
                    "A Node replacement changed after the Patch was published.");
            }
            for (const auto &change : replacement.Links) {
                if (!change.Applied)
                    continue;
                CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, change.Link, CKCID_BEHAVIORLINK);
                if (!link ||
                    Capture(link->GetInBehaviorIO()) != change.InstalledSource ||
                    Capture(link->GetOutBehaviorIO()) != change.InstalledSink) {
                    return replacementConflict(
                        RevertSubject::Link,
                        "A replacement Link changed after the Patch was published.");
                }
            }
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
                    return replacementConflict(
                        RevertSubject::PinSource,
                        "A replacement Pin source changed after the Patch was published.");
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
                    return replacementConflict(
                        RevertSubject::PinSource,
                        "A replacement Pout destination changed after the Patch was published.");
                }
            }
        }
    }
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
        auto *before = Resolve<CKParameterLocal>(
            m_Context, item->Before, CKCID_PARAMETERLOCAL);
        auto *expected = Resolve<CKParameterLocal>(
            m_Context, item->Expected, CKCID_PARAMETERLOCAL);
        const auto releaseCopies = [&] {
            if (before)
                m_Context->DestroyObject(before);
            if (expected && expected != before)
                m_Context->DestroyObject(expected);
            item->Before = {};
            item->Expected = {};
        };
        if (!stored) {
            // The parameter went with its Block, so there is nothing left to
            // hand back. The journal still owns and releases both copies.
            item->Reverted = true;
            releaseCopies();
            continue;
        }
        if (!before) {
            Status conflict = Failure(
                Error::RevertConflict,
                "The previous Local value disappeared from the Patch journal.",
                CKERR_INVALIDOBJECT);
            conflict.Details.Stage = Phase::Teardown;
            noteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            remember(std::move(conflict));
            continue;
        }
        if (item->Expected.Id && !expected) {
            Status conflict = Failure(
                Error::RevertConflict,
                "The installed Local value disappeared from the Patch journal.",
                CKERR_INVALIDOBJECT);
            conflict.Details.Stage = Phase::Teardown;
            noteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            remember(std::move(conflict));
            continue;
        }
        bool unchanged = true;
        Status compared;
        if (expected)
            compared = Parameter::Equal(
                m_Context, stored, expected, unchanged);
        if (!compared) {
            Status conflict = Failure(
                Error::RevertConflict,
                "The written Local value could not be compared through its Virtools type.",
                compared.CkError);
            conflict.Details.Stage = Phase::Teardown;
            noteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            remember(std::move(conflict));
            continue;
        }
        if (!unchanged) {
            Status conflict = Failure(
                Error::RevertConflict,
                "A written Local changed after the Patch was published.");
            conflict.Details.Stage = Phase::Teardown;
            noteConflict({RevertSubject::Value, item->Slot, {}, {}, {}, {},
                          conflict});
            remember(std::move(conflict));
            continue;
        }
        const CKERROR error = stored->CopyValue(before, FALSE);
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
            releaseCopies();
        }
    }

    for (auto item = patch.Binds.rbegin(); item != patch.Binds.rend(); ++item) {
        if (item->Reverted)
            continue;
        auto *input = Resolve<CKParameterIn>(
            m_Context, item->Input, CKCID_PARAMETERIN);
        if (item->OwnedTarget) {
            if (input) {
                if (input->GetSharedSource())
                    (void) input->ShareSourceWith(nullptr);
                else
                    (void) input->SetDirectSource(nullptr);
            }
            item->Reverted = true;
            continue;
        }
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
            : input->GetSharedSource() == nullptr &&
                input->GetDirectSource() == installedDirect;
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

    // Restore replacement relations while the installed Block still has its
    // complete public interface. Ports introduced by this Patch are removed
    // only after the original Node once again owns every native relation.
    const bool pinsRetained = std::any_of(
            patch.Binds.begin(), patch.Binds.end(),
            [](const Patch::Journal::Binding &item) { return !item.Reverted; }) ||
        std::any_of(
            patch.Values.begin(), patch.Values.end(),
            [](const Patch::Journal::Written &item) { return !item.Reverted; });
    if (!pinsRetained) {
        for (auto item = patch.Replacements.rbegin();
             item != patch.Replacements.rend(); ++item) {
            if (item->Restored)
                continue;
            graph = Resolve<CKBehavior>(
                m_Context, patch.Graph, CKCID_BEHAVIOR);
            if (!graph) {
                item->Restored = true;
                continue;
            }
            CKBehavior *original = Resolve<CKBehavior>(
                m_Context, item->Original, CKCID_BEHAVIOR);
            if (!original) {
                remember(replacementConflict(
                    RevertSubject::Node,
                    "The original replacement Node no longer exists."));
                continue;
            }
            if (item->OriginalRemoved) {
                const CKERROR added = graph->AddSubBehavior(original);
                if (added != CK_OK) {
                    remember(replacementConflict(
                        RevertSubject::Node,
                        "Virtools could not restore the original Behavior Node."));
                    continue;
                }
                item->OriginalRemoved = false;
            }

            bool restored = true;
            for (auto change = item->Links.rbegin();
                 change != item->Links.rend(); ++change) {
                if (!change->Applied)
                    continue;
                CKBehaviorLink *link = Resolve<CKBehaviorLink>(
                    m_Context, change->Link, CKCID_BEHAVIORLINK);
                CKBehaviorIO *source = ResolveIo(
                    m_Context, change->OriginalSource);
                CKBehaviorIO *sink = ResolveIo(
                    m_Context, change->OriginalSink);
                CKERROR error = link && source && sink
                    ? link->SetInBehaviorIO(source) : CKERR_INVALIDOBJECT;
                if (error == CK_OK)
                    error = link->SetOutBehaviorIO(sink);
                if (error != CK_OK) {
                    remember(replacementConflict(
                        RevertSubject::Link,
                        "Virtools could not restore a replacement Link."));
                    restored = false;
                    break;
                }
                change->Applied = false;
            }
            if (!restored)
                continue;

            for (auto change = item->Inputs.rbegin();
                 change != item->Inputs.rend(); ++change) {
                if (!change->Applied)
                    continue;
                CKParameterIn *input = Resolve<CKParameterIn>(
                    m_Context, change->Input, CKCID_PARAMETERIN);
                CKERROR error = CKERR_INVALIDOBJECT;
                if (input && change->OriginalShared.Id) {
                    error = input->ShareSourceWith(Resolve<CKParameterIn>(
                        m_Context, change->OriginalShared,
                        CKCID_PARAMETERIN));
                } else if (input) {
                    error = input->SetDirectSource(Resolve<CKParameter>(
                        m_Context, change->OriginalDirect,
                        CKCID_PARAMETER));
                }
                if (error != CK_OK) {
                    remember(replacementConflict(
                        RevertSubject::PinSource,
                        "Virtools could not restore a replacement Pin source."));
                    restored = false;
                    break;
                }
                change->Applied = false;
            }
            if (!restored)
                continue;

            for (auto change = item->Destinations.rbegin();
                 change != item->Destinations.rend(); ++change) {
                if (!change->Applied)
                    continue;
                CKParameterOut *oldOutput = Resolve<CKParameterOut>(
                    m_Context, change->OriginalSource,
                    CKCID_PARAMETEROUT);
                CKParameterOut *newOutput = Resolve<CKParameterOut>(
                    m_Context, change->InstalledSource,
                    CKCID_PARAMETEROUT);
                CKParameter *originalDestination = Resolve<CKParameter>(
                    m_Context, change->OriginalParameter,
                    CKCID_PARAMETER);
                CKParameter *installedDestination = Resolve<CKParameter>(
                    m_Context, change->InstalledParameter,
                    CKCID_PARAMETER);
                const CKERROR error = oldOutput && newOutput &&
                        originalDestination && installedDestination
                    ? oldOutput->AddDestination(originalDestination, TRUE)
                    : CKERR_INVALIDOBJECT;
                if (error != CK_OK) {
                    remember(replacementConflict(
                        RevertSubject::PinSource,
                        "Virtools could not restore a replacement Pout destination."));
                    restored = false;
                    break;
                }
                newOutput->RemoveDestination(installedDestination);
                change->Applied = false;
            }
            if (restored)
                item->Restored = true;
        }
    }

    const bool replacementsRestored = std::all_of(
        patch.Replacements.begin(), patch.Replacements.end(),
        [](const Patch::Journal::Replacement &item) {
            return item.Restored;
        });
    if (!replacementsRestored)
        return first;
    if (patch.ReplacementClaim) {
        m_Replacing.erase(graphId);
        patch.ReplacementClaim = false;
    }

    for (auto item = patch.Operations.rbegin();
         item != patch.Operations.rend(); ++item) {
        auto *operation = Resolve<CKParameterOperation>(
            m_Context, item->Value, CKCID_PARAMETEROPERATION);
        if (!operation)
            continue;
        const Stamp result = Capture(operation->GetOutParameter());
        const bool retained = std::any_of(
            patch.Binds.begin(), patch.Binds.end(),
            [&](const Patch::Journal::Binding &binding) {
                return !binding.Reverted && binding.InstalledDirect == result;
            });
        if (retained)
            continue;
        auto *owner = Resolve<CKBehavior>(
            m_Context, item->Owner, CKCID_BEHAVIOR);
        if (owner)
            (void) owner->RemoveParameterOperation(operation);
        m_Context->DestroyObject(operation);
        item->Value = {};
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
        case SlotKind::Local: {
            auto *parameter = static_cast<CKParameterLocal *>(port);
            const int index = behavior->GetLocalParameterPosition(parameter);
            removed = index >= 0 ? behavior->RemoveLocalParameter(index) : nullptr;
            break;
        }
        default:
            break;
        }
        if (removed == port) {
            m_Context->DestroyObject(removed);
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

    // Restore the graph before retiring Blocks introduced by this Patch. A
    // Block can still inspect its parent during native teardown, but no graph
    // relation may continue to depend on a Block once it is destroyed.
    for (auto item = patch.Nodes.rbegin(); item != patch.Nodes.rend(); ++item) {
        CKBehavior *node = Resolve<CKBehavior>(
            m_Context, *item, CKCID_BEHAVIOR);
        if (!node)
            continue;
        Status closed = m_Runtime.Close(node);
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

    // Closing an authored Block runs native teardown callbacks, and publishing
    // the Logical view can invoke author observers. Either may delete or
    // replace the graph, so no graph pointer from before those calls survives
    // this boundary.
    graph = Resolve<CKBehavior>(m_Context, patch.Graph, CKCID_BEHAVIOR);

    // Only now is the previous graph observable again: data relations and
    // interface changes are gone, added Blocks have retired, and the Logical
    // view has been republished. Notify only Blocks that actually observed the
    // candidate state when Apply failed before publication.
    const std::vector<Stamp> &observedBlocks = patch.Published
        ? patch.EditedNodes : patch.ObservedEditedNodes;
    if (graph && !pinsRetained && !patch.RestoredEdited &&
        !observedBlocks.empty()) {
        for (Stamp edited : observedBlocks) {
            graph = Resolve<CKBehavior>(
                m_Context, patch.Graph, CKCID_BEHAVIOR);
            if (!graph) {
                remember(Failure(
                    Error::GraphChanged,
                    "The graph disappeared before its Blocks observed Patch teardown.",
                    CKERR_INVALIDOBJECT));
                break;
            }
            CKBehavior *block = Resolve<CKBehavior>(
                m_Context, edited, CKCID_BEHAVIOR);
            if (!block)
                continue;
            const int result = block->CallCallbackFunction(CKM_BEHAVIOREDITED);
            if (result != CK_OK) {
                remember(Failure(
                    Error::CallbackFailed,
                    "A Block EDITED callback failed while the Patch closed.",
                    result));
                continue;
            }
            graph = Resolve<CKBehavior>(
                m_Context, patch.Graph, CKCID_BEHAVIOR);
            block = Resolve<CKBehavior>(m_Context, edited, CKCID_BEHAVIOR);
            if (!graph || !block || block->GetParent() != graph) {
                remember(Failure(
                    Error::GraphChanged,
                    "A Block or its graph changed identity during its teardown EDITED callback.",
                    CKERR_INVALIDOBJECT));
                continue;
            }
            (void) m_Runtime.Describe(block);
        }
        patch.RestoredEdited = true;
    }

    graph = Resolve<CKBehavior>(m_Context, patch.Graph, CKCID_BEHAVIOR);
    if (graph && !pinsRetained && !patch.RestoredGraph &&
        (patch.Published || patch.GraphObserved)) {
        const int result = graph->CallCallbackFunction(CKM_BEHAVIOREDITED);
        if (result != CK_OK)
            remember(Failure(Error::CallbackFailed,
                             "The graph EDITED callback failed while the Patch closed.",
                             result));
        else if (!Resolve<CKBehavior>(
                     m_Context, patch.Graph, CKCID_BEHAVIOR))
            remember(Failure(
                Error::GraphChanged,
                "The graph changed identity during its teardown EDITED callback.",
                CKERR_INVALIDOBJECT));
        patch.RestoredGraph = true;
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
        status = Undo(*patch);
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
