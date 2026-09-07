#include "Behavior/Patches.h"

#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
#include <utility>

namespace BML::Behavior::Internal {
namespace {

Status Failure(Error error, std::string message,
               Phase phase = Phase::Edit) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = phase;
    return status;
}

bool SameTarget(const Patches::Target &left,
                const Patches::Target &right) noexcept {
    return left.Graph == right.Graph &&
        left.Fingerprint == right.Fingerprint &&
        left.Body && right.Body && left.Body->SameAs(*right.Body);
}

std::size_t CommonPrefix(const std::vector<Patches::Target> &left,
                         const std::vector<Patches::Target> &right) noexcept {
    const std::size_t count = (std::min)(left.size(), right.size());
    std::size_t prefix = 0;
    while (prefix < count && SameTarget(left[prefix], right[prefix]))
        ++prefix;
    return prefix;
}

bool SameRule(const Patches::Rule &left,
              const Patches::Rule &right) noexcept {
    return left.Scripts == right.Scripts &&
        left.Body && right.Body && left.Body->SameAs(*right.Body);
}

std::size_t CommonPrefix(const std::vector<Patches::Rule> &left,
                         const std::vector<Patches::Rule> &right) noexcept {
    const std::size_t count = (std::min)(left.size(), right.size());
    std::size_t prefix = 0;
    while (prefix < count && SameRule(left[prefix], right[prefix]))
        ++prefix;
    return prefix;
}

} // namespace

Patches::Patches(CKContext *context, Runtime &runtime,
                 PrototypeCatalog *catalog, GraphSource &graph,
                 ResolveObject resolveObject, IssueObject issueObject)
    : m_Edit(context, runtime, catalog, graph),
      m_Graph(graph), m_ResolveObject(std::move(resolveObject)),
      m_IssueObject(std::move(issueObject)),
      m_Thread(std::this_thread::get_id()) {}

Patches::~Patches() {
    ResetWorld();
}

Status Patches::Ready() const {
    return std::this_thread::get_id() == m_Thread
        ? Status{}
        : Failure(Error::WrongThread,
                  "Behavior Patches require the game thread.");
}

PatchId Patches::NextId() {
    if (m_NextId == 0 ||
        m_NextId == (std::numeric_limits<PatchId>::max)())
        return 0;
    return m_NextId++;
}

PlanId Patches::NextPlanId() {
    if (m_NextPlanId == 0 ||
        m_NextPlanId == (std::numeric_limits<PlanId>::max)())
        return 0;
    return m_NextPlanId++;
}

Status Patches::Begin(const SessionOwner &owner, CKBehavior *graph,
                      std::string name, Edit &out) {
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || name.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Patch requires a live owner and name.");
    return m_Edit.Begin(graph, {owner.Id, std::move(name)}, out);
}

Status Patches::Use(Edit &edit, CKBehavior *behavior, Node &out) {
    return m_Edit.Use(edit, behavior, out);
}

Status Patches::Use(Edit &edit, CKBehaviorLink *link, Link &out) {
    return m_Edit.Use(edit, link, out);
}

Status Patches::Add(Edit &edit, BlockSpec block, Node &out) {
    return m_Edit.Add(edit, std::move(block), out);
}

Status Patches::AddGraph(Edit &edit, std::string name, int priority,
                         Node &out) {
    return m_Edit.AddGraph(edit, std::move(name), priority, out);
}

Status Patches::Apply(const SessionOwner &owner, const Edit &edit,
                      PatchId &out,
                      const std::map<std::uint32_t, Node> *handles) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || edit.Key().Owner != owner.Id || edit.Key().Name.empty())
        return Failure(Error::OwnerInvalid,
                       "The Behavior Edit does not belong to this Mod generation.");

    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const PatchId id = NextId();
    if (!id)
        return Failure(Error::InvalidState,
                       "Behavior Patch ids are exhausted.");

    const NativeRef graph = edit.GraphRef();
    if (!graph || graph.Id >
            static_cast<std::uint64_t>((std::numeric_limits<CK_ID>::max)())) {
        return Failure(Error::InvalidGraphLocality,
                       "The Behavior Edit graph has no live CK identity.");
    }

    Patch patch;
    status = m_Edit.Apply(edit, patch);
    if (!patch)
        return status;

    try {
        auto [stored, inserted] = m_Patches.try_emplace(id);
        if (!inserted) {
            (void) m_Edit.Close(patch);
            return Failure(Error::InvalidState,
                           "Behavior Patch id collision.");
        }
        try {
            stored->second.Id = id;
            stored->second.Owner = owner;
            stored->second.Graph = static_cast<CK_ID>(graph.Id);
            OwnedPatch::Scope scope;
            scope.Id = 1;
            scope.Graph = static_cast<CK_ID>(graph.Id);
            scope.Value = std::move(patch);
            if (handles)
                scope.Handles = *handles;
            stored->second.Scopes.push_back(std::move(scope));
            stored->second.Retiring = !status;
            if (handles) {
                for (const auto &[handle, node] : *handles)
                    stored->second.Handles.emplace(
                        handle, std::make_pair(std::size_t{0}, node));
            }
        } catch (...) {
            m_Patches.erase(stored);
            throw;
        }
    } catch (...) {
        (void) m_Edit.Close(patch);
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch.");
    }
    // A failed Apply normally rolls back completely and has no Patch value.
    // RevertConflict is different: retain its conflict journal under the owner,
    // but do not hand a successful installation id to the caller.
    if (status)
        out = id;
    return status;
}

Status Patches::Apply(const SessionOwner &owner, const ObjectRef &graph,
                      std::string name, GraphEdit edit, PatchId &out,
                      const HandleMap *authorNodes) {
    try {
        Target target;
        target.Graph = graph;
        target.Body = std::make_shared<GraphEdit>(std::move(edit));
        if (authorNodes)
            target.Handles = *authorNodes;
        std::vector<Target> targets;
        targets.push_back(std::move(target));
        return Apply(owner, std::move(name), std::move(targets), out);
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch definition.");
    }
}

