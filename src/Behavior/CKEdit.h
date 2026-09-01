#ifndef BML_BEHAVIOR_CKEDIT_H
#define BML_BEHAVIOR_CKEDIT_H

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <thread>

#include "Behavior/Edit.h"
#include "Behavior/PrototypeCatalog.h"

namespace BML::Behavior {

class Patch final {
public:
    Patch();
    ~Patch();
    Patch(const Patch &) = delete;
    Patch &operator=(const Patch &) = delete;
    Patch(Patch &&) noexcept;
    Patch &operator=(Patch &&) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;

private:
    struct Data;
    std::unique_ptr<Data> m_Data;

    friend class CKEdit;
};

// The CK2 adapter for one-shot additive Edits. Durable reconciliation and
// dispatch-safe queuing intentionally remain outside this step.
class CKEdit final {
public:
    CKEdit(CKContext *context, Runtime &runtime, PrototypeCatalog *catalog,
           GraphSource &graph);

    Status Begin(CKBehavior *graph, PatchKey key, Edit &out);
    Status Use(Edit &edit, CKBehavior *behavior, Node &out);
    Status Add(Edit &edit, Spec block, Node &out);
    Status Apply(const Edit &edit, Patch &out);
    Status Close(Patch &patch);

    [[nodiscard]] std::uint64_t TopologyFingerprint(CKBehavior *graph) const;

private:
    Status Ready() const;
    Status Undo(Patch::Data &patch, bool notify);

    CKContext *m_Context = nullptr;
    Runtime &m_Runtime;
    PrototypeCatalog *m_Catalog = nullptr;
    GraphSource &m_Graph;
    std::thread::id m_Thread;
    std::map<std::uint64_t, Topology> m_Topology;
    std::map<std::uint64_t, std::set<PatchKey>> m_Active;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_CKEDIT_H
