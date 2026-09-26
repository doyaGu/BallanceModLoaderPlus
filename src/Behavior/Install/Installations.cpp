#include "Behavior/Install/Installations.h"

#include "Behavior/Blocks/HookBlock.h"

#include <algorithm>
#include <limits>
#include <set>
#include <string_view>
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

template <class Definition>
Status ExtendDefinitionBindings(
    const std::set<std::uint64_t> &current,
    const std::vector<Definition> &definitions,
    std::set<std::uint64_t> &extended) {
    try {
        extended = current;
        for (const Definition &definition : definitions) {
            if (definition.Binding &&
                !extended.insert(definition.Binding).second) {
                return Failure(
                    Error::InvalidGraphLocality,
                    "A Behavior definition binding cannot be reused by one handle.");
            }
        }
    } catch (...) {
        extended.clear();
        return Failure(
            Error::CreateFailed,
            "The Loader could not retain Behavior definition bindings.");
    }
    return {};
}

bool Same(const Installations::Target &left,
          const Installations::Target &right) noexcept {
    // Bindings identify one submission's symbols; they do not change the CK
    // edit. Settle publishes the current binding after reconciliation.
    return left.Graph == right.Graph &&
        left.Fingerprint == right.Fingerprint &&
        left.Body && right.Body && left.Body->SameAs(*right.Body);
}

bool Same(const Installations::Rule &left,
          const Installations::Rule &right) noexcept {
    return left.Scripts == right.Scripts &&
        left.Binding == right.Binding &&
        left.Body && right.Body && left.Body->SameAs(*right.Body) &&
        left.Symbols == right.Symbols;
}

template <class Definition>
std::size_t CommonPrefix(const std::vector<Definition> &left,
                         const std::vector<Definition> &right) noexcept {
    const std::size_t count = (std::min)(left.size(), right.size());
    std::size_t prefix = 0;
    while (prefix < count && Same(left[prefix], right[prefix]))
        ++prefix;
    return prefix;
}

// Remembers where the first failure of the current pass happened: on the
// requested definition, or on the previous one while falling back to it.
template <class R>
void NoteFailure(R &record, std::size_t index, std::string_view script = {}) {
    if (record.Recovery == decltype(record.Recovery)::Previous) {
        if (!record.RestoreAt) {
            record.RestoreAt = index;
            record.RestoreScript = script;
        }
    } else if (!record.ApplyAt) {
        record.ApplyAt = index;
        record.ApplyScript = script;
    }
}

template <class R>
void ClearFailures(R &record) {
    record.PrimaryFailure = {};
    record.RecoveryFailure = {};
    record.ApplyAt.reset();
    record.RestoreAt.reset();
    record.ApplyScript.clear();
    record.RestoreScript.clear();
}

// The record under id when it belongs to this owner generation.
template <class Map>
auto Owned(Map &records, typename Map::key_type id, const SessionOwner &owner)
    -> decltype(&records.begin()->second) {
    const auto found = records.find(id);
    if (found == records.end() || found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return nullptr;
    return &found->second;
}

Status StalePatch(Error error) {
    return Failure(error, "The Behavior Patch handle is stale.");
}

Status StalePlan() {
    return Failure(Error::OwnerInvalid, "The Behavior Plan handle is stale.");
}

Status PatchStatus(Status status, const std::string &name,
                   const SessionOwner &owner,
                   std::optional<std::size_t> target = {}) {
    if (status)
        return status;
    std::string where = "Behavior Patch '" + name + "', Mod '" + owner.Id +
        "', generation " + std::to_string(owner.Generation);
    if (target)
        where += ", target index " + std::to_string(*target);
    status.Message += " [" + where + "]";
    return status;
}

Status PlanStatus(Status status, const std::string &name,
                  const SessionOwner &owner, Epoch world,
                  std::optional<std::size_t> rule = {},
                  std::string_view script = {}) {
    if (status)
        return status;
    std::string where = "Behavior Plan '" + name + "', Mod '" + owner.Id +
        "', generation " + std::to_string(owner.Generation) +
        ", world " + std::to_string(world);
    if (rule)
        where += ", rule index " + std::to_string(*rule);
    if (!script.empty())
        where += ", Script '" + std::string(script) + "'";
    status.Message += " [" + where + "]";
    return status;
}

} // namespace

// Installs one Plan rule into each Script its Selection matches, as a Patch
// owned by the Plan's owner and admitted under the Plan's admission.
class Installations::PlanWorld final : public Selection::World {
public:
    PlanWorld(Installations &installations, SessionOwner owner, PatchKey patch,
              std::shared_ptr<const Program> edit,
              SymbolMap symbols,
              std::shared_ptr<const CallbackAdmission> admission = {})
        : m_Installations(installations), m_Owner(std::move(owner)),
          m_Patch(std::move(patch)), m_Edit(std::move(edit)),
          m_Symbols(std::move(symbols)),
          m_Admission(std::move(admission)) {}

    Status Install(const PatchKey &, const ObjectRef &target, Epoch,
                   Installation &out) override {
        out = 0;
        if (!m_Owner || (m_Admission && !m_Admission->IsOpen()))
            return Failure(Error::InvalidState, "The Behavior Session is closed.");
        try {
            Target graph;
            graph.Graph = target;
            graph.Body = m_Edit;
            graph.Symbols = m_Symbols;
            std::vector<Target> targets;
            targets.push_back(std::move(graph));

            PatchId patch = 0;
            Status status = m_Installations.Apply(
                m_Owner, m_Patch.Name, std::move(targets), patch, m_Admission);
            if (status)
                out = static_cast<Installation>(patch);
            return status;
        } catch (...) {
            return Failure(
                Error::CreateFailed,
                "The Loader could not retain the Behavior Plan target.");
        }
    }

    Status Close(Installation installation) override {
        return m_Installations.Close(
            m_Owner, static_cast<PatchId>(installation));
    }

private:
    Installations &m_Installations;
    SessionOwner m_Owner;
    PatchKey m_Patch;
    std::shared_ptr<const Program> m_Edit;
    SymbolMap m_Symbols;
    std::shared_ptr<const CallbackAdmission> m_Admission;
};

Installations::Installations(CKContext *context, Runtime &runtime,
                             PrototypeCatalog *catalog, GraphSource &graph,
                             ResolveObject resolveObject,
                             IssueObject issueObject)
    : m_Edit(context, runtime, catalog, graph),
      m_Graph(graph), m_ResolveObject(std::move(resolveObject)),
      m_IssueObject(std::move(issueObject)),
      m_Thread(std::this_thread::get_id()) {}

Installations::~Installations() {
    ResetWorld();
}

Status Installations::Ready() const {
    return std::this_thread::get_id() == m_Thread
        ? Status{}
        : Failure(Error::WrongThread,
                  "Behavior Patches require the game thread.");
}

PatchId Installations::NextId() {
    if (m_NextId == 0 ||
        m_NextId == (std::numeric_limits<PatchId>::max)())
        return 0;
    return m_NextId++;
}

PlanId Installations::NextPlanId() {
    if (m_NextPlanId == 0 ||
        m_NextPlanId == (std::numeric_limits<PlanId>::max)())
        return 0;
    return m_NextPlanId++;
}

std::shared_ptr<CallbackAdmission> Installations::RegisterAdmission(
    bool plan, std::uint64_t id, const SessionOwner &owner,
    std::shared_ptr<const CallbackAdmission> parent) {
    auto admission = std::make_shared<CallbackAdmission>(
        parent ? std::move(parent) : owner.Admission);
    std::lock_guard<std::mutex> lock(m_AdmissionMutex);
    m_Admissions.emplace(std::make_pair(plan, id), AdmissionRecord{owner, admission});
    m_HasAdmissions = true;
    return admission;
}