Status Patches::Apply(const SessionOwner &owner, std::string name,
                      std::vector<Target> targets, PatchId &out) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || name.empty() || targets.empty())
        return Failure(Error::OwnerInvalid,
                       "A Behavior Patch requires an owner, name, and target Graph.");

    std::set<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> graphs;
    for (const Target &target : targets) {
        if (target.Graph.IsNull() || !target.Body)
            return Failure(Error::TargetInvalid,
                           "A Behavior Patch target Graph is missing.");
        status = target.Body->Validate();
        if (!status)
            return status;
        const auto identity = std::make_tuple(
            target.Graph.Domain, target.Graph.Slot, target.Graph.Generation);
        if (!graphs.insert(identity).second)
            return Failure(Error::InvalidGraphLocality,
                           "A Graph may appear only once in one Behavior Patch.");
    }

    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const PatchId id = NextId();
    if (!id)
        return Failure(Error::InvalidState,
                       "Behavior Patch ids are exhausted.");

    OwnedPatch patch;
    patch.Id = id;
    patch.Owner = owner;
    patch.Name = std::move(name);
    patch.Definition = std::move(targets);
    if (m_Edit.CanPublish()) {
        status = Install(patch);
        patch.LastStatus = status;
        patch.Failed = !status && patch.Scopes.empty();
        if (!status && !patch.Scopes.empty()) {
            patch.ChangeFrom = 0;
            patch.ChangeFault = status;
        }
    }

    // A validation or admission failure that changed no Graph has nothing to
    // retire and must not manufacture a public Patch handle. A failed inverse
    // is different: keep its private journal for owner retirement, but still
    // do not report a successfully created Patch to the caller.
    if (!status && patch.Scopes.empty())
        return status;

    try {
        auto [stored, inserted] = m_Patches.emplace(id, std::move(patch));
        if (!inserted)
            return Failure(Error::InvalidState,
                           "Behavior Patch id collision.");
    } catch (...) {
        for (auto scope = patch.Scopes.rbegin();
             scope != patch.Scopes.rend(); ++scope)
            (void) m_Edit.Close(scope->Value);
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch.");
    }
    if (status)
        out = id;
    return status;
}

Status Patches::ResolveNode(const SessionOwner &owner, PatchId patch,
                            std::uint32_t handle, ObjectRef &out) const {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end() || found->second.TargetDeleted ||
        found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::InvalidState,
                       "The Behavior Patch handle is stale.");
    const auto named = found->second.Handles.find(handle);
    if (named == found->second.Handles.end())
        return Failure(Error::QueryNotFound,
                       "The Behavior Patch names no Node under this handle.");
    if (named->second.first >= found->second.Scopes.size())
        return Failure(Error::InvalidState,
                       "The Behavior Patch Node scope is unavailable.");
    CKBehavior *native = nullptr;
    status = m_Edit.ResolveNode(
        found->second.Scopes[named->second.first].Value,
        named->second.second, native);
    if (!status)
        return status;
    if (!m_IssueObject)
        return Failure(Error::InvalidState,
                       "This Loader cannot issue object references.");
    out = m_IssueObject(native);
    if (out.IsNull())
        return Failure(Error::CreateFailed,
                       "The Loader could not issue a reference for the Node.");
    return {};
}

class Patches::PlanWorld final : public Plan::World {
public:
    PlanWorld(Patches &patches, SessionOwner owner, PatchKey patch,
              std::shared_ptr<const GraphEdit> edit)
        : m_Patches(patches), m_Owner(std::move(owner)),
          m_Patch(std::move(patch)), m_Edit(std::move(edit)) {}

    Status Install(const PatchKey &key, const ObjectRef &target, Epoch,
                   Installation &out) override {
        out = 0;
        PatchId patch = 0;
        Status status = m_Patches.Install(
            m_Owner, m_Patch, target, *m_Edit, patch);
        if (status)
            out = static_cast<Installation>(patch);
        return status;
    }

    Status Close(Installation installation) override {
        return m_Patches.Close(
            m_Owner, static_cast<PatchId>(installation));
    }

private:
    Patches &m_Patches;
    SessionOwner m_Owner;
    PatchKey m_Patch;
    std::shared_ptr<const GraphEdit> m_Edit;
};

Status Patches::Submit(Plans &plans, const SessionOwner &owner,
                       ScriptSelection target, std::string name,
                       GraphEdit edit,
                       PlanId &out) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || !target || name.empty())
        return Failure(Error::OwnerInvalid,
                       "A durable Behavior Edit requires an owner, script, and patch name.");
    // A durable Plan installs into scripts that do not exist yet, so it cannot
    // carry a reference issued against one live world.
    if (edit.UsesIdentity())
        return Failure(
            Error::WorldBoundValue,
            "A durable Behavior Edit cannot retain a live Object, Node, or "
            "Link; query graph objects by name or Prototype instead.");
    status = edit.Validate();
    if (!status)
        return status;
    try {
        auto body = std::make_shared<GraphEdit>(std::move(edit));
        auto world = std::make_shared<PlanWorld>(
            *this, owner, PatchKey{owner.Id, name}, std::move(body));
        return plans.Submit(
            {owner.Id, std::move(name)}, owner.Generation, std::move(target),
            std::move(world), out);
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the durable Behavior Edit.");
    }
}

Status Patches::Submit(Plans &plans, const SessionOwner &owner,
                       std::string name, std::vector<Rule> rules,
                       PlanId &out) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || name.empty() || rules.empty())
        return Failure(Error::OwnerInvalid,
                       "A Behavior Plan requires an owner, name, and Script rule.");
    std::set<std::pair<std::string, TargetSet>> selections;
    for (const Rule &rule : rules) {
        if (!rule.Scripts || !rule.Body)
            return Failure(Error::TargetInvalid,
                           "A Behavior Plan Script rule is incomplete.");
        if (rule.Body->UsesIdentity())
            return Failure(
                Error::WorldBoundValue,
                "A Behavior Plan cannot retain a live Object, Node, or Link.");
        status = rule.Body->Validate();
        if (!status)
            return status;
        if (!selections.emplace(rule.Scripts.Name,
                                rule.Scripts.Instances).second) {
            return Failure(
                Error::InvalidGraphLocality,
                "A Script selection may appear only once in one Behavior Plan.");
        }
    }

    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const PlanId id = NextPlanId();
    if (!id)
        return Failure(Error::InvalidState,
                       "Behavior Plan ids are exhausted.");
    OwnedPlan plan;
    plan.Id = id;
    plan.Owner = owner;
    plan.Name = std::move(name);
    plan.Definition = std::move(rules);
    status = Activate(plans, plan);
    plan.LastStatus = status;
    if (!status && plan.Rules.empty())
        return status;
    try {
        m_Plans.emplace(id, std::move(plan));
    } catch (...) {
        (void) Deactivate(plans, plan);
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Plan.");
    }
    out = id;
    return status;
}

