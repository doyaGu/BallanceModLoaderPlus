#ifndef BML_BEHAVIOR_PATCHES_H
#define BML_BEHAVIOR_PATCHES_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Behavior/CKEdit.h"
#include "Behavior/GraphEdit.h"
#include "Behavior/Plan.h"
#include "Behavior/Sessions.h"

namespace BML::Behavior::Internal {

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
    using IssueObject = std::function<ObjectRef(CKObject *)>;
    // Maps the handle an author's own edit program used for a Node onto the
    // handle the symbolic intent gave it, so a Patch can be read back through
    // the names its author chose.
    using HandleMap = std::map<std::uint32_t, std::uint32_t>;

    Patches(CKContext *context, Runtime &runtime, PrototypeCatalog *catalog,
            GraphSource &graph, ResolveObject resolveObject,
            IssueObject issueObject);
    ~Patches();

    Patches(const Patches &) = delete;
    Patches &operator=(const Patches &) = delete;

    Status Begin(const SessionOwner &owner, CKBehavior *graph,
                 std::string name, Edit &out);
    Status Use(Edit &edit, CKBehavior *behavior, Node &out);
    Status Use(Edit &edit, CKBehaviorLink *link, Link &out);
    Status Add(Edit &edit, BlockSpec block, Node &out) override;
    Status Apply(const SessionOwner &owner, const Edit &edit, PatchId &out,
                 const std::map<std::uint32_t, Node> *handles = nullptr);
    Status Apply(const SessionOwner &owner, const ObjectRef &graph,
                 std::string name, GraphEdit edit, PatchId &out,
                 const HandleMap *authorNodes = nullptr);
    // Issues a reference for a Node this Patch named. Busy while the Patch is
    // still waiting for its safe point.
    Status ResolveNode(const SessionOwner &owner, PatchId patch,
                       std::uint32_t handle, ObjectRef &out) const;
    Status Submit(Plans &plans, const SessionOwner &owner,
                  ScriptSelection target,
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
        bool Retiring = false;
        // The author's handle for each Node, in the live Edit's own handles.
        std::map<std::uint32_t, Node> Handles;
    };

    [[nodiscard]] Status Ready() const;
    [[nodiscard]] PatchId NextId();
    Status Close(OwnedPatch &patch);
    Status Install(const SessionOwner &owner, const PatchKey &patch,
                   const ObjectRef &graph, const GraphEdit &edit,
                   PatchId &out, const HandleMap *authorNodes = nullptr);
    void Collect();

    Status Begin(const PatchKey &patch, const ObjectRef &graph,
                 Edit &out, GraphModel &base) override;
    Status UseNode(Edit &edit, const ObjectRef &node, Node &out) override;
    Status UseLink(Edit &edit, const ObjectRef &link, Link &out) override;
    Status Tap(Edit &edit, Port source,
               const HookBlock::Hook &hook) override;
    Status Interpose(Edit &edit, Link link,
                     const HookBlock::Hook &hook) override;

    CKEdit m_Edit;
    GraphSource &m_Graph;
    ResolveObject m_ResolveObject;
    IssueObject m_IssueObject;
    std::thread::id m_Thread;
    mutable std::recursive_mutex m_Mutex;
    PatchId m_NextId = 1;
    std::map<PatchId, OwnedPatch> m_Patches;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_PATCHES_H