Status Installations::RequestClose(bool plan, std::uint64_t id,
                                   const SessionOwner &owner) {
    std::lock_guard<std::mutex> lock(m_AdmissionMutex);
    const auto found = m_Admissions.find({plan, id});
    if (found == m_Admissions.end())
        return {};
    if (found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior handle belongs to another Mod generation.");
    if (auto admission = found->second.Admission.lock())
        admission->Close();
    return {};
}

Status Installations::Validate(const std::vector<Target> &targets,
                               bool replacing) {
    if (targets.empty()) {
        return replacing
            ? Failure(Error::InvalidState,
                      "A Behavior Patch replacement requires a target Graph.")
            : Failure(Error::OwnerInvalid,
                      "A Behavior Patch requires an owner, name, and target Graph.");
    }
    std::set<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> graphs;
    for (const Target &target : targets) {
        if (target.Graph.IsNull() || !target.Body) {
            return Failure(Error::TargetInvalid,
                           replacing
                               ? "A Behavior Patch replacement target is missing."
                               : "A Behavior Patch target Graph is missing.");
        }
        Status status = target.Body->Validate();
        if (!status)
            return status;
        const auto identity = std::make_tuple(
            target.Graph.Domain, target.Graph.Slot, target.Graph.Generation);
        if (!graphs.insert(identity).second)
            return Failure(Error::InvalidGraphLocality,
                           "A Graph may appear only once in one Behavior Patch.");
    }
    return {};
}

Status Installations::Validate(const std::vector<Rule> &rules,
                               bool replacing) {
    if (rules.empty()) {
        return replacing
            ? Failure(Error::InvalidState,
                      "A Behavior Plan replacement requires a Script rule.")
            : Failure(Error::OwnerInvalid,
                      "A Behavior Plan requires an owner, name, and Script rule.");
    }
    std::set<std::pair<std::string, TargetSet>> selections;
    for (const Rule &rule : rules) {
        if (!rule.Scripts || !rule.Body) {
            return replacing
                ? Failure(Error::InvalidState,
                          "A Behavior Plan replacement requires complete Script rules.")
                : Failure(Error::TargetInvalid,
                          "A Behavior Plan Script rule is incomplete.");
        }
        if (rule.Body->UsesIdentity()) {
            return Failure(
                Error::WorldBoundValue,
                replacing
                    ? "A Behavior Plan cannot retain live graph identity across worlds."
                    : "A Behavior Plan cannot retain a live Object, Node, or Link.");
        }
        Status status = rule.Body->Validate();
        if (!status)
            return status;
        if (!selections.emplace(rule.Scripts.Name,
                                rule.Scripts.Instances).second) {
            return Failure(
                Error::InvalidGraphLocality,
                "A Script selection may appear only once in one Behavior Plan.");
        }
    }
    return {};
}

// Moves one installation toward its goal: every rule of the requested
// definition installed, or none. A replacement keeps the installed prefix it
// shares with the request, restores the rest, and installs the new suffix. A
// failed suffix falls back to the previous definition when there is one.
template <class R>
Status Installations::Reconcile(R &record) {
    if ((!record.Owner || !record.Admission->IsOpen()) &&
        record.Goal != InstallGoal::Closed)
        Retire(record);
    if (record.Goal != InstallGoal::Enabled) {
        Status status = Uninstall(record, 0);
        if (status) {
            record.Applied.clear();
            record.Previous.clear();
            record.RestoreFrom.reset();
            ClearFailures(record);
            record.Recovery = InstallRecovery::None;
        } else {
            record.RecoveryFailure = status;
        }
        record.LastStatus = status;
        return status;
    }
    if (record.Recovery == InstallRecovery::Blocked)
        return record.LastStatus;

    if (record.RestoreFrom) {
        const std::size_t first = *record.RestoreFrom;
        Status status = Uninstall(record, first);
        if (!status) {
            record.RecoveryFailure = status;
            record.LastStatus = status;
            return status;
        }
        record.RecoveryFailure = {};
        record.RestoreAt.reset();
        record.RestoreScript.clear();
        record.Applied.resize((std::min)(record.Applied.size(), first));
        record.RestoreFrom.reset();
        if (!record.PrimaryFailure &&
            record.Recovery != InstallRecovery::Previous) {
            record.Recovery = InstallRecovery::Blocked;
            record.LastStatus = record.PrimaryFailure;
            return record.PrimaryFailure;
        }
    }

    const std::size_t prefix = CommonPrefix(record.Applied, record.Requested);
    Status refreshed = Refresh(record, prefix);
    if (!refreshed)
        return refreshed;
    if (prefix < record.Applied.size()) {
        if (record.Previous.empty())
            record.Previous = record.Applied;
        record.RestoreFrom = prefix;
        return Reconcile(record);
    }
    if (prefix == record.Requested.size() &&
        prefix == record.Applied.size()) {
        record.Previous.clear();
        record.Recovery = InstallRecovery::None;
        Settle(record);
        return {};
    }

    const auto requested = record.Requested;
    const std::uint64_t revision = record.Revision;
    const bool restoringPrevious =
        record.Recovery == InstallRecovery::Previous;
    Status status = InstallFrom(record, requested, prefix);
    if constexpr (!R::IsPlan) {
        if (!record.Owner || !record.Admission->IsOpen() ||
            record.Goal == InstallGoal::Closed) {
            CloseAdmission(record);
            (void) Uninstall(record, 0);
            return Failure(Error::InvalidState,
                           "Behavior Patch admission closed while installing.");
        }
    }
    if (status) {
        // A Plan appends each installed rule itself. A Patch retains the
        // request whole, since a kept prefix target may carry new symbols.
        if constexpr (!R::IsPlan)
            record.Applied = requested;
        Settle(record);
        if (revision == record.Revision) {
            record.Previous.clear();
            record.RecoveryFailure = {};
            if (record.Recovery == InstallRecovery::Previous &&
                !record.PrimaryFailure) {
                record.LastStatus = record.PrimaryFailure;
                Status reported = record.PrimaryFailure;
                record.Recovery = InstallRecovery::None;
                return reported;
            }
            record.PrimaryFailure = {};
            record.ApplyAt.reset();
            record.ApplyScript.clear();
            record.Recovery = InstallRecovery::None;
            record.LastStatus = {};
        }
        return {};
    }

    const Status requestedFailure = status;
    if (restoringPrevious) {
        if (record.RecoveryFailure)
            record.RecoveryFailure = requestedFailure;
        record.RestoreFrom = prefix;
        record.Recovery = InstallRecovery::Blocked;
        record.LastStatus = record.RecoveryFailure;
        return record.LastStatus;
    }
    if (record.PrimaryFailure)
        record.PrimaryFailure = requestedFailure;
    record.RestoreFrom = prefix;
    return Fallback(record, prefix, requestedFailure);
}

// A Patch restores the failed suffix at once and reinstalls its previous
// definition only at a CK edit safe point.
Status Installations::Fallback(PatchRecord &patch, std::size_t prefix,
                               const Status &requestedFailure) {
    if (!patch.Previous.empty()) {
        patch.Requested = std::move(patch.Previous);
        patch.Previous.clear();
        patch.Recovery = InstallRecovery::Previous;
        ++patch.Revision;
    }
    Status restored = Uninstall(patch, prefix);
    if (restored) {
        patch.RecoveryFailure = {};
        patch.Applied.resize((std::min)(patch.Applied.size(), prefix));
        patch.RestoreFrom.reset();
    } else if (restored.Code != Error::Busy) {
        patch.RecoveryFailure = restored;
        patch.LastStatus = restored;
        return restored;
    }

    if (patch.Recovery == InstallRecovery::Previous) {
        patch.LastStatus = requestedFailure;
        if (restored && m_Edit.CanPublish()) {
            Status fallback = Reconcile(patch);
            if (!fallback && fallback.Code != requestedFailure.Code)
                return fallback;
        }
        return requestedFailure;
    }

    if (restored)
        patch.Recovery = InstallRecovery::Blocked;
    patch.LastStatus = requestedFailure;
    return requestedFailure;
}

// A Plan returns to its previous rules at once. Reconcile closes the failed
// suffix first through RestoreFrom.
Status Installations::Fallback(PlanRecord &plan, std::size_t prefix,
                               const Status &requestedFailure) {
    if (!plan.Previous.empty()) {
        plan.Requested = std::move(plan.Previous);
        plan.Previous.clear();
        ++plan.Revision;
        plan.Recovery = InstallRecovery::Previous;
        plan.LastStatus = requestedFailure;
        Status fallback = Reconcile(plan);
        if (!fallback && fallback.Code != requestedFailure.Code)
            return fallback;
        return requestedFailure;
    }

    if (plan.Selections.size() == prefix) {
        plan.RestoreFrom.reset();
        plan.Recovery = InstallRecovery::Blocked;
    }
    plan.LastStatus = requestedFailure;
    return requestedFailure;
}

template <class R>
Status Installations::SetGoal(R &record, bool active) {
    if (record.Goal == InstallGoal::Closed || !record.Admission->IsOpen()) {
        return Failure(Error::InvalidState,
                       R::IsPlan
                           ? "A retiring Behavior Plan cannot be enabled."
                           : "A retiring Behavior Patch cannot be enabled.");
    }
    const InstallGoal goal =
        active ? InstallGoal::Enabled : InstallGoal::Disabled;
    if (record.Goal == goal && record.Recovery == InstallRecovery::None &&
        !record.RestoreFrom && record.LastStatus) {
        using StateKind = decltype(State(record));
        const StateKind current = State(record);
        if ((active && current == StateKind::Active) ||
            (!active && current == StateKind::Disabled))
            return {};
    }
    record.Goal = goal;
    ++record.Revision;
    record.Recovery = InstallRecovery::None;
    record.LastStatus = {};
    ClearFailures(record);
    if (active) {
        Status retry = Refresh(record, record.Applied.size());
        if (!retry)
            return retry;
    }
    if (!active)
        record.RestoreFrom = 0;
    Status status = m_Edit.CanPublish() ? Reconcile(record) : Status{};
    if constexpr (R::IsPlan)
        record.LastStatus = status;
    return status;
}

template <class R, class Definition>
Status Installations::Redefine(R &record,
                               std::vector<Definition> definitions) {
    if (record.Goal == InstallGoal::Closed || !record.Admission->IsOpen()) {
        return Failure(Error::InvalidState,
                       R::IsPlan
                           ? "A retiring Behavior Plan cannot be replaced."
                           : "A retiring Behavior Patch cannot be replaced.");
    }
    std::set<std::uint64_t> definitionBindings;
    Status status = ExtendDefinitionBindings(
        record.DefinitionBindings, definitions, definitionBindings);
    if (!status)
        return status;

    // Retain the new definition before touching CK. A callback can replace it
    // again, in which case the next safe point sees only the final request.
    if (record.Goal == InstallGoal::Enabled && record.Previous.empty())
        record.Previous = record.Applied;
    record.DefinitionBindings = std::move(definitionBindings);
    record.Requested = std::move(definitions);
    ++record.Revision;
    record.Recovery = InstallRecovery::None;
    record.LastStatus = {};
    ClearFailures(record);
    const std::size_t prefix = CommonPrefix(record.Applied, record.Requested);
    if (prefix < record.Applied.size()) {
        record.RestoreFrom = record.RestoreFrom
            ? (std::min)(*record.RestoreFrom, prefix) : prefix;
    }
    if (record.Goal == InstallGoal::Disabled) {
        record.Previous.clear();
        return {};
    }
    status = m_Edit.CanPublish() ? Reconcile(record) : Status{};
    if constexpr (R::IsPlan)
        record.LastStatus = status;
    return status;
}

template <class R>
bool Installations::HasPendingChange(const R &record) {
    if (!record.Owner || !record.Admission->IsOpen())
        return record.Goal != InstallGoal::Closed || Installed(record);
    if (record.Recovery == InstallRecovery::Blocked)
        return false;
    if (record.RestoreFrom)
        return true;
    return record.Goal == InstallGoal::Enabled
        ? record.Applied.size() != record.Requested.size()
        : Installed(record);
}

Status Installations::Begin(const SessionOwner &owner, CKBehavior *graph,
                            std::string name, Ops &out) {
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || name.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Patch requires a live owner and name.");
    return m_Edit.Begin(graph, {owner.Id, std::move(name)}, out);
}

Status Installations::Use(Ops &edit, CKBehavior *behavior, Node &out) {
    return m_Edit.Use(edit, behavior, out);
}

Status Installations::Use(Ops &edit, CKBehaviorLink *link, Link &out) {
    return m_Edit.Use(edit, link, out);
}

Status Installations::Add(Ops &edit, BlockSpec block, Node &out) {
    return m_Edit.Add(edit, std::move(block), out);
}

Status Installations::AddGraph(Ops &edit, std::string name, int priority,
                               Node &out) {
    return m_Edit.AddGraph(edit, std::move(name), priority, out);
}

Status Installations::Apply(const SessionOwner &owner, const Ops &edit,
                            PatchId &out,
                            const std::map<std::uint32_t, Node> *handles) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || edit.Key().Owner != owner.Id || edit.Key().Name.empty())
        return Failure(Error::OwnerInvalid,
                       "The Behavior Edit does not belong to this Mod generation.");

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

    auto admission = RegisterAdmission(false, id, owner);
    PatchRecord owned;
    try {
        owned.Id = id;
        owned.Owner = owner;
        owned.Admission = admission;
        owned.Graph = static_cast<CK_ID>(graph.Id);
        PatchRecord::Scope scope;
        scope.Id = RootGraphScope;
        scope.Graph = static_cast<CK_ID>(graph.Id);
        if (handles)
            scope.Symbols.Nodes = *handles;
        owned.Scopes.push_back(std::move(scope));
        if (handles) {
            for (const auto &[handle, node] : *handles)
                owned.Symbols.emplace(
                    SymbolRef{0, RootGraphScope, handle},
                    PatchRecord::ResolvedSymbol{0, node});
        }
        const auto [stored, inserted] =
            m_Patches.emplace(id, std::move(owned));
        if (!inserted)
            return Failure(Error::InvalidState,
                           "Behavior Patch id collision.");
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch journal.");
    }

    Patch patch;
    try {
        status = m_Edit.Apply(edit, patch, admission);
    } catch (...) {
        if (patch)
            (void) m_Edit.Close(patch);
        m_Patches.erase(id);
        return Failure(Error::CreateFailed,
                       "The Loader could not apply the Behavior Patch.");
    }
    if (!patch) {
        m_Patches.erase(id);
        return status;
    }
    PatchRecord &stored = m_Patches.at(id);
    stored.Scopes.front().Value = std::move(patch);
    const bool admissionInterrupted =
        !owner || !stored.Admission->IsOpen();
    if (!status)
        CloseAdmission(stored);
    // A failed Apply normally rolls back completely and has no Patch value.
    // RevertConflict is different: retain its conflict journal under the owner,
    // but do not hand a successful installation id to the caller.
    if (admissionInterrupted) {
        CloseAdmission(stored);
        status = Failure(Error::InvalidState,
                         "Behavior Patch admission closed while installing.");
    }
    if (!status) {
        if (stored.PrimaryFailure)
            stored.PrimaryFailure = status;
        stored.LastStatus = status;
    }
    if (!owner) {
        return Failure(Error::InvalidState,
                       "The Behavior Session closed while the Patch was opening.");
    }
    if (status)
        out = id;
    return status;
}

