#ifndef BML_BEHAVIOR_PLAN_H
#define BML_BEHAVIOR_PLAN_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "Behavior/Topology.h"

namespace BML::Behavior {

using Epoch = std::uint64_t;
using Installation = std::uint64_t;

enum class TargetSet {
    Each,
    One,
};

// A durable script selection is evaluated against every root script currently
// known to the Loader. Name matching is exact and every matching instance has
// its own world-scoped installation unless One is requested.
struct Script {
    std::string Name;
    TargetSet Instances = TargetSet::Each;

    [[nodiscard]] explicit operator bool() const noexcept {
        return !Name.empty();
    }

    friend bool operator==(const Script &, const Script &) = default;
};

enum class PlanState {
    Reconciling,
    Active,
    Unsatisfied,
    Conflicted,
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

    Plan(PatchKey patch, Script target);

    Status Reconcile(std::vector<ObjectRef> targets, Epoch epoch, World &world);
    Status LeaveWorld(World &world);
    Status Retire(World &world);

    [[nodiscard]] const PatchKey &Key() const noexcept { return m_Patch; }
    [[nodiscard]] const Script &Target() const noexcept { return m_Target; }
    [[nodiscard]] PlanState State() const noexcept { return m_State; }
    [[nodiscard]] Epoch WorldEpoch() const noexcept { return m_Epoch; }
    [[nodiscard]] std::size_t Size() const noexcept { return m_Installed.size(); }
    [[nodiscard]] bool Contains(const ObjectRef &target) const noexcept;
    [[nodiscard]] bool Retiring() const noexcept { return m_Retiring; }

private:
    struct RefLess {
        bool operator()(const ObjectRef &left,
                        const ObjectRef &right) const noexcept;
    };

    Status CloseAll(World &world);

    PatchKey m_Patch;
    Script m_Target;
    // A Plan the Loader has accepted but never reconciled is Reconciling, not
    // Unsatisfied. Unsatisfied means a pass ran and found no usable target.
    PlanState m_State = PlanState::Reconciling;
    Epoch m_Epoch = 0;
    bool m_Retiring = false;
    std::map<ObjectRef, Installation, RefLess> m_Installed;
};

using PlanId = std::uint64_t;

struct PlanInfo {
    PlanState State = PlanState::Unsatisfied;
    Epoch World = 0;
    std::size_t Matches = 0;
    std::size_t Installations = 0;
    Status Diagnostic;
};

// Loader-owned durable Behavior intent. Script load/unload events only change
// the known target set; native reconciliation happens at ProcessFrame. World
// implementations and their canonical Edit data are owned by the Loader, not
// by a Mod callback or a live CK object.
class Plans final {
public:
    Plans() = default;

    Status Submit(PatchKey patch, std::uint64_t ownerGeneration,
                  Script target,
                  std::shared_ptr<Plan::World> world, PlanId &out);
    Status Read(PlanId id, PlanInfo &out) const;
    Status Read(std::string_view owner, std::uint64_t ownerGeneration,
                PlanId id, PlanInfo &out) const;
    Status Close(PlanId id);
    Status Close(std::string_view owner, std::uint64_t ownerGeneration,
                 PlanId id);
    Status RetireOwner(std::string_view owner);

    Status LoadScript(std::string name, ObjectRef script);
    void Remove(ObjectRef script);
    void Remove(const std::vector<ObjectRef> &scripts);
    void RemoveObject(std::uint32_t domain, std::uint32_t slot);
    Status ResetWorld();
    Status ProcessFrame();

    [[nodiscard]] Epoch WorldEpoch() const;
    [[nodiscard]] std::size_t Size() const;

private:
    struct Record {
        PlanId Id = 0;
        std::uint64_t OwnerGeneration = 0;
        std::shared_ptr<Plan::World> World;
        Plan Value;
        std::size_t Matches = 0;
        Status Diagnostic;
        bool Dirty = true;

        Record(PlanId id, std::uint64_t ownerGeneration,
               std::shared_ptr<Plan::World> world,
               PatchKey patch, Script target)
            : Id(id), OwnerGeneration(ownerGeneration),
              World(std::move(world)),
              Value(std::move(patch), std::move(target)) {}
    };

    struct RefLess {
        bool operator()(const ObjectRef &left,
                        const ObjectRef &right) const noexcept;
    };

    [[nodiscard]] PlanId NextId() noexcept;
    void Mark(std::string_view name) noexcept;

    Epoch m_Epoch = 1;
    PlanId m_NextId = 1;
    mutable std::recursive_mutex m_Mutex;
    std::map<PlanId, std::unique_ptr<Record>> m_Plans;
    std::map<PatchKey, PlanId> m_Keys;
    std::map<ObjectRef, std::string, RefLess> m_Scripts;
};

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_PLAN_H
