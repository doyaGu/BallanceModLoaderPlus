#ifndef BML_VIRTOOLS_CKGRAPHORDER_H
#define BML_VIRTOOLS_CKGRAPHORDER_H

#include <CKBehavior.h>
#include <CKBehaviorIO.h>

namespace BML {

// CK2 schedules equal-priority children in m_SubBehaviors order and traverses
// each source IO's m_Links in order. The retail SDK exposes the backing array
// types but no positional graph-edit API, so the CK adapter is the one place
// that reads and restores those domain-significant orders.
class CKGraphOrder {
public:
    static XObjectPointerArray *Nodes(CKBehavior *graph) noexcept;
    static XObjectPointerArray *Links(CKBehavior *graph) noexcept;
    static XSObjectPointerArray *Outgoing(CKBehaviorIO *source) noexcept;

    // AddSubBehavior uses an unstable priority-only qsort. Retain the old
    // relative order and insert the new Node after existing Nodes of the same
    // final priority. A child parked by retail RemoveSubBehavior still names
    // this graph as its parent even though it is no longer a member.
    // Working storage is prepared before CK mutates the graph; priorities are
    // read after ATTACH callbacks complete.
    static CKERROR Add(CKBehavior *graph, CKBehavior *node);
};

} // namespace BML

#endif // BML_VIRTOOLS_CKGRAPHORDER_H