Status Installations::Apply(const SessionOwner &owner, const ObjectRef &graph,
                            std::string name, Program edit, PatchId &out,
                            const SymbolMap *authorSymbols) {
    try {
        Target target;
        target.Graph = graph;
        target.Body = std::make_shared<Program>(std::move(edit));
        if (authorSymbols)
            target.Symbols = *authorSymbols;
        std::vector<Target> targets;
        targets.push_back(std::move(target));
        return Apply(owner, std::move(name), std::move(targets), out);
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch definition.");
    }
}

Status Installations::Apply(const SessionOwner &owner, std::string name,
                            std::vector<Target> targets, PatchId &out) {
    return Apply(owner, std::move(name), std::move(targets), out, {});
}

Status Installations::Apply(
    const SessionOwner &owner, std::string name,
    std::vector<Target> targets, PatchId &out,
    std::shared_ptr<const CallbackAdmission> parentAdmission) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || name.empty())
        return Failure(Error::OwnerInvalid,
                       "A Behavior Patch requires an owner, name, and target Graph.");
    status = Validate(targets, false);
    if (!status)
        return status;
    std::set<std::uint64_t> definitionBindings;
    status = ExtendDefinitionBindings({}, targets, definitionBindings);
    if (!status)
        return status;

    const PatchId id = NextId();
    if (!id)
        return Failure(Error::InvalidState,
                       "Behavior Patch ids are exhausted.");

    PatchRecord patch;
    patch.Id = id;
    patch.Owner = owner;
    patch.Admission = RegisterAdmission(
        false, id, owner, std::move(parentAdmission));
    patch.Name = std::move(name);
    patch.DefinitionBindings = std::move(definitionBindings);
    patch.Requested = std::move(targets);
    if (m_Edit.CanPublish()) {
        status = Install(patch);
        patch.LastStatus = status;
        if (!status && !patch.Scopes.empty()) {
            patch.RestoreFrom = 0;
            if (patch.PrimaryFailure)
                patch.PrimaryFailure = status;
        }
    }

    if (!owner || !patch.Admission->IsOpen()) {
        CloseAdmission(patch);
        status = Failure(Error::InvalidState,
                         "Behavior Patch admission closed while installing.");
    }

    // A failed opening never produced an author-owned handle. Keep any
    // remaining journal under the Mod owner only until its inverse completes.
    if (!status)
        CloseAdmission(patch);

    // A validation or admission failure that changed no Graph has nothing to
    // retire and must not manufacture a public Patch handle. A failed inverse
    // is different: keep its private journal for owner retirement, but still
    // do not report a successfully created Patch to the caller.
    if (!status && patch.Scopes.empty())
        return status;

    try {
        auto [stored, inserted] = m_Patches.try_emplace(
            id, std::move(patch));
        if (!inserted) {
            CloseAdmission(patch);
            (void) Uninstall(patch, 0);
            return Failure(Error::InvalidState,
                           "Behavior Patch id collision.");
        }
    } catch (...) {
        CloseAdmission(patch);
        (void) Uninstall(patch, 0);
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch.");
    }
    if (status)
        out = id;
    return status;
}

