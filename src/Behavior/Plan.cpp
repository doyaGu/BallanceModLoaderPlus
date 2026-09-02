#include "Behavior/Plan.h"

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

namespace BML::Behavior {
namespace {

Status Failure(Error error, std::string message) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = Phase::Edit;
    return status;
}

} // namespace

Plan::Plan(PatchKey patch, Script target)
    : m_Patch(std::move(patch)), m_Target(std::move(target)) {}

bool Plan::RefLess::operator()(const ObjectRef &left,
                               const ObjectRef &right) const noexcept {
    return std::tie(left.Domain, left.Slot, left.Generation) <
           std::tie(right.Domain, right.Slot, right.Generation);
}

bool Plan::Contains(const ObjectRef &target) const noexcept {
    return m_Installed.contains(target);
}

Status Plan::CloseAll(World &world) {
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
    }
    if (!first) {
        m_State = m_Retiring && first.Code == Error::Busy
            ? PlanState::Retiring : PlanState::Conflicted;
        return first;
    }
    return {};
}

Status Plan::Reconcile(std::vector<ObjectRef> targets, Epoch epoch,
                       World &world) {
    if (m_Retiring)
        return Failure(Error::InvalidState,
                       "A retiring Behavior Plan cannot accept a target set.");
    if (m_Patch.Owner.empty() || m_Patch.Name.empty() || !m_Target ||
        epoch == 0)
        return Failure(Error::InvalidState,
                       "A Behavior Plan requires an owner, key, script, and world epoch.");
    m_State = PlanState::Reconciling;

    std::sort(targets.begin(), targets.end(), RefLess{});
    if (std::any_of(targets.begin(), targets.end(),
                    [](const ObjectRef &target) { return target.IsNull(); })) {
        m_State = PlanState::Unsatisfied;
        return Failure(Error::TargetInvalid,
                       "A Behavior Plan target is null.");
    }
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

    if (m_Epoch != 0 && m_Epoch != epoch) {
        Status status = CloseAll(world);
        if (!status)
            return status;
    }
    m_Epoch = epoch;

    // Only an ambiguous target set is a cardinality failure. A world with no
    // matching script yet, or one whose only script was just deleted, leaves
    // the Plan Unsatisfied below and waits for the next epoch.
    if (m_Target.Instances == TargetSet::One && targets.size() > 1) {
        Status status = CloseAll(world);
        if (!status)
            return status;
        m_State = PlanState::Unsatisfied;
        return Failure(Error::TargetCardinality,
                       "This Behavior Plan matched more than one live target.");
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
    }
    if (!closeFailure) {
        m_State = PlanState::Conflicted;
        return closeFailure;
    }

    if (targets.empty()) {
        m_State = PlanState::Unsatisfied;
        return {};
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
            }
            if (!rollbackFailure) {
                if (!status.Message.empty())
                    rollbackFailure.Message = status.Message + " " +
                                              rollbackFailure.Message;
                m_State = PlanState::Conflicted;
                return rollbackFailure;
            }
            m_State = PlanState::Unsatisfied;
            return status;
        }
        m_Installed.emplace(target, installation);
        added.push_back(target);
    }

    m_State = PlanState::Active;
    return {};
}

Status Plan::LeaveWorld(World &world) {
    Status status = CloseAll(world);
    // The old world is no longer a revert target. Even when an inverse cannot
    // be applied, no Installation identity may cross the epoch boundary.
    m_Installed.clear();
    m_Epoch = 0;
    m_State = m_Retiring ? PlanState::Retiring : PlanState::Unsatisfied;
    return status;
}

Status Plan::Retire(World &world) {
    m_Retiring = true;
    m_State = PlanState::Retiring;
    Status status = CloseAll(world);
    if (!status)
        return status;
    m_State = PlanState::Retiring;
    return {};
}

bool Plans::RefLess::operator()(const ObjectRef &left,
                                const ObjectRef &right) const noexcept {
    return std::tie(left.Domain, left.Slot, left.Generation) <
           std::tie(right.Domain, right.Slot, right.Generation);
}

PlanId Plans::NextId() noexcept {
    if (m_NextId == 0 ||
        m_NextId == (std::numeric_limits<PlanId>::max)())
        return 0;
    return m_NextId++;
}

