#include "Behavior/Relations.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <set>
#include <sstream>
#include <tuple>
#include <type_traits>
#include <utility>

namespace BML::Behavior::Internal {
namespace {

constexpr std::uint64_t kHashOffset = 1469598103934665603ull;
constexpr std::uint64_t kHashPrime = 1099511628211ull;

Status Failure(Error error, std::string message) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = Phase::Edit;
    return status;
}

bool PatchLess(const PatchKey &left, const PatchKey &right) {
    return std::tie(left.Owner, left.Name) <
           std::tie(right.Owner, right.Name);
}

std::string PatchName(const PatchKey &patch) {
    return patch.Owner + ":" + patch.Name;
}

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

void Hash(std::uint64_t &hash, const GraphEndpoint &value) {
    Hash(hash, value.Node);
    Hash(hash, value.Kind);
    Hash(hash, static_cast<std::int32_t>(value.Index));
}

void Hash(std::uint64_t &hash, const PatchKey &value) {
    HashText(hash, value.Owner);
    HashText(hash, value.Name);
}

struct Group {
    PatchKey Patch;
    int Priority = 0;
    const PinRelations *Value = nullptr;
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
        std::vector<std::size_t> previous(
            groups.size(), (std::numeric_limits<std::size_t>::max)());
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

bool Relations::EndpointLess::operator()(
    const GraphEndpoint &left, const GraphEndpoint &right) const noexcept {
    return std::tie(left.Node, left.Kind, left.Index) <
           std::tie(right.Node, right.Kind, right.Index);
}

Status Relations::Normalize(RelationLayer &layer) const {
    if (layer.Patch.Owner.empty() || layer.Patch.Name.empty())
        return Failure(Error::InvalidState,
                       "Behavior relations require an owner and a local Patch key.");
    if (layer.Pins.empty())
        return Failure(Error::InvalidState,
                       "A Behavior relation layer must contain a Pin source.");

    std::set<GraphEndpoint, EndpointLess> pins;
    std::set<std::uint32_t> ordinals;
    for (PinRelations &pin : layer.Pins) {
        if (pin.Pin.Node == 0 ||
            (pin.Pin.Kind != SlotKind::InputParameter &&
             pin.Pin.Kind != SlotKind::Target) ||
            pin.Pin.Index < 0 || !pins.insert(pin.Pin).second ||
            pin.Relations.empty()) {
            return Failure(Error::InvalidState,
                           "A relation layer requires one non-empty group per Pin.");
        }
        std::sort(pin.Ordering.begin(), pin.Ordering.end(),
                  [](const Order &left, const Order &right) {
                      if (left.Other != right.Other)
                          return PatchLess(left.Other, right.Other);
                      return left.Kind < right.Kind;
                  });
        pin.Ordering.erase(
            std::unique(pin.Ordering.begin(), pin.Ordering.end()),
            pin.Ordering.end());
        for (const Order &order : pin.Ordering) {
            if (order.Other.Owner.empty() || order.Other.Name.empty() ||
                (order.Kind != OrderKind::Before &&
                 order.Kind != OrderKind::After)) {
                return Failure(Error::InvalidState,
                               "A Pin source order requires a complete Patch key.");
            }
        }
        std::sort(pin.Relations.begin(), pin.Relations.end(),
                  [](const Relation &left, const Relation &right) {
                      return left.Ordinal < right.Ordinal;
                  });
        const std::size_t binds = static_cast<std::size_t>(std::count_if(
            pin.Relations.begin(), pin.Relations.end(),
            [](const Relation &relation) {
                return relation.Kind == RelationKind::Bind;
            }));
        if ((binds != 0 && pin.Relations.size() != 1) ||
            (binds != 0 && !pin.Ordering.empty())) {
            return Failure(Error::SourceConflict,
                           "Bind owns the complete Pin source and cannot share a layer.");
        }
        for (const Relation &relation : pin.Relations) {
            if ((relation.Kind != RelationKind::Bind &&
                 relation.Kind != RelationKind::Transform) ||
                !ordinals.insert(relation.Ordinal).second) {
                return Failure(Error::InvalidState,
                               "Relation ordinals must be unique within a Patch.");
            }
        }
    }
    std::sort(layer.Pins.begin(), layer.Pins.end(),
              [](const PinRelations &left, const PinRelations &right) {
                  return EndpointLess{}(left.Pin, right.Pin);
              });
    return {};
}

Status Relations::Validate(RelationLayer layer) const {
    Status status = Normalize(layer);
    if (!status)
        return status;
    LayerMap layers = m_Layers;
    layers[layer.Patch] = std::move(layer);
    PinMap pins;
    std::uint64_t fingerprint = 0;
    return Compose(layers, pins, fingerprint);
}

Status Relations::Set(RelationLayer layer) {
    Status status = Normalize(layer);
    if (!status)
        return status;
    LayerMap layers = m_Layers;
    layers[layer.Patch] = std::move(layer);
    PinMap pins;
    std::uint64_t fingerprint = 0;
    status = Compose(layers, pins, fingerprint);
    if (!status)
        return status;
    m_Layers = std::move(layers);
    m_Pins = std::move(pins);
    m_Fingerprint = fingerprint;
    return {};
}

bool Relations::Remove(const PatchKey &patch) {
    LayerMap layers = m_Layers;
    if (layers.erase(patch) == 0)
        return false;
    PinMap pins;
    std::uint64_t fingerprint = 0;
    const Status status = Compose(layers, pins, fingerprint);
    if (!status)
        return false;
    m_Layers = std::move(layers);
    m_Pins = std::move(pins);
    m_Fingerprint = fingerprint;
    return true;
}

const LogicalPin *Relations::Find(const GraphEndpoint &pin) const noexcept {
    const auto found = m_Pins.find(pin);
    return found == m_Pins.end() ? nullptr : &found->second;
}

Status Relations::Compose(const LayerMap &layers, PinMap &pins,
                          std::uint64_t &fingerprint) const {
    pins.clear();
    std::map<PatchKey, std::set<GraphEndpoint, EndpointLess>> presence;
    for (const auto &[key, layer] : layers) {
        for (const PinRelations &pin : layer.Pins) {
            presence[key].insert(pin.Pin);
            pins.try_emplace(pin.Pin, LogicalPin{pin.Pin});
        }
    }

    for (auto &[endpoint, logical] : pins) {
        std::vector<Group> groups;
        bool exclusive = false;
        for (const auto &[key, layer] : layers) {
            const auto found = std::find_if(
                layer.Pins.begin(), layer.Pins.end(),
                [&](const PinRelations &candidate) {
                    return candidate.Pin == endpoint;
                });
            if (found == layer.Pins.end())
                continue;
            groups.push_back({key, layer.Priority, &*found});
            exclusive = exclusive ||
                found->Relations.front().Kind == RelationKind::Bind;
        }
        std::sort(groups.begin(), groups.end(), StableLess);
        if (exclusive && groups.size() != 1) {
            std::ostringstream message;
            message << "Pin " << endpoint.Node << ':' << endpoint.Index
                    << " already has an exclusive Bind owned by ";
            for (std::size_t index = 0; index < groups.size(); ++index) {
                if (index)
                    message << ", ";
                message << PatchName(groups[index].Patch);
            }
            message << '.';
            return Failure(Error::SourceConflict, message.str());
        }

        std::vector<std::vector<std::size_t>> edges(groups.size());
        std::vector<std::size_t> indegree(groups.size(), 0);
        for (std::size_t index = 0; index < groups.size(); ++index) {
            for (const Order &order : groups[index].Value->Ordering) {
                const auto targetPresence = presence.find(order.Other);
                if (targetPresence == presence.end())
                    continue;
                if (!targetPresence->second.contains(endpoint)) {
                    return Failure(
                        Error::OrderingTargetMismatch,
                        "A Pin source order targets a Patch on another Pin.");
                }
                const auto target = std::find_if(
                    groups.begin(), groups.end(),
                    [&](const Group &candidate) {
                        return candidate.Patch == order.Other;
                    });
                if (target == groups.end())
                    return Failure(Error::OrderingTargetMismatch,
                                   "A Pin source order has the wrong relation class.");
                const std::size_t targetIndex = static_cast<std::size_t>(
                    std::distance(groups.begin(), target));
                const std::size_t from = order.Kind == OrderKind::Before
                    ? index : targetIndex;
                const std::size_t to = order.Kind == OrderKind::Before
                    ? targetIndex : index;
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
            message << "Pin source order contains a cycle";
            if (!cycle.empty()) {
                message << ": ";
                for (std::size_t index = 0; index < cycle.size(); ++index) {
                    if (index)
                        message << " -> ";
                    message << PatchName(groups[cycle[index]].Patch);
                }
            }
            message << '.';
            return Failure(Error::SourceOrderCycle, message.str());
        }

        logical.Fingerprint = kHashOffset;
        Hash(logical.Fingerprint, endpoint);
        for (const std::size_t index : ordered) {
            const Group &group = groups[index];
            for (const Relation &relation : group.Value->Relations) {
                logical.Relations.push_back(
                    {group.Patch, group.Priority, relation.Kind,
                     relation.Ordinal, relation.Fingerprint});
                Hash(logical.Fingerprint, group.Patch);
                Hash(logical.Fingerprint,
                     static_cast<std::int32_t>(group.Priority));
                Hash(logical.Fingerprint, relation.Kind);
                Hash(logical.Fingerprint, relation.Ordinal);
                Hash(logical.Fingerprint, relation.Fingerprint);
            }
        }
    }

    fingerprint = kHashOffset;
    Hash(fingerprint, static_cast<std::uint64_t>(pins.size()));
    for (const auto &[endpoint, logical] : pins) {
        Hash(fingerprint, endpoint);
        Hash(fingerprint, logical.Fingerprint);
    }
    return {};
}

} // namespace BML::Behavior::Internal