Status Installations::Install(PatchRecord &patch) {
    if (!patch.Scopes.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Patch is already installed.");
    const std::vector<Target> definition = patch.Requested;
    Status status = InstallFrom(patch, definition, 0);
    if (status)
        patch.Applied = definition;
    return status;
}

Status Installations::InstallFrom(PatchRecord &patch,
                                  const std::vector<Target> &definition,
                                  std::size_t firstTarget) {
    if (firstTarget > definition.size())
        return Failure(Error::InvalidState,
                       "A Behavior Patch replacement prefix is invalid.");

    struct Prepared {
        const Target *TargetValue = nullptr;
        Ops Value;
        Program::ResolvedSymbols Symbols;
    };

    const PatchKey key{patch.Owner.Id, patch.Name};
    Status status;
    std::vector<Prepared> prepared;
    try {
        prepared.reserve(definition.size() - firstTarget);
    } catch (...) {
        NoteFailure(patch, firstTarget);
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
            if (!status) {
                NoteFailure(patch, index);
                break;
            }
            if (current.Fingerprint != target.Fingerprint) {
                status = Failure(
                    Error::GraphChanged,
                    "A target Graph changed after its logical snapshot was read.");
                NoteFailure(patch, index);
                break;
            }
        }
        Prepared item;
        item.TargetValue = &target;
        status = target.Body->Resolve(
            key, target.Graph, *this, item.Value, &item.Symbols);
        if (!status) {
            NoteFailure(patch, index);
            break;
        }
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
            std::move(item.Symbols), 1, index, patch,
            &item.TargetValue->Symbols);
        if (!status) {
            NoteFailure(patch, index);
            break;
        }
    }
    if (status)
        return {};

    const Status failure = status;
    if (patch.PrimaryFailure)
        patch.PrimaryFailure = failure;
    Status restored = Uninstall(patch, firstTarget);
    if (!restored) {
        patch.RecoveryFailure = restored;
        if (restored.Code != Error::Busy)
            return restored;
    }
    return failure;
}

Status Installations::InstallScope(const SessionOwner &owner,
                                   const PatchKey &patch,
                                   const ObjectRef &graph,
                                   const Program &edit,
                                   std::uint32_t scopeId,
                                   std::size_t targetIndex,
                                   PatchRecord &out,
                                   const SymbolMap *authorSymbols) {
    Ops resolved;
    Program::ResolvedSymbols compiled;
    PatchKey scopedKey = patch;
    if (scopeId != RootGraphScope)
        scopedKey.Name += "/" + std::to_string(scopeId);
    Status status = edit.Resolve(
        scopedKey, graph, *this, resolved, &compiled,
        scopeId != RootGraphScope);
    if (!status)
        return status;

    return PublishScope(owner, patch, graph, edit, std::move(resolved),
                        std::move(compiled), scopeId, targetIndex, out,
                        authorSymbols);
}

Status Installations::PublishScope(const SessionOwner &owner,
                                   const PatchKey &patch,
                                   const ObjectRef &graph,
                                   const Program &edit,
                                   Ops resolved,
                                   Program::ResolvedSymbols compiled,
                                   std::uint32_t scopeId,
                                   std::size_t targetIndex,
                                   PatchRecord &out,
                                   const SymbolMap *authorSymbols) {
    Status status;

    Patch value;
    status = m_Edit.Apply(resolved, value, out.Admission);
    if (!value)
        return status;
    if (value.State() == PatchState::Pending && !edit.NestedGraphs().empty()) {
        (void) m_Edit.Close(value);
        return Failure(
            Error::Busy,
            "A nested Graph Edit cannot begin until its parent Patch reaches the safe point.");
    }

    PatchRecord::Scope applied;
    bool retained = false;
    try {
        applied.Id = scopeId;
        applied.Target = targetIndex;
        CKObject *graphObject = m_ResolveObject
            ? m_ResolveObject(graph) : nullptr;
        CKBehavior *graphBehavior = graphObject
            ? CKBehavior::Cast(graphObject) : nullptr;
        if (!graphBehavior) {
            (void) m_Edit.Close(value);
            return Failure(Error::GraphChanged,
                           "A Graph Edit target disappeared after Apply.");
        }
        applied.Graph = graphBehavior->GetID();
        applied.Value = std::move(value);
        applied.Symbols = compiled;
        const std::size_t scopeIndex = out.Scopes.size();
        out.Scopes.push_back(std::move(applied));
        retained = true;

        if (authorSymbols) {
            for (const auto &[author, symbol] : *authorSymbols) {
                if (author.Scope != scopeId)
                    continue;
                if (symbol.Kind == SymbolKind::Node) {
                    const auto found = compiled.Nodes.find(
                        symbol.NodeValue.Value);
                    if (found != compiled.Nodes.end()) {
                        out.Symbols.emplace(
                            author, PatchRecord::ResolvedSymbol{
                                        scopeIndex, found->second});
                    }
                } else {
                    const auto found = compiled.Ports.find(
                        symbol.PortValue.Selector.Index);
                    if (found != compiled.Ports.end()) {
                        out.Symbols.emplace(
                            author, PatchRecord::ResolvedSymbol{
                                        scopeIndex, found->second});
                    }
                }
            }
        } else if (scopeId == RootGraphScope) {
            for (const auto &[handle, node] : compiled.Nodes) {
                out.Symbols.emplace(
                    SymbolRef{0, RootGraphScope, handle},
                    PatchRecord::ResolvedSymbol{scopeIndex, node});
            }
        }

        if (!status)
            return status;

        for (const Program::Nested &nested : edit.NestedGraphs()) {
            if (!nested.Body)
                return Failure(Error::InvalidState,
                               "A nested Graph Edit has no body.");
            const auto parent = compiled.Nodes.find(nested.Parent.Value);
            if (parent == compiled.Nodes.end())
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
                return Failure(
                    Error::CreateFailed,
                    "The Loader could not retain the nested graph identity.");
            status = InstallScope(owner, patch, nestedGraph, *nested.Body,
                                  nested.Scope, targetIndex, out, authorSymbols);
            if (!status)
                return status;
        }
        return status;
    } catch (...) {
        if (!retained) {
            if (applied.Value)
                (void) m_Edit.Close(applied.Value);
            else if (value)
                (void) m_Edit.Close(value);
        }
        return Failure(
            Error::CreateFailed,
            "The Loader could not retain the applied Behavior Graph scope.");
    }
}

// Restores every scope installed for target and later targets, newest first.
Status Installations::Uninstall(PatchRecord &patch, std::size_t target) {
    const auto firstScope = std::find_if(
        patch.Scopes.begin(), patch.Scopes.end(),
        [&](const PatchRecord::Scope &scope) {
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
            patch.RestoreAt = scope->Target;
            busy = true;
            break;
        }
        Status status = m_Edit.Close(scope->Value);
        if (!status && first) {
            patch.RestoreAt = scope->Target;
            first = std::move(status);
        }
        if (!status || scope->Value.State() == PatchState::Conflicted)
            break;
        if (scope->Value.State() == PatchState::Closing) {
            patch.RestoreAt = scope->Target;
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
            [](const PatchRecord::Scope &scope) {
                const PatchState state = scope.Value.State();
                return state == PatchState::Closed ||
                       state == PatchState::Failed;
            })) {
        patch.Scopes.resize(firstIndex);
        for (auto symbol = patch.Symbols.begin();
             symbol != patch.Symbols.end();) {
            if (symbol->second.ScopeIndex >= firstIndex)
                symbol = patch.Symbols.erase(symbol);
            else
                ++symbol;
        }
    }
    return result;
}

