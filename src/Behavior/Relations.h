#ifndef BML_BEHAVIOR_RELATIONS_H
#define BML_BEHAVIOR_RELATIONS_H

#include <cstdint>
#include <map>
#include <vector>

#include "Behavior/Topology.h"

namespace BML::Behavior::Internal {

enum class RelationKind {
    Bind,
    Transform,
};

struct Relation {
    RelationKind Kind = RelationKind::Bind;
    std::uint32_t Ordinal = 0;
    std::uint64_t Fingerprint = 0;
};

struct PinRelations {
    GraphEndpoint Pin;
    std::vector<Order> Ordering;
    std::vector<Relation> Relations;
};

struct RelationLayer {
    PatchKey Patch;
    int Priority = 0;
    std::vector<PinRelations> Pins;
};

struct OrderedRelation {
    PatchKey Patch;
    int Priority = 0;
    RelationKind Kind = RelationKind::Bind;
    std::uint32_t Ordinal = 0;
    std::uint64_t Fingerprint = 0;
};

struct LogicalPin {
    GraphEndpoint Pin;
    std::uint64_t Fingerprint = 0;
    std::vector<OrderedRelation> Relations;
};

// Logical Pin-source ownership for one live graph. Bind is exclusive;
// Transform is explicitly layered and ordered. This class validates and
// publishes relation intent only; the CK adapter owns native mutation.
class Relations final {
public:
    Status Validate(RelationLayer layer) const;
    Status Set(RelationLayer layer);
    bool Remove(const PatchKey &patch);

    [[nodiscard]] const LogicalPin *Find(
        const GraphEndpoint &pin) const noexcept;
    [[nodiscard]] std::uint64_t Fingerprint() const noexcept {
        return m_Fingerprint;
    }

private:
    struct EndpointLess {
        bool operator()(const GraphEndpoint &left,
                        const GraphEndpoint &right) const noexcept;
    };

    using LayerMap = std::map<PatchKey, RelationLayer>;
    using PinMap = std::map<GraphEndpoint, LogicalPin, EndpointLess>;

    Status Normalize(RelationLayer &layer) const;
    Status Compose(const LayerMap &layers, PinMap &pins,
                   std::uint64_t &fingerprint) const;

    LayerMap m_Layers;
    PinMap m_Pins;
    std::uint64_t m_Fingerprint = 0;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_RELATIONS_H
