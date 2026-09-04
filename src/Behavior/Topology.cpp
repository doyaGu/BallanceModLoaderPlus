#include "Behavior/Topology.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace BML::Behavior {
namespace {

constexpr std::uint64_t kHashOffset = 1469598103934665603ull;
constexpr std::uint64_t kHashPrime = 1099511628211ull;

Status Failure(Error error, std::string message) {
    return {error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR, std::move(message)};
}

bool SameEndpoint(const GraphEndpoint &left, const GraphEndpoint &right) {
    return left == right;
}

struct GraphEndpointLess {
    bool operator()(const GraphEndpoint &left,
                    const GraphEndpoint &right) const noexcept {
        return std::tie(left.Node, left.Kind, left.Index) <
               std::tie(right.Node, right.Kind, right.Index);
    }
};

template <typename T> void Hash(std::uint64_t &hash, T value) {
    static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
    if constexpr (std::is_enum_v<T>) {
        using Unsigned = std::make_unsigned_t<std::underlying_type_t<T>>;
        Unsigned bits = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(bits); ++index) {
            hash ^= static_cast<unsigned char>(bits & 0xffu);
            hash *= kHashPrime;
            bits >>= 8u;
        }
    } else {
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned bits = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(bits); ++index) {
            hash ^= static_cast<unsigned char>(bits & 0xffu);
            hash *= kHashPrime;
            bits >>= 8u;
        }
    }
}

void HashText(std::uint64_t &hash, const std::string &text) {
    Hash(hash, static_cast<std::uint64_t>(text.size()));
    for (const unsigned char byte : text) {
        hash ^= byte;
        hash *= kHashPrime;
    }
}

void Hash(std::uint64_t &hash, const ObjectRef &value) {
    Hash(hash, value.Domain);
    Hash(hash, value.Slot);
    Hash(hash, value.Generation);
}

void Hash(std::uint64_t &hash, const GraphEndpoint &value) {
    Hash(hash, value.Node);
    Hash(hash, value.Kind);
    Hash(hash, static_cast<std::int32_t>(value.Index));
}

void Hash(std::uint64_t &hash, const PatchKey &value) {
    HashText(hash, value.Owner);
    HashText(hash, value.Name);
}

void Hash(std::uint64_t &hash, const LinkBase &value) {
    Hash(hash, value.Anchor);
    Hash(hash, value.Source);
    Hash(hash, value.Sink);
    Hash(hash, static_cast<std::int32_t>(value.Delay));
}

bool ControlEndpoint(const GraphEndpoint &endpoint) {
    return endpoint.Node != 0 && endpoint.Index >= 0 &&
           (endpoint.Kind == SlotKind::Input || endpoint.Kind == SlotKind::Output);
}

bool PatchLess(const PatchKey &left, const PatchKey &right) {
    return std::tie(left.Owner, left.Name) < std::tie(right.Owner, right.Name);
}

std::string PatchName(const PatchKey &patch) {
    return patch.Owner + ":" + patch.Name;
}

struct Presence {
    std::set<LinkId> Links;
};

struct Group {
    PatchKey Patch;
    int Priority = 0;
    const LinkOverlays *Value = nullptr;
};

bool StableLess(const Group &left, const Group &right) {
    if (left.Priority != right.Priority)
        return left.Priority < right.Priority;
    return PatchLess(left.Patch, right.Patch);
}

