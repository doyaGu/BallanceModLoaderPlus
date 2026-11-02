#include "Behavior/Edit/Journal.h"

#include "Behavior/Engine/Access.h"

#include <algorithm>
#include <cstddef>
#include <unordered_set>

namespace BML::Behavior::Internal {
namespace {

Status OrderFailure(bool restoring, std::string message) {
    Status status = Failure(
        restoring ? Error::RevertConflict : Error::GraphChanged,
        std::move(message), CKERR_INVALIDOBJECT);
    if (restoring)
        status.Details.Stage = Phase::Teardown;
    return status;
}

bool HasGraph(CKBehavior *graph) {
#if BML_RUNTIME_SDL
    return graph && graph->GetType() != CKBEHAVIORTYPE_BASE;
#else
    return Engine::Nodes(graph) && Engine::Links(graph);
#endif
}

std::vector<CKBehavior *> ReadNodes(CKBehavior *graph) {
    std::vector<CKBehavior *> result;
    if (graph) {
        result.reserve(static_cast<std::size_t>(graph->GetSubBehaviorCount()));
        for (int i = 0; i < graph->GetSubBehaviorCount(); ++i)
            result.push_back(graph->GetSubBehavior(i));
    }
    return result;
}

std::vector<CKBehaviorLink *> ReadSource(CKBehaviorIO *source) {
    std::vector<CKBehaviorLink *> result;
#if BML_RUNTIME_SDL
    for (int i = 0; source && i < source->GetLinkCount(); ++i)
        result.push_back(source->GetLink(i));
#else
    XSObjectPointerArray *array = Engine::Outgoing(source);
    for (int i = 0; array && i < array->Size(); ++i)
        result.push_back(static_cast<CKBehaviorLink *>((*array)[i]));
#endif
    return result;
}

CKERROR WriteNodes(CKBehavior *graph, const std::vector<CKBehavior *> &order) {
#if BML_RUNTIME_SDL
    for (int dst = 0; dst < static_cast<int>(order.size()); ++dst) {
        int src = dst;
        while (src < graph->GetSubBehaviorCount() && graph->GetSubBehavior(src) != order[dst])
            ++src;
        const CKERROR error = graph->MoveSubBehavior(src, dst);
        if (error != CK_OK)
            return error;
    }
#else
    XObjectPointerArray *array = Engine::Nodes(graph);
    for (int i = 0; i < array->Size(); ++i)
        (*array)[i] = order[static_cast<std::size_t>(i)];
#endif
    return CK_OK;
}

CKERROR WriteSource(CKBehaviorIO *source, const std::vector<CKBehaviorLink *> &order) {
#if BML_RUNTIME_SDL
    for (int dst = 0; dst < static_cast<int>(order.size()); ++dst) {
        int src = dst;
        while (src < source->GetLinkCount() && source->GetLink(src) != order[dst])
            ++src;
        const CKERROR error = source->MoveLink(src, dst);
        if (error != CK_OK)
            return error;
    }
#else
    XSObjectPointerArray *array = Engine::Outgoing(source);
    for (int i = 0; i < array->Size(); ++i)
        (*array)[i] = order[static_cast<std::size_t>(i)];
#endif
    return CK_OK;
}

template <typename T>
T *ResolveOrderObject(CKContext *context, Stamp original,
                      const OrderAliases &aliases, CK_CLASSID type) {
    const auto alias = aliases.find(original.Id);
    return Resolve<T>(context, alias == aliases.end() ? original : alias->second,
                      type);
}

CKERROR ArrangeNodes(CKContext *context, CKBehavior *graph,
                     const std::vector<Stamp> &before,
                     const OrderAliases &aliases) {
    std::vector<CKBehavior *> current = ReadNodes(graph);
    std::vector<CKBehavior *> expected;
    expected.reserve(before.size());
    for (Stamp stamp : before) {
        CKBehavior *item = ResolveOrderObject<CKBehavior>(
            context, stamp, aliases, CKCID_BEHAVIOR);
        if (item && std::find(current.begin(), current.end(), item) != current.end())
            expected.push_back(item);
        // Another Patch may have owned an object that existed when this layer
        // was installed and retired before this layer. Order the surviving
        // intersection; topology and identity validation diagnose deletion of
        // objects this Patch itself requires.
    }

    std::unordered_map<CKBehavior *, std::size_t> rank;
    for (std::size_t index = 0; index < expected.size(); ++index)
        rank.emplace(expected[index], index);
    std::stable_sort(
        current.begin(), current.end(),
        [&](CKBehavior *left, CKBehavior *right) {
            const int leftPriority = left ? left->GetPriority() : 0;
            const int rightPriority = right ? right->GetPriority() : 0;
            if (leftPriority != rightPriority)
                return leftPriority > rightPriority;
            const auto leftRank = rank.find(left);
            const auto rightRank = rank.find(right);
            if (leftRank != rank.end() && rightRank != rank.end())
                return leftRank->second < rightRank->second;
            if (leftRank != rank.end() || rightRank != rank.end())
                return leftRank != rank.end();
            return false;
        });
    return WriteNodes(graph, current);
}

Status ArrangeSource(CKContext *context,
                     const NativeGraphOrder::Source &before,
                     const OrderAliases &portAliases, bool restoring) {
    const auto alias = portAliases.find(before.Port.Id);
    CKBehaviorIO *source = ResolveIo(
        context, alias == portAliases.end() ? before.Port : alias->second);
    if (!source)
        return restoring
            ? Status{}
            : OrderFailure(false,
                           "A source Behavior IO disappeared while the Edit was applied.");
    std::vector<CKBehaviorLink *> current = ReadSource(source);
    std::vector<CKBehaviorLink *> expected;
    expected.reserve(before.Links.size());
    for (Stamp stamp : before.Links) {
        CKBehaviorLink *link = Resolve<CKBehaviorLink>(
            context, stamp, CKCID_BEHAVIORLINK);
        if (link && std::find(current.begin(), current.end(), link) != current.end())
            expected.push_back(link);
        // Missing links can belong to a previously retired Patch layer.
    }
    std::unordered_set<CKBehaviorLink *> original(expected.begin(), expected.end());
    std::vector<std::size_t> positions;
    for (std::size_t index = 0; index < current.size(); ++index) {
        if (original.contains(current[index]))
            positions.push_back(index);
    }
    if (positions.size() != expected.size())
        return OrderFailure(
            restoring,
            restoring
                ? "A Link disappeared before its source order could be restored."
                : "A source Behavior IO changed Link identity while the Edit was applied.");
    for (std::size_t index = 0; index < positions.size(); ++index)
        current[positions[index]] = expected[index];
    if (WriteSource(source, current) != CK_OK)
        return OrderFailure(restoring, "The source Link order could not be restored.");
    return {};
}

template <typename T>
bool KeepsRelativeOrder(CKContext *context, const std::vector<T *> &current,
                        const std::vector<Stamp> &expected,
                        CK_CLASSID type) {
    std::size_t position = 0;
    for (Stamp stamp : expected) {
        T *item = Resolve<T>(context, stamp, type);
        if (!item || std::find(current.begin(), current.end(), item) ==
                         current.end()) {
            // An adjacent Patch may have owned an object that was present in
            // this after-image and retired before this Patch. Only compare the
            // intersection still owned by the graph.
            continue;
        }
        while (position < current.size() && current[position] != item)
            ++position;
        if (position == current.size())
            return false;
        ++position;
    }
    return true;
}

bool KeepsNodeOrder(CKContext *context, CKBehavior *graph,
                    const std::vector<Stamp> &expected,
                    const std::vector<int> &priorities) {
    if (!HasGraph(graph) || priorities.size() != expected.size())
        return false;
    const std::vector<CKBehavior *> current = ReadNodes(graph);
    std::unordered_map<CKBehavior *, std::size_t> rank;
    rank.reserve(expected.size());
    bool reprioritized = false;
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CKBehavior *node = Resolve<CKBehavior>(
            context, expected[index], CKCID_BEHAVIOR);
        if (!node || std::find(current.begin(), current.end(), node) ==
                         current.end())
            continue;
        reprioritized = reprioritized ||
            node->GetPriority() != priorities[index];
        rank.emplace(node, index);
    }

