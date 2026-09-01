#ifndef BML_BEHAVIOR_PLAN_H
#define BML_BEHAVIOR_PLAN_H

#include <cstdint>
#include <map>
#include <vector>

#include "Behavior/Topology.h"

namespace BML::Behavior {

using Epoch = std::uint64_t;
using Installation = std::uint64_t;

enum class TargetSet {
    Each,
    One,
};

enum class PlanState {
    Reconciling,
    Active,
    Unsatisfied,
    RepairRequired,
    Retiring,
};

// A durable owner plan has no retained author callback. Its World resolves a
// canonical Edit for each target and owns the native Installation it returns.
class Plan final {
public:
    class World {
    public:
        virtual ~World() = default;
        virtual Status Install(const PatchKey &patch, const ObjectRef &target,
                               Epoch epoch, Installation &out) = 0;
        virtual Status Close(Installation installation) = 0;
    };

    Plan(PatchKey patch, TargetSet targets = TargetSet::Each);

    Status Reconcile(std::vector<ObjectRef> targets, Epoch epoch, World &world);
    Status Retire(World &world);

    [[nodiscard]] const PatchKey &Key() const noexcept { return m_Patch; }
    [[nodiscard]] PlanState State() const noexcept { return m_State; }
    [[nodiscard]] Epoch WorldEpoch() const noexcept { return m_Epoch; }
    [[nodiscard]] std::size_t Size() const noexcept { return m_Installed.size(); }
    [[nodiscard]] bool Contains(const ObjectRef &target) const noexcept;

private:
    struct RefLess {
        bool operator()(const ObjectRef &left,
                        const ObjectRef &right) const noexcept;
    };

    Status CloseAll(World &world);

    PatchKey m_Patch;
    TargetSet m_Targets = TargetSet::Each;
    PlanState m_State = PlanState::Unsatisfied;
    Epoch m_Epoch = 0;
    bool m_Retiring = false;
    std::map<ObjectRef, Installation, RefLess> m_Installed;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_PLAN_H
