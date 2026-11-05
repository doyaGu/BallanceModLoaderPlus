#ifndef BML_BEHAVIOR_ENGINE_ACCESS_H
#define BML_BEHAVIOR_ENGINE_ACCESS_H

#include <CKBehavior.h>
#include <CKBehaviorIO.h>
#include <CKBehaviorLink.h>

namespace BML::Behavior::Internal::Engine {

// Restore the parent reference through CK2's public SetParent method.
void SetParentId(CKBehavior *behavior, CKBehavior *parent) noexcept;

// True while CK2 holds the Link in its graph's delayed-link list. CK2 sets
// bit 0x1 of m_OldFlags when it pushes a delayed activation and clears it
// when the delay completes, when the graph resets, and when the Link is
// copied (Ballanced CKBehavior.cpp and CKBehaviorLink.cpp). The SDK header
// does not name the bit.
bool IsDelayed(CKBehaviorLink *link) noexcept;

} // namespace BML::Behavior::Internal::Engine

#endif // BML_BEHAVIOR_ENGINE_ACCESS_H