void Plans::Mark(std::string_view name) noexcept {
    for (auto &[id, record] : m_Plans) {
        if (record->Value.Target().Name == name)
            record->Dirty = true;
    }
}

Status Plans::Submit(PatchKey patch, std::uint64_t ownerGeneration,
                     Script target,
                     std::shared_ptr<Plan::World> world, PlanId &out) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    out = 0;
    if (patch.Owner.empty() || patch.Name.empty() || ownerGeneration == 0 ||
        !target || !world)
        return Failure(Error::InvalidState,
                       "A durable Behavior Plan requires an owner, key, script, and world.");

    const PlanId id = NextId();
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
        const PlanId previous = current->second;
        Status status = m_Plans.at(previous)->Value.Retire(
            *m_Plans.at(previous)->World);
        m_Plans.at(previous)->Diagnostic = status;
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

Status Plans::Read(PlanId id, PlanInfo &out) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end())
        return Failure(Error::InvalidState,
                       "The Behavior Plan handle is stale.");
    out.State = found->second->Value.State();
    out.World = found->second->Value.WorldEpoch();
    out.Matches = found->second->Matches;
    out.Installations = found->second->Value.Size();
    out.Diagnostic = found->second->Diagnostic;
    return {};
}

Status Plans::Read(std::string_view owner, std::uint64_t ownerGeneration,
                   PlanId id, PlanInfo &out) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end() ||
        found->second->Value.Key().Owner != owner ||
        found->second->OwnerGeneration != ownerGeneration) {
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan belongs to another Mod generation.");
    }
    return Read(id, out);
}

Status Plans::Close(PlanId id) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end())
        return {};
    Status status = found->second->Value.Retire(*found->second->World);
    found->second->Diagnostic = status;
    if (!status)
        return status;
    m_Keys.erase(found->second->Value.Key());
    m_Plans.erase(found);
    return {};
}

Status Plans::Close(std::string_view owner, std::uint64_t ownerGeneration,
                    PlanId id) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Plans.find(id);
    if (found == m_Plans.end())
        return {};
    if (found->second->Value.Key().Owner != owner ||
        found->second->OwnerGeneration != ownerGeneration)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Plan belongs to another Mod generation.");
    return Close(id);
}

Status Plans::RetireOwner(std::string_view owner) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status first;
    for (auto item = m_Plans.begin(); item != m_Plans.end();) {
        if (item->second->Value.Key().Owner != owner) {
            ++item;
            continue;
        }
        Status status = item->second->Value.Retire(*item->second->World);
        item->second->Diagnostic = status;
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

Status Plans::LoadScript(std::string name, ObjectRef script) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
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

void Plans::Remove(ObjectRef script) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Scripts.find(script);
    if (found == m_Scripts.end())
        return;
    const std::string name = found->second;
    m_Scripts.erase(found);
    Mark(name);
}

void Plans::Remove(const std::vector<ObjectRef> &scripts) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (const ObjectRef &script : scripts)
        Remove(script);
}

void Plans::RemoveObject(std::uint32_t domain, std::uint32_t slot) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
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

Status Plans::ResetWorld() {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status first;
    for (auto &[id, record] : m_Plans) {
        Status status = record->Value.LeaveWorld(*record->World);
        record->Diagnostic = status;
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

Status Plans::ProcessFrame() {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status first;
    for (auto item = m_Plans.begin(); item != m_Plans.end();) {
        Record &record = *item->second;
        if (record.Value.Retiring()) {
            Status status = record.Value.Retire(*record.World);
            record.Diagnostic = status;
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
            record.Diagnostic = status;
            if (first)
                first = status;
            ++item;
            continue;
        }

        record.Matches = targets.size();
        Status status = record.Value.Reconcile(
            std::move(targets), m_Epoch, *record.World);
        record.Diagnostic = status;
        record.Dirty = false;
        if (!status && first)
            first = status;
        ++item;
    }
    return first;
}

Epoch Plans::WorldEpoch() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_Epoch;
}

std::size_t Plans::Size() const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_Plans.size();
}

} // namespace BML::Behavior
