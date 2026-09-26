#include "Behavior/Install/Selection.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

namespace BML::Behavior::Internal {
namespace {

Status Failure(Error error, std::string message) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = Phase::Edit;
    return status;
}

class WorldCall final {
public:
    explicit WorldCall(bool &active) : m_Active(active) {
        m_Active = true;
    }

    ~WorldCall() { m_Active = false; }

    WorldCall(const WorldCall &) = delete;
    WorldCall &operator=(const WorldCall &) = delete;

private:
    bool &m_Active;
};

} // namespace

Selection::Selection(PatchKey patch, ScriptSelection target)
    : m_Patch(std::move(patch)), m_Target(std::move(target)) {}

bool Selection::RefLess::operator()(const ObjectRef &left,
                               const ObjectRef &right) const noexcept {
    return std::tie(left.Domain, left.Slot, left.Generation) <
           std::tie(right.Domain, right.Slot, right.Generation);
}

bool Selection::Contains(const ObjectRef &target) const noexcept {
    return m_Installed.contains(target);
}

std::vector<InstallationInfo> Selection::Installations() const {
    std::vector<InstallationInfo> result;
    result.reserve(m_Installed.size());
    for (const auto &[target, installation] : m_Installed)
        result.push_back({target, installation, m_Epoch, m_Revision});
    return result;
}

void Selection::Touch() noexcept {
    m_Revision = m_Revision == (std::numeric_limits<std::uint64_t>::max)()
        ? 1 : m_Revision + 1;
}

Status Selection::Applied(Status status) {
    m_LastStatus = status;
    if (!status && m_ApplyFailure)
        m_ApplyFailure = status;
    return status;
}

Status Selection::Restored(Status status) {
    m_LastStatus = status;
    m_RestoreFailure = status;
    return status;
}

Status Selection::Settled() {
    m_LastStatus = {};
    m_ApplyFailure = {};
    m_RestoreFailure = {};
    return {};
}

Status Selection::CloseAll(World &world) {
    Status first;
    for (auto item = m_Installed.begin(); item != m_Installed.end();) {
        Status status = world.Close(item->second);
        if (!status) {
            if (first ||
                (first.Code == Error::Busy && status.Code != Error::Busy))
                first = std::move(status);
            ++item;
            continue;
        }
        item = m_Installed.erase(item);
        Touch();
    }
    if (!first) {
        m_State = m_Retiring && first.Code == Error::Busy
            ? PlanState::Retiring : PlanState::Conflicted;
        return first;
    }
    return {};
}

Status Selection::Reconcile(std::vector<ObjectRef> targets, Epoch epoch,
                       World &world) {
    // A changed Script set begins a new reconciliation attempt. Keep an apply
    // failure separate from a rollback or removal that cannot be restored.
    m_LastStatus = {};
    m_ApplyFailure = {};
    m_RestoreFailure = {};
    if (m_Retiring)
        return Applied(Failure(
            Error::InvalidState,
            "A retiring Behavior Plan cannot accept a target set."));
    if (m_Patch.Owner.empty() || m_Patch.Name.empty() || !m_Target ||
        epoch == 0)
        return Applied(Failure(
            Error::InvalidState,
            "A Behavior Plan requires an owner, key, script, and world epoch."));
    m_State = PlanState::Reconciling;

    std::sort(targets.begin(), targets.end(), RefLess{});
    if (std::any_of(targets.begin(), targets.end(),
                    [](const ObjectRef &target) { return target.IsNull(); })) {
        m_State = PlanState::Unsatisfied;
        return Applied(Failure(Error::TargetInvalid,
                               "A Behavior Plan target is null."));
    }
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

    if (m_Epoch != 0 && m_Epoch != epoch) {
        Status status = CloseAll(world);
        if (!status)
            return Restored(std::move(status));
    }
    if (m_Epoch != epoch) {
        m_Epoch = epoch;
        Touch();
    }

    // Only an ambiguous target set is a cardinality failure. A world with no
    // matching script yet, or one whose only script was just deleted, leaves
    // the Selection Unsatisfied below and waits for the next epoch.
    if (m_Target.Instances == TargetSet::One && targets.size() > 1) {
        Status status = CloseAll(world);
        if (!status)
            return Restored(std::move(status));
        m_State = PlanState::Unsatisfied;
        return Applied(Failure(
            Error::TargetCardinality,
            "This Behavior Plan matched more than one live target."));
    }

    Status closeFailure;
    for (auto item = m_Installed.begin(); item != m_Installed.end();) {
        if (std::binary_search(targets.begin(), targets.end(), item->first,
                               RefLess{})) {
            ++item;
            continue;
        }
        Status status = world.Close(item->second);
        if (!status) {
            if (closeFailure)
                closeFailure = std::move(status);
            ++item;
            continue;
        }
        item = m_Installed.erase(item);
        Touch();
    }
    if (!closeFailure) {
        m_State = PlanState::Conflicted;
        return Restored(std::move(closeFailure));
    }

    if (targets.empty()) {
        m_State = PlanState::Unsatisfied;
        return Settled();
    }

    std::vector<ObjectRef> added;
    for (const ObjectRef &target : targets) {
        if (m_Installed.contains(target))
            continue;
        Installation installation = 0;
        Status status = world.Install(m_Patch, target, epoch, installation);
        if (!status || installation == 0) {
            if (status && installation == 0)
                status = Failure(Error::InvalidState,
                                  "A Behavior installation has no identity.");
            (void) Applied(status);
            Status rollbackFailure;
            for (auto item = added.rbegin(); item != added.rend(); ++item) {
                const auto installed = m_Installed.find(*item);
                if (installed == m_Installed.end())
                    continue;
                Status closed = world.Close(installed->second);
                if (!closed) {
                    if (rollbackFailure)
                        rollbackFailure = std::move(closed);
                    continue;
                }
                m_Installed.erase(installed);
                Touch();
            }
            if (!rollbackFailure) {
                m_State = PlanState::Conflicted;
                return Restored(std::move(rollbackFailure));
            }
            m_State = m_Installed.empty()
                ? PlanState::Unsatisfied : PlanState::Partial;
            return status;
        }
        m_Installed.emplace(target, installation);
        Touch();
        added.push_back(target);
    }

    m_State = PlanState::Active;
    return Settled();
}

