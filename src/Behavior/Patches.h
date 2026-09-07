#ifndef BML_BEHAVIOR_PATCHES_H
#define BML_BEHAVIOR_PATCHES_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
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
    struct Symbol {
        std::uint32_t Scope = 1;
        std::uint32_t Node = 0;
    };
    using HandleMap = std::map<std::uint32_t, Symbol>;

    struct Target {
        ObjectRef Graph;
        std::uint64_t Fingerprint = 0;
        std::shared_ptr<const GraphEdit> Body;
        HandleMap Handles;
    };

    struct Rule {
        ScriptSelection Scripts;
        std::shared_ptr<const GraphEdit> Body;
    };

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
    Status AddGraph(Edit &edit, std::string name, int priority,
                    Node &out) override;
    Status Apply(const SessionOwner &owner, const Edit &edit, PatchId &out,
                 const std::map<std::uint32_t, Node> *handles = nullptr);
    Status Apply(const SessionOwner &owner, const ObjectRef &graph,
                 std::string name, GraphEdit edit, PatchId &out,
                 const HandleMap *authorNodes = nullptr);
    Status Apply(const SessionOwner &owner, std::string name,
                 std::vector<Target> targets, PatchId &out);
    // Issues a reference for a Node this Patch named. Busy while the Patch is
    // still waiting for its safe point.
    Status ResolveNode(const SessionOwner &owner, PatchId patch,
                       std::uint32_t handle, ObjectRef &out) const;
    Status Submit(Plans &plans, const SessionOwner &owner,
                  ScriptSelection target,
                  std::string name, GraphEdit edit, PlanId &out);
    Status Submit(Plans &plans, const SessionOwner &owner,
                  std::string name, std::vector<Rule> rules, PlanId &out);
    Status ReadPlan(Plans &plans, const SessionOwner &owner,
                    PlanId plan, PlanInfo &out) const;
    Status SetPlanActive(Plans &plans, const SessionOwner &owner,
                         PlanId plan, bool active);
    Status ReplacePlan(Plans &plans, const SessionOwner &owner,
                       PlanId plan, std::vector<Rule> rules);
    Status ClosePlan(Plans &plans, const SessionOwner &owner, PlanId plan);
    Status Read(const SessionOwner &owner, PatchId patch,
                PatchInfo &out) const;
    Status SetActive(const SessionOwner &owner, PatchId patch, bool active);
    Status Replace(const SessionOwner &owner, PatchId patch,
                   std::vector<Target> targets);
    Status Close(const SessionOwner &owner, PatchId patch);

    Status RetireOwner(const std::string &ownerId);
    void ObjectsToBeDeleted(const CK_ID *ids, int count);
    void ResetWorld();
    void ProcessFrame(Plans &plans);