Status Patches::Activate(Plans &plans, OwnedPlan &plan) {
    Status status = ActivateFrom(
        plans, plan, plan.Definition, plan.Rules.size());
    if (status)
        plan.LiveDefinition = plan.Definition;
    return status;
}

Status Patches::ActivateFrom(Plans &plans, OwnedPlan &plan,
                             const std::vector<Rule> &definition,
                             std::size_t firstRule) {
    if (firstRule > definition.size() || plan.Rules.size() != firstRule)
        return Failure(Error::InvalidState,
                       "A Behavior Plan replacement prefix is invalid.");
    std::vector<PlanId> installed;
    Status status;
    try {
        installed.reserve(definition.size() - firstRule);
        for (std::size_t index = firstRule; index < definition.size(); ++index) {
            const Rule &rule = definition[index];
            auto world = std::make_shared<PlanWorld>(
                *this, plan.Owner, PatchKey{plan.Owner.Id, plan.Name},
                rule.Body);
            PlanId id = 0;
            status = plans.Submit(
                {plan.Owner.Id, plan.Name + "/" + std::to_string(plan.Id) +
                                      "/" + std::to_string(index)},
                plan.Owner.Generation, rule.Scripts, std::move(world), id);
            if (!status)
                break;
            installed.push_back(id);
        }
    } catch (...) {
        status = Failure(Error::CreateFailed,
                         "The Loader could not retain the Behavior Plan rules.");
    }
    if (!status) {
        for (auto id = installed.rbegin(); id != installed.rend(); ++id)
            (void) plans.Close(*id);
        return status;
    }
    plan.Rules.insert(plan.Rules.end(), installed.begin(), installed.end());
    return {};
}

Status Patches::Deactivate(Plans &plans, OwnedPlan &plan) {
    return DeactivateFrom(plans, plan, 0);
}

Status Patches::DeactivateFrom(Plans &plans, OwnedPlan &plan,
                               std::size_t firstRule) {
    if (firstRule > plan.Rules.size())
        return Failure(Error::InvalidState,
                       "A Behavior Plan replacement prefix is invalid.");
    while (plan.Rules.size() > firstRule) {
        Status status = plans.Close(plan.Rules.back());
        if (!status)
            return status;
        plan.Rules.pop_back();
        if (plan.LiveDefinition.size() > plan.Rules.size())
            plan.LiveDefinition.pop_back();
    }
    return {};
}

Status Patches::ReconcilePlan(Plans &plans, OwnedPlan &plan) {
    if (plan.Retiring || !plan.DesiredActive) {
        Status status = Deactivate(plans, plan);
        if (status) {
            plan.LiveDefinition.clear();
            plan.PreviousDefinition.clear();
            plan.ChangeFrom.reset();
            plan.ChangeFault = {};
            plan.Failed = false;
            plan.ReturningPrevious = false;
        }
        plan.LastStatus = status;
        return status;
    }
    if (plan.Failed)
        return plan.LastStatus;

    if (plan.ChangeFrom) {
        Status status = DeactivateFrom(plans, plan, *plan.ChangeFrom);
        if (!status) {
            plan.LastStatus = status;
            return status;
        }
        plan.ChangeFrom.reset();
        if (!plan.ChangeFault && !plan.ReturningPrevious) {
            plan.Failed = true;
            plan.LastStatus = plan.ChangeFault;
            return plan.ChangeFault;
        }
    }

    const std::size_t prefix = CommonPrefix(
        plan.LiveDefinition, plan.Definition);
    if (prefix < plan.LiveDefinition.size()) {
        if (plan.PreviousDefinition.empty())
            plan.PreviousDefinition = plan.LiveDefinition;
        plan.ChangeFrom = prefix;
        return ReconcilePlan(plans, plan);
    }
    if (prefix == plan.Definition.size() &&
        prefix == plan.LiveDefinition.size()) {
        plan.Failed = false;
        plan.PreviousDefinition.clear();
        plan.ReturningPrevious = false;
        return {};
    }

    const std::vector<Rule> requested = plan.Definition;
    const std::uint64_t revision = plan.Revision;
    Status status = ActivateFrom(plans, plan, requested, prefix);
    if (status) {
        plan.LiveDefinition = requested;
        plan.Failed = false;
        if (revision == plan.Revision) {
            plan.PreviousDefinition.clear();
            if (!plan.ChangeFault) {
                plan.LastStatus = plan.ChangeFault;
                Status reported = plan.ChangeFault;
                plan.ChangeFault = {};
                plan.ReturningPrevious = false;
                return reported;
            }
            plan.LastStatus = {};
        }
        return {};
    }

    const Status requestedFailure = status;
    if (!plan.PreviousDefinition.empty()) {
        plan.Definition = std::move(plan.PreviousDefinition);
        plan.PreviousDefinition.clear();
        ++plan.Revision;
        plan.ChangeFault = requestedFailure;
        plan.ReturningPrevious = true;
        plan.LastStatus = requestedFailure;
        Status fallback = ReconcilePlan(plans, plan);
        if (!fallback && fallback.Code != requestedFailure.Code)
            return fallback;
        return requestedFailure;
    }

    plan.ChangeFault = requestedFailure;
    plan.ReturningPrevious = false;
    plan.Failed = true;
    plan.LastStatus = requestedFailure;
    return requestedFailure;
}

