#ifndef BML_BEHAVIOR_PLAN_H
#define BML_BEHAVIOR_PLAN_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "Behavior/Topology.h"

namespace BML::Behavior::Internal {

using Epoch = std::uint64_t;
using Installation = std::uint64_t;

enum class TargetSet {
    Each,
    One,
};

// A Script selection is evaluated against every root Script currently known to
// the Loader. Name matching is exact and every matching instance has
// its own world-scoped installation unless One is requested.
struct ScriptSelection {
    std::string Name;
    TargetSet Instances = TargetSet::Each;

    [[nodiscard]] explicit operator bool() const noexcept {
        return !Name.empty();
    }

    friend bool operator==(const ScriptSelection &,
                           const ScriptSelection &) = default;
};

enum class PlanState {
    Reconciling,
    Active,
    Partial,
    Unsatisfied,
    Disabled,
    Conflicted,
    Retiring,
};

// A Plan has no retained author callback. Its World resolves a canonical Edit
// for each target and owns the native Installation it returns.
class Plan final {
public:
    class World {
    public:
        virtual ~World() = default;
        virtual Status Install(const PatchKey &patch, const ObjectRef &target,
                               Epoch epoch, Installation &out) = 0;
        virtual Status Close(Installation installation) = 0;
    };

    Plan(PatchKey patch, ScriptSelection target);

    Status Reconcile(std::vector<ObjectRef> targets, Epoch epoch, World &world);
    Status LeaveWorld(World &world);
    Status Retire(World &world);

    [[nodiscard]] const PatchKey &Key() const noexcept { return m_Patch; }
    [[nodiscard]] const ScriptSelection &Target() const noexcept {
        return m_Target;
    }
    [[nodiscard]] PlanState State() const noexcept { return m_State; }
    [[nodiscard]] Epoch WorldEpoch() const noexcept { return m_Epoch; }
    [[nodiscard]] std::size_t Size() const noexcept { return m_Installed.size(); }
    [[nodiscard]] bool Contains(const ObjectRef &target) const noexcept;
    [[nodiscard]] bool Retiring() const noexcept { return m_Retiring; }
    [[nodiscard]] const Status &LastStatus() const noexcept {
        return m_LastStatus;
    }
    [[nodiscard]] const Status &ApplyFailure() const noexcept {
        return m_ApplyFailure;
    }
    [[nodiscard]] const Status &RestoreFailure() const noexcept {
        return m_RestoreFailure;
    }

private:
    friend class Plans;

    struct RefLess {
        bool operator()(const ObjectRef &left,
                        const ObjectRef &right) const noexcept;
    };

    Status CloseAll(World &world);
    Status Applied(Status status);
    Status Restored(Status status);
    Status Settled();

    PatchKey m_Patch;
    ScriptSelection m_Target;
    // A Plan the Loader has accepted but never reconciled is Reconciling, not
    // Unsatisfied. Unsatisfied means a pass ran and found no usable target.
    PlanState m_State = PlanState::Reconciling;
    Epoch m_Epoch = 0;
    bool m_Retiring = false;
    std::map<ObjectRef, Installation, RefLess> m_Installed;
    Status m_LastStatus;
    Status m_ApplyFailure;
    Status m_RestoreFailure;
};

using PlanId = std::uint64_t;

struct PlanInfo {
    PlanState State = PlanState::Unsatisfied;
    Epoch World = 0;
    std::size_t Matches = 0;
    std::size_t Installations = 0;
    Status LastStatus;
    Status ApplyFailure;
    Status RestoreFailure;
};

// A loader-owned Behavior Plan. Script load/unload events only change the known
// target set; native reconciliation happens on the game thread at
// ProcessFrame. World implementations and their canonical Edit data are owned
// by the Loader, not by a Mod callback or a live CK object. A World callback
// may request retirement or mark another Script change, but those facts are
// consumed only after the current World call returns.
class Plans final {
public:
    Plans() = default;

    Status Submit(PatchKey patch, std::uint64_t ownerGeneration,
                  ScriptSelection target,
                  std::shared_ptr<Plan::World> world, PlanId &out);
    Status Read(PlanId id, PlanInfo &out) const;
    Status Read(std::string_view owner, std::uint64_t ownerGeneration,
                PlanId id, PlanInfo &out) const;
    Status Close(PlanId id);
    Status Close(std::string_view owner, std::uint64_t ownerGeneration,
                 PlanId id);
    Status Retry(PlanId id);
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
        bool Dirty = true;
        bool CloseRequested = false;

        Record(PlanId id, std::uint64_t ownerGeneration,
               std::shared_ptr<Plan::World> world,
               PatchKey patch, ScriptSelection target)
            : Id(id), OwnerGeneration(ownerGeneration),
              World(std::move(world)),
              Value(std::move(patch), std::move(target)) {}
    };

    struct RefLess {
        bool operator()(const ObjectRef &left,
                        const ObjectRef &right) const noexcept;
    };

    [[nodiscard]] PlanId NextId() noexcept;
    [[nodiscard]] Status Ready() const;
    void Mark(std::string_view name) noexcept;

    Epoch m_Epoch = 1;
    PlanId m_NextId = 1;
    std::thread::id m_Thread = std::this_thread::get_id();
    bool m_InWorld = false;
    std::map<PlanId, std::unique_ptr<Record>> m_Plans;
    std::map<PatchKey, PlanId> m_Keys;
    std::map<ObjectRef, std::string, RefLess> m_Scripts;
};

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_PLAN_H