Status Selection::LeaveWorld(World &world) {
    const bool hadWorld = m_Epoch != 0;
    Status status = CloseAll(world);
    // The old world is no longer a revert target. Even when an inverse cannot
    // be applied, no Installation identity may cross the epoch boundary.
    m_Installed.clear();
    m_Epoch = 0;
    if (hadWorld)
        Touch();
    m_State = m_Retiring ? PlanState::Retiring : PlanState::Unsatisfied;
    m_ApplyFailure = {};
    return status ? Settled() : Restored(std::move(status));
}

Status Selection::Retire(World &world) {
    m_Retiring = true;
    m_State = PlanState::Retiring;
    Status status = CloseAll(world);
    if (!status)
        return Restored(std::move(status));
    m_State = PlanState::Retiring;
    return Settled();
}

bool Selections::RefLess::operator()(const ObjectRef &left,
                                const ObjectRef &right) const noexcept {
    return std::tie(left.Domain, left.Slot, left.Generation) <
           std::tie(right.Domain, right.Slot, right.Generation);
}

SelectionId Selections::NextId() noexcept {
    if (m_NextId == 0 ||
        m_NextId == (std::numeric_limits<SelectionId>::max)())
        return 0;
    return m_NextId++;
}

Status Selections::Ready() const {
    return std::this_thread::get_id() == m_Thread
        ? Status{}
        : Failure(Error::WrongThread,
                  "Behavior Plans require the game thread.");
}

void Selections::Mark(std::string_view name) noexcept {
    for (auto &[id, record] : m_Plans) {
        if (record->Value.Target().Name == name)
            record->Dirty = true;
    }
}

Status Selections::Submit(PatchKey patch, std::uint64_t ownerGeneration,
                     ScriptSelection target,
                     std::shared_ptr<Selection::World> world, SelectionId &out) {
    out = 0;
    Status ready = Ready();
    if (!ready)
        return ready;
    if (m_InWorld)
        return Failure(Error::Busy,
                       "Behavior Plan reconciliation is already in progress.");
    if (patch.Owner.empty() || patch.Name.empty() || ownerGeneration == 0 ||
        !target || !world)
        return Failure(
            Error::InvalidState,
            "A Behavior Plan requires an owner, key, script, and world.");

    const SelectionId id = NextId();
    if (!id)
        return Failure(Error::InvalidState,
                       "Behavior Plan ids are exhausted.");

    try {
        auto record = std::make_unique<Record>(
            id, ownerGeneration, std::move(world), patch,
            std::move(target));
        const auto [stored, inserted] =
            m_Plans.emplace(id, std::move(record));
        if (!inserted)
            return Failure(Error::InvalidState,
                           "Behavior Plan id collision.");
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Plan.");
    }

    const auto current = m_Keys.find(patch);
    if (current != m_Keys.end()) {
        const SelectionId previous = current->second;
        Status status;
        {
            WorldCall call(m_InWorld);
            status = m_Plans.at(previous)->Value.Retire(
                *m_Plans.at(previous)->World);
        }
        if (!status) {
            m_Plans.erase(id);
            return status;
        }
        current->second = id;
        m_Plans.erase(previous);
    } else {
        try {
            m_Keys.emplace(std::move(patch), id);
        } catch (...) {
            m_Plans.erase(id);
            return Failure(Error::CreateFailed,
                           "The Loader could not retain the Behavior Plan key.");
        }
    }
    out = id;
    return {};
}

