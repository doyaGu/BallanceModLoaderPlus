#ifndef BML_BEHAVIOR_PATCHES_H
#define BML_BEHAVIOR_PATCHES_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "Behavior/CKEdit.h"
#include "Behavior/GraphEdit.h"
#include "Behavior/Plan.h"
#include "Behavior/Sessions.h"

namespace BML::Behavior::Internal {

using PatchId = std::uintptr_t;
inline constexpr std::uint32_t RootGraphScope = 1;

struct PatchInfo {
    PatchState State = PatchState::Closed;
    Status LastStatus;
    Status ApplyFailure;
    Status RestoreFailure;
    std::vector<RevertConflict> Conflicts;
};

// Owns live graph Patches for Mod generations. CKEdit remains the CK2 graph
// transaction adapter; this collection supplies the Loader lifetime boundary.
class Patches final : private GraphEdit::Compiler {
public:
    using ResolveObject = std::function<CKObject *(const ObjectRef &)>;
    using IssueObject = std::function<ObjectRef(CKObject *)>;
    enum class SymbolKind {
        Node,
        Port,
    };

    struct SymbolRef {
        std::uint64_t Binding = 0;
        std::uint32_t Scope = RootGraphScope;
        std::uint32_t Handle = 0;

        friend bool operator<(const SymbolRef &left,
                              const SymbolRef &right) noexcept {
            return std::tie(left.Binding, left.Scope, left.Handle) <
                std::tie(right.Binding, right.Scope, right.Handle);
        }
        friend bool operator==(const SymbolRef &, const SymbolRef &) = default;
    };

    // Maps one handle in an author's edit program onto the symbol compiled in
    // the canonical Graph Edit. Scope keeps nested Graph symbols local.
    struct Symbol {
        SymbolKind Kind = SymbolKind::Node;
        Node NodeValue;
        Port PortValue;

        friend bool operator==(const Symbol &, const Symbol &) = default;
    };
    using SymbolMap = std::map<SymbolRef, Symbol>;

    struct PortQuery {
        std::uint64_t Binding = 0;
        std::uint32_t Scope = RootGraphScope;
        std::uint32_t Handle = 0;
        std::optional<Slot> Selector;
    };

    struct PlanInstance {
        std::uint32_t Rule = 0;
        std::uint64_t PlanRevision = 0;
        std::uint64_t Binding = 0;
        InstallationInfo Value;
    };

    struct Target {
        ObjectRef Graph;
        std::uint64_t Fingerprint = 0;
        std::uint64_t Binding = 0;
        std::shared_ptr<const GraphEdit> Body;
        SymbolMap Symbols;
    };

    struct Rule {
        ScriptSelection Scripts;
        std::uint64_t Binding = 0;
        std::shared_ptr<const GraphEdit> Body;
        SymbolMap Symbols;
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
                 const SymbolMap *authorSymbols = nullptr);
    Status Apply(const SessionOwner &owner, std::string name,
                 std::vector<Target> targets, PatchId &out);
    // Issues a reference for a Node this Patch named. Busy while the Patch is
    // still waiting for its safe point.
    Status ResolveNode(const SessionOwner &owner, PatchId patch,
                       const SymbolRef &node, ObjectRef &out) const;
    Status ReadValue(const SessionOwner &owner, PatchId patch,
                     const PortQuery &port, GraphValue &out) const;
    Status WriteValue(const SessionOwner &owner, PatchId patch,
                      const PortQuery &port,
                      const Parameter::Binding &value);
    Status Submit(Plans &plans, const SessionOwner &owner,
                  ScriptSelection target,
                  std::string name, GraphEdit edit, PlanId &out);
    Status Submit(Plans &plans, const SessionOwner &owner,
                  std::string name, std::vector<Rule> rules, PlanId &out);
    Status ReadPlan(Plans &plans, const SessionOwner &owner,
                    PlanId plan, PlanInfo &out) const;
    Status ReadPlanInstances(
        Plans &plans, const SessionOwner &owner, PlanId plan,
        std::vector<PlanInstance> &out) const;
    Status ResolvePlanNode(
        Plans &plans, const SessionOwner &owner, PlanId plan,
        const PlanInstance &instance, const SymbolRef &node,
        ObjectRef &out) const;
    Status ReadPlanValue(
        Plans &plans, const SessionOwner &owner, PlanId plan,
        const PlanInstance &instance, const PortQuery &port,
        GraphValue &out) const;
    Status WritePlanValue(
        Plans &plans, const SessionOwner &owner, PlanId plan,
        const PlanInstance &instance, const PortQuery &port,
        const Parameter::Binding &value);
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

