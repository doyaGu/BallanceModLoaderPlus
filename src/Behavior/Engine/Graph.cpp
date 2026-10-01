#include "Behavior/Engine/Graph.h"

#include "Behavior/Engine/Access.h"

#include <algorithm>
#include <vector>

namespace BML::Behavior::Internal::Engine {

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
        SetParentId(node, parent);
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
    // GetInBehaviorIO is just a pointer read. Checking that pointer against
    // CK2's live IO list avoids reading an id or owner from freed memory.
    CKBehaviorIO *source = link->GetInBehaviorIO();
    if (source) {
        const int count = context->GetObjectsCountByClassID(CKCID_BEHAVIORIO);
        const CK_ID *ids = context->GetObjectsListByClassID(CKCID_BEHAVIORIO);
        for (int index = 0; index < count; ++index) {
            if (context->GetObject(ids[index]) != source)
                continue;
            RemoveOutgoingLink(source, link);
            break;
        }
    }
    ClearEndpoints(link);
    return context->DestroyObject(link);
}

} // namespace BML::Behavior::Internal::Engine
