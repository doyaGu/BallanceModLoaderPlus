#ifndef BML_BEHAVIOR_PATCHES_H
#define BML_BEHAVIOR_PATCHES_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Behavior/CKEdit.h"
#include "Behavior/GraphEdit.h"
#include "Behavior/Plan.h"
#include "Behavior/Sessions.h"

namespace BML::Behavior {

using PatchId = std::uintptr_t;

struct PatchInfo {
    PatchState State = PatchState::Closed;
    Status Diagnostic;
    std::vector<RevertConflict> Conflicts;
};

// Owns live graph Patches for Mod generations. CKEdit remains the CK2 graph
// transaction adapter; this collection supplies the Loader lifetime boundary.
class Patches final : private GraphEdit::Compiler {
public:
    using ResolveObject = std::function<CKObject *(const ObjectRef &)>;

    Patches(CKContext *context, Runtime &runtime, PrototypeCatalog *catalog,
            GraphSource &graph, ResolveObject resolveObject);
    ~Patches();

    Patches(const Patches &) = delete;
    Patches &operator=(const Patches &) = delete;

    Status Begin(const SessionOwner &owner, CKBehavior *graph,
                 std::string name, Edit &out);
    Status Use(Edit &edit, CKBehavior *behavior, Node &out);
    Status Use(Edit &edit, CKBehaviorLink *link, Link &out);
    Status Add(Edit &edit, Spec block, Node &out);
    Status Apply(const SessionOwner &owner, const Edit &edit, PatchId &out);
    Status Apply(const SessionOwner &owner, const ObjectRef &graph,
                 std::string name, GraphEdit edit, PatchId &out);
    Status Submit(Plans &plans, const SessionOwner &owner, Script target,
                  std::string name, GraphEdit edit, PlanId &out);
    Status Read(const SessionOwner &owner, PatchId patch,
                PatchInfo &out) const;
    Status Close(const SessionOwner &owner, PatchId patch);

    Status RetireOwner(const std::string &ownerId);
    void ObjectsToBeDeleted(const CK_ID *ids, int count);
    void ResetWorld();
    void ProcessFrame();

private:
    class PlanWorld;

    struct OwnedPatch {
        PatchId Id = 0;
        SessionOwner Owner;
        CK_ID Graph = 0;
        Patch Value;
    };

    [[nodiscard]] Status Ready() const;
    [[nodiscard]] PatchId NextId();
    Status Close(OwnedPatch &patch);
    Status Install(const SessionOwner &owner, const PatchKey &patch,
                   const ObjectRef &graph, const GraphEdit &edit,
                   PatchId &out);
    void Collect();

    Status Begin(const PatchKey &patch, const ObjectRef &graph,
                 Edit &out, GraphModel &base) override;
    Status UseNode(Edit &edit, const ObjectRef &node, Node &out) override;
    Status UseLink(Edit &edit, const ObjectRef &link, Link &out) override;
    Status Add(Edit &edit, CKGUID prototype, Node &out) override;
    Status Tap(Edit &edit, Port source,
               const HookBlock::Hook &hook) override;
    Status After(Edit &edit, Link link,
                 const HookBlock::Hook &hook) override;

    CKEdit m_Edit;
    GraphSource &m_Graph;
    ResolveObject m_ResolveObject;
    std::thread::id m_Thread;
    mutable std::recursive_mutex m_Mutex;
    PatchId m_NextId = 1;
    std::map<PatchId, OwnedPatch> m_Patches;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_PATCHES_H