Status Selections::Read(SelectionId id, PlanInfo &out) const {
    Status ready = Ready();
    if (!ready)
        return ready;
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end())
        return Failure(Error::InvalidState,
                       "The Behavior Plan handle is stale.");
    out.State = found->second->CloseRequested &&
            found->second->Value.State() != PlanState::Conflicted
        ? PlanState::Retiring : found->second->Value.State();
    out.World = found->second->Value.WorldEpoch();
    out.Matches = found->second->Matches;
    out.Instances = found->second->Value.Size();
    out.LastStatus = found->second->Value.LastStatus();
    out.ApplyFailure = found->second->Value.ApplyFailure();
    out.RestoreFailure = found->second->Value.RestoreFailure();
    return {};
}

Status Selections::Read(std::string_view owner, std::uint64_t ownerGeneration,
                   SelectionId id, PlanInfo &out) const {
    Status ready = Ready();
    if (!ready)
        return ready;
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end() ||
        found->second->Value.Key().Owner != owner ||
        found->second->OwnerGeneration != ownerGeneration) {
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan belongs to another Mod generation.");
    }
    out.State = found->second->CloseRequested &&
            found->second->Value.State() != PlanState::Conflicted
        ? PlanState::Retiring : found->second->Value.State();
    out.World = found->second->Value.WorldEpoch();
    out.Matches = found->second->Matches;
    out.Instances = found->second->Value.Size();
    out.LastStatus = found->second->Value.LastStatus();
    out.ApplyFailure = found->second->Value.ApplyFailure();
    out.RestoreFailure = found->second->Value.RestoreFailure();
    return {};
}

Status Selections::ReadInstallations(
    std::string_view owner, std::uint64_t ownerGeneration, SelectionId id,
    std::vector<InstallationInfo> &out) const {
    out.clear();
    Status ready = Ready();
    if (!ready)
        return ready;
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end() ||
        found->second->Value.Key().Owner != owner ||
        found->second->OwnerGeneration != ownerGeneration) {
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan belongs to another Mod generation.");
    }
    try {
        out = found->second->Value.Installations();
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not snapshot Behavior Plan installations.");
    }
    return {};
}

Status Selections::Close(SelectionId id) {
    Status ready = Ready();
    if (!ready)
        return ready;
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end())
        return {};
    found->second->CloseRequested = true;
    if (m_InWorld)
        return Failure(Error::Busy, "The Behavior Plan is Retiring.");
    Status status;
    {
        WorldCall call(m_InWorld);
        status = found->second->Value.Retire(*found->second->World);
    }
    if (!status)
        return status;
    m_Keys.erase(found->second->Value.Key());
    m_Plans.erase(found);
    return {};
}

Status Selections::Close(std::string_view owner, std::uint64_t ownerGeneration,
                    SelectionId id) {
    Status ready = Ready();
    if (!ready)
        return ready;
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end())
        return {};
    if (found->second->Value.Key().Owner != owner ||
        found->second->OwnerGeneration != ownerGeneration)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan belongs to another Mod generation.");
    found->second->CloseRequested = true;
    if (m_InWorld)
        return Failure(Error::Busy, "The Behavior Plan is Retiring.");
    Status status;
    {
        WorldCall call(m_InWorld);
        status = found->second->Value.Retire(*found->second->World);
    }
    if (!status)
        return status;
    m_Keys.erase(found->second->Value.Key());
    m_Plans.erase(found);
    return {};
}

Status Selections::Retry(SelectionId id) {
    Status ready = Ready();
    if (!ready)
        return ready;
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end())
        return Failure(Error::InvalidState,
                       "The Behavior Plan handle is stale.");
    if (found->second->CloseRequested || found->second->Value.Retiring())
        return Failure(Error::InvalidState,
                       "A retiring Behavior Plan cannot be reconciled.");
    found->second->Value.m_State = PlanState::Reconciling;
    found->second->Value.m_LastStatus = {};
    found->second->Value.m_ApplyFailure = {};
    found->second->Value.m_RestoreFailure = {};
    found->second->Dirty = true;
    return {};
}

Status Selections::RetireOwner(std::string_view owner) {
    Status ready = Ready();
    if (!ready)
        return ready;
    if (m_InWorld) {
        for (auto &[id, record] : m_Plans) {
            if (record->Value.Key().Owner == owner)
                record->CloseRequested = true;
        }
        return Failure(Error::Busy, "Behavior Plans are Retiring.");
    }
    Status first;
    for (auto item = m_Plans.begin(); item != m_Plans.end();) {
        if (item->second->Value.Key().Owner != owner) {
            ++item;
            continue;
        }
        item->second->CloseRequested = true;
        Status status;
        {
            WorldCall call(m_InWorld);
            status = item->second->Value.Retire(*item->second->World);
        }
        if (!status) {
            if (first)
                first = status;
            ++item;
            continue;
        }
        m_Keys.erase(item->second->Value.Key());
        item = m_Plans.erase(item);
    }
    return first;
}

