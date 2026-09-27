#ifndef BML_BEHAVIOR_EDIT_JOURNAL_H
#define BML_BEHAVIOR_EDIT_JOURNAL_H

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Behavior/CKEdit.h"
#include "Behavior/Core/Hash.h"

namespace BML::Behavior::Internal {

inline Status Failure(Error error, std::string message,
                      CKERROR ckError = CKERR_INVALIDPARAMETER) {
    Status status{error, ckError, CKBR_PARAMETERERROR, std::move(message)};
    status.Details.Stage = Phase::Edit;
    return status;
}

struct Stamp {
    CK_ID Id = 0;
    CKObject *Address = nullptr;

    friend bool operator==(const Stamp &, const Stamp &) = default;
};

inline Stamp Capture(CKObject *object) {
    return {object ? object->GetID() : 0, object};
}

inline Stamp Capture(NativeRef native) {
    return {static_cast<CK_ID>(native.Id),
            const_cast<CKObject *>(
                static_cast<const CKObject *>(native.Address))};
}

inline NativeRef Native(Stamp stamp) {
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

inline bool IsSameObject(CKContext *context, CKObject *current, Stamp expected,
                         CK_CLASSID type) {
    if (!expected.Id || !expected.Address)
        return current == nullptr && !expected.Id && !expected.Address;
    return current == expected.Address &&
        Resolve<CKObject>(context, expected, type) == current;
}

inline CKBehavior *ResolveBehavior(CKContext *context, NativeRef native) {
    if (!context || !native ||
        native.Id > static_cast<std::uint64_t>((std::numeric_limits<CK_ID>::max)()))
        return nullptr;
    CKObject *object = context->GetObject(static_cast<CK_ID>(native.Id));
    if (object != native.Address || !object || object->IsToBeDeleted() ||
        !CKIsChildClassOf(object, CKCID_BEHAVIOR))
        return nullptr;
    return static_cast<CKBehavior *>(object);
}

inline bool ContainsDestination(CKParameterOut *source, CKParameter *destination) {
    if (!source || !destination)
        return false;
    for (int index = 0; index < source->GetDestinationCount(); ++index) {
        if (source->GetDestination(index) == destination)
            return true;
    }
    return false;
}

inline std::uint64_t HashRedirect(const LinkBase &link, std::uint32_t ordinal,
                                  const GraphEndpoint &sink) {
    std::uint64_t hash = Fnv::Offset;
    const auto add = [&](std::uint64_t value) { Fnv::Value(hash, value); };
    add(link.Anchor.Domain);
    add(link.Anchor.Slot);
    add(link.Anchor.Generation);
    add(ordinal);
    add(sink.Node);
    add(static_cast<std::uint64_t>(sink.Kind));
    add(static_cast<std::uint32_t>(sink.Index));
    return hash;
}

inline CKBehaviorIO *ResolveIo(CKContext *context, Stamp stamp) {
    return Resolve<CKBehaviorIO>(context, stamp, CKCID_BEHAVIORIO);
}

inline bool Describe(CKBehaviorIO *io, GraphEndpoint &out) {
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

inline bool Describe(CKBehaviorLink *link, GraphLinkShape &out) {
    out = {};
    return link && Describe(link->GetInBehaviorIO(), out.Source) &&
           Describe(link->GetOutBehaviorIO(), out.Target) &&
           (out.InitialDelay = link->GetInitialActivationDelay(), true);
}

inline PinSource DescribeSource(CKParameterIn *input) {
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

struct NativeGraphOrder {
    struct Source {
        Stamp Port;
        std::vector<Stamp> Links;
    };

    std::vector<Stamp> Nodes;
    std::vector<int> NodePriorities;
    std::vector<Source> Sources;
};

using OrderAliases = std::unordered_map<CK_ID, Stamp>;

// Scheduler-visible graph order. CK2 runs children in m_SubBehaviors
// order and fires each source IO's Links in that IO's order.
Status CaptureOrder(CKBehavior *graph, NativeGraphOrder &out);
Status ArrangeOrder(CKContext *context, CKBehavior *graph,
                    const NativeGraphOrder &before,
                    const OrderAliases &nodeAliases,
                    const OrderAliases &portAliases, bool restoring);
Status ValidateOrder(CKContext *context, CKBehavior *graph,
                     const NativeGraphOrder &expected,
                     RevertSubject &subject);

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

    // A value written through a graph port. Before and Expected are ordinary
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

    struct Removal {
        Stamp Node;
        bool Removed = false;
        bool Restored = false;
    };

    struct RemovedLink {
        Stamp Value;
        Stamp Source;
        Stamp Sink;
        int InitialDelay = 0;
        int Delay = 0;
        bool Removed = false;
        bool SourceDetached = false;
        bool SinkDetached = false;
        bool Restored = false;
    };

    struct Reconnection {
        ObjectRef Anchor;
        Stamp Link;
        Stamp OriginalSource;
        Stamp OriginalSink;
        Stamp InstalledSource;
        Stamp InstalledSink;
        int InitialDelay = 0;
        int Delay = 0;
        bool Applied = false;
        bool Reverted = false;
    };

    CKEdit *Editor = nullptr;
    PatchState State = PatchState::Pending;
    Status LastStatus;
    bool Queued = false;
    bool Published = false;
    bool GraphObserved = false;
    bool RestoredEdited = false;
    bool RestoredGraph = false;
    bool StructuralEditClaim = false;
    Stamp Graph;
    Stamp GraphOwner;
    Stamp GraphParent;
    PatchKey Key;
    // A Script body. It closes only as its root is destroyed, which also
    // closes the Patches built on it, so it does not wait for them.
    bool Defines = false;
    // The existing objects this Patch edits. While it is active, a Patch that
    // introduced one of them cannot close.
    std::vector<Stamp> Named;
    std::vector<std::shared_ptr<CallbackResource>> Callbacks;
    // Plain graph-backed Behaviors created by AddGraph. They do not belong to
    // Runtime and therefore have no native BB lifecycle callbacks.
    std::vector<Stamp> GraphNodes;
    std::vector<Stamp> Nodes;
    // Scheduler-visible order captured before this Patch mutates the graph.
    // CK2 runs children in m_SubBehaviors order and fires each source IO's
    // Links in that IO's order. The graph Link array only feeds activation
    // flags, so its order is not journaled.
    NativeGraphOrder BeforeOrder;
    NativeGraphOrder AfterOrder;
    // Every Node this Edit named, borrowed or added, keyed by its Edit handle.
    // Filled once the Edit reaches the graph, which is what makes a Pending
    // Patch answer Busy instead of naming an object that does not exist yet.
    std::map<std::uint32_t, Stamp> Handles;
    std::vector<Stamp> InfrastructureNodes;
    std::vector<Stamp> InfrastructureLinks;
    // Existing Blocks whose parameter state or relations this Patch changes.
    // Every changed Block receives a final EDITED after relations are ready and
    // one more after successful teardown. A callback-owned port may require an
    // earlier reconciliation EDITED before that port exists.
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
    std::vector<Removal> Removals;
    std::vector<RemovedLink> RemovedLinks;
    std::vector<Reconnection> Reconnections;
    Stamp DetachedSource;
    Stamp DetachedSink;
    PatchLayer Layer;
    RelationLayer Data;
    std::vector<std::pair<LinkId, std::uint32_t>> Splices;
    std::vector<std::pair<LinkId, std::uint32_t>> Redirects;
    std::vector<RevertConflict> Conflicts;
};

// Whether match accepts any native object the journal introduced into the
// graph and destroys again when it closes.
template <typename Match>
bool AnyOwned(const Patch::Journal &journal, Match &&match) {
    const auto owned = [&](Stamp object) {
        return object.Id != 0 && match(object);
    };
    const auto any = [&](const auto &items, const auto &project) {
        for (const auto &item : items) {
            if (owned(project(item)))
                return true;
        }
        return false;
    };
    const auto self = [](Stamp object) { return object; };
    return any(journal.Nodes, self) || any(journal.GraphNodes, self) ||
        any(journal.InfrastructureNodes, self) ||
        any(journal.InfrastructureLinks, self) ||
        any(journal.Links,
            [](const Patch::Journal::Link &link) { return link.Value; }) ||
        any(journal.Ports,
            [](const Patch::Journal::Interface &port) { return port.Port; }) ||
        any(journal.Operations,
            [](const Patch::Journal::Operation &operation) {
                return operation.Value;
            }) ||
        owned(journal.DetachedSource) || owned(journal.DetachedSink) ||
        any(journal.Binds, [](const Patch::Journal::Binding &binding) {
            return binding.Literal;
        });
}

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

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_EDIT_JOURNAL_H