PlanState Patches::State(Plans &plans, const OwnedPlan &plan) const {
    if (plan.Retiring)
        return PlanState::Retiring;
    if (plan.Failed)
        return PlanState::Conflicted;
    if (!plan.DesiredActive && plan.Rules.empty())
        return PlanState::Disabled;
    if (!plan.DesiredActive)
        return PlanState::Reconciling;
    if (plan.Rules.empty())
        return PlanState::Reconciling;
    bool active = false;
    bool unsatisfied = false;
    bool reconciling = false;
    for (PlanId id : plan.Rules) {
        PlanInfo info;
        if (!plans.Read(id, info))
            return PlanState::Conflicted;
        switch (info.State) {
        case PlanState::Conflicted: return PlanState::Conflicted;
        case PlanState::Retiring: return PlanState::Retiring;
        case PlanState::Active: active = true; break;
        case PlanState::Unsatisfied: unsatisfied = true; break;
        case PlanState::Reconciling: reconciling = true; break;
        case PlanState::Partial: active = true; unsatisfied = true; break;
        case PlanState::Disabled: unsatisfied = true; break;
        }
    }
    const bool definitionPending = plan.ChangeFrom ||
        plan.Rules.size() != plan.Definition.size() ||
        CommonPrefix(plan.LiveDefinition, plan.Definition) !=
            plan.Definition.size();
    if (active && (unsatisfied || reconciling || definitionPending))
        return PlanState::Partial;
    if (active)
        return PlanState::Active;
    if (reconciling || definitionPending)
        return PlanState::Reconciling;
    return PlanState::Unsatisfied;
}

Status Patches::ReadPlan(Plans &plans, const SessionOwner &owner,
                         PlanId id, PlanInfo &out) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end() || found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan handle is stale.");
    out = {};
    out.State = State(plans, found->second);
    out.World = plans.WorldEpoch();
    out.Diagnostic = found->second.LastStatus;
    for (PlanId rule : found->second.Rules) {
        PlanInfo info;
        Status status = plans.Read(rule, info);
        if (!status) {
            if (out.Diagnostic)
                out.Diagnostic = status;
            continue;
        }
        out.Matches += info.Matches;
        out.Installations += info.Installations;
        if (!info.Diagnostic && out.Diagnostic)
            out.Diagnostic = info.Diagnostic;
    }
    return {};
}

Status Patches::SetPlanActive(Plans &plans, const SessionOwner &owner,
                              PlanId id, bool active) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end() || found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan handle is stale.");
    OwnedPlan &plan = found->second;
    if (plan.Retiring)
        return Failure(Error::InvalidState,
                       "A retiring Behavior Plan cannot be enabled.");
    plan.DesiredActive = active;
    ++plan.Revision;
    plan.Failed = false;
    plan.ReturningPrevious = false;
    plan.ChangeFault = {};
    plan.LastStatus = {};
    if (!active)
        plan.ChangeFrom = 0;
    Status status = m_Edit.CanPublish()
        ? ReconcilePlan(plans, plan) : Status{};
    plan.LastStatus = status;
    return status;
}

Status Patches::ReplacePlan(Plans &plans, const SessionOwner &owner,
                            PlanId id, std::vector<Rule> rules) {
    if (rules.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Plan replacement requires a Script rule.");
    std::set<std::pair<std::string, TargetSet>> selections;
    for (const Rule &rule : rules) {
        if (!rule.Scripts || !rule.Body || rule.Body->UsesIdentity())
            return Failure(Error::WorldBoundValue,
                           "A Behavior Plan replacement must be durable.");
        Status valid = rule.Body->Validate();
        if (!valid)
            return valid;
        if (!selections.emplace(rule.Scripts.Name,
                                rule.Scripts.Instances).second) {
            return Failure(
                Error::InvalidGraphLocality,
                "A Script selection may appear only once in one Behavior Plan.");
        }
    }
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end() || found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan handle is stale.");
    OwnedPlan &plan = found->second;
    if (plan.Retiring)
        return Failure(Error::InvalidState,
                       "A retiring Behavior Plan cannot be replaced.");
    if (plan.DesiredActive && plan.PreviousDefinition.empty())
        plan.PreviousDefinition = plan.LiveDefinition;
    plan.Definition = std::move(rules);
    ++plan.Revision;
    plan.Failed = false;
    plan.ReturningPrevious = false;
    plan.ChangeFault = {};
    plan.LastStatus = {};
    const std::size_t prefix = CommonPrefix(
        plan.LiveDefinition, plan.Definition);
    if (prefix < plan.LiveDefinition.size()) {
        plan.ChangeFrom = plan.ChangeFrom
            ? (std::min)(*plan.ChangeFrom, prefix) : prefix;
    }
    if (!plan.DesiredActive) {
        plan.PreviousDefinition.clear();
        return {};
    }
    Status status = m_Edit.CanPublish()
        ? ReconcilePlan(plans, plan) : Status{};
    plan.LastStatus = status;
    return status;
}

Status Patches::ClosePlan(Plans &plans, const SessionOwner &owner,
                          PlanId id) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end())
        return {};
    if (found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan belongs to another Mod generation.");
    found->second.Retiring = true;
    found->second.DesiredActive = false;
    ++found->second.Revision;
    found->second.ChangeFrom = 0;
    // Closing admission is thread-safe, but Plans owns the rule records that
    // must be retired. Touch those records only at a CK edit safe point; this
    // also keeps the lock order between Plans and Patches consistent.
    if (!m_Edit.CanPublish()) {
        Status queued = Failure(Error::Busy,
                                "The Behavior Plan is Retiring.",
                                Phase::Teardown);
        found->second.LastStatus = queued;
        return queued;
    }
    Status status = ReconcilePlan(plans, found->second);
    found->second.LastStatus = status;
    if (status)
        m_Plans.erase(found);
    return status;
}

Status Patches::Install(const SessionOwner &owner, const PatchKey &patch,
                        const ObjectRef &graph, const GraphEdit &edit,
                        PatchId &out, const HandleMap *authorNodes) {
    out = 0;
    const PatchId id = NextId();
    if (!id)
        return Failure(Error::InvalidState,
                       "Behavior Patch ids are exhausted.");
    OwnedPatch installed;
    installed.Id = id;
    installed.Owner = owner;
    CKObject *rootObject = m_ResolveObject ? m_ResolveObject(graph) : nullptr;
    CKBehavior *rootBehavior = rootObject
        ? CKBehavior::Cast(rootObject) : nullptr;
    if (!rootBehavior)
        return Failure(Error::TargetInvalid,
                       "The Behavior Patch target graph is stale.");
    installed.Graph = rootBehavior->GetID();
    Status status;
    try {
        status = InstallScope(owner, patch, graph, edit, 1, 0, installed,
                              authorNodes);
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch.");
    }
    if (!status) {
        for (auto scope = installed.Scopes.rbegin();
             scope != installed.Scopes.rend(); ++scope)
            (void) m_Edit.Close(scope->Value);
        installed.Retiring = true;
    }
    if (installed.Scopes.empty())
        return status;
    auto [stored, inserted] = m_Patches.emplace(id, std::move(installed));
    if (!inserted)
        return Failure(Error::InvalidState,
                       "Behavior Patch id collision.");
    if (status)
        out = id;
    return status;
}