    // SetPriority asks CK2 to sort the complete child array with an unstable
    // priority-only comparator. One legitimate priority change can therefore
    // reorder unchanged siblings as a side effect; the old array cannot prove
    // an external order conflict after that operation.
    if (reprioritized)
        return true;

    // With stable priorities, a Patch owns order only within each scheduler
    // priority group. Moving an entire group is not a same-priority reorder.
    std::unordered_map<int, std::size_t> last;
    for (CKBehavior *node : current) {
        const auto found = rank.find(node);
        if (found == rank.end())
            continue;
        const int priority = node->GetPriority();
        const auto previous = last.find(priority);
        if (previous != last.end() && found->second < previous->second)
            return false;
        last[priority] = found->second;
    }
    return true;
}

} // namespace

Status CaptureOrder(CKBehavior *graph, NativeGraphOrder &out) {
    out = {};
    if (!HasGraph(graph))
        return OrderFailure(false, "The target is not a live Behavior graph.");

    const std::vector<CKBehavior *> nodes = ReadNodes(graph);
    out.Nodes.reserve(nodes.size());
    for (CKBehavior *node : nodes) {
        if (!CKBehavior::Cast(node))
            return OrderFailure(false, "A graph child is not a Behavior Node.");
        out.Nodes.push_back(Capture(node));
        out.NodePriorities.push_back(node->GetPriority());
    }

    std::unordered_set<CKBehaviorLink *> graphLinks;
    for (int i = 0; i < graph->GetSubBehaviorLinkCount(); ++i) {
        CKBehaviorLink *link = graph->GetSubBehaviorLink(i);
        if (!CKBehaviorLink::Cast(link) || !graphLinks.insert(link).second)
            return OrderFailure(false, "A graph contains an invalid or repeated Link.");
    }
    std::unordered_set<CKBehaviorIO *> sources;
    for (int i = 0; i < graph->GetSubBehaviorLinkCount(); ++i) {
        CKBehaviorLink *link = graph->GetSubBehaviorLink(i);
        CKBehaviorIO *source = link->GetInBehaviorIO();
        if (!source)
            return OrderFailure(false, "A graph Link has no source Behavior IO.");
        if (!sources.insert(source).second)
            continue;
        NativeGraphOrder::Source record;
        record.Port = Capture(source);
        const std::vector<CKBehaviorLink *> outgoing = ReadSource(source);
        record.Links.reserve(outgoing.size());
        for (CKBehaviorLink *outgoingLink : outgoing) {
            if (!outgoingLink || !graphLinks.contains(outgoingLink))
                return OrderFailure(false, "A source Behavior IO reaches a Link owned by another graph.");
            record.Links.push_back(Capture(outgoingLink));
        }
        out.Sources.push_back(std::move(record));
    }
    return {};
}

