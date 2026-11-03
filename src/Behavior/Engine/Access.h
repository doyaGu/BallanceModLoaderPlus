#ifndef BML_BEHAVIOR_ENGINE_ACCESS_H
#define BML_BEHAVIOR_ENGINE_ACCESS_H

#include <CKBehavior.h>
#include <CKBehaviorIO.h>
#include <CKBehaviorLink.h>

namespace BML::Behavior::Internal::Engine {

// The retail SDK declares these CK2 fields protected and exposes no public
// accessor for them. This is the one place that reads or writes them.

// Retail CREATE callback workaround. SDL uses CallCallbackFunction instead.
// The live Block callback, mask, and argument; null for a graph.
#if !BML_RUNTIME_SDL
BehaviorBlockData *BlockData(CKBehavior *behavior) noexcept;
#endif

// Retail graph order access. SDL uses the public graph queries and moves.
#if !BML_RUNTIME_SDL
XObjectPointerArray *Nodes(CKBehavior *graph) noexcept;
XObjectPointerArray *Links(CKBehavior *graph) noexcept;
#endif
// Orphan Link cleanup still needs the surviving source IO's outgoing list.
XSObjectPointerArray *Outgoing(CKBehaviorIO *source) noexcept;

// Compact without reallocating: the backing storage belongs to CK2's CRT.
// SDK XSArray::Remove would free it through the caller's CRT instead.
void RemoveOutgoingLink(CKBehaviorIO *source, CKBehaviorLink *link) noexcept;

// Restores the parent reference through SetParent on SDL. Retail CK2 ignores
// SetParent(null), so win32 retains its direct-field workaround.
void SetParentId(CKBehavior *behavior, CKBehavior *parent) noexcept;

// The native Link setters reject null. Use only after removing the Link from
// its surviving source IO's outgoing array; neither old endpoint is read.
void ClearEndpoints(CKBehaviorLink *link) noexcept;

// True while CK2 holds the Link in its graph's delayed-link list. CK2 sets
// bit 0x1 of m_OldFlags when it pushes a delayed activation and clears it
// when the delay completes, when the graph resets, and when the Link is
// copied (Ballanced CKBehavior.cpp and CKBehaviorLink.cpp). The SDK header
// does not name the bit.
bool IsDelayed(CKBehaviorLink *link) noexcept;

} // namespace BML::Behavior::Internal::Engine

#endif // BML_BEHAVIOR_ENGINE_ACCESS_H
