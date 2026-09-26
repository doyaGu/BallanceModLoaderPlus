#include "Behavior/Engine/Access.h"

namespace BML::Behavior::Internal::Engine {
namespace {

constexpr CKDWORD InDelayedList = 0x00000001;

// A derived-class member pointer names a protected base field without
// constructing or casting to the derived type.
class BehaviorMember final : public CKBehavior {
public:
    static BehaviorBlockData *Block(CKBehavior *behavior) noexcept {
        return behavior ? behavior->*(&BehaviorMember::m_BlockData) : nullptr;
    }

    static BehaviorGraphData *Graph(CKBehavior *behavior) noexcept {
        return behavior ? behavior->*(&BehaviorMember::m_GraphData) : nullptr;
    }

    static void Parent(CKBehavior *behavior, CKBehavior *parent) noexcept {
        if (behavior)
            behavior->*(&BehaviorMember::m_BehParent) =
                parent ? parent->GetID() : 0;
    }
};

class IoMember final : public CKBehaviorIO {
public:
    static XSObjectPointerArray *Links(CKBehaviorIO *io) noexcept {
        return io ? &(io->*(&IoMember::m_Links)) : nullptr;
    }
};

class LinkMember final : public CKBehaviorLink {
public:
    static CKDWORD OldFlags(CKBehaviorLink *link) noexcept {
        return link ? link->*(&LinkMember::m_OldFlags) : 0;
    }
};

} // namespace

BehaviorBlockData *BlockData(CKBehavior *behavior) noexcept {
    return BehaviorMember::Block(behavior);
}

XObjectPointerArray *Nodes(CKBehavior *graph) noexcept {
    BehaviorGraphData *data = BehaviorMember::Graph(graph);
    return data ? &data->m_SubBehaviors : nullptr;
}

XObjectPointerArray *Links(CKBehavior *graph) noexcept {
    BehaviorGraphData *data = BehaviorMember::Graph(graph);
    return data ? &data->m_SubBehaviorLinks : nullptr;
}

XSObjectPointerArray *Outgoing(CKBehaviorIO *source) noexcept {
    return IoMember::Links(source);
}

void SetParentId(CKBehavior *behavior, CKBehavior *parent) noexcept {
    BehaviorMember::Parent(behavior, parent);
}

bool IsDelayed(CKBehaviorLink *link) noexcept {
    return (LinkMember::OldFlags(link) & InDelayedList) != 0;
}

} // namespace BML::Behavior::Internal::Engine
