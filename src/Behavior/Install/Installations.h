#ifndef BML_BEHAVIOR_INSTALL_INSTALLATIONS_H
#define BML_BEHAVIOR_INSTALL_INSTALLATIONS_H

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
#include "Behavior/Install/Selection.h"
#include "Behavior/Sessions.h"

namespace BML::Behavior::Internal {

using PatchId = std::uintptr_t;
using PlanId = std::uint64_t;
inline constexpr std::uint32_t RootGraphScope = 1;

struct PatchInfo {
    PatchState State = PatchState::Closed;
    Status LastStatus;
    Status ApplyFailure;
    Status RestoreFailure;
    std::vector<RevertConflict> Conflicts;
};

// Owns every live graph installation a Mod generation asked for. A Patch is a
// definition whose targets name Graphs in the current world; a Plan is one
// whose rules select Scripts and is installed again into every later world
// through Selections. Both share one lifecycle: a requested definition, the
// prefix actually applied, the previous definition to fall back to, and the
// failures met on the way. CKEdit remains the CK2 graph transaction adapter.
class Installations final : private GraphEdit::Compiler {
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

    Installations(CKContext *context, Runtime &runtime,
                  PrototypeCatalog *catalog, GraphSource &graph,
                  ResolveObject resolveObject, IssueObject issueObject);
    ~Installations();

    Installations(const Installations &) = delete;
    Installations &operator=(const Installations &) = delete;

    // Legacy Edits name native objects directly and install as one Patch.
    Status Begin(const SessionOwner &owner, CKBehavior *graph,
                 std::string name, Edit &out);
    Status Use(Edit &edit, CKBehavior *behavior, Node &out);
    Status Use(Edit &edit, CKBehaviorLink *link, Link &out);
    Status Add(Edit &edit, BlockSpec block, Node &out) override;
    Status AddGraph(Edit &edit, std::string name, int priority,
                    Node &out) override;
    Status Apply(const SessionOwner &owner, const Edit &edit, PatchId &out,
                 const std::map<std::uint32_t, Node> *handles = nullptr);

    // Patches.
    Status Apply(const SessionOwner &owner, const ObjectRef &graph,
                 std::string name, GraphEdit edit, PatchId &out,
                 const SymbolMap *authorSymbols = nullptr);
    Status Apply(const SessionOwner &owner, std::string name,
                 std::vector<Target> targets, PatchId &out);
    Status Read(const SessionOwner &owner, PatchId patch,
                PatchInfo &out) const;
    // Issues a reference for a Node this Patch named. Busy while the Patch is
    // still waiting for its safe point.
    Status ResolveNode(const SessionOwner &owner, PatchId patch,
                       const SymbolRef &node, ObjectRef &out) const;
    Status ReadValue(const SessionOwner &owner, PatchId patch,
                     const PortQuery &port, GraphValue &out) const;
    Status WriteValue(const SessionOwner &owner, PatchId patch,
                      const PortQuery &port,
                      const Parameter::Binding &value);
    Status SetActive(const SessionOwner &owner, PatchId patch, bool active);
    Status Replace(const SessionOwner &owner, PatchId patch,
                   std::vector<Target> targets);
    Status Close(const SessionOwner &owner, PatchId patch);

    // Plans.
    Status Submit(const SessionOwner &owner, std::string name,
                  std::vector<Rule> rules, PlanId &out);
    Status ReadPlan(const SessionOwner &owner, PlanId plan,
                    PlanInfo &out) const;
    Status ReadPlanInstances(const SessionOwner &owner, PlanId plan,
                             std::vector<PlanInstance> &out) const;
    Status ResolvePlanNode(const SessionOwner &owner, PlanId plan,
                           const PlanInstance &instance,
                           const SymbolRef &node, ObjectRef &out) const;
    Status ReadPlanValue(const SessionOwner &owner, PlanId plan,
                         const PlanInstance &instance, const PortQuery &port,
                         GraphValue &out) const;
    Status WritePlanValue(const SessionOwner &owner, PlanId plan,
                          const PlanInstance &instance, const PortQuery &port,
                          const Parameter::Binding &value);
    Status SetPlanActive(const SessionOwner &owner, PlanId plan, bool active);
    Status ReplacePlan(const SessionOwner &owner, PlanId plan,
                       std::vector<Rule> rules);
    Status ClosePlan(const SessionOwner &owner, PlanId plan);

    // One bare Script Selection with a single canonical body, for the Behavior
    // test interface.
    Status SubmitSelection(const SessionOwner &owner, ScriptSelection target,
                           std::string name, GraphEdit edit,
                           SelectionId &out);
    Status ReadSelection(const SessionOwner &owner, SelectionId selection,
                         PlanInfo &out) const;
    Status CloseSelection(const SessionOwner &owner, SelectionId selection);

    // Retires every Plan and Patch the owner holds.
    Status RetireOwner(const std::string &ownerId);

    // Script load and unload only change the known Plan targets; Plans are
    // reconciled against them at ProcessFrame.
    Status LoadScript(std::string name, ObjectRef script);
    void RemoveObject(std::uint32_t domain, std::uint32_t slot);
    void ObjectsToBeDeleted(const CK_ID *ids, int count);
    // A world reset leaves the Plan world first, before Scripts reset, and
    // closes every Patch afterwards.
    Status LeaveWorld();
    void ResetWorld();
    // Reports only a failure to reconcile Plan Selections. Patch failures stay
    // on the Patch.
    Status ProcessFrame();

private:
    class PlanWorld;

    enum class InstallGoal {
        Enabled,
        Disabled,
        Closed,
    };

    enum class InstallRecovery {
        None,
        Previous,
        Blocked,
    };

