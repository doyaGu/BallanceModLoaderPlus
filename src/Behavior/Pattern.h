#ifndef BML_BEHAVIOR_PATTERN_H
#define BML_BEHAVIOR_PATTERN_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Behavior/Graph.h"
#include "Behavior/Value.h"

namespace BML::Behavior::Internal {

// A structural description of one child Behavior in a graph. It contains only
// facts that can be observed again after a world change; no CK object or
// author callback is retained.
struct NodePattern {
    enum class SelectorKind {
        Index,
        Name,
        Only,
    } Selector = SelectorKind::Only;

    struct PortCount {
        SlotKind Kind = SlotKind::Input;
        int Count = -1;

        friend bool operator==(const PortCount &,
                               const PortCount &) = default;
    };

    struct PortValue {
        Slot Port;
        Value Expected;

        friend bool operator==(const PortValue &,
                               const PortValue &) = default;
    };

    int Index = -1;
    std::string Name;
    int Occurrence = 0;
    bool Unique = false;
    CKGUID Prototype = CKGUID();
    std::optional<BehaviorKind> ExpectedKind;
    // Exact structural identity retained by Require(snapshot Node).
    std::uint64_t PortShape = 0;
    std::vector<PortCount> PortCounts;
    std::vector<PortValue> PortValues;

    NodePattern() = default;
    NodePattern(std::string name, CKGUID prototype = CKGUID())
        : Selector(name.empty() ? SelectorKind::Only : SelectorKind::Name),
          Name(std::move(name)), Unique(true), Prototype(prototype) {}

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] Status Validate() const;

    friend bool operator==(const NodePattern &,
                           const NodePattern &) = default;
};

// Reads only the values a NodePattern asks for. Implementations must use
// non-forcing Virtools reads: pattern resolution is observation, not graph
// execution.
class PatternValues {
public:
    virtual ~PatternValues() = default;
    virtual Status ReadPatternValue(const GraphNode &node, const Slot &slot,
                                    GraphValue &out) = 0;
};

// Resolves in native child-index order. Name occurrences are selected after
// every structural and value condition has been applied.
[[nodiscard]] Status Resolve(const GraphModel &graph, std::uint64_t parent,
                             const NodePattern &pattern,
                             PatternValues &values,
                             std::vector<const GraphNode *> &out);

// Resolves every matching child in stable native child-index order. Unlike
// Require, occurrence and uniqueness select no single element here.
[[nodiscard]] Status ResolveAll(const GraphModel &graph, std::uint64_t parent,
                                const NodePattern &pattern,
                                PatternValues &values,
                                std::vector<const GraphNode *> &out);

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_PATTERN_H