    enum class PatchGoal {
        Enabled,
        Disabled,
        Closed,
    };

    enum class PatchRecovery {
        None,
        PreviousDefinition,
        Blocked,
    };

    enum class PlanGoal {
        Enabled,
        Disabled,
        Closed,
    };

    enum class PlanRecovery {
        None,
        PreviousRules,
        Blocked,
    };

    struct OwnedPlan {
        struct MaintainedRule {
            Rule Definition;
            PlanId Id = 0;
        };

        PlanId Id = 0;
        SessionOwner Owner;
        std::shared_ptr<CallbackAdmission> Admission;
        std::string Name;
        PlanGoal Goal = PlanGoal::Enabled;
        PlanRecovery Recovery = PlanRecovery::None;
        std::uint64_t Revision = 1;
        Status LastStatus;
        Status PrimaryFailure;
        Status RecoveryFailure;
        std::optional<std::size_t> ApplyAt;
        std::optional<std::size_t> RestoreAt;
        std::string ApplyScript;
        std::string RestoreScript;
        // Definition bindings are never reused while this Plan handle lives;
        // retired tokens keep stale symbols from aliasing later definitions.
        std::set<std::uint64_t> DefinitionBindings;
        std::vector<Rule> RequestedRules;
        std::vector<Rule> PreviousRules;
        std::optional<std::size_t> RestoreFrom;
        std::vector<MaintainedRule> Rules;
    };

    struct OwnedPatch {
        struct ResolvedSymbol {
            ResolvedSymbol() = default;
            ResolvedSymbol(std::size_t scopeIndex, Node node)
                : ScopeIndex(scopeIndex), Kind(SymbolKind::Node),
                  NodeValue(node) {}
            ResolvedSymbol(std::size_t scopeIndex, Port port)
                : ScopeIndex(scopeIndex), Kind(SymbolKind::Port),
                  PortValue(std::move(port)) {}

            std::size_t ScopeIndex = 0;
            SymbolKind Kind = SymbolKind::Node;
            Node NodeValue;
            Port PortValue;
        };

        struct Scope {
            std::uint32_t Id = 0;
            std::size_t Target = 0;
            CK_ID Graph = 0;
            Patch Value;
            GraphEdit::CompiledSymbols Symbols;
        };

