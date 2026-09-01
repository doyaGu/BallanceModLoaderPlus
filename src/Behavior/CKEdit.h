#ifndef BML_BEHAVIOR_CKEDIT_H
#define BML_BEHAVIOR_CKEDIT_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "Behavior/Edit.h"
#include "Behavior/PrototypeCatalog.h"

namespace BML::Behavior {

enum class PatchState {
    Pending,
    Active,
    Closing,
    RepairRequired,
    Closed,
    Failed,
};

class Patch final {
public:
    Patch();
    ~Patch();
    Patch(const Patch &) = delete;
    Patch &operator=(const Patch &) = delete;
    Patch(Patch &&) noexcept;
    Patch &operator=(Patch &&) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] PatchState State() const noexcept;
    [[nodiscard]] Status Diagnostic() const;

private:
    struct Journal;
    std::shared_ptr<Journal> m_Journal;

    friend class CKEdit;
};

// The CK2 adapter for one live graph. Patch requests made by an author
// callback, or Close requests made from another thread, are published only by
// ProcessFrame on the game thread.
class CKEdit final {
public:
    CKEdit(CKContext *context, Runtime &runtime, PrototypeCatalog *catalog,
           GraphSource &graph);
    ~CKEdit();

    Status Begin(CKBehavior *graph, PatchKey key, Edit &out);
    Status Use(Edit &edit, CKBehavior *behavior, Node &out);
    Status Use(Edit &edit, CKBehaviorLink *link, Link &out);
    Status Add(Edit &edit, Spec block, Node &out);
    Status Apply(const Edit &edit, Patch &out);
    Status Close(Patch &patch);
    void ProcessFrame();

    [[nodiscard]] std::uint64_t TopologyFingerprint(CKBehavior *graph) const;

private:
    Status Ready() const;
    [[nodiscard]] bool InDispatch() const noexcept;
    Status ApplyNow(const Edit &edit,
                    const std::shared_ptr<Patch::Journal> &journal);
    Status CloseNow(const std::shared_ptr<Patch::Journal> &journal);
    Status Undo(Patch::Journal &journal, bool notify);
    Status Materialize(std::uint64_t graphId, CKBehavior *graph);
    void CloseAdmission(Patch::Journal &journal) noexcept;

    struct Request;
    void Queue(Request request);

    struct Links;

    CKContext *m_Context = nullptr;
    Runtime &m_Runtime;
    PrototypeCatalog *m_Catalog = nullptr;
    GraphSource &m_Graph;
    std::thread::id m_Thread;
    std::map<std::uint64_t, Topology> m_Topology;
    std::map<std::uint64_t, std::set<PatchKey>> m_Active;
    std::unique_ptr<Links> m_Links;
    std::mutex m_QueueMutex;
    std::vector<Request> m_Queue;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_CKEDIT_H
