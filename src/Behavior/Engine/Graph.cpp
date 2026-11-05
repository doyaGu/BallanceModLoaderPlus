#include "Behavior/Engine/Graph.h"

#include "Behavior/Engine/Access.h"

#include <algorithm>
#include <vector>

namespace BML::Behavior::Internal::Engine {

namespace {

bool MatchesChildren(CKBehavior *graph, const std::vector<CKBehavior *> &order) {
    return graph->GetSubBehaviorCount() == static_cast<int>(order.size()) &&
        std::all_of(order.begin(), order.end(), [&](CKBehavior *node) {
            return Contains(graph, node);
        });
}

void SortChildren(std::vector<CKBehavior *> &order) {
    std::stable_sort(order.begin(), order.end(), [](CKBehavior *left, CKBehavior *right) {
        return left->GetPriority() > right->GetPriority();
    });
}

CKERROR ArrangeChildren(CKBehavior *graph, const std::vector<CKBehavior *> &order) {
    for (int dst = 0; dst < static_cast<int>(order.size()); ++dst) {
        int src = dst;
        while (src < graph->GetSubBehaviorCount() && graph->GetSubBehavior(src) != order[dst])
            ++src;
        if (src == graph->GetSubBehaviorCount())
            return CKERR_INVALIDOBJECT;
        if (src != dst) {
            const CKERROR error = graph->MoveSubBehavior(src, dst);
            if (error != CK_OK)
                return error;
        }
    }
    return CK_OK;
}

} // namespace

bool Contains(CKBehavior *graph, CKBehavior *node) {
    if (!graph || !node)
        return false;
    for (int index = 0; index < graph->GetSubBehaviorCount(); ++index) {
        if (graph->GetSubBehavior(index) == node)
            return true;
    }
    return false;
}

bool Contains(CKBehavior *graph, CKBehaviorLink *link) {
    if (!graph || !link)
        return false;
    for (int index = 0; index < graph->GetSubBehaviorLinkCount(); ++index) {
        if (graph->GetSubBehaviorLink(index) == link)
            return true;
    }
    return false;
}

CKERROR AddChild(CKBehavior *graph, CKBehavior *node) {
    if (!graph || !node || graph == node || graph->GetType() == CKBEHAVIORTYPE_BASE)
        return CKERR_INVALIDPARAMETER;
    CKBehavior *const parent = node->GetParent();
    if (parent && parent != graph)
        return CKERR_INVALIDPARAMETER;

    std::vector<CKBehavior *> before;
    before.reserve(static_cast<std::size_t>(graph->GetSubBehaviorCount()));
    for (int index = 0; index < graph->GetSubBehaviorCount(); ++index) {
        CKBehavior *existing = graph->GetSubBehavior(index);
        if (!existing || existing == node)
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

    if (MatchesChildren(graph, order) && node->GetParent() == graph) {
        // Native attachment may change priorities and uses an unstable sort.
        // Keep its final priorities and restore the original order within ties.
        SortChildren(order);
        if (ArrangeChildren(graph, order) == CK_OK)
            return CK_OK;
    }

    if (Contains(graph, node))
        (void) graph->RemoveSubBehavior(node);
    node->SetParent(parent);
    (void) node->SetOwner(owner, TRUE);
    node->SetFlags(flags);
    if (MatchesChildren(graph, before)) {
        graph->SetCompatibleClassID(compatible);
        // Retain priority changes made by callbacks. Public moves preserve
        // those priority groups while restoring their previous relative order.
        SortChildren(before);
        (void) ArrangeChildren(graph, before);
    }
    return CKERR_INVALIDOBJECT;
}

void DestroyConnectedLinks(CKContext *context, CKBehavior *graph,
                           CKBehavior *node) {
    if (!context || !graph || !node)
        return;
    for (int i = graph->GetSubBehaviorLinkCount() - 1; i >= 0; --i) {
        CKBehaviorLink *link = graph->GetSubBehaviorLink(i);
        if (!link)
            continue;
        CKBehaviorIO *source = link->GetInBehaviorIO();
        CKBehaviorIO *destination = link->GetOutBehaviorIO();
        if ((!source || source->GetOwner() != node) &&
            (!destination || destination->GetOwner() != node)) {
            continue;
        }
        link = graph->RemoveSubBehaviorLink(i);
        if (link)
            context->DestroyObject(link);
    }
}

void DestroyConnectedLinks(CKContext *context, CKBehaviorIO *io) {
    CKBehavior *owner = io ? io->GetOwner() : nullptr;
    if (!context || !owner)
        return;
    for (CKBehavior *graph : {owner, owner->GetParent()}) {
        if (!graph)
            continue;
        for (int i = graph->GetSubBehaviorLinkCount() - 1; i >= 0; --i) {
            CKBehaviorLink *link = graph->GetSubBehaviorLink(i);
            if (!link || (link->GetInBehaviorIO() != io &&
                          link->GetOutBehaviorIO() != io))
                continue;
            link = graph->RemoveSubBehaviorLink(i);
            if (link)
                context->DestroyObject(link);
        }
    }
}

CKERROR DestroyOrphanLink(CKContext *context, CKBehaviorLink *link) {
    if (!context || !link)
        return CKERR_INVALIDPARAMETER;
    return context->DestroyObject(link);
}

} // namespace BML::Behavior::Internal::Engine