private:
    class PlanWorld;

    struct OwnedPlan {
        PlanId Id = 0;
        SessionOwner Owner;
        std::shared_ptr<CallbackAdmission> Admission;
        std::string Name;
        bool DesiredActive = true;
        bool Retiring = false;
        bool Failed = false;
        bool ReturningPrevious = false;
        std::uint64_t Revision = 1;
        Status LastStatus;
        Status ChangeFault;
        std::vector<Rule> Definition;
        std::vector<Rule> LiveDefinition;
        std::vector<Rule> PreviousDefinition;
        std::optional<std::size_t> ChangeFrom;
        std::vector<PlanId> Rules;
    };

    struct OwnedPatch {
        struct Scope {
            std::uint32_t Id = 0;
            std::size_t Target = 0;
            CK_ID Graph = 0;
            Patch Value;
            std::map<std::uint32_t, Node> Handles;
        };

        PatchId Id = 0;
        SessionOwner Owner;
        std::shared_ptr<CallbackAdmission> Admission;
        std::string Name;
        CK_ID Graph = 0;
        bool DesiredActive = true;
        bool Retiring = false;
        bool TargetDeleted = false;
        bool Failed = false;
        bool ReturningPrevious = false;
        std::uint64_t Revision = 1;
        Status LastStatus;
        Status ChangeFault;
        // Definition is the last requested content. LiveDefinition describes
        // the installed prefix while a replacement is being reconciled;
        // PreviousDefinition is the last complete definition to restore when
        // new content cannot be installed.
        std::vector<Target> Definition;
        std::vector<Target> LiveDefinition;
        std::vector<Target> PreviousDefinition;
        std::optional<std::size_t> ChangeFrom;
        std::vector<Scope> Scopes;
        // Public handle -> (scope index, resolved Edit Node).
        std::map<std::uint32_t, std::pair<std::size_t, Node>> Handles;
    };

    [[nodiscard]] Status Ready() const;
    [[nodiscard]] PatchId NextId();
    [[nodiscard]] PlanId NextPlanId();
    std::shared_ptr<CallbackAdmission> RegisterAdmission(
        bool plan, std::uint64_t id, const SessionOwner &owner,
        std::shared_ptr<const CallbackAdmission> parent = {});
    Status RequestClose(bool plan, std::uint64_t id, const SessionOwner &owner);
    Status Restore(OwnedPatch &patch);
    Status RestoreFrom(OwnedPatch &patch, std::size_t target);
    Status Close(OwnedPatch &patch);
    void CloseAdmission(OwnedPatch &patch);
    Status Install(const SessionOwner &owner, const PatchKey &patch,
                   const ObjectRef &graph, const GraphEdit &edit,
                   PatchId &out, const HandleMap *authorNodes = nullptr,
                   std::shared_ptr<const CallbackAdmission> admission = {});
    Status Install(OwnedPatch &patch);
    Status InstallFrom(OwnedPatch &patch,
                       const std::vector<Target> &definition,
                       std::size_t target);
    Status InstallScope(const SessionOwner &owner, const PatchKey &patch,
                        const ObjectRef &graph, const GraphEdit &edit,
                        std::uint32_t scope, std::size_t target,
                        OwnedPatch &out,
                        const HandleMap *authorNodes);
    Status PublishScope(const SessionOwner &owner, const PatchKey &patch,
                        const ObjectRef &graph, const GraphEdit &edit,
                        Edit resolved,
                        std::map<std::uint32_t, Node> compiled,
                        std::uint32_t scope, std::size_t target,
                        OwnedPatch &out,
                        const HandleMap *authorNodes);
    Status Reconcile(OwnedPatch &patch);
    Status Activate(Plans &plans, OwnedPlan &plan);
    Status Deactivate(Plans &plans, OwnedPlan &plan);
    Status ActivateFrom(Plans &plans, OwnedPlan &plan,
                        const std::vector<Rule> &definition,
                        std::size_t rule);
    Status DeactivateFrom(Plans &plans, OwnedPlan &plan,
                          std::size_t rule);
    Status ReconcilePlan(Plans &plans, OwnedPlan &plan);
    [[nodiscard]] PlanState State(Plans &plans,
                                  const OwnedPlan &plan) const;
    [[nodiscard]] PatchState State(const OwnedPatch &patch) const;
    [[nodiscard]] Status Diagnostic(const OwnedPatch &patch) const;
    void RebuildHandles(OwnedPatch &patch);
    void Collect();

    Status Begin(const PatchKey &patch, const ObjectRef &graph,
                 Edit &out, GraphModel &base) override;
    Status UseNode(Edit &edit, const ObjectRef &node, Node &out) override;
    Status UseLink(Edit &edit, const ObjectRef &link, Link &out) override;
    Status ReadPatternValue(const GraphNode &node, const Slot &slot,
                            GraphValue &out) override;
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
    struct AdmissionRecord {
        SessionOwner Owner;
        std::weak_ptr<CallbackAdmission> Admission;
    };
    // No native or author callbacks run under this mutex. Close requests only
    // touch these records, then try the graph registry without waiting.
    std::mutex m_AdmissionMutex;
    std::map<std::pair<bool, std::uint64_t>, AdmissionRecord> m_Admissions;
    PatchId m_NextId = 1;
    PlanId m_NextPlanId = 1;
    std::map<PatchId, OwnedPatch> m_Patches;
    std::map<PlanId, OwnedPlan> m_Plans;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_PATCHES_H