std::vector<std::size_t>
ShortestCycle(const std::vector<Group> &groups,
              const std::vector<std::vector<std::size_t>> &edges) {
    std::vector<std::size_t> best;
    std::string bestName;
    for (std::size_t start = 0; start < groups.size(); ++start) {
        std::queue<std::size_t> pending;
        std::vector<std::size_t> previous(groups.size(),
                                          (std::numeric_limits<std::size_t>::max)());
        std::vector<bool> seen(groups.size(), false);
        pending.push(start);
        seen[start] = true;
        while (!pending.empty()) {
            const std::size_t current = pending.front();
            pending.pop();
            for (const std::size_t next : edges[current]) {
                if (next == start) {
                    std::vector<std::size_t> cycle{current};
                    while (cycle.back() != start)
                        cycle.push_back(previous[cycle.back()]);
                    std::reverse(cycle.begin(), cycle.end());
                    cycle.push_back(start);
                    std::ostringstream name;
                    for (std::size_t index = 0; index < cycle.size(); ++index) {
                        if (index)
                            name << " -> ";
                        name << PatchName(groups[cycle[index]].Patch);
                    }
                    if (best.empty() || cycle.size() < best.size() ||
                        (cycle.size() == best.size() && name.str() < bestName)) {
                        best = std::move(cycle);
                        bestName = name.str();
                    }
                    continue;
                }
                if (seen[next])
                    continue;
                seen[next] = true;
                previous[next] = current;
                pending.push(next);
            }
        }
    }
    return best;
}

} // namespace

bool Topology::ObjectRefLess::operator()(const ObjectRef &left,
                                         const ObjectRef &right) const noexcept {
    return std::tie(left.Domain, left.Slot, left.Generation) <
           std::tie(right.Domain, right.Slot, right.Generation);
}

bool Topology::EndpointLess::operator()(const GraphEndpoint &left,
                                        const GraphEndpoint &right) const noexcept {
    return std::tie(left.Node, left.Kind, left.Index) <
           std::tie(right.Node, right.Kind, right.Index);
}

Status Topology::Identify(const LinkBase &base, LinkId &out) {
    out = {};
    if (base.Anchor.IsNull() || !ControlEndpoint(base.Source) ||
        !ControlEndpoint(base.Sink) || base.Delay < 0) {
        return Failure(Error::InvalidState,
                       "A logical Link requires a live anchor, two control "
                       "endpoints, and a non-negative delay.");
    }
    const auto known = m_Anchors.find(base.Anchor);
    if (known != m_Anchors.end()) {
        const auto link = m_Links.find(known->second);
        if (link == m_Links.end() || !(link->second.Base == base)) {
            return Failure(Error::GraphChanged,
                           "A native Link anchor no longer has its recorded base "
                           "endpoints and delay.");
        }
        out = known->second;
        return {};
    }
    if (m_NextLink == 0)
        return Failure(Error::InvalidState, "Logical Link identities are exhausted.");

    const LinkId id{m_NextLink++};
    LogicalLink link;
    link.Id = id;
    link.Base = base;
    link.Fingerprint = kHashOffset;
    Hash(link.Fingerprint, base);
    m_Anchors.emplace(base.Anchor, id);
    m_Links.emplace(id, std::move(link));

    LinkMap links = m_Links;
    TapMap taps;
    std::uint64_t fingerprint = 0;
    Status status = Compose(m_Patches, links, taps, fingerprint);
    if (!status) {
        m_Links.erase(id);
        m_Anchors.erase(base.Anchor);
        return status;
    }
    m_Links = std::move(links);
    m_Taps = std::move(taps);
    m_Fingerprint = fingerprint;
    out = id;
    return {};
}

