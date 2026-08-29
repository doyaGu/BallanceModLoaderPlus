#ifndef BML_VIRTOOLSACTIONS_H
#define BML_VIRTOOLSACTIONS_H

#include <array>
#include <thread>
#include <utility>
#include <vector>

#include "CKAll.h"

#include "Virtools/BehaviorGraphRecipes.h"

namespace BML {

enum class VirtoolsActionError {
    None,
    NotBound,
    WrongThread,
    OwnerExpired,
    BlockUnavailable,
    BehaviorFailed,
};

struct VirtoolsActionResult {
    VirtoolsActionError Error = VirtoolsActionError::None;
    int BehaviorResult = CKBR_OK;

    explicit operator bool() const { return Error == VirtoolsActionError::None; }
};

class ObjectLoadResult {
public:
    explicit operator bool() const { return Status.Error == VirtoolsActionError::None; }

    VirtoolsActionResult Status;
    std::vector<CK_ID> Objects;
    CK_ID MasterObject = 0;
    bool HasObjectArray = false;

private:
    XObjectArray *m_BorrowedObjects = nullptr;
    CKObject *m_BorrowedMasterObject = nullptr;

    friend class VirtoolsActions;
    friend struct LegacyVirtoolsActions;
};

const char *DescribeVirtoolsActionError(VirtoolsActionError error);

class VirtoolsActions {
public:
    VirtoolsActions() = default;
    VirtoolsActions(const VirtoolsActions &) = delete;
    VirtoolsActions(VirtoolsActions &&) = delete;
    VirtoolsActions &operator=(const VirtoolsActions &) = delete;
    VirtoolsActions &operator=(VirtoolsActions &&) = delete;

    VirtoolsActionResult Bind(CKBehavior *ownerScript);
    void Reset();
    bool IsReady() const;

    VirtoolsActionResult PhysicalizeConvex(const BehaviorGraphRecipes::PhysicalizeDefinition &definition,
                                           CKMesh *mesh = nullptr);
    VirtoolsActionResult PhysicalizeBall(const BehaviorGraphRecipes::PhysicalizeDefinition &definition,
                                         VxVector ballCenter = VxVector(0.0f, 0.0f, 0.0f),
                                         float ballRadius = 2.0f);
    VirtoolsActionResult PhysicalizeConcave(const BehaviorGraphRecipes::PhysicalizeDefinition &definition,
                                            CKMesh *mesh = nullptr);
    VirtoolsActionResult Unphysicalize(CK3dEntity *target);
    VirtoolsActionResult SetPhysicsForce(const BehaviorGraphRecipes::ForceDefinition &definition);
    VirtoolsActionResult UnsetPhysicsForce(CK3dEntity *target);
    VirtoolsActionResult PhysicsImpulse(const BehaviorGraphRecipes::ForceDefinition &definition);
    VirtoolsActionResult PhysicsWakeUp(CK3dEntity *target);
    ObjectLoadResult LoadObjects(const BehaviorGraphRecipes::ObjectLoadDefinition &definition);

private:
    enum BlockSlot : std::size_t {
        PhysicalizeConvexSlot,
        PhysicalizeBallSlot,
        PhysicalizeConcaveSlot,
        ObjectLoadSlot,
        PhysicsImpulseSlot,
        PhysicsForceSlot,
        PhysicsWakeUpSlot,
        BlockSlotCount,
    };

    VirtoolsActionResult ReadyStatus() const;
    CKBehavior *ResolveBlock(BlockSlot slot) const;
    VirtoolsActionResult Run(BlockSlot slot, int input);
    VirtoolsActionResult SetPhysicalizeParameters(BlockSlot slot,
                                                  const BehaviorGraphRecipes::PhysicalizeDefinition &definition);

    CKContext *m_Context = nullptr;
    CK_ID m_OwnerScript = 0;
    std::array<CK_ID, BlockSlotCount> m_Blocks{};
    std::vector<CK_ID> m_Parameters;
    unsigned int m_LoadCount = 0;
    std::thread::id m_OwnerThread;

    friend struct LegacyVirtoolsActions;
};

struct LegacyVirtoolsActions {
    static std::pair<XObjectArray *, CKObject *> LoadObjects(
        VirtoolsActions &actions, const BehaviorGraphRecipes::ObjectLoadDefinition &definition);
};

} // namespace BML

#endif // BML_VIRTOOLSACTIONS_H