void Installations::CloseAdmission(PatchRecord &patch) {
    patch.Admission->Close();
    if (patch.Goal != InstallGoal::Closed) {
        patch.Goal = InstallGoal::Closed;
        ++patch.Revision;
    }
    // Stop every scope, even when restoring a later scope must wait or has a
    // conflict. No still-installed sibling Hook may admit another callback.
    for (auto &scope : patch.Scopes)
        m_Edit.CloseAdmission(scope.Value);
}

Status Installations::Close(PatchRecord &patch) {
    CloseAdmission(patch);
    return Uninstall(patch, 0);
}

void Installations::Settle(PatchRecord &patch) {
    patch.Symbols.clear();
    for (std::size_t scopeIndex = 0; scopeIndex < patch.Scopes.size();
         ++scopeIndex) {
        const PatchRecord::Scope &scope = patch.Scopes[scopeIndex];
        if (scope.Target >= patch.Requested.size())
            continue;
        for (const auto &[author, symbol] :
             patch.Requested[scope.Target].Symbols) {
            if (author.Scope != scope.Id)
                continue;
            if (symbol.Kind == SymbolKind::Node) {
                const auto found = scope.Symbols.Nodes.find(
                    symbol.NodeValue.Value);
                if (found != scope.Symbols.Nodes.end()) {
                    patch.Symbols.emplace(
                        author, PatchRecord::ResolvedSymbol{
                                    scopeIndex, found->second});
                }
            } else {
                const auto found = scope.Symbols.Ports.find(
                    symbol.PortValue.Selector.Index);
                if (found != scope.Symbols.Ports.end()) {
                    patch.Symbols.emplace(
                        author, PatchRecord::ResolvedSymbol{
                                    scopeIndex, found->second});
                }
            }
        }
    }
}

Status Installations::Read(const SessionOwner &owner, PatchId id,
                           PatchInfo &out) const {
    Status ready = Ready();
    if (!ready)
        return ready;
    const PatchRecord *patch = Owned(m_Patches, id, owner);
    if (!patch || patch->TargetDeleted)
        return StalePatch(Error::InvalidState);
    out.State = State(*patch);
    out.LastStatus = PatchStatus(
        LastStatus(*patch), patch->Name, patch->Owner);
    out.ApplyFailure = PatchStatus(
        patch->PrimaryFailure, patch->Name, patch->Owner, patch->ApplyAt);
    out.RestoreFailure = PatchStatus(
        RestoreFailure(*patch), patch->Name, patch->Owner, patch->RestoreAt);
    out.Conflicts.clear();
    for (const PatchRecord::Scope &scope : patch->Scopes) {
        const std::vector<RevertConflict> conflicts = scope.Value.Conflicts();
        out.Conflicts.insert(out.Conflicts.end(), conflicts.begin(),
                             conflicts.end());
    }
    return {};
}

PatchState Installations::State(const PatchRecord &patch) const {
    const bool closingRequested = patch.Goal == InstallGoal::Closed ||
        !patch.Admission->IsOpen();
    if (patch.Goal != InstallGoal::Closed && !patch.Admission->IsOpen())
        return PatchState::Closing;
    if (patch.Scopes.empty()) {
        if (closingRequested)
            return PatchState::Closed;
        if (patch.Recovery == InstallRecovery::Blocked)
            return PatchState::Conflicted;
        return patch.Goal == InstallGoal::Enabled
            ? PatchState::Pending : PatchState::Disabled;
    }
    if (patch.Recovery == InstallRecovery::Blocked)
        return PatchState::Conflicted;
    bool pending = false;
    bool active = false;
    for (const PatchRecord::Scope &scope : patch.Scopes) {
        switch (scope.Value.State()) {
        case PatchState::Conflicted: return PatchState::Conflicted;
        case PatchState::Failed: return PatchState::Failed;
        case PatchState::Pending: pending = true; break;
        case PatchState::Closing: break;
        case PatchState::Active: active = true; break;
        case PatchState::Disabled: break;
        case PatchState::Closed: break;
        }
    }
    if (closingRequested)
        return PatchState::Closing;
    if (patch.Goal == InstallGoal::Disabled)
        return PatchState::Closing;
    if (pending)
        return PatchState::Pending;
    if (patch.RestoreFrom ||
        CommonPrefix(patch.Applied, patch.Requested) !=
            patch.Requested.size() ||
        patch.Applied.size() != patch.Requested.size())
        return PatchState::Pending;
    if (active)
        return PatchState::Active;
    return PatchState::Closed;
}

Status Installations::LastStatus(const PatchRecord &patch) const {
    for (const PatchRecord::Scope &scope : patch.Scopes) {
        Status current = scope.Value.Diagnostic();
        if (!current)
            return current;
    }
    return patch.LastStatus;
}

Status Installations::RestoreFailure(const PatchRecord &patch) const {
    if (!patch.RecoveryFailure)
        return patch.RecoveryFailure;
    for (const PatchRecord::Scope &scope : patch.Scopes) {
        if (scope.Value.State() != PatchState::Conflicted)
            continue;
        Status current = scope.Value.Diagnostic();
        if (!current)
            return current;
    }
    return {};
}

Status Installations::ResolvePort(const PatchRecord &patch,
                                  const PortQuery &query, Port &out) const {
    out = Port();
    if (patch.TargetDeleted)
        return Failure(Error::InvalidState,
                       "The Behavior Patch installation is gone.");
    const PatchState state = State(patch);
    if (state == PatchState::Pending)
        return Failure(Error::Busy,
                       "The Behavior Patch has not reached its safe point yet.");
    if (state != PatchState::Active)
        return Failure(Error::InvalidState,
                       "Only an active Behavior Patch exposes values.");

    const auto named = patch.Symbols.find(
        {query.Binding, query.Scope, query.Handle});
    if (named == patch.Symbols.end())
        return Failure(Error::QueryNotFound,
                       "The Behavior Patch names no symbol under this handle.");
    if (named->second.ScopeIndex >= patch.Scopes.size())
        return Failure(Error::InvalidState,
                       "The Behavior Patch symbol scope is unavailable.");
    if (!query.Selector) {
        if (named->second.Kind != SymbolKind::Port)
            return Failure(Error::QueryNotFound,
                           "The Behavior Patch handle does not name a Port.");
        out = named->second.PortValue;
        return {};
    }
    if (named->second.Kind != SymbolKind::Node)
        return Failure(Error::QueryNotFound,
                       "A Port selector requires a Node handle.");
    out.Owner = named->second.NodeValue.Value;
    out.Selector = *query.Selector;
    return {};
}