Status Patches::Install(OwnedPatch &patch) {
    if (!patch.Scopes.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Patch is already installed.");
    const std::vector<Target> definition = patch.Definition;
    Status status = InstallFrom(patch, definition, 0);
    if (status)
        patch.LiveDefinition = definition;
    return status;
}

Status Patches::InstallFrom(OwnedPatch &patch,
                            const std::vector<Target> &definition,
                            std::size_t firstTarget) {
    if (firstTarget > definition.size())
        return Failure(Error::InvalidState,
                       "A Behavior Patch replacement prefix is invalid.");

    struct Prepared {
        const Target *TargetValue = nullptr;
        Edit Value;
        std::map<std::uint32_t, Node> Nodes;
    };

    const PatchKey key{patch.Owner.Id, patch.Name};
    Status status;
    std::vector<Prepared> prepared;
    try {
        prepared.reserve(definition.size() - firstTarget);
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not prepare the Behavior Patch targets.");
    }

    // Resolve and compile every top-level Graph before publishing any of
    // them. Nested scopes are compiled after their parent exists, but remain
    // inside the same rollback journal.
    for (std::size_t index = firstTarget; index < definition.size(); ++index) {
        const Target &target = definition[index];
        if (target.Fingerprint != 0) {
            CKObject *object = m_ResolveObject
                ? m_ResolveObject(target.Graph) : nullptr;
            CKBehavior *graph = object ? CKBehavior::Cast(object) : nullptr;
            NativeRef native;
            GraphModel current;
            status = graph ? m_Graph.Refer(graph, native)
                           : Failure(Error::TargetInvalid,
                                     "A Behavior Patch target Graph is stale.");
            if (status)
                status = m_Graph.Read(native, GraphView::Logical, current);
            if (!status)
                break;
            if (current.Fingerprint != target.Fingerprint) {
                status = Failure(
                    Error::GraphChanged,
                    "A target Graph changed after its logical snapshot was read.");
                break;
            }
        }
        Prepared item;
        item.TargetValue = &target;
        status = target.Body->Compile(
            key, target.Graph, *this, item.Value, &item.Nodes);
        if (!status)
            break;
        prepared.push_back(std::move(item));
    }
    if (!status)
        return status;

    for (std::size_t offset = 0; offset < prepared.size(); ++offset) {
        const std::size_t index = firstTarget + offset;
        Prepared &item = prepared[offset];
        status = PublishScope(
            patch.Owner, key, item.TargetValue->Graph,
            *item.TargetValue->Body, std::move(item.Value),
            std::move(item.Nodes), 1, index, patch,
            &item.TargetValue->Handles);
        if (!status)
            break;
    }
    if (status)
        return {};

    const Status failure = status;
    Status restored = RestoreFrom(patch, firstTarget);
    if (!restored && restored.Code != Error::Busy)
        return restored;
    return failure;
}

Status Patches::InstallScope(const SessionOwner &owner,
                             const PatchKey &patch,
                             const ObjectRef &graph,
                             const GraphEdit &edit,
                             std::uint32_t scopeId,
                             std::size_t targetIndex,
                             OwnedPatch &out,
                             const HandleMap *authorNodes) {
    Edit resolved;
    std::map<std::uint32_t, Node> compiled;
    PatchKey scopedKey = patch;
    if (scopeId != 1)
        scopedKey.Name += "/" + std::to_string(scopeId);
    Status status = edit.Compile(
        scopedKey, graph, *this, resolved, &compiled, scopeId != 1);
    if (!status)
        return status;

    return PublishScope(owner, patch, graph, edit, std::move(resolved),
                        std::move(compiled), scopeId, targetIndex, out,
                        authorNodes);
}

Status Patches::PublishScope(const SessionOwner &owner,
                             const PatchKey &patch,
                             const ObjectRef &graph,
                             const GraphEdit &edit,
                             Edit resolved,
                             std::map<std::uint32_t, Node> compiled,
                             std::uint32_t scopeId,
                             std::size_t targetIndex,
                             OwnedPatch &out,
                             const HandleMap *authorNodes) {
    Status status;

    Patch value;
    status = m_Edit.Apply(resolved, value);
    if (!value)
        return status;
    if (value.State() == PatchState::Pending && !edit.NestedGraphs().empty()) {
        (void) m_Edit.Close(value);
        return Failure(
            Error::Busy,
            "A nested Graph Edit cannot begin until its parent Patch reaches the safe point.");
    }

    OwnedPatch::Scope applied;
    applied.Id = scopeId;
    applied.Target = targetIndex;
    CKObject *graphObject = m_ResolveObject ? m_ResolveObject(graph) : nullptr;
    CKBehavior *graphBehavior = graphObject
        ? CKBehavior::Cast(graphObject) : nullptr;
    if (!graphBehavior) {
        (void) m_Edit.Close(value);
        return Failure(Error::GraphChanged,
                       "A Graph Edit target disappeared after Apply.");
    }
    applied.Graph = graphBehavior->GetID();
    applied.Value = std::move(value);
    applied.Handles = compiled;
    const std::size_t scopeIndex = out.Scopes.size();
    out.Scopes.push_back(std::move(applied));

    if (authorNodes) {
        for (const auto &[author, symbol] : *authorNodes) {
            if (symbol.Scope != scopeId)
                continue;
            const auto found = compiled.find(symbol.Node);
            if (found != compiled.end())
                out.Handles.emplace(
                    author, std::make_pair(scopeIndex, found->second));
        }
    } else if (scopeId == 1) {
        for (const auto &[handle, node] : compiled)
            out.Handles.emplace(
                handle, std::make_pair(scopeIndex, node));
    }

    if (!status)
        return status;

    for (const GraphEdit::Nested &nested : edit.NestedGraphs()) {
        if (!nested.Body)
            return Failure(Error::InvalidState,
                           "A nested Graph Edit has no body.");
        const auto parent = compiled.find(nested.Parent.Value);
        if (parent == compiled.end())
            return Failure(Error::InvalidState,
                           "A nested Graph Edit lost its parent Node.");
        CKBehavior *native = nullptr;
        status = m_Edit.ResolveNode(
            out.Scopes[scopeIndex].Value, parent->second, native);
        if (!status)
            return status;
        if (!native || native->IsUsingFunction())
            return Failure(
                Error::InvalidGraphLocality,
                "A nested Graph Edit requires a graph-backed Behavior Node.");
        if (!m_IssueObject)
            return Failure(Error::Unavailable,
                           "The Loader cannot name the nested graph.");
        const ObjectRef nestedGraph = m_IssueObject(native);
        if (nestedGraph.IsNull())
            return Failure(Error::CreateFailed,
                           "The Loader could not retain the nested graph identity.");
        status = InstallScope(owner, patch, nestedGraph, *nested.Body,
                              nested.Scope, targetIndex, out, authorNodes);
        if (!status)
            return status;
    }
    return status;
}

Status Patches::Begin(const PatchKey &patch, const ObjectRef &graph,
                      Edit &out, GraphModel &base) {
    out = {};
    base = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(graph) : nullptr;
    CKBehavior *behavior = object ? CKBehavior::Cast(object) : nullptr;
    if (!behavior)
        return Failure(Error::TargetInvalid,
                       "The Behavior Patch target graph is stale.");
    Status status = m_Edit.Begin(behavior, patch, out);
    if (status)
        status = m_Graph.Read(out.GraphRef(), GraphView::Logical, base);
    return status;
}

Status Patches::UseNode(Edit &edit, const ObjectRef &node, Node &out) {
    out = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(node) : nullptr;
    CKBehavior *behavior = object ? CKBehavior::Cast(object) : nullptr;
    return behavior
        ? m_Edit.Use(edit, behavior, out)
        : Failure(Error::GraphChanged,
                  "A Behavior Node disappeared during compilation.");
}

Status Patches::UseLink(Edit &edit, const ObjectRef &link, Link &out) {
    out = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(link) : nullptr;
    CKBehaviorLink *native = object ? CKBehaviorLink::Cast(object) : nullptr;
    return native
        ? m_Edit.Use(edit, native, out)
        : Failure(Error::GraphChanged,
                  "A Behavior Link disappeared during compilation.");
}

Status Patches::Tap(Edit &edit, Port source,
                    const HookBlock::Hook &hook) {
    std::shared_ptr<HookBlock::Binding> binding = hook.Bind();
    if (!binding) {
        return Failure(Error::CallbackFailed,
                       "The Tap callback is no longer available.");
    }
    edit.Tap(std::move(source), std::move(binding));
    return {};
}

Status Patches::Interpose(Edit &edit, Link link,
                          const HookBlock::Hook &hook) {
    std::shared_ptr<HookBlock::Binding> binding = hook.Bind();
    if (!binding) {
        return Failure(Error::CallbackFailed,
                       "The Link callback is no longer available.");
    }
    Node block;
    Status status = m_Edit.Add(
        edit, HookBlock::Make(std::move(binding), 1, 1), block,
        NodeRole::Infrastructure);
    if (status)
        edit.Splice(link, block);
    return status;
}

Status Patches::Read(const SessionOwner &owner, PatchId patch,
                     PatchInfo &out) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end() || found->second.TargetDeleted ||
        found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::InvalidState,
                       "The Behavior Patch handle is stale.");
    out.State = State(found->second);
    out.Diagnostic = Diagnostic(found->second);
    out.Conflicts.clear();
    for (const OwnedPatch::Scope &scope : found->second.Scopes) {
        const std::vector<RevertConflict> conflicts = scope.Value.Conflicts();
        out.Conflicts.insert(out.Conflicts.end(), conflicts.begin(),
                             conflicts.end());
    }
    return {};
}

