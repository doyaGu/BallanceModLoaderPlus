#ifndef BML_BEHAVIOR_ENGINE_GRAPH_H
#define BML_BEHAVIOR_ENGINE_GRAPH_H

#include "CKAll.h"

namespace BML::Behavior::Internal::Engine {

// Membership is the graph fact used by scheduling. Retail CK2 may retain a
// cached GetParent() value after removal, so parent identity alone does not
// prove that a Behavior is still a sub-behavior.
bool Contains(CKBehavior *graph, CKBehavior *node);
bool Contains(CKBehavior *graph, CKBehaviorLink *link);

// AddSubBehavior uses an unstable priority-only qsort. Retain the old
// relative order and insert the new Node after existing Nodes of the same
// final priority. A child parked by retail RemoveSubBehavior still names
// this graph as its parent even though it is no longer a member.
// Working storage is prepared before CK mutates the graph; priorities are
// read after ATTACH callbacks complete.
CKERROR AddChild(CKBehavior *graph, CKBehavior *node);

// Destroys every Link of the graph that starts or ends at the Node's IOs.
// RemoveSubBehavior leaves them in place.
void DestroyConnectedLinks(CKContext *context, CKBehavior *graph,
                           CKBehavior *node);

// Destroys every Link that starts or ends at the IO, in its owner's graph and
// in the owner's parent. Deleting the IO would only clear the Link's end.
void DestroyConnectedLinks(CKContext *context, CKBehaviorIO *io);

} // namespace BML::Behavior::Internal::Engine

#endif // BML_BEHAVIOR_ENGINE_GRAPH_H