Status ArrangeOrder(CKContext *context, CKBehavior *graph,
                    const NativeGraphOrder &before,
                    const OrderAliases &nodeAliases,
                    const OrderAliases &portAliases, bool restoring) {
    if (!graph)
        return OrderFailure(restoring, "The ordered Behavior graph disappeared.");
    if (ArrangeNodes(context, graph, before.Nodes, nodeAliases) != CK_OK)
        return OrderFailure(restoring, "The Behavior Node order could not be restored.");
    for (const NativeGraphOrder::Source &source : before.Sources) {
        Status status = ArrangeSource(context, source, portAliases, restoring);
        if (!status)
            return status;
    }
    return {};
}

Status ValidateOrder(CKContext *context, CKBehavior *graph,
                     const NativeGraphOrder &expected,
                     RevertSubject &subject) {
    subject = RevertSubject::Node;
    if (!graph || !KeepsNodeOrder(
            context, graph, expected.Nodes,
            expected.NodePriorities)) {
        return OrderFailure(
            true,
            "Behavior Node order changed after the Patch was published.");
    }

    subject = RevertSubject::Link;
    for (const NativeGraphOrder::Source &sourceOrder : expected.Sources) {
        CKBehaviorIO *source = ResolveIo(context, sourceOrder.Port);
        if (!source)
            continue;
        if (!KeepsRelativeOrder<CKBehaviorLink>(
                context, ReadSource(source), sourceOrder.Links,
                CKCID_BEHAVIORLINK)) {
            return OrderFailure(
                true,
                "A source Behavior IO's Link order changed after the Patch was published.");
        }
    }
    return {};
}

} // namespace BML::Behavior::Internal