PatchState Patches::State(const OwnedPatch &patch) const {
    if (patch.Scopes.empty()) {
        if (patch.Retiring)
            return PatchState::Closed;
        if (patch.Failed)
            return PatchState::Failed;
        return patch.DesiredActive
            ? PatchState::Pending : PatchState::Disabled;
    }
    bool pending = false;
    bool closing = false;
    bool active = false;
    for (const OwnedPatch::Scope &scope : patch.Scopes) {
        switch (scope.Value.State()) {
        case PatchState::Conflicted: return PatchState::Conflicted;
        case PatchState::Failed: return PatchState::Failed;
        case PatchState::Pending: pending = true; break;
        case PatchState::Closing: closing = true; break;
        case PatchState::Active: active = true; break;
        case PatchState::Disabled: break;
        case PatchState::Closed: break;
        }
    }
    if (closing)
        return PatchState::Closing;
    if (!patch.DesiredActive)
        return PatchState::Closing;
    if (pending)
        return PatchState::Pending;
    if (patch.ChangeFrom ||
        CommonPrefix(patch.LiveDefinition, patch.Definition) !=
            patch.Definition.size() ||
        patch.LiveDefinition.size() != patch.Definition.size())
        return PatchState::Pending;
    if (active)
        return PatchState::Active;
    return PatchState::Closed;
}

Status Patches::Diagnostic(const OwnedPatch &patch) const {
    Status first = patch.LastStatus;
    for (const OwnedPatch::Scope &scope : patch.Scopes) {
        Status current = scope.Value.Diagnostic();
        if (!current && first)
            first = std::move(current);
    }
    return first;
}

void Patches::RebuildHandles(OwnedPatch &patch) {
    patch.Handles.clear();
    for (std::size_t scopeIndex = 0; scopeIndex < patch.Scopes.size();
         ++scopeIndex) {
        const OwnedPatch::Scope &scope = patch.Scopes[scopeIndex];
        if (scope.Target >= patch.Definition.size())
            continue;
        for (const auto &[author, symbol] :
             patch.Definition[scope.Target].Handles) {
            if (symbol.Scope != scope.Id)
                continue;
            const auto found = scope.Handles.find(symbol.Node);
            if (found != scope.Handles.end())
                patch.Handles.emplace(
                    author, std::make_pair(scopeIndex, found->second));
        }
    }
}

Status Patches::Restore(OwnedPatch &patch) {
    return RestoreFrom(patch, 0);
}

