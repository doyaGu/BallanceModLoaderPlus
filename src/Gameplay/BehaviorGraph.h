#ifndef BML_GAMEPLAY_BEHAVIORGRAPH_H
#define BML_GAMEPLAY_BEHAVIORGRAPH_H

#include <cstdint>
#include <string_view>

#include "BML/Behavior.hpp"

namespace Gameplay::Graph {

inline BML::Behavior::Node NodeById(const BML::Behavior::Graph &graph,
                                    std::uint64_t id) {
    for (BML::Behavior::Node node : graph.Nodes()) {
        if (node.Id() == id)
            return node;
    }
    return {};
}

inline int PortCount(const BML::Behavior::Node &node,
                     BML::Behavior::SlotKind kind) {
    int count = 0;
    for (BML::Behavior::Port port : node.Ports()) {
        if (port.Kind() == kind)
            ++count;
    }
    return count;
}

inline BML::Behavior::Node Find(const BML::Behavior::Graph &graph,
                                std::string_view name,
                                int inputs = -1, int outputs = -1,
                                int pins = -1, int pouts = -1) {
    using BML::Behavior::SlotKind;
    BML::Behavior::Node found;
    for (BML::Behavior::Node node : graph.Nodes()) {
        if (node.Index() < 0 || node.Name() != name)
            continue;
        if ((inputs >= 0 && PortCount(node, SlotKind::In) != inputs) ||
            (outputs >= 0 && PortCount(node, SlotKind::Out) != outputs) ||
            (pins >= 0 && PortCount(node, SlotKind::Pin) != pins) ||
            (pouts >= 0 && PortCount(node, SlotKind::Pout) != pouts))
            continue;
        if (found)
            return {};
        found = node;
    }
    return found;
}

inline BML::Behavior::Node Next(const BML::Behavior::Graph &graph,
                                const BML::Behavior::Node &node,
                                int output = 0) {
    const auto next = graph.Next(node.Out(output));
    return next ? next.Value() : BML::Behavior::Node{};
}

inline BML::Behavior::Node Previous(const BML::Behavior::Graph &graph,
                                    const BML::Behavior::Node &node,
                                    int input = 0) {
    const auto previous = graph.Previous(node.In(input));
    return previous ? previous.Value() : BML::Behavior::Node{};
}

inline BML::Behavior::Node Follow(const BML::Behavior::Graph &graph,
                                  BML::Behavior::Node node, int count) {
    for (int index = 0; node && index < count; ++index)
        node = Next(graph, node);
    return node;
}

inline BML::Behavior::Node EndOfChain(const BML::Behavior::Graph &graph,
                                      BML::Behavior::Node node) {
    for (std::size_t depth = 0; node && depth <= graph.Nodes().size();
         ++depth) {
        if (graph.Outgoing(node).empty())
            return node;
        const auto next = graph.Next(node);
        if (!next)
            return {};
        node = next.Value();
    }
    return {};
}

inline BML::Behavior::Link Leaving(const BML::Behavior::Graph &graph,
                                   const BML::Behavior::Node &node,
                                   int output = 0) {
    const auto link = graph.Leaving(node.Out(output));
    return link ? link.Value() : BML::Behavior::Link{};
}

inline BML::Behavior::Link Entering(const BML::Behavior::Graph &graph,
                                    const BML::Behavior::Node &node,
                                    int input = 0) {
    const auto link = graph.Entering(node.In(input));
    return link ? link.Value() : BML::Behavior::Link{};
}

inline BML::Behavior::Link LeavingFor(const BML::Behavior::Graph &graph,
                                      const BML::Behavior::Node &source,
                                      std::string_view sinkName,
                                      int output = 0) {
    BML::Behavior::Link found;
    for (BML::Behavior::Link link : graph.Outgoing(source.Out(output))) {
        const BML::Behavior::Node sink = NodeById(
            graph, link.Target().Node());
        if (!sink || sink.Name() != sinkName)
            continue;
        if (found)
            return {};
        found = link;
    }
    return found;
}

inline BML::Behavior::Node Sink(const BML::Behavior::Graph &graph,
                                const BML::Behavior::Link &link) {
    return link ? NodeById(graph, link.Target().Node())
                : BML::Behavior::Node{};
}

} // namespace Gameplay::Graph

#endif // BML_GAMEPLAY_BEHAVIORGRAPH_H
