#include "Behavior/Plan.h"

#include <algorithm>
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

Plan::Plan(PatchKey patch, TargetSet targets)
    : m_Patch(std::move(patch)), m_Targets(targets) {}

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
            if (first)
                first = std::move(status);
            ++item;
            continue;
        }
        item = m_Installed.erase(item);
    }
    if (!first) {
        m_State = PlanState::RepairRequired;
        return first;
    }
    return {};
}

Status Plan::Reconcile(std::vector<ObjectRef> targets, Epoch epoch,
                       World &world) {
    if (m_Retiring)
        return Failure(Error::InvalidState,
                       "A retiring Behavior Plan cannot accept a target set.");
    if (m_Patch.Owner.empty() || m_Patch.Name.empty() || epoch == 0)
        return Failure(Error::InvalidState,
                       "A Behavior Plan requires an owner, key, and world epoch.");
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

    if (m_Targets == TargetSet::One && targets.size() != 1) {
        Status status = CloseAll(world);
        if (!status)
            return status;
        m_State = PlanState::Unsatisfied;
        return Failure(Error::TargetCardinality,
                       "This Behavior Plan requires exactly one live target.");
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
        m_State = PlanState::RepairRequired;
        return closeFailure;
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
                m_State = PlanState::RepairRequired;
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

Status Plan::Retire(World &world) {
    m_Retiring = true;
    m_State = PlanState::Retiring;
    Status status = CloseAll(world);
    if (!status)
        return status;
    m_State = PlanState::Retiring;
    return {};
}

} // namespace BML::Behavior
