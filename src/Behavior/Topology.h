#ifndef BML_BEHAVIOR_TOPOLOGY_H
#define BML_BEHAVIOR_TOPOLOGY_H

#include <compare>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "Behavior/Graph.h"

namespace BML::Behavior::Internal {

struct LinkId {
    std::uint64_t Value = 0;

    [[nodiscard]] explicit operator bool() const noexcept { return Value != 0; }

    friend auto operator<=>(const LinkId &, const LinkId &) = default;
};

struct LinkBase {
    ObjectRef Anchor;
    GraphEndpoint Source;
    GraphEndpoint Sink;
    int Delay = 0;

    friend bool operator==(const LinkBase &, const LinkBase &) = default;
};

struct PatchKey {
    std::string Owner;
    std::string Name;

    friend auto operator<=>(const PatchKey &, const PatchKey &) = default;
};

enum class OrderKind {
    Before,
    After,
};

struct Order {
    OrderKind Kind = OrderKind::Before;
    PatchKey Other;

    friend bool operator==(const Order &, const Order &) = default;
};

enum class OverlayKind {
    Splice,
    Tap,
    // Sends the Link somewhere else. Unlike a Splice, which keeps the original
    // destination at the end of the inserted chain, a Redirect replaces it, so
    // at most one Patch may hold a Redirect on one Link.
    Redirect,
};

struct Overlay {
    OverlayKind Kind = OverlayKind::Splice;
    std::uint32_t Ordinal = 0;
    std::uint64_t Fingerprint = 0;
    // Where a Redirect sends the Link. Unused by the other kinds.
    GraphEndpoint Target;
};

struct LinkOverlays {
    LinkId Link;
    std::vector<Order> Ordering;
    std::vector<Overlay> Overlays;
};

struct OutTap {
    std::uint32_t Ordinal = 0;
    std::uint64_t Fingerprint = 0;
};

struct OutTaps {
    GraphEndpoint Out;
    std::vector<OutTap> Taps;
};

// One Patch is submitted atomically. Its Link overlays and Out taps remain
// separate control-flow families even when the Patch contains both.
struct PatchLayer {
    PatchKey Patch;
    int Priority = 0;
    std::vector<LinkOverlays> Links;
    std::vector<OutTaps> Outs;
};

struct OrderedOverlay {
    PatchKey Patch;
    int Priority = 0;
    OverlayKind Kind = OverlayKind::Splice;
    std::uint32_t Ordinal = 0;
    std::uint64_t Fingerprint = 0;
    GraphEndpoint Target;
};

struct LogicalLink {
    LinkId Id;
    LinkBase Base;
    std::uint64_t Fingerprint = 0;
    std::vector<OrderedOverlay> Overlays;
};

// Where a Link ends once every Patch on it is composed: the Redirect target
// when one Patch holds a Redirect, otherwise the native sink.
GraphEndpoint EffectiveSink(const LogicalLink &link);

// A unique, non-branching control-flow path in the logical graph. Links keep
// their native anchor identity; End is the final Out/Exit reached after the
// last Link, or Start when the path is already a dead end.
struct Path {
    GraphEndpoint Start;
    std::vector<LinkBase> Links;
    GraphEndpoint End;
};

// Completes the logical path beginning at an Entry or node Out. A branch,
// parallel Link, cycle, or dead-end node with more than one possible Out is
// rejected instead of being guessed.
Status CompletePath(const GraphModel &graph, const GraphEndpoint &start,
                    Path &out);

struct PatchOutTap {
    PatchKey Patch;
    int Priority = 0;
    std::uint32_t Ordinal = 0;
    std::uint64_t Fingerprint = 0;
};

// The logical control-flow topology of one live Behavior graph. It assigns a
// non-reused identity to each native anchor Link and validates the layered
// intent that a later CK adapter may materialize. This class never mutates CK.
class Topology final {
public:
    Status Identify(const LinkBase &base, LinkId &out);

    // Replaces all declarations owned by the same (owner, patch) key only when
    // the complete candidate topology has a valid order.
    Status Validate(PatchLayer patch) const;
    Status Set(PatchLayer patch);
    bool Remove(const PatchKey &patch);

    [[nodiscard]] const LogicalLink *Find(LinkId link) const noexcept;
    [[nodiscard]] const LogicalLink *Find(const ObjectRef &anchor) const noexcept;
    [[nodiscard]] const std::vector<PatchOutTap> *
    Taps(const GraphEndpoint &out) const noexcept;
    [[nodiscard]] std::uint64_t Fingerprint() const noexcept { return m_Fingerprint; }

private:
    struct ObjectRefLess {
        bool operator()(const ObjectRef &left, const ObjectRef &right) const noexcept;
    };

    struct EndpointLess {
        bool operator()(const GraphEndpoint &left,
                        const GraphEndpoint &right) const noexcept;
    };

    using PatchMap = std::map<PatchKey, PatchLayer>;
    using LinkMap = std::map<LinkId, LogicalLink>;
    using TapMap = std::map<GraphEndpoint, std::vector<PatchOutTap>, EndpointLess>;

    Status Normalize(PatchLayer &patch) const;
    Status Compose(const PatchMap &patches, LinkMap &links, TapMap &taps,
                   std::uint64_t &fingerprint) const;

    std::uint64_t m_NextLink = 1;
    std::map<ObjectRef, LinkId, ObjectRefLess> m_Anchors;
    LinkMap m_Links;
    PatchMap m_Patches;
    TapMap m_Taps;
    std::uint64_t m_Fingerprint = 0;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_TOPOLOGY_H
