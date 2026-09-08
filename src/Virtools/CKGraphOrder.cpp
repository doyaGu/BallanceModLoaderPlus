#include "Virtools/CKGraphOrder.h"

#include <algorithm>
#include <vector>

namespace BML {
namespace {

class BehaviorMember : public CKBehavior {
public:
    static BehaviorGraphData *Data(CKBehavior *behavior) noexcept {
        return behavior
            ? behavior->*(&BehaviorMember::m_GraphData) : nullptr;
    }

    static void Parent(CKBehavior *behavior, CKBehavior *parent) noexcept {
        if (behavior)
            behavior->*(&BehaviorMember::m_BehParent) =
                parent ? parent->GetID() : 0;
    }
};

class IoMember : public CKBehaviorIO {
public:
    static XSObjectPointerArray *Links(CKBehaviorIO *io) noexcept {
        return io ? &(io->*(&IoMember::m_Links)) : nullptr;
    }
};

} // namespace

XObjectPointerArray *CKGraphOrder::Nodes(CKBehavior *graph) noexcept {
    BehaviorGraphData *data = BehaviorMember::Data(graph);
    return data ? &data->m_SubBehaviors : nullptr;
}

XObjectPointerArray *CKGraphOrder::Links(CKBehavior *graph) noexcept {
    BehaviorGraphData *data = BehaviorMember::Data(graph);
    return data ? &data->m_SubBehaviorLinks : nullptr;
}

XSObjectPointerArray *CKGraphOrder::Outgoing(
    CKBehaviorIO *source) noexcept {
    return IoMember::Links(source);
}

CKERROR CKGraphOrder::Add(CKBehavior *graph, CKBehavior *node) {
    XObjectPointerArray *nodes = Nodes(graph);
    if (!nodes || !node)
        return CKERR_INVALIDPARAMETER;

    // Retail CK2's SetParent(nullptr) is a no-op, so RemoveSubBehavior removes
    // array membership without clearing m_BehParent. Membership in
    // m_SubBehaviors is therefore authoritative. Accept that parked state,
    // but never move a child that belongs to another graph.
    CKBehavior *const parent = node->GetParent();
    if (parent && parent != graph)
        return CKERR_INVALIDPARAMETER;

    std::vector<CKBehavior *> before;
    before.reserve(static_cast<std::size_t>(nodes->Size()));
    for (int index = 0; index < nodes->Size(); ++index) {
        auto *existing = static_cast<CKBehavior *>((*nodes)[index]);
        if (existing == node)
            return CKERR_INVALIDPARAMETER;
        before.push_back(existing);
    }
    std::vector<CKBehavior *> order = before;
    order.push_back(node);

    CKBeObject *const owner = node->GetOwner();
    const CK_BEHAVIOR_FLAGS flags = node->GetFlags();
    const CK_CLASSID compatible = graph->GetCompatibleClassID();
    const CKERROR error = graph->AddSubBehavior(node);
    if (error != CK_OK)
        return error;

    bool sameMembers = nodes->Size() == static_cast<int>(order.size()) &&
        nodes->GetPosition(node) >= 0 && node->GetParent() == graph;
    for (CKBehavior *existing : before)
        sameMembers = sameMembers && nodes->GetPosition(existing) >= 0;
    if (!sameMembers) {
        // AddSubBehavior can report success even when AddIfNotHere could not
        // grow its array. Restore every field it changed before the caller is
        // allowed to destroy the rejected Node.
        if (nodes->GetPosition(node) >= 0)
            (void) graph->RemoveSubBehavior(node);
        BehaviorMember::Parent(node, parent);
        (void) node->SetOwner(owner, TRUE);
        node->SetFlags(flags);
        const bool originalMembers = nodes->Size() ==
            static_cast<int>(before.size()) &&
            std::all_of(before.begin(), before.end(), [&](CKBehavior *item) {
                return nodes->GetPosition(item) >= 0;
            });
        if (originalMembers) {
            graph->SetCompatibleClassID(compatible);
            for (int index = 0; index < nodes->Size(); ++index)
                (*nodes)[index] = before[static_cast<std::size_t>(index)];
        }
        return CKERR_INVALIDOBJECT;
    }

    // ATTACH callbacks may have changed a Node priority. Compute the final
    // scheduler order only after the native relation and callbacks complete.
    std::stable_sort(
        order.begin(), order.end(),
        [](CKBehavior *left, CKBehavior *right) {
            return left && right
                ? left->GetPriority() > right->GetPriority()
                : left != nullptr;
        });
    for (int index = 0; index < nodes->Size(); ++index)
        (*nodes)[index] = order[static_cast<std::size_t>(index)];
    return CK_OK;
}

} // namespace BML