Status Installations::ResolveNode(const SessionOwner &owner, PatchId id,
                                  const SymbolRef &node, ObjectRef &out) const {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    const PatchRecord *patch = Owned(m_Patches, id, owner);
    if (!patch || patch->TargetDeleted)
        return StalePatch(Error::InvalidState);
    const PatchState state = State(*patch);
    if (state == PatchState::Pending) {
        return Failure(
            Error::Busy,
            "The Behavior Patch has not reached its safe point yet.");
    }
    if (state != PatchState::Active) {
        return Failure(
            Error::InvalidState,
            "Only an active Behavior Patch names live Nodes.");
    }
    const auto named = patch->Symbols.find(node);
    if (named == patch->Symbols.end() ||
        named->second.Kind != SymbolKind::Node)
        return Failure(Error::QueryNotFound,
                       "The Behavior Patch names no Node under this handle.");
    if (named->second.ScopeIndex >= patch->Scopes.size())
        return Failure(Error::InvalidState,
                       "The Behavior Patch Node scope is unavailable.");
    CKBehavior *native = nullptr;
    status = m_Edit.ResolveNode(
        patch->Scopes[named->second.ScopeIndex].Value,
        named->second.NodeValue, native);
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

Status Installations::ReadValue(const SessionOwner &owner, PatchId id,
                                const PortQuery &query,
                                GraphValue &out) const {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    const PatchRecord *patch = Owned(m_Patches, id, owner);
    if (!patch)
        return StalePatch(Error::OwnerInvalid);
    Port port;
    status = ResolvePort(*patch, query, port);
    if (!status)
        return status;
    const auto named = patch->Symbols.find(
        {query.Binding, query.Scope, query.Handle});
    return m_Edit.ReadValue(patch->Scopes[named->second.ScopeIndex].Value,
                            std::move(port), out);
}

Status Installations::WriteValue(const SessionOwner &owner, PatchId id,
                                 const PortQuery &query,
                                 const Parameter::Binding &value) {
    Status status = Ready();
    if (!status)
        return status;
    const PatchRecord *patch = Owned(m_Patches, id, owner);
    if (!patch)
        return StalePatch(Error::OwnerInvalid);
    Port port;
    status = ResolvePort(*patch, query, port);
    if (!status)
        return status;
    const auto named = patch->Symbols.find(
        {query.Binding, query.Scope, query.Handle});
    return m_Edit.WriteValue(patch->Scopes[named->second.ScopeIndex].Value,
                             std::move(port), value);
}

Status Installations::SetActive(const SessionOwner &owner, PatchId id,
                                bool active) {
    Status ready = Ready();
    if (!ready)
        return ready;
    PatchRecord *patch = Owned(m_Patches, id, owner);
    if (!patch || patch->TargetDeleted)
        return StalePatch(Error::OwnerInvalid);
    return SetGoal(*patch, active);
}

Status Installations::Replace(const SessionOwner &owner, PatchId id,
                              std::vector<Target> targets) {
    Status ready = Ready();
    if (!ready)
        return ready;
    Status valid = Validate(targets, true);
    if (!valid)
        return valid;
    PatchRecord *patch = Owned(m_Patches, id, owner);
    if (!patch || patch->TargetDeleted)
        return StalePatch(Error::OwnerInvalid);
    return Redefine(*patch, std::move(targets));
}

Status Installations::Close(const SessionOwner &owner, PatchId id) {
    Status requested = RequestClose(false, id, owner);
    if (!requested)
        return requested;
    if (std::this_thread::get_id() != m_Thread)
        return Failure(Error::Busy, "The Behavior Patch is Closing.", Phase::Teardown);
    const auto found = m_Patches.find(id);
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

Status Installations::Submit(const SessionOwner &owner, std::string name,
                             std::vector<Rule> rules, PlanId &out) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || name.empty())
        return Failure(Error::OwnerInvalid,
                       "A Behavior Plan requires an owner, name, and Script rule.");
    status = Validate(rules, false);
    if (!status)
        return status;
    std::set<std::uint64_t> definitionBindings;
    status = ExtendDefinitionBindings({}, rules, definitionBindings);
    if (!status)
        return status;

    const PlanId id = NextPlanId();
    if (!id)
        return Failure(Error::InvalidState,
                       "Behavior Plan ids are exhausted.");
    PlanRecord plan;
    plan.Id = id;
    plan.Owner = owner;
    plan.Admission = RegisterAdmission(true, id, owner);
    plan.Name = std::move(name);
    plan.DefinitionBindings = std::move(definitionBindings);
    plan.Requested = std::move(rules);
    status = InstallFrom(plan, plan.Requested, plan.Selections.size());
    plan.LastStatus = status;
    if (!status) {
        plan.Admission->Close();
        plan.Goal = InstallGoal::Closed;
        plan.PrimaryFailure = status;
        plan.RestoreFrom = 0;
        if (plan.Selections.empty())
            return status;
    }
    try {
        m_Plans.emplace(id, std::move(plan));
    } catch (...) {
        plan.Admission->Close();
        plan.Goal = InstallGoal::Closed;
        (void) Uninstall(plan, 0);
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Plan.");
    }
    if (status)
        out = id;
    return status;
}

// Submits one Selection for each rule from firstRule on. A failure closes the
// Selections this call submitted.
Status Installations::InstallFrom(PlanRecord &plan,
                                  const std::vector<Rule> &definition,
                                  std::size_t firstRule) {
    if (firstRule > definition.size() || plan.Selections.size() != firstRule)
        return Failure(Error::InvalidState,
                       "A Behavior Plan replacement prefix is invalid.");
    Status status;
    const std::size_t originalSize = plan.Selections.size();
    try {
        plan.Applied.reserve(definition.size());
        plan.Selections.reserve(definition.size());
        for (std::size_t index = firstRule; index < definition.size(); ++index) {
            const Rule &rule = definition[index];
            Rule applied = rule;
            SelectionId selection = 0;
            auto world = std::make_shared<PlanWorld>(
                *this, plan.Owner, PatchKey{plan.Owner.Id, plan.Name},
                rule.Body, rule.Symbols, plan.Admission);
            status = m_Selections.Submit(
                {plan.Owner.Id, plan.Name + "/" + std::to_string(plan.Id) +
                                      "/" + std::to_string(index)},
                plan.Owner.Generation, rule.Scripts, std::move(world),
                selection);
            if (!status) {
                NoteFailure(plan, index, rule.Scripts.Name);
                break;
            }
            plan.Applied.push_back(std::move(applied));
            plan.Selections.push_back(selection);
        }
    } catch (...) {
        status = Failure(Error::CreateFailed,
                         "The Loader could not retain the Behavior Plan rules.");
        const std::size_t index = plan.Selections.size();
        NoteFailure(plan, index,
                    index < definition.size()
                        ? std::string_view(definition[index].Scripts.Name)
                        : std::string_view{});
    }
    if (!status) {
        Status cleanup = Uninstall(plan, originalSize);
        if (!cleanup)
            plan.RecoveryFailure = cleanup;
        return status;
    }
    return {};
}

// Closes the Selections of rule and every later rule, newest first.
Status Installations::Uninstall(PlanRecord &plan, std::size_t rule) {
    if (rule > plan.Selections.size())
        return Failure(Error::InvalidState,
                       "A Behavior Plan replacement prefix is invalid.");
    while (plan.Selections.size() > rule) {
        const std::size_t index = plan.Selections.size() - 1;
        Status status = m_Selections.Close(plan.Selections.back());
        if (!status) {
            plan.RestoreAt = index;
            plan.RestoreScript = plan.Applied.back().Scripts.Name;
            return status;
        }
        plan.Selections.pop_back();
        plan.Applied.pop_back();
    }
    return {};
}

// Asks each kept rule's Selection to match its Scripts again.
Status Installations::Refresh(PlanRecord &plan, std::size_t prefix) {
    for (std::size_t index = 0; index < prefix; ++index) {
        Status retry = m_Selections.Retry(plan.Selections[index]);
        if (!retry)
            return retry;
    }
    return {};
}