Status Selections::LoadScript(std::string name, ObjectRef script) {
    Status ready = Ready();
    if (!ready)
        return ready;
    if (name.empty() || script.IsNull())
        return Failure(Error::TargetInvalid,
                       "A live script requires an exact name and object reference.");
    try {
        const auto current = m_Scripts.find(script);
        if (current != m_Scripts.end()) {
            if (current->second == name)
                return {};
            const std::string previous = current->second;
            current->second = std::move(name);
            Mark(previous);
            Mark(current->second);
            return {};
        }
        const auto [stored, inserted] =
            m_Scripts.emplace(script, std::move(name));
        if (inserted)
            Mark(stored->second);
    } catch (...) {
        return Failure(Error::CreateFailed,
                       "The Loader could not retain a live script identity.");
    }
    return {};
}

void Selections::Remove(ObjectRef script) {
    if (!Ready())
        return;
    const auto found = m_Scripts.find(script);
    if (found == m_Scripts.end())
        return;
    const std::string name = found->second;
    m_Scripts.erase(found);
    Mark(name);
}

void Selections::Remove(const std::vector<ObjectRef> &scripts) {
    if (!Ready())
        return;
    for (const ObjectRef &script : scripts) {
        const auto found = m_Scripts.find(script);
        if (found == m_Scripts.end())
            continue;
        const std::string name = found->second;
        m_Scripts.erase(found);
        Mark(name);
    }
}

void Selections::RemoveObject(std::uint32_t domain, std::uint32_t slot) {
    if (!Ready())
        return;
    for (auto script = m_Scripts.begin(); script != m_Scripts.end();) {
        if (script->first.Domain != domain || script->first.Slot != slot) {
            ++script;
            continue;
        }
        const std::string name = script->second;
        script = m_Scripts.erase(script);
        Mark(name);
    }
}

Status Selections::ResetWorld() {
    Status ready = Ready();
    if (!ready)
        return ready;
    if (m_InWorld)
        return Failure(Error::Busy,
                       "The Behavior world is already being reconciled.");
    Status first;
    WorldCall call(m_InWorld);
    for (auto &[id, record] : m_Plans) {
        Status status = record->Value.LeaveWorld(*record->World);
        record->Matches = 0;
        record->Dirty = false;
        if (!status && first)
            first = status;
    }
    m_Scripts.clear();
    m_Epoch = m_Epoch == (std::numeric_limits<Epoch>::max)()
        ? 1 : m_Epoch + 1;
    return first;
}

Status Selections::ProcessFrame() {
    Status ready = Ready();
    if (!ready)
        return ready;
    if (m_InWorld)
        return Failure(Error::Busy,
                       "Behavior Plan reconciliation is already in progress.");
    Status first;
    for (auto item = m_Plans.begin(); item != m_Plans.end();) {
        Record &record = *item->second;
        if (record.CloseRequested || record.Value.Retiring()) {
            Status status;
            {
                WorldCall call(m_InWorld);
                status = record.Value.Retire(*record.World);
            }
            if (status) {
                m_Keys.erase(record.Value.Key());
                item = m_Plans.erase(item);
                continue;
            }
            ++item;
            continue;
        }
        if (!record.Dirty) {
            ++item;
            continue;
        }
        std::vector<ObjectRef> targets;
        try {
            for (const auto &[script, name] : m_Scripts) {
                if (name == record.Value.Target().Name)
                    targets.push_back(script);
            }
        } catch (...) {
            Status status = Failure(
                Error::CreateFailed,
                "The Loader could not form the Behavior Plan target set.");
            (void) record.Value.Applied(status);
            if (first)
                first = status;
            ++item;
            continue;
        }

        record.Matches = targets.size();
        // Clear the consumed notification before entering the world. A Script
        // load/remove nested in a provider callback marks it again for the next
        // safe point instead of being overwritten when this pass returns.
        record.Dirty = false;
        Status status;
        {
            WorldCall call(m_InWorld);
            status = record.Value.Reconcile(
                std::move(targets), m_Epoch, *record.World);
        }
        if (!status &&
            record.Value.RestoreFailure().Code == Error::Busy)
            record.Dirty = true;
        if (!status && first)
            first = status;
        ++item;
    }
    return first;
}

Epoch Selections::WorldEpoch() const {
    return m_Epoch;
}

std::size_t Selections::Size() const {
    return m_Plans.size();
}

} // namespace BML::Behavior::Internal
