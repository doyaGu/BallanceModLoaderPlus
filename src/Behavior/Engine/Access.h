#ifndef BML_BEHAVIOR_ENGINE_ACCESS_H
#define BML_BEHAVIOR_ENGINE_ACCESS_H

#include <CKBehavior.h>
#include <CKBehaviorIO.h>
#include <CKBehaviorLink.h>

namespace BML::Behavior::Internal::Engine {

// The retail SDK declares these CK2 fields protected and exposes no public
// accessor for them. This is the one place that reads or writes them.

// The prototype's callback, mask, and argument that the Block was created
// with. Null for a graph.
BehaviorBlockData *BlockData(CKBehavior *behavior) noexcept;

// CK2 schedules equal-priority children in m_SubBehaviors order and traverses
// each source IO's m_Links in order. The SDK exposes the backing arrays but no
// positional edit API, so graph edits that must restore an order use these.
XObjectPointerArray *Nodes(CKBehavior *graph) noexcept;
XObjectPointerArray *Links(CKBehavior *graph) noexcept;
XSObjectPointerArray *Outgoing(CKBehaviorIO *source) noexcept;

// Writes m_BehParent without SetParent, which retail CK2 ignores for null.
void SetParentId(CKBehavior *behavior, CKBehavior *parent) noexcept;

// True while CK2 holds the Link in its graph's delayed-link list. CK2 sets
// bit 0x1 of m_OldFlags when it pushes a delayed activation and clears it
// when the delay completes, when the graph resets, and when the Link is
// copied (Ballanced CKBehavior.cpp and CKBehaviorLink.cpp). The SDK header
// does not name the bit.
bool IsDelayed(CKBehaviorLink *link) noexcept;

} // namespace BML::Behavior::Internal::Engine

#endif // BML_BEHAVIOR_ENGINE_ACCESS_H