PlanState Installations::State(const PlanRecord &plan) const {
    if (plan.Goal == InstallGoal::Closed || !plan.Admission->IsOpen())
        return PlanState::Retiring;
    if (plan.Recovery == InstallRecovery::Blocked)
        return PlanState::Conflicted;
    if (plan.Goal == InstallGoal::Disabled && plan.Selections.empty())
        return PlanState::Disabled;
    if (plan.Goal == InstallGoal::Disabled)
        return PlanState::Reconciling;
    if (plan.Selections.empty())
        return PlanState::Reconciling;
    bool active = false;
    bool unsatisfied = false;
    bool reconciling = false;
    for (const SelectionId selection : plan.Selections) {
        PlanInfo info;
        if (!m_Selections.Read(selection, info))
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
    const bool definitionPending = plan.RestoreFrom ||
        plan.Applied.size() != plan.Requested.size() ||
        CommonPrefix(plan.Applied, plan.Requested) != plan.Requested.size();
    if (active && (unsatisfied || reconciling || definitionPending))
        return PlanState::Partial;
    if (active)
        return PlanState::Active;
    if (reconciling || definitionPending)
        return PlanState::Reconciling;
    return PlanState::Unsatisfied;
}

Status Installations::ReadPlan(const SessionOwner &owner, PlanId id,
                               PlanInfo &out) const {
    Status ready = Ready();
    if (!ready)
        return ready;
    const PlanRecord *found = Owned(m_Plans, id, owner);
    if (!found)
        return StalePlan();
    const PlanRecord &plan = *found;
    out = {};
    out.State = State(plan);
    out.World = m_Selections.WorldEpoch();
    out.LastStatus = plan.LastStatus;
    out.ApplyFailure = plan.PrimaryFailure;
    out.RestoreFailure = plan.RecoveryFailure;
    std::optional<std::size_t> lastRule;
    std::optional<std::size_t> applyRule = plan.ApplyAt;
    std::optional<std::size_t> restoreRule = plan.RestoreAt;
    std::string applyScript = plan.ApplyScript;
    std::string restoreScript = plan.RestoreScript;
    for (std::size_t index = 0; index < plan.Selections.size(); ++index) {
        const std::string &script = plan.Applied[index].Scripts.Name;
        PlanInfo info;
        Status status = m_Selections.Read(plan.Selections[index], info);
        if (!status) {
            if (out.LastStatus) {
                out.LastStatus = status;
                lastRule = index;
            }
            continue;
        }
        out.Matches += info.Matches;
        out.Instances += info.Instances;
        if (!info.LastStatus && out.LastStatus) {
            out.LastStatus = info.LastStatus;
            lastRule = index;
        }
        if (!info.ApplyFailure && out.ApplyFailure) {
            out.ApplyFailure = info.ApplyFailure;
            applyRule = index;
            applyScript = script;
        }
        if (!info.RestoreFailure && out.RestoreFailure) {
            out.RestoreFailure = info.RestoreFailure;
            restoreRule = index;
            restoreScript = script;
        }
        const bool unclassified = !info.LastStatus &&
            info.ApplyFailure && info.RestoreFailure;
        if (unclassified && info.State == PlanState::Conflicted &&
            out.RestoreFailure) {
            out.RestoreFailure = info.LastStatus;
            restoreRule = index;
            restoreScript = script;
        } else if (unclassified && out.ApplyFailure) {
            out.ApplyFailure = info.LastStatus;
            applyRule = index;
            applyScript = script;
        }
    }
    const auto scriptAt = [&](std::optional<std::size_t> index)
        -> std::string_view {
        if (!index || *index >= plan.Selections.size())
            return {};
        return plan.Applied[*index].Scripts.Name;
    };
    out.LastStatus = PlanStatus(
        std::move(out.LastStatus), plan.Name, plan.Owner, out.World,
        lastRule, scriptAt(lastRule));
    out.ApplyFailure = PlanStatus(
        std::move(out.ApplyFailure), plan.Name, plan.Owner, out.World,
        applyRule, applyScript.empty() ? scriptAt(applyRule) : applyScript);
    out.RestoreFailure = PlanStatus(
        std::move(out.RestoreFailure), plan.Name, plan.Owner, out.World,
        restoreRule,
        restoreScript.empty() ? scriptAt(restoreRule) : restoreScript);
    return {};
}

Status Installations::ReadPlanInstances(
    const SessionOwner &owner, PlanId id,
    std::vector<PlanInstance> &out) const {
    out.clear();
    Status status = Ready();
    if (!status)
        return status;
    const PlanRecord *plan = Owned(m_Plans, id, owner);
    if (!plan)
        return StalePlan();

    try {
        for (std::size_t index = 0; index < plan->Selections.size(); ++index) {
            std::vector<InstallationInfo> installations;
            status = m_Selections.ReadInstallations(
                owner.Id, owner.Generation, plan->Selections[index],
                installations);
            if (!status) {
                out.clear();
                return status;
            }
            for (InstallationInfo &installation : installations) {
                out.push_back({static_cast<std::uint32_t>(index),
                               plan->Revision,
                               plan->Applied[index].Binding,
                               std::move(installation)});
            }
        }
    } catch (...) {
        out.clear();
        return Failure(
            Error::CreateFailed,
            "The Loader could not snapshot Behavior Plan instances.");
    }
    return {};
}

Status Installations::FindPlanInstance(const PlanRecord &plan,
                                       const PlanInstance &instance,
                                       PatchId &out) const {
    out = 0;
    if (instance.PlanRevision != plan.Revision ||
        instance.Rule >= plan.Selections.size() ||
        instance.Binding != plan.Applied[instance.Rule].Binding) {
        return Failure(Error::GraphChanged,
                       "The Behavior Plan instance snapshot is stale.");
    }

    std::vector<InstallationInfo> current;
    Status status = m_Selections.ReadInstallations(
        plan.Owner.Id, plan.Owner.Generation,
        plan.Selections[instance.Rule], current);
    if (!status)
        return status;
    const auto found = std::find_if(
        current.begin(), current.end(),
        [&](const InstallationInfo &candidate) {
            return candidate.Id == instance.Value.Id &&
                candidate.Target == instance.Value.Target &&
                candidate.World == instance.Value.World &&
                candidate.Revision == instance.Value.Revision;
        });
    if (found == current.end())
        return Failure(Error::GraphChanged,
                       "The Behavior Plan instance snapshot is stale.");
    out = static_cast<PatchId>(found->Id);
    return {};
}

Status Installations::ResolvePlanNode(
    const SessionOwner &owner, PlanId id, const PlanInstance &instance,
    const SymbolRef &node, ObjectRef &out) const {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    const PlanRecord *plan = Owned(m_Plans, id, owner);
    if (!plan)
        return StalePlan();
    PatchId patch = 0;
    status = FindPlanInstance(*plan, instance, patch);
    if (status && node.Binding != instance.Binding)
        status = Failure(Error::InvalidGraphLocality,
                         "The Node belongs to another Behavior Plan definition.");
    return status ? ResolveNode(owner, patch, node, out) : status;
}

Status Installations::ReadPlanValue(
    const SessionOwner &owner, PlanId id, const PlanInstance &instance,
    const PortQuery &port, GraphValue &out) const {
    out = {};
    Status status = Ready();
    if (!status)
        return status;
    const PlanRecord *plan = Owned(m_Plans, id, owner);
    if (!plan)
        return StalePlan();
    PatchId patch = 0;
    status = FindPlanInstance(*plan, instance, patch);
    if (status && port.Binding != instance.Binding)
        status = Failure(Error::InvalidGraphLocality,
                         "The Port belongs to another Behavior Plan definition.");
    return status ? ReadValue(owner, patch, port, out) : status;
}

Status Installations::WritePlanValue(
    const SessionOwner &owner, PlanId id, const PlanInstance &instance,
    const PortQuery &port, const Parameter::Binding &value) {
    Status status = Ready();
    if (!status)
        return status;
    const PlanRecord *plan = Owned(m_Plans, id, owner);
    if (!plan)
        return StalePlan();
    PatchId patch = 0;
    status = FindPlanInstance(*plan, instance, patch);
    if (status && port.Binding != instance.Binding)
        status = Failure(Error::InvalidGraphLocality,
                         "The Port belongs to another Behavior Plan definition.");
    return status ? WriteValue(owner, patch, port, value) : status;
}

Status Installations::SetPlanActive(const SessionOwner &owner, PlanId id,
                                    bool active) {
    Status ready = Ready();
    if (!ready)
        return ready;
    PlanRecord *plan = Owned(m_Plans, id, owner);
    if (!plan)
        return StalePlan();
    return SetGoal(*plan, active);
}

Status Installations::ReplacePlan(const SessionOwner &owner, PlanId id,
                                  std::vector<Rule> rules) {
    Status ready = Ready();
    if (!ready)
        return ready;
    Status valid = Validate(rules, true);
    if (!valid)
        return valid;
    PlanRecord *plan = Owned(m_Plans, id, owner);
    if (!plan)
        return StalePlan();
    return Redefine(*plan, std::move(rules));
}

Status Installations::ClosePlan(const SessionOwner &owner, PlanId id) {
    Status requested = RequestClose(true, id, owner);
    if (!requested)
        return requested;
    if (std::this_thread::get_id() != m_Thread)
        return Failure(Error::Busy, "The Behavior Plan is Retiring.", Phase::Teardown);
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end())
        return {};
    if (found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan belongs to another Mod generation.");
    found->second.Goal = InstallGoal::Closed;
    ++found->second.Revision;
    found->second.RestoreFrom = 0;
    // Closing admission is thread-safe, but the rule Selections this Plan
    // retires may be closed only at a CK edit safe point.
    if (!m_Edit.CanPublish()) {
        Status queued = Failure(Error::Busy,
                                "The Behavior Plan is Retiring.",
                                Phase::Teardown);
        found->second.LastStatus = queued;
        return queued;
    }
    Status status = Reconcile(found->second);
    found->second.LastStatus = status;
    if (status)
        m_Plans.erase(found);
    return status;
}

Status Installations::SubmitSelection(const SessionOwner &owner,
                                      ScriptSelection target,
                                      std::string name, Program edit,
                                      SelectionId &out) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || !target || name.empty())
        return Failure(Error::OwnerInvalid,
                       "A Behavior Plan requires an owner, script, and patch name.");
    // A Plan installs into scripts that do not exist yet, so it cannot
    // carry a reference issued against one live world.
    if (edit.UsesIdentity())
        return Failure(
            Error::WorldBoundValue,
            "A Behavior Plan cannot retain a live Object, Node, or "
            "Link; query graph objects by name or Prototype instead.");
    status = edit.Validate();
    if (!status)
        return status;
    try {
        auto body = std::make_shared<Program>(std::move(edit));
        auto world = std::make_shared<PlanWorld>(
            *this, owner, PatchKey{owner.Id, name}, std::move(body),
            SymbolMap{});
        return m_Selections.Submit(
            {owner.Id, std::move(name)}, owner.Generation, std::move(target),
            std::move(world), out);
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Plan definition.");
    }
}