Status Topology::Normalize(PatchLayer &patch) const {
    if (patch.Patch.Owner.empty() || patch.Patch.Name.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Patch requires an owner and a local key.");
    if (patch.Links.empty() && patch.Outs.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Patch must contain a Link overlay or an Out tap.");

    std::set<LinkId> links;
    std::set<GraphEndpoint, EndpointLess> outs;
    std::set<std::uint32_t> ordinals;
    for (LinkOverlays &group : patch.Links) {
        if (!group.Link || m_Links.find(group.Link) == m_Links.end())
            return Failure(Error::GraphChanged,
                           "A Patch refers to a logical Link outside this graph.");
        if (!links.insert(group.Link).second || group.Overlays.empty())
            return Failure(
                Error::InvalidState,
                "A Patch must declare one non-empty overlay group per logical Link.");
        for (const Order &order : group.Ordering) {
            if (order.Other.Owner.empty() || order.Other.Name.empty())
                return Failure(Error::InvalidState,
                               "A Link overlay order requires a complete Patch key.");
            if (order.Kind != OrderKind::Before && order.Kind != OrderKind::After)
                return Failure(Error::InvalidState, "A Link overlay order is invalid.");
        }
        std::sort(group.Ordering.begin(), group.Ordering.end(),
                  [](const Order &left, const Order &right) {
                      if (left.Other != right.Other)
                          return PatchLess(left.Other, right.Other);
                      return left.Kind < right.Kind;
                  });
        group.Ordering.erase(std::unique(group.Ordering.begin(), group.Ordering.end()),
                             group.Ordering.end());
        std::sort(group.Overlays.begin(), group.Overlays.end(),
                  [](const Overlay &left, const Overlay &right) {
                      return left.Ordinal < right.Ordinal;
                  });
        int redirects = 0;
        for (const Overlay &item : group.Overlays) {
            if ((item.Kind != OverlayKind::Splice &&
                 item.Kind != OverlayKind::Tap &&
                 item.Kind != OverlayKind::Redirect) ||
                !ordinals.insert(item.Ordinal).second) {
                return Failure(Error::InvalidState,
                               "Patch action ordinals must be unique and name a Link "
                               "overlay kind.");
            }
            if (item.Kind != OverlayKind::Redirect)
                continue;
            if (!ControlEndpoint(item.Target))
                return Failure(Error::InvalidState,
                               "A Redirect requires a control port to send the "
                               "Link to.");
            if (++redirects > 1)
                return Failure(Error::RedirectConflict,
                               "One Patch cannot redirect a Link twice.");
        }
    }

    for (OutTaps &group : patch.Outs) {
        if (!ControlEndpoint(group.Out) || group.Out.Kind != SlotKind::Output ||
            !outs.insert(group.Out).second || group.Taps.empty()) {
            return Failure(Error::InvalidState,
                           "A Patch must declare one non-empty tap group per Out.");
        }
        std::sort(group.Taps.begin(), group.Taps.end(),
                  [](const OutTap &left, const OutTap &right) {
                      return left.Ordinal < right.Ordinal;
                  });
        for (const OutTap &item : group.Taps) {
            if (!ordinals.insert(item.Ordinal).second)
                return Failure(Error::InvalidState,
                               "Patch action ordinals must be unique across Link "
                               "overlays and Out taps.");
        }
    }

    std::sort(patch.Links.begin(), patch.Links.end(),
              [](const LinkOverlays &left, const LinkOverlays &right) {
                  return left.Link < right.Link;
              });
    std::sort(patch.Outs.begin(), patch.Outs.end(),
              [](const OutTaps &left, const OutTaps &right) {
                  return EndpointLess{}(left.Out, right.Out);
              });
    return {};
}

Status Topology::Set(PatchLayer patch) {
    Status status = Normalize(patch);
    if (!status)
        return status;

    PatchMap patches = m_Patches;
    patches[patch.Patch] = std::move(patch);
    LinkMap links = m_Links;
    TapMap taps;
    std::uint64_t fingerprint = 0;
    status = Compose(patches, links, taps, fingerprint);
    if (!status)
        return status;

    m_Patches = std::move(patches);
    m_Links = std::move(links);
    m_Taps = std::move(taps);
    m_Fingerprint = fingerprint;
    return {};
}

Status Topology::Validate(PatchLayer patch) const {
    Status status = Normalize(patch);
    if (!status)
        return status;
    PatchMap patches = m_Patches;
    patches[patch.Patch] = std::move(patch);
    LinkMap links = m_Links;
    TapMap taps;
    std::uint64_t fingerprint = 0;
    return Compose(patches, links, taps, fingerprint);
}

bool Topology::Remove(const PatchKey &patch) {
    PatchMap patches = m_Patches;
    if (patches.erase(patch) == 0)
        return false;
    LinkMap links = m_Links;
    TapMap taps;
    std::uint64_t fingerprint = 0;
    const Status status = Compose(patches, links, taps, fingerprint);
    if (!status)
        return false;
    m_Patches = std::move(patches);
    m_Links = std::move(links);
    m_Taps = std::move(taps);
    m_Fingerprint = fingerprint;
    return true;
}

const LogicalLink *Topology::Find(LinkId link) const noexcept {
    const auto found = m_Links.find(link);
    return found == m_Links.end() ? nullptr : &found->second;
}

const LogicalLink *Topology::Find(const ObjectRef &anchor) const noexcept {
    const auto found = m_Anchors.find(anchor);
    return found == m_Anchors.end() ? nullptr : Find(found->second);
}

const std::vector<PatchOutTap> *
Topology::Taps(const GraphEndpoint &out) const noexcept {
    const auto found = m_Taps.find(out);
    return found == m_Taps.end() ? nullptr : &found->second;
}

GraphEndpoint EffectiveSink(const LogicalLink &link) {
    for (const OrderedOverlay &overlay : link.Overlays) {
        if (overlay.Kind == OverlayKind::Redirect)
            return overlay.Target;
    }
    return link.Base.Sink;
}

Status CompletePath(const GraphModel &graph, const GraphEndpoint &start,
                    Path &out) {
    out = {};
    const auto root = std::find_if(
        graph.Nodes.begin(), graph.Nodes.end(),
        [](const GraphNode &node) { return node.Id != 0 && node.Parent == 0; });
    const auto node = [&](std::uint64_t id) {
        return std::find_if(graph.Nodes.begin(), graph.Nodes.end(),
                            [id](const GraphNode &item) { return item.Id == id; });
    };
    const auto startNode = node(start.Node);
    const auto hasPort = [](const GraphNode &owner,
                            const GraphEndpoint &endpoint) {
        return std::any_of(
            owner.Ports.begin(), owner.Ports.end(),
            [&](const GraphPort &port) {
                return port.Kind == endpoint.Kind &&
                       port.Index == endpoint.Index;
            });
    };
    if (root == graph.Nodes.end() || startNode == graph.Nodes.end() ||
        start.Index < 0 || !hasPort(*startNode, start) ||
        (start.Node == root->Id ? start.Kind != SlotKind::Input
                               : start.Kind != SlotKind::Output)) {
        return Failure(Error::InvalidState,
                       "A Path must begin at a graph Entry or node Out.");
    }

    out.Start = start;
    GraphEndpoint current = start;
    std::set<GraphEndpoint, GraphEndpointLess> visited;
    for (;;) {
        if (!visited.insert(current).second) {
            out = {};
            return Failure(Error::PathCycle,
                           "The logical Path returns to an earlier control port.");
        }

        std::vector<const GraphLink *> links;
        for (const GraphLink &link : graph.Links) {
            if (SameEndpoint(link.Source, current))
                links.push_back(&link);
        }
        if (links.empty()) {
            out.End = current;
            return {};
        }
        if (links.size() != 1) {
            out = {};
            return Failure(Error::PathAmbiguous,
                           "The logical Path has parallel Links or a branch.");
        }

        const GraphLink &link = *links.front();
        if (link.Object.IsNull() || link.InitialDelay < 0) {
            out = {};
            return Failure(Error::GraphChanged,
                           "The logical Path contains an invalid Link anchor.");
        }
        out.Links.push_back(
            {link.Object, link.Source, link.Target, link.InitialDelay});

        const auto target = node(link.Target.Node);
        if (target == graph.Nodes.end() || link.Target.Index < 0 ||
            !hasPort(*target, link.Target)) {
            out = {};
            return Failure(Error::GraphChanged,
                           "The logical Path reaches a missing node or port.");
        }
        if (target->Id == root->Id) {
            if (link.Target.Kind != SlotKind::Output) {
                out = {};
                return Failure(Error::GraphChanged,
                               "The logical Path reaches an invalid graph boundary.");
            }
            out.End = link.Target;
            return {};
        }
        if (link.Target.Kind != SlotKind::Input) {
            out = {};
            return Failure(Error::GraphChanged,
                           "The logical Path reaches a node through a non-In port.");
        }

        std::set<GraphEndpoint, GraphEndpointLess> next;
        for (const GraphPort &port : target->Ports) {
            if (port.Kind == SlotKind::Output && port.Index >= 0)
                next.insert({target->Id, SlotKind::Output, port.Index});
        }
        if (next.size() != 1) {
            out = {};
            return Failure(Error::PathAmbiguous,
                           "A logical Path node must have exactly one Out.");
        }
        current = *next.begin();
    }
}

Status Topology::Compose(const PatchMap &patches, LinkMap &links, TapMap &taps,
                         std::uint64_t &fingerprint) const {
    for (auto &[id, link] : links)
        link.Overlays.clear();
    taps.clear();

    std::map<PatchKey, Presence> presence;
    for (const auto &[key, patch] : patches) {
        Presence &entry = presence[key];
        for (const LinkOverlays &group : patch.Links)
            entry.Links.insert(group.Link);
    }

    for (auto &[linkId, link] : links) {
        std::vector<Group> groups;
        for (const auto &[key, patch] : patches) {
            const auto group = std::find_if(patch.Links.begin(), patch.Links.end(),
                                            [linkId](const LinkOverlays &candidate) {
                                                return candidate.Link == linkId;
                                            });
            if (group != patch.Links.end())
                groups.push_back({key, patch.Priority, &*group});
        }
        std::sort(groups.begin(), groups.end(), StableLess);

        std::vector<std::vector<std::size_t>> edges(groups.size());
        std::vector<std::size_t> indegree(groups.size(), 0);
        for (std::size_t index = 0; index < groups.size(); ++index) {
            for (const Order &order : groups[index].Value->Ordering) {
                const auto targetPresence = presence.find(order.Other);
                if (targetPresence == presence.end())
                    continue;
                if (!targetPresence->second.Links.contains(linkId)) {
                    std::ostringstream message;
                    message << "Patch " << PatchName(groups[index].Patch)
                            << " orders its logical Link " << linkId.Value
                            << " against " << PatchName(order.Other)
                            << ", but that Patch has no overlay on this Link.";
                    return Failure(Error::OrderingTargetMismatch, message.str());
                }
                const auto target = std::find_if(
                    groups.begin(), groups.end(), [&](const Group &candidate) {
                        return candidate.Patch == order.Other;
                    });
                if (target == groups.end())
                    return Failure(
                        Error::OrderingTargetMismatch,
                        "A Link overlay ordering target has the wrong ordering class.");
                const std::size_t targetIndex =
                    static_cast<std::size_t>(std::distance(groups.begin(), target));
                const std::size_t from =
                    order.Kind == OrderKind::Before ? index : targetIndex;
                const std::size_t to =
                    order.Kind == OrderKind::Before ? targetIndex : index;
                if (std::find(edges[from].begin(), edges[from].end(), to) ==
                    edges[from].end()) {
                    edges[from].push_back(to);
                    ++indegree[to];
                }
            }
        }
        for (auto &targets : edges) {
            std::sort(targets.begin(), targets.end(),
                      [&](std::size_t left, std::size_t right) {
                          return StableLess(groups[left], groups[right]);
                      });
        }

        std::vector<bool> emitted(groups.size(), false);
        std::vector<std::size_t> ordered;
        ordered.reserve(groups.size());
        while (ordered.size() != groups.size()) {
            std::size_t next = groups.size();
            for (std::size_t index = 0; index < groups.size(); ++index) {
                if (emitted[index] || indegree[index] != 0)
                    continue;
                if (next == groups.size() || StableLess(groups[index], groups[next]))
                    next = index;
            }
            if (next == groups.size())
                break;
            emitted[next] = true;
            ordered.push_back(next);
            for (const std::size_t target : edges[next])
                --indegree[target];
        }
        if (ordered.size() != groups.size()) {
            const std::vector<std::size_t> cycle = ShortestCycle(groups, edges);
            std::ostringstream message;
            message << "Behavior Link overlay order contains a cycle";
            if (!cycle.empty()) {
                message << ": ";
                for (std::size_t index = 0; index < cycle.size(); ++index) {
                    if (index)
                        message << " -> ";
                    message << PatchName(groups[cycle[index]].Patch);
                }
            }
            message << '.';
            return Failure(Error::OverlayOrderCycle, message.str());
        }

        link.Fingerprint = kHashOffset;
        Hash(link.Fingerprint, link.Base);
        const PatchKey *redirected = nullptr;
        for (const std::size_t index : ordered) {
            const Group &group = groups[index];
            for (const Overlay &item : group.Value->Overlays) {
                if (item.Kind == OverlayKind::Redirect) {
                    if (redirected) {
                        std::ostringstream message;
                        message << "Patch " << PatchName(group.Patch)
                                << " and Patch " << PatchName(*redirected)
                                << " both redirect logical Link "
                                << linkId.Value
                                << ", so its destination is ambiguous.";
                        return Failure(Error::RedirectConflict, message.str());
                    }
                    redirected = &group.Patch;
                }
                link.Overlays.push_back({group.Patch, group.Priority, item.Kind,
                                         item.Ordinal, item.Fingerprint,
                                         item.Target});
                Hash(link.Fingerprint, group.Patch);
                Hash(link.Fingerprint, static_cast<std::int32_t>(group.Priority));
                Hash(link.Fingerprint, item.Kind);
                Hash(link.Fingerprint, item.Ordinal);
                Hash(link.Fingerprint, item.Fingerprint);
                Hash(link.Fingerprint, item.Target);
            }
        }
    }

    for (const auto &[key, patch] : patches) {
        for (const OutTaps &group : patch.Outs) {
            std::vector<PatchOutTap> &installed = taps[group.Out];
            for (const OutTap &item : group.Taps) {
                installed.push_back(
                    {key, patch.Priority, item.Ordinal, item.Fingerprint});
            }
        }
    }
    for (auto &[out, installed] : taps) {
        std::sort(installed.begin(), installed.end(),
                  [](const PatchOutTap &left, const PatchOutTap &right) {
                      if (left.Priority != right.Priority)
                          return left.Priority < right.Priority;
                      if (left.Patch != right.Patch)
                          return PatchLess(left.Patch, right.Patch);
                      return left.Ordinal < right.Ordinal;
                  });
    }

    fingerprint = kHashOffset;
    std::vector<const LogicalLink *> stableLinks;
    stableLinks.reserve(links.size());
    for (const auto &[id, link] : links)
        stableLinks.push_back(&link);
    std::sort(stableLinks.begin(), stableLinks.end(),
              [](const LogicalLink *left, const LogicalLink *right) {
                  const LinkBase &a = left->Base;
                  const LinkBase &b = right->Base;
                  const auto anchorA =
                      std::tie(a.Anchor.Domain, a.Anchor.Slot, a.Anchor.Generation);
                  const auto anchorB =
                      std::tie(b.Anchor.Domain, b.Anchor.Slot, b.Anchor.Generation);
                  if (anchorA != anchorB)
                      return anchorA < anchorB;
                  const auto sourceA =
                      std::tie(a.Source.Node, a.Source.Kind, a.Source.Index);
                  const auto sourceB =
                      std::tie(b.Source.Node, b.Source.Kind, b.Source.Index);
                  if (sourceA != sourceB)
                      return sourceA < sourceB;
                  const auto sinkA = std::tie(a.Sink.Node, a.Sink.Kind, a.Sink.Index);
                  const auto sinkB = std::tie(b.Sink.Node, b.Sink.Kind, b.Sink.Index);
                  return sinkA != sinkB ? sinkA < sinkB : a.Delay < b.Delay;
              });
    Hash(fingerprint, static_cast<std::uint64_t>(stableLinks.size()));
    for (const LogicalLink *link : stableLinks) {
        Hash(fingerprint, link->Base);
        Hash(fingerprint, link->Fingerprint);
    }
    Hash(fingerprint, static_cast<std::uint64_t>(taps.size()));
    for (const auto &[out, installed] : taps) {
        Hash(fingerprint, out);
        Hash(fingerprint, static_cast<std::uint64_t>(installed.size()));
        for (const PatchOutTap &tap : installed) {
            Hash(fingerprint, tap.Patch);
            Hash(fingerprint, static_cast<std::int32_t>(tap.Priority));
            Hash(fingerprint, tap.Ordinal);
            Hash(fingerprint, tap.Fingerprint);
        }
    }
    return {};
}

} // namespace BML::Behavior