Status Patches::RestoreFrom(OwnedPatch &patch, std::size_t target) {
    const auto firstScope = std::find_if(
        patch.Scopes.begin(), patch.Scopes.end(),
        [&](const OwnedPatch::Scope &scope) {
            return scope.Target >= target;
        });
    if (firstScope == patch.Scopes.end())
        return {};
    const std::size_t firstIndex = static_cast<std::size_t>(
        std::distance(patch.Scopes.begin(), firstScope));
    Status first;
    bool busy = false;
    for (auto scope = patch.Scopes.rbegin();
         scope != patch.Scopes.rend() -
                      static_cast<std::ptrdiff_t>(firstIndex); ++scope) {
        const PatchState state = scope->Value.State();
        if (state == PatchState::Closed || state == PatchState::Failed)
            continue;
        if (state == PatchState::Closing) {
            busy = true;
            break;
        }
        Status status = m_Edit.Close(scope->Value);
        if (!status && first)
            first = std::move(status);
        if (!status || scope->Value.State() == PatchState::Conflicted)
            break;
        if (scope->Value.State() == PatchState::Closing) {
            busy = true;
            break;
        }
    }
    if (!first)
        return first;
    Status result = busy
        ? Failure(Error::Busy, "The Behavior Patch is Closing.",
                  Phase::Teardown)
        : Status{};
    if (result && std::all_of(
            patch.Scopes.begin() + static_cast<std::ptrdiff_t>(firstIndex),
            patch.Scopes.end(),
            [](const OwnedPatch::Scope &scope) {
                const PatchState state = scope.Value.State();
                return state == PatchState::Closed ||
                       state == PatchState::Failed;
            })) {
        patch.Scopes.resize(firstIndex);
        for (auto handle = patch.Handles.begin();
             handle != patch.Handles.end();) {
            if (handle->second.first >= firstIndex)
                handle = patch.Handles.erase(handle);
            else
                ++handle;
        }
    }
    return result;
}

Status Patches::Close(OwnedPatch &patch) {
    patch.Retiring = true;
    patch.DesiredActive = false;
    ++patch.Revision;
    return Restore(patch);
}

Status Patches::Reconcile(OwnedPatch &patch) {
    if (patch.Retiring || !patch.DesiredActive) {
        Status status = Restore(patch);
        if (status) {
            patch.LiveDefinition.clear();
            patch.PreviousDefinition.clear();
            patch.ChangeFrom.reset();
            patch.ChangeFault = {};
            patch.Failed = false;
            patch.ReturningPrevious = false;
        }
        patch.LastStatus = status;
        return status;
    }
    if (patch.Failed)
        return patch.LastStatus;

    if (patch.ChangeFrom) {
        const std::size_t first = *patch.ChangeFrom;
        Status status = RestoreFrom(patch, first);
        if (!status) {
            patch.LastStatus = status;
            return status;
        }
        patch.LiveDefinition.resize(
            (std::min)(patch.LiveDefinition.size(), first));
        patch.ChangeFrom.reset();
        if (!patch.ChangeFault && !patch.ReturningPrevious) {
            patch.Failed = true;
            patch.LastStatus = patch.ChangeFault;
            return patch.ChangeFault;
        }
    }

    const std::size_t prefix = CommonPrefix(
        patch.LiveDefinition, patch.Definition);
    if (prefix < patch.LiveDefinition.size()) {
        if (patch.PreviousDefinition.empty())
            patch.PreviousDefinition = patch.LiveDefinition;
        patch.ChangeFrom = prefix;
        return Reconcile(patch);
    }
    if (prefix == patch.Definition.size() &&
        prefix == patch.LiveDefinition.size()) {
        patch.Failed = false;
        patch.PreviousDefinition.clear();
        patch.ReturningPrevious = false;
        RebuildHandles(patch);
        return {};
    }

    const std::vector<Target> requested = patch.Definition;
    const std::uint64_t revision = patch.Revision;
    Status status = InstallFrom(patch, requested, prefix);
    if (status) {
        patch.LiveDefinition = requested;
        patch.Failed = false;
        RebuildHandles(patch);
        if (revision == patch.Revision) {
            patch.PreviousDefinition.clear();
            if (!patch.ChangeFault) {
                patch.LastStatus = patch.ChangeFault;
                Status reported = patch.ChangeFault;
                patch.ChangeFault = {};
                patch.ReturningPrevious = false;
                return reported;
            }
            patch.LastStatus = {};
        }
        return {};
    }

    const Status requestedFailure = status;
    patch.ChangeFrom = prefix;
    Status restored = RestoreFrom(patch, prefix);
    if (restored) {
        patch.LiveDefinition.resize(
            (std::min)(patch.LiveDefinition.size(), prefix));
        patch.ChangeFrom.reset();
    } else if (restored.Code != Error::Busy) {
        patch.Failed = true;
        patch.LastStatus = restored;
        return restored;
    }

    if (!patch.PreviousDefinition.empty()) {
        patch.Definition = std::move(patch.PreviousDefinition);
        patch.PreviousDefinition.clear();
        ++patch.Revision;
        patch.ChangeFault = requestedFailure;
        patch.ReturningPrevious = true;
        patch.LastStatus = requestedFailure;
        if (restored && m_Edit.CanPublish()) {
            Status fallback = Reconcile(patch);
            if (!fallback && fallback.Code != requestedFailure.Code)
                return fallback;
        }
        return requestedFailure;
    }

    patch.ChangeFault = requestedFailure;
    patch.ReturningPrevious = false;
    patch.Failed = restored || restored.Code != Error::Busy;
    patch.LastStatus = requestedFailure;
    return requestedFailure;
}

Status Patches::SetActive(const SessionOwner &owner, PatchId patch,
                          bool active) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end() || found->second.TargetDeleted ||
        found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Patch handle is stale.");
    OwnedPatch &value = found->second;
    if (value.Retiring)
        return Failure(Error::InvalidState,
                       "A retiring Behavior Patch cannot be enabled.");
    value.DesiredActive = active;
    ++value.Revision;
    value.Failed = false;
    value.ReturningPrevious = false;
    value.LastStatus = {};
    value.ChangeFault = {};
    if (!active)
        value.ChangeFrom = 0;
    if (m_Edit.CanPublish())
        return Reconcile(value);
    return {};
}