    template <class Definition>
    struct Record {
        std::uint64_t Id = 0;
        SessionOwner Owner;
        std::shared_ptr<CallbackAdmission> Admission;
        std::string Name;
        InstallGoal Goal = InstallGoal::Enabled;
        InstallRecovery Recovery = InstallRecovery::None;
        std::uint64_t Revision = 1;
        Status LastStatus;
        Status PrimaryFailure;
        Status RecoveryFailure;
        std::optional<std::size_t> ApplyAt;
        std::optional<std::size_t> RestoreAt;
        // Only a Plan names the Script its failing rule selects.
        std::string ApplyScript;
        std::string RestoreScript;
        // Definition bindings are never reused while this handle lives;
        // retired tokens keep stale symbols from aliasing later definitions.
        std::set<std::uint64_t> DefinitionBindings;
        // Requested is the last requested content. Applied describes the
        // installed prefix while a replacement is being reconciled. Previous
        // is the last complete definition to restore when new content cannot
        // be installed.
        std::vector<Definition> Requested;
        std::vector<Definition> Applied;
        std::vector<Definition> Previous;
        std::optional<std::size_t> RestoreFrom;
    };

    struct PatchRecord : Record<Target> {
        static constexpr bool IsPlan = false;

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

        CK_ID Graph = 0;
        bool TargetDeleted = false;
        std::vector<Scope> Scopes;
        // Public handle -> resolved symbol in one installed scope.
        std::map<SymbolRef, ResolvedSymbol> Symbols;
    };

    struct PlanRecord : Record<Rule> {
        static constexpr bool IsPlan = true;

        // The Selection that keeps each applied rule installed, parallel to
        // Applied.
        std::vector<SelectionId> Selections;
    };

    [[nodiscard]] Status Ready() const;
    [[nodiscard]] PatchId NextId();
    [[nodiscard]] PlanId NextPlanId();
    std::shared_ptr<CallbackAdmission> RegisterAdmission(
        bool plan, std::uint64_t id, const SessionOwner &owner,
        std::shared_ptr<const CallbackAdmission> parent = {});
    Status RequestClose(bool plan, std::uint64_t id, const SessionOwner &owner);
    static Status Validate(const std::vector<Target> &targets, bool replacing);
    static Status Validate(const std::vector<Rule> &rules, bool replacing);

    Status Apply(const SessionOwner &owner, std::string name,
                 std::vector<Target> targets, PatchId &out,
                 std::shared_ptr<const CallbackAdmission> parentAdmission);
    Status Install(PatchRecord &patch);
    Status InstallFrom(PatchRecord &patch,
                       const std::vector<Target> &definition,
                       std::size_t target);
    Status InstallFrom(PlanRecord &plan, const std::vector<Rule> &definition,
                       std::size_t rule);
    Status InstallScope(const SessionOwner &owner, const PatchKey &patch,
                        const ObjectRef &graph, const GraphEdit &edit,
                        std::uint32_t scope, std::size_t target,
                        PatchRecord &out,
                        const SymbolMap *authorSymbols);
    Status PublishScope(const SessionOwner &owner, const PatchKey &patch,
                        const ObjectRef &graph, const GraphEdit &edit,
                        Edit resolved,
                        GraphEdit::CompiledSymbols compiled,
                        std::uint32_t scope, std::size_t target,
                        PatchRecord &out,
                        const SymbolMap *authorSymbols);
    Status Uninstall(PatchRecord &patch, std::size_t target);
    Status Uninstall(PlanRecord &plan, std::size_t rule);
    void CloseAdmission(PatchRecord &patch);
    Status Close(PatchRecord &patch);
    void Retire(PatchRecord &patch) { CloseAdmission(patch); }
    static void Retire(PlanRecord &plan) { plan.Goal = InstallGoal::Closed; }
    void Settle(PatchRecord &patch);
    static void Settle(PlanRecord &) {}
    static Status Refresh(PatchRecord &, std::size_t) { return {}; }
    Status Refresh(PlanRecord &plan, std::size_t prefix);

    template <class R>
    Status Reconcile(R &record);
    Status Fallback(PatchRecord &patch, std::size_t prefix,
                    const Status &requestedFailure);
    Status Fallback(PlanRecord &plan, std::size_t prefix,
                    const Status &requestedFailure);
    template <class R>
    Status SetGoal(R &record, bool active);
    template <class R, class Definition>
    Status Redefine(R &record, std::vector<Definition> definitions);
    Status RetireRecords(const std::string &ownerId);
    void Collect();

    [[nodiscard]] PatchState State(const PatchRecord &patch) const;
    [[nodiscard]] PlanState State(const PlanRecord &plan) const;
    [[nodiscard]] Status LastStatus(const PatchRecord &patch) const;
    [[nodiscard]] Status RestoreFailure(const PatchRecord &patch) const;
    [[nodiscard]] static bool Installed(const PatchRecord &patch) noexcept {
        return !patch.Scopes.empty();
    }
    [[nodiscard]] static bool Installed(const PlanRecord &plan) noexcept {
        return !plan.Selections.empty();
    }
    template <class R>
    [[nodiscard]] static bool HasPendingChange(const R &record);
    Status ResolvePort(const PatchRecord &patch, const PortQuery &query,
                       Port &out) const;
    Status FindPlanInstance(const PlanRecord &plan,
                            const PlanInstance &instance,
                            PatchId &out) const;

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
    Selections m_Selections;
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
    // Registration and collection are game-thread operations. Worker threads
    // only look up an admission while holding m_AdmissionMutex.
    bool m_HasAdmissions{false};
    PatchId m_NextId = 1;
    PlanId m_NextPlanId = 1;
    std::map<PatchId, PatchRecord> m_Patches;
    std::map<PlanId, PlanRecord> m_Plans;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_INSTALL_INSTALLATIONS_H