Status Installations::ReadSelection(const SessionOwner &owner,
                                    SelectionId selection,
                                    PlanInfo &out) const {
    return m_Selections.Read(owner.Id, owner.Generation, selection, out);
}

Status Installations::CloseSelection(const SessionOwner &owner,
                                     SelectionId selection) {
    return m_Selections.Close(owner.Id, owner.Generation, selection);
}

Status Installations::Begin(const PatchKey &patch, const ObjectRef &graph,
                            Ops &out, GraphModel &base) {
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

Status Installations::UseNode(Ops &edit, const ObjectRef &node, Node &out) {
    out = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(node) : nullptr;
    CKBehavior *behavior = object ? CKBehavior::Cast(object) : nullptr;
    return behavior
        ? m_Edit.Use(edit, behavior, out)
        : Failure(Error::GraphChanged,
                  "A Behavior Node disappeared during compilation.");
}

Status Installations::UseLink(Ops &edit, const ObjectRef &link, Link &out) {
    out = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(link) : nullptr;
    CKBehaviorLink *native = object ? CKBehaviorLink::Cast(object) : nullptr;
    return native
        ? m_Edit.Use(edit, native, out)
        : Failure(Error::GraphChanged,
                  "A Behavior Link disappeared during compilation.");
}

Status Installations::ReadPatternValue(const GraphNode &node, const Slot &slot,
                                       GraphValue &out) {
    out = {};
    CKObject *object = m_ResolveObject ? m_ResolveObject(node.Object) : nullptr;
    NativeRef reference;
    Status status = object ? m_Graph.Refer(object, reference)
                           : Failure(Error::GraphChanged,
                                     "A Behavior Node disappeared while its Pattern was resolved.");
    return status ? m_Graph.ReadValue(reference, node.LayoutGeneration, slot,
                                      ReadMode::NonForcing, out)
                  : status;
}

Status Installations::Tap(Ops &edit, Port source,
                          const HookBlock::Hook &hook) {
    std::shared_ptr<HookBlock::Binding> binding = hook.Bind();
    if (!binding) {
        return Failure(Error::CallbackFailed,
                       "The Tap callback is no longer available.");
    }
    edit.Tap(std::move(source), std::move(binding));
    return {};
}

Status Installations::Interpose(Ops &edit, Link link,
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

Status Installations::Interpose(Ops &edit, Port source, Port sink,
                                const HookBlock::Hook &hook) {
    std::shared_ptr<HookBlock::Binding> binding = hook.Bind();
    if (!binding) {
        return Failure(Error::CallbackFailed,
                       "The control-flow callback is no longer available.");
    }
    Node block;
    Status status = m_Edit.Add(
        edit, HookBlock::Make(std::move(binding), 1, 1), block,
        NodeRole::Infrastructure);
    if (status) {
        edit.Flow(std::move(source), block.In());
        edit.Flow(block.Out(), std::move(sink));
    }
    return status;
}

Status Installations::RetireOwner(const std::string &ownerId) {
    // A Plan can be waiting on a Patch request that was already queued by an
    // off-thread Close. Request Plan retirement, complete every owned Patch at
    // the CK edit safe point, then collect Plans whose installations closed.
    (void) m_Selections.RetireOwner(ownerId);
    const Status patches = RetireRecords(ownerId);
    const Status plans = m_Selections.RetireOwner(ownerId);
    return plans ? patches : plans;
}

Status Installations::RetireRecords(const std::string &ownerId) {
    Status ready = Ready();
    if (!ready)
        return ready;
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
        Status status = LastStatus(patch);
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

Status Installations::LoadScript(std::string name, ObjectRef script) {
    return m_Selections.LoadScript(std::move(name), std::move(script));
}

void Installations::RemoveObject(std::uint32_t domain, std::uint32_t slot) {
    m_Selections.RemoveObject(domain, slot);
}

void Installations::ObjectsToBeDeleted(const CK_ID *ids, int count) {
    if (!ids || count <= 0 || std::this_thread::get_id() != m_Thread)
        return;
    const std::set<CK_ID> deleting(ids, ids + count);
    for (auto &entry : m_Patches) {
        PatchRecord &patch = entry.second;
        const bool affected = std::any_of(
            patch.Scopes.begin(), patch.Scopes.end(),
            [&](const PatchRecord::Scope &scope) {
                const PatchState state = scope.Value.State();
                // Closing a parent scope destroys graph-backed Nodes created
                // by that Patch. Their nested scope has already been restored
                // in reverse order, so this deletion is part of the journal's
                // own inverse rather than loss of an external target.
                if (state == PatchState::Closed ||
                    state == PatchState::Failed)
                    return false;
                if (deleting.contains(scope.Graph))
                    return true;
                // Native world teardown can delete dynamic Nodes before the
                // graph that owns them. A close already in progress owns those
                // deletions; otherwise the installation has been lost.
                return state != PatchState::Closing &&
                    m_Edit.OwnsAny(scope.Value, deleting);
            }) || std::any_of(
                patch.Requested.begin(), patch.Requested.end(),
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
            const PatchState state = scope.Value.State();
            if (deleting.contains(scope.Graph)) {
                m_Edit.GraphDeleted(scope.Value);
            } else if (state != PatchState::Closing &&
                       state != PatchState::Closed &&
                       state != PatchState::Failed &&
                       m_Edit.OwnsAny(scope.Value, deleting)) {
                m_Edit.InstallationDeleted(scope.Value);
            }
        }
        // Losing one target retires the composed Patch as a whole. Stop Hook
        // admission on every surviving Graph now; only the native inverse is
        // deferred to ProcessFrame.
        CloseAdmission(patch);
        patch.TargetDeleted = true;
    }
    m_Edit.ObjectsToBeDeleted(ids, count);
}

Status Installations::LeaveWorld() {
    return m_Selections.ResetWorld();
}

void Installations::ResetWorld() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    for (auto &[id, patch] : m_Patches)
        (void) Close(patch);
    m_Edit.ProcessFrame();
    Collect();

    // A conflicting external edit may prevent exact restoration. Admission is
    // already closed, and the CK world is about to disappear; do not retain a
    // journal whose native identities belong to the old world.
    m_Patches.clear();
    Collect();
}

Status Installations::ProcessFrame() {
    const Status selections = m_Selections.ProcessFrame();
    if (std::this_thread::get_id() != m_Thread)
        return selections;
    const bool editPending = m_Edit.NeedsFrameProcessing();
    if (m_Plans.empty() && m_Patches.empty() && !editPending) {
        if (m_HasAdmissions)
            Collect();
        return selections;
    }
    if (editPending)
        m_Edit.ProcessFrame();

    bool needsCollection = editPending;
    for (auto plan = m_Plans.begin(); plan != m_Plans.end();) {
        if (HasPendingChange(plan->second)) {
            Status status = Reconcile(plan->second);
            plan->second.LastStatus = status;
        }
        if (plan->second.Goal == InstallGoal::Closed &&
            plan->second.Selections.empty()) {
            plan = m_Plans.erase(plan);
            needsCollection = true;
            continue;
        }
        ++plan;
    }
    for (auto &entry : m_Patches) {
        PatchRecord &patch = entry.second;
        if (HasPendingChange(patch) && m_Edit.CanPublish()) {
            (void) Reconcile(patch);
            needsCollection = true;
        }
        if (patch.Goal == InstallGoal::Closed)
            needsCollection = true;
    }
    if (needsCollection)
        Collect();
    return selections;
}

void Installations::Collect() {
    for (auto patch = m_Patches.begin(); patch != m_Patches.end();) {
        const PatchState state = State(patch->second);
        if (patch->second.Goal == InstallGoal::Closed &&
            (state == PatchState::Closed || state == PatchState::Failed))
            patch = m_Patches.erase(patch);
        else
            ++patch;
    }
    if (m_HasAdmissions) {
        std::lock_guard<std::mutex> lock(m_AdmissionMutex);
        for (auto record = m_Admissions.begin(); record != m_Admissions.end();) {
            if (record->second.Admission.expired())
                record = m_Admissions.erase(record);
            else
                ++record;
        }
        m_HasAdmissions = !m_Admissions.empty();
    }
}

} // namespace BML::Behavior::Internal