        PatchId Id = 0;
        SessionOwner Owner;
        std::shared_ptr<CallbackAdmission> Admission;
        std::string Name;
        CK_ID Graph = 0;
        PatchGoal Goal = PatchGoal::Enabled;
        bool TargetDeleted = false;
        PatchRecovery Recovery = PatchRecovery::None;
        std::uint64_t Revision = 1;
        Status LastStatus;
        Status PrimaryFailure;
        Status RecoveryFailure;
        std::optional<std::size_t> ApplyAt;
        std::optional<std::size_t> RestoreAt;
        // See OwnedPlan::DefinitionBindings.
        std::set<std::uint64_t> DefinitionBindings;
        // RequestedDefinition is the last requested content.
        // AppliedDefinition describes the installed prefix while a
        // replacement is being reconciled;
        // PreviousDefinition is the last complete definition to restore when
        // new content cannot be installed.
        std::vector<Target> RequestedDefinition;
        std::vector<Target> AppliedDefinition;
        std::vector<Target> PreviousDefinition;
        std::optional<std::size_t> RestoreFrom;
        std::vector<Scope> Scopes;
        // Public handle -> resolved symbol in one installed scope.
        std::map<SymbolRef, ResolvedSymbol> Symbols;
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
    Status Apply(const SessionOwner &owner, std::string name,
                 std::vector<Target> targets, PatchId &out,
                 std::shared_ptr<const CallbackAdmission> parentAdmission);
    Status Install(OwnedPatch &patch);
    Status InstallFrom(OwnedPatch &patch,
                       const std::vector<Target> &definition,
                       std::size_t target);
    Status InstallScope(const SessionOwner &owner, const PatchKey &patch,
                        const ObjectRef &graph, const GraphEdit &edit,
                        std::uint32_t scope, std::size_t target,
                        OwnedPatch &out,
                        const SymbolMap *authorSymbols);
    Status PublishScope(const SessionOwner &owner, const PatchKey &patch,
                        const ObjectRef &graph, const GraphEdit &edit,
                        Edit resolved,
                        GraphEdit::CompiledSymbols compiled,
                        std::uint32_t scope, std::size_t target,
                        OwnedPatch &out,
                        const SymbolMap *authorSymbols);
    Status Reconcile(OwnedPatch &patch);
    Status Activate(Plans &plans, OwnedPlan &plan);
    Status Deactivate(Plans &plans, OwnedPlan &plan);
    Status ActivateFrom(Plans &plans, OwnedPlan &plan,
                        const std::vector<Rule> &definition,
                        std::size_t rule);
    Status DeactivateFrom(Plans &plans, OwnedPlan &plan,
                          std::size_t rule);
    Status ReconcilePlan(Plans &plans, OwnedPlan &plan);
    [[nodiscard]] static std::size_t CommonRulePrefix(
        const OwnedPlan &plan, const std::vector<Rule> &rules);
    [[nodiscard]] static std::vector<Rule> CurrentRules(
        const OwnedPlan &plan);
    [[nodiscard]] PlanState State(Plans &plans,
                                  const OwnedPlan &plan) const;
    [[nodiscard]] PatchState State(const OwnedPatch &patch) const;
    [[nodiscard]] Status LastStatus(const OwnedPatch &patch) const;
    [[nodiscard]] Status ApplyFailure(const OwnedPatch &patch) const;
    [[nodiscard]] Status RestoreFailure(const OwnedPatch &patch) const;
    [[nodiscard]] static bool HasPendingChange(const OwnedPlan &plan);
    [[nodiscard]] static bool HasPendingChange(const OwnedPatch &patch);
    void RebuildSymbols(OwnedPatch &patch);
    Status ResolvePort(const OwnedPatch &patch, const PortQuery &query,
                       Port &out) const;
    Status ValidatePlanInstance(
        Plans &plans, const OwnedPlan &plan,
        const PlanInstance &instance, PatchId &out) const;
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
    Status Interpose(Edit &edit, Port source, Port sink,
                     const HookBlock::Hook &hook) override;

    CKEdit m_Edit;
    GraphSource &m_Graph;
    ResolveObject m_ResolveObject;
    IssueObject m_IssueObject;
    std::thread::id m_Thread;
    struct AdmissionRecord {
        SessionOwner Owner;
        std::weak_ptr<CallbackAdmission> Admission;
    };
    // Only this admission registry crosses threads. A worker Close stops new
    // callbacks here; Patch and Plan graph state remains game-thread owned and
    // is reconciled at the next safe point.
    std::mutex m_AdmissionMutex;
    std::map<std::pair<bool, std::uint64_t>, AdmissionRecord> m_Admissions;
    PatchId m_NextId = 1;
    PlanId m_NextPlanId = 1;
    std::map<PatchId, OwnedPatch> m_Patches;
    std::map<PlanId, OwnedPlan> m_Plans;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_PATCHES_H