Status Patches::Replace(const SessionOwner &owner, PatchId patch,
                        std::vector<Target> targets) {
    if (targets.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Patch replacement requires a target Graph.");
    std::set<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> graphs;
    for (const Target &target : targets) {
        if (target.Graph.IsNull() || !target.Body)
            return Failure(Error::TargetInvalid,
                           "A Behavior Patch replacement target is missing.");
        Status valid = target.Body->Validate();
        if (!valid)
            return valid;
        const auto identity = std::make_tuple(
            target.Graph.Domain, target.Graph.Slot, target.Graph.Generation);
        if (!graphs.insert(identity).second)
            return Failure(Error::InvalidGraphLocality,
                           "A Graph may appear only once in one Behavior Patch.");
    }
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end() || found->second.TargetDeleted ||
        found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Patch handle is stale.");
    OwnedPatch &value = found->second;
    if (value.Retiring)
        return Failure(Error::InvalidState,
                       "A retiring Behavior Patch cannot be replaced.");

    // Retain the new definition before touching CK. A callback can replace it
    // again, in which case the next safe point sees only the final request.
    if (value.DesiredActive && value.PreviousDefinition.empty())
        value.PreviousDefinition = value.LiveDefinition;
    value.Definition = std::move(targets);
    ++value.Revision;
    value.Failed = false;
    value.ReturningPrevious = false;
    value.LastStatus = {};
    value.ChangeFault = {};
    const std::size_t prefix = CommonPrefix(
        value.LiveDefinition, value.Definition);
    if (prefix < value.LiveDefinition.size()) {
        value.ChangeFrom = value.ChangeFrom
            ? (std::min)(*value.ChangeFrom, prefix) : prefix;
    }
    if (!value.DesiredActive) {
        value.PreviousDefinition.clear();
        return {};
    }
    return m_Edit.CanPublish() ? Reconcile(value) : Status{};
}

Status Patches::Close(const SessionOwner &owner, PatchId patch) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end())
        return {};
    if (found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Patch belongs to another Mod generation.");
    Status status = Close(found->second);
    const PatchState state = State(found->second);
    if (state == PatchState::Closed || state == PatchState::Failed)
        m_Patches.erase(found);
    return status;
}

Status Patches::RetireOwner(const std::string &ownerId) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (auto plan = m_Plans.begin(); plan != m_Plans.end();) {
        if (plan->second.Owner.Id == ownerId)
            plan = m_Plans.erase(plan);
        else
            ++plan;
    }
    for (auto &[id, patch] : m_Patches) {
        if (patch.Owner.Id != ownerId)
            continue;
        (void) Close(patch);
    }
    m_Edit.ProcessFrame();
    Collect();
    Status remaining;
    for (const auto &[id, patch] : m_Patches) {
        if (patch.Owner.Id != ownerId)
            continue;
        Status status = Diagnostic(patch);
        if (status) {
            status = Failure(Error::Busy,
                             "A Behavior Patch is still Closing.",
                             Phase::Teardown);
        }
        if (remaining ||
            (remaining.Code == Error::Busy && status.Code != Error::Busy))
            remaining = std::move(status);
    }
    return remaining;
}

void Patches::ObjectsToBeDeleted(const CK_ID *ids, int count) {
    if (!ids || count <= 0 || std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const std::set<CK_ID> deleting(ids, ids + count);
    for (auto &entry : m_Patches) {
        OwnedPatch &patch = entry.second;
        const bool affected = std::any_of(
            patch.Scopes.begin(), patch.Scopes.end(),
            [&](const OwnedPatch::Scope &scope) {
                const PatchState state = scope.Value.State();
                // Closing a parent scope destroys graph-backed Nodes created
                // by that Patch. Their nested scope has already been restored
                // in reverse order, so this deletion is part of the journal's
                // own inverse rather than loss of an external target.
                return state != PatchState::Closed &&
                       state != PatchState::Failed &&
                       deleting.contains(scope.Graph);
            }) || std::any_of(
                patch.Definition.begin(), patch.Definition.end(),
                [&](const Target &target) {
                    CKObject *object = m_ResolveObject
                        ? m_ResolveObject(target.Graph) : nullptr;
                    return object && deleting.contains(object->GetID());
                });
        if (!affected)
            continue;

        // Top-level targets are independent Graphs. Restore every surviving
        // Graph even when one target (or one nested graph) has already entered
        // CK deletion; a missing target must not strand the rest of this
        // composed Patch in its after-image. The deletion callback is not a
        // graph-mutation safe point, so only forget journals whose graph is
        // disappearing here. ProcessFrame restores every surviving scope.
        for (auto &scope : patch.Scopes) {
            if (deleting.contains(scope.Graph))
                m_Edit.GraphDeleted(scope.Value);
        }
        patch.DesiredActive = false;
        patch.Retiring = true;
        patch.TargetDeleted = true;
        ++patch.Revision;
    }
    m_Edit.ObjectsToBeDeleted(ids, count);
}

void Patches::ResetWorld() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (auto &[id, patch] : m_Patches)
        (void) Close(patch);
    m_Edit.ProcessFrame();
    Collect();

    // A conflicting external edit may prevent exact restoration. Admission is
    // already closed, and the CK world is about to disappear; do not retain a
    // journal whose native identities belong to the old world.
    m_Patches.clear();
}

void Patches::ProcessFrame(Plans &plans) {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_Edit.ProcessFrame();
    for (auto plan = m_Plans.begin(); plan != m_Plans.end();) {
        Status status = ReconcilePlan(plans, plan->second);
        plan->second.LastStatus = status;
        if (plan->second.Retiring && plan->second.Rules.empty()) {
            plan = m_Plans.erase(plan);
            continue;
        }
        ++plan;
    }
    for (auto &entry : m_Patches) {
        OwnedPatch &patch = entry.second;
        if (m_Edit.CanPublish())
            (void) Reconcile(patch);
    }
    Collect();
}

void Patches::Collect() {
    for (auto patch = m_Patches.begin(); patch != m_Patches.end();) {
        const PatchState state = State(patch->second);
        if (patch->second.Retiring &&
            (state == PatchState::Closed || state == PatchState::Failed))
            patch = m_Patches.erase(patch);
        else
            ++patch;
    }
}

} // namespace BML::Behavior::Internal
