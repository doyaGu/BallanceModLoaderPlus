#include "Virtools/VirtoolsActions.h"

#include <string>
#include <vector>

#include "BML/ScriptHelper.h"

namespace BML {
namespace {

bool BehaviorFailed(int result) {
    return result != CKBR_OK;
}

CK_ID BehaviorId(CKBehavior *behavior) {
    return behavior ? behavior->GetID() : 0;
}

} // namespace

const char *DescribeVirtoolsActionError(VirtoolsActionError error) {
    switch (error) {
    case VirtoolsActionError::None:
        return "none";
    case VirtoolsActionError::NotBound:
        return "not bound";
    case VirtoolsActionError::WrongThread:
        return "wrong thread";
    case VirtoolsActionError::OwnerExpired:
        return "owner script expired";
    case VirtoolsActionError::BlockUnavailable:
        return "building block unavailable";
    case VirtoolsActionError::BehaviorFailed:
        return "building block execution failed";
    }
    return "unknown";
}

VirtoolsActionResult VirtoolsActions::Bind(CKBehavior *ownerScript) {
    if (m_Context && m_OwnerThread != std::thread::id{} &&
        m_OwnerThread != std::this_thread::get_id()) {
        return {VirtoolsActionError::WrongThread, CKBR_OK};
    }
    if (!ownerScript || !ownerScript->GetCKContext())
        return {VirtoolsActionError::OwnerExpired, CKBR_OK};

    Reset();
    m_Context = ownerScript->GetCKContext();
    m_OwnerScript = ownerScript->GetID();
    m_OwnerThread = std::this_thread::get_id();

    const BehaviorGraphRecipes::PhysicalizeDefinition physicalize;
    const BehaviorGraphRecipes::ForceDefinition force;
    const BehaviorGraphRecipes::ObjectLoadDefinition objectLoad;
    m_Blocks[PhysicalizeConvexSlot] = BehaviorId(
        BehaviorGraphRecipes::AddPhysicalizeConvex(ownerScript, physicalize));
    m_Blocks[PhysicalizeBallSlot] = BehaviorId(
        BehaviorGraphRecipes::AddPhysicalizeBall(ownerScript, physicalize));
    m_Blocks[PhysicalizeConcaveSlot] = BehaviorId(
        BehaviorGraphRecipes::AddPhysicalizeConcave(ownerScript, physicalize));
    m_Blocks[ObjectLoadSlot] = BehaviorId(
        BehaviorGraphRecipes::AddObjectLoad(ownerScript, objectLoad));
    m_Blocks[PhysicsImpulseSlot] = BehaviorId(
        BehaviorGraphRecipes::AddPhysicsImpulse(ownerScript, force));
    m_Blocks[PhysicsForceSlot] = BehaviorId(
        BehaviorGraphRecipes::AddPhysicsForce(ownerScript, force));
    m_Blocks[PhysicsWakeUpSlot] = BehaviorId(
        BehaviorGraphRecipes::AddPhysicsWakeUp(ownerScript));

    for (CK_ID blockId : m_Blocks) {
        auto *behavior = static_cast<CKBehavior *>(m_Context->GetObject(blockId));
        if (!behavior)
            continue;
        if (CKParameterIn *target = behavior->GetTargetParameter()) {
            if (CKParameter *source = target->GetDirectSource())
                m_Parameters.push_back(source->GetID());
        }
        for (int i = 0; i < behavior->GetInputParameterCount(); ++i) {
            if (CKParameter *source = behavior->GetInputParameter(i)->GetDirectSource())
                m_Parameters.push_back(source->GetID());
        }
    }
    for (CK_ID block : m_Blocks) {
        if (!block) {
            Reset();
            return {VirtoolsActionError::BlockUnavailable, CKBR_OK};
        }
    }
    return {};
}

void VirtoolsActions::Reset() {
    if (m_Context && (m_OwnerThread == std::thread::id{} || m_OwnerThread == std::this_thread::get_id())) {
        CKBehavior *ownerScript = nullptr;
        CKObject *ownerObject = m_Context->GetObject(m_OwnerScript);
        if (ownerObject && CKIsChildClassOf(ownerObject, CKCID_BEHAVIOR))
            ownerScript = static_cast<CKBehavior *>(ownerObject);

        for (CK_ID block : m_Blocks) {
            CKObject *object = m_Context->GetObject(block);
            if (!ownerScript || !object || !CKIsChildClassOf(object, CKCID_BEHAVIOR))
                continue;
            auto *behavior = static_cast<CKBehavior *>(object);
            if (behavior->GetParent() != ownerScript)
                continue;
            ownerScript->RemoveSubBehavior(behavior);
            if (m_Context->GetObject(block) == behavior)
                m_Context->DestroyObject(block);
        }

        std::vector<CK_ID> removedParameters;
        if (ownerScript) {
            for (int i = ownerScript->GetLocalParameterCount() - 1; i >= 0; --i) {
                CKParameterLocal *parameter = ownerScript->GetLocalParameter(i);
                if (!parameter)
                    continue;
                bool ownedByActions = false;
                for (CK_ID id : m_Parameters) {
                    if (parameter->GetID() == id) {
                        ownedByActions = true;
                        break;
                    }
                }
                if (ownedByActions) {
                    CKParameterLocal *removed = ownerScript->RemoveLocalParameter(i);
                    if (removed)
                        removedParameters.push_back(removed->GetID());
                }
            }
        }
        for (CK_ID parameter : removedParameters) {
            if (parameter && m_Context->GetObject(parameter))
                m_Context->DestroyObject(parameter);
        }
    }
    m_Blocks.fill(0);
    m_Parameters.clear();
    m_OwnerScript = 0;
    m_Context = nullptr;
    m_OwnerThread = {};
}

bool VirtoolsActions::IsReady() const {
    if (!ReadyStatus())
        return false;
    for (std::size_t slot = 0; slot < BlockSlotCount; ++slot) {
        if (!ResolveBlock(static_cast<BlockSlot>(slot)))
            return false;
    }
    return true;
}

VirtoolsActionResult VirtoolsActions::ReadyStatus() const {
    if (!m_Context || !m_OwnerScript)
        return {VirtoolsActionError::NotBound, CKBR_OK};
    if (m_OwnerThread != std::this_thread::get_id())
        return {VirtoolsActionError::WrongThread, CKBR_OK};
    CKObject *owner = m_Context->GetObject(m_OwnerScript);
    if (!owner || !CKIsChildClassOf(owner, CKCID_BEHAVIOR))
        return {VirtoolsActionError::OwnerExpired, CKBR_OK};
    return {};
}

CKBehavior *VirtoolsActions::ResolveBlock(BlockSlot slot) const {
    if (!m_Context || slot >= BlockSlotCount || !m_Blocks[slot])
        return nullptr;
    CKObject *ownerObject = m_Context->GetObject(m_OwnerScript);
    CKObject *blockObject = m_Context->GetObject(m_Blocks[slot]);
    if (!ownerObject || !CKIsChildClassOf(ownerObject, CKCID_BEHAVIOR) ||
        !blockObject || !CKIsChildClassOf(blockObject, CKCID_BEHAVIOR)) {
        return nullptr;
    }
    auto *ownerScript = static_cast<CKBehavior *>(ownerObject);
    auto *behavior = static_cast<CKBehavior *>(blockObject);
    return behavior->GetParent() == ownerScript ? behavior : nullptr;
}

VirtoolsActionResult VirtoolsActions::Run(BlockSlot slot, int input) {
    VirtoolsActionResult status = ReadyStatus();
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(slot);
    if (!behavior)
        return {VirtoolsActionError::BlockUnavailable, CKBR_OK};
    behavior->ActivateInput(input);
    const int result = behavior->Execute(0.0f);
    return {BehaviorFailed(result) ? VirtoolsActionError::BehaviorFailed : VirtoolsActionError::None, result};
}

VirtoolsActionResult VirtoolsActions::SetPhysicalizeParameters(
    BlockSlot slot, const BehaviorGraphRecipes::PhysicalizeDefinition &definition) {
    VirtoolsActionResult status = ReadyStatus();
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(slot);
    if (!behavior)
        return {VirtoolsActionError::BlockUnavailable, CKBR_OK};

    ScriptHelper::SetParamObject(behavior->GetTargetParameter()->GetDirectSource(), definition.Target);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(0)->GetDirectSource(), definition.Fixed);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(1)->GetDirectSource(), definition.Friction);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(2)->GetDirectSource(), definition.Elasticity);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(3)->GetDirectSource(), definition.Mass);
    ScriptHelper::SetParamString(behavior->GetInputParameter(4)->GetDirectSource(), definition.CollisionGroup.c_str());
    ScriptHelper::SetParamValue(behavior->GetInputParameter(5)->GetDirectSource(), definition.StartFrozen);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(6)->GetDirectSource(), definition.EnableCollision);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(7)->GetDirectSource(), definition.CalculateMassCenter);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(8)->GetDirectSource(), definition.LinearDamping);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(9)->GetDirectSource(), definition.RotationalDamping);
    ScriptHelper::SetParamString(behavior->GetInputParameter(10)->GetDirectSource(),
                                 definition.CollisionSurface.c_str());
    ScriptHelper::SetParamValue(behavior->GetLocalParameter(3), definition.MassCenter);
    return {};
}

VirtoolsActionResult VirtoolsActions::PhysicalizeConvex(
    const BehaviorGraphRecipes::PhysicalizeDefinition &definition, CKMesh *mesh) {
    VirtoolsActionResult status = SetPhysicalizeParameters(PhysicalizeConvexSlot, definition);
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(PhysicalizeConvexSlot);
    ScriptHelper::SetParamObject(behavior->GetInputParameter(11)->GetDirectSource(), mesh);
    return Run(PhysicalizeConvexSlot, 0);
}

VirtoolsActionResult VirtoolsActions::PhysicalizeBall(
    const BehaviorGraphRecipes::PhysicalizeDefinition &definition, VxVector ballCenter, float ballRadius) {
    VirtoolsActionResult status = SetPhysicalizeParameters(PhysicalizeBallSlot, definition);
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(PhysicalizeBallSlot);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(11)->GetDirectSource(), ballCenter);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(12)->GetDirectSource(), ballRadius);
    return Run(PhysicalizeBallSlot, 0);
}

VirtoolsActionResult VirtoolsActions::PhysicalizeConcave(
    const BehaviorGraphRecipes::PhysicalizeDefinition &definition, CKMesh *mesh) {
    VirtoolsActionResult status = SetPhysicalizeParameters(PhysicalizeConcaveSlot, definition);
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(PhysicalizeConcaveSlot);
    ScriptHelper::SetParamObject(behavior->GetInputParameter(11)->GetDirectSource(), mesh);
    return Run(PhysicalizeConcaveSlot, 0);
}

VirtoolsActionResult VirtoolsActions::Unphysicalize(CK3dEntity *target) {
    VirtoolsActionResult status = ReadyStatus();
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(PhysicalizeConvexSlot);
    if (!behavior)
        return {VirtoolsActionError::BlockUnavailable, CKBR_OK};
    ScriptHelper::SetParamObject(behavior->GetTargetParameter()->GetDirectSource(), target);
    return Run(PhysicalizeConvexSlot, 1);
}

VirtoolsActionResult VirtoolsActions::SetPhysicsForce(const BehaviorGraphRecipes::ForceDefinition &definition) {
    VirtoolsActionResult status = ReadyStatus();
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(PhysicsForceSlot);
    if (!behavior)
        return {VirtoolsActionError::BlockUnavailable, CKBR_OK};
    ScriptHelper::SetParamObject(behavior->GetTargetParameter()->GetDirectSource(), definition.Target);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(0)->GetDirectSource(), definition.Position);
    ScriptHelper::SetParamObject(behavior->GetInputParameter(1)->GetDirectSource(), definition.PositionReference);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(2)->GetDirectSource(), definition.Direction);
    ScriptHelper::SetParamObject(behavior->GetInputParameter(3)->GetDirectSource(), definition.DirectionReference);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(4)->GetDirectSource(), definition.Magnitude);
    return Run(PhysicsForceSlot, 0);
}

VirtoolsActionResult VirtoolsActions::UnsetPhysicsForce(CK3dEntity *target) {
    VirtoolsActionResult status = ReadyStatus();
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(PhysicsForceSlot);
    if (!behavior)
        return {VirtoolsActionError::BlockUnavailable, CKBR_OK};
    ScriptHelper::SetParamObject(behavior->GetTargetParameter()->GetDirectSource(), target);
    return Run(PhysicsForceSlot, 1);
}

VirtoolsActionResult VirtoolsActions::PhysicsImpulse(const BehaviorGraphRecipes::ForceDefinition &definition) {
    VirtoolsActionResult status = ReadyStatus();
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(PhysicsImpulseSlot);
    if (!behavior)
        return {VirtoolsActionError::BlockUnavailable, CKBR_OK};
    ScriptHelper::SetParamObject(behavior->GetTargetParameter()->GetDirectSource(), definition.Target);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(0)->GetDirectSource(), definition.Position);
    ScriptHelper::SetParamObject(behavior->GetInputParameter(1)->GetDirectSource(), definition.PositionReference);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(2)->GetDirectSource(), definition.Direction);
    ScriptHelper::SetParamObject(behavior->GetInputParameter(3)->GetDirectSource(), definition.DirectionReference);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(4)->GetDirectSource(), definition.Magnitude);
    return Run(PhysicsImpulseSlot, 0);
}

VirtoolsActionResult VirtoolsActions::PhysicsWakeUp(CK3dEntity *target) {
    VirtoolsActionResult status = ReadyStatus();
    if (!status)
        return status;
    CKBehavior *behavior = ResolveBlock(PhysicsWakeUpSlot);
    if (!behavior)
        return {VirtoolsActionError::BlockUnavailable, CKBR_OK};
    ScriptHelper::SetParamObject(behavior->GetTargetParameter()->GetDirectSource(), target);
    return Run(PhysicsWakeUpSlot, 0);
}

ObjectLoadResult VirtoolsActions::LoadObjects(const BehaviorGraphRecipes::ObjectLoadDefinition &definition) {
    ObjectLoadResult result;
    result.Status = ReadyStatus();
    if (!result.Status)
        return result;
    CKBehavior *behavior = ResolveBlock(ObjectLoadSlot);
    if (!behavior) {
        result.Status = {VirtoolsActionError::BlockUnavailable, CKBR_OK};
        return result;
    }

    ScriptHelper::SetParamString(behavior->GetInputParameter(0)->GetDirectSource(), definition.File.c_str());
    ScriptHelper::SetParamString(behavior->GetInputParameter(1)->GetDirectSource(), definition.MasterName.c_str());
    ScriptHelper::SetParamValue(behavior->GetInputParameter(2)->GetDirectSource(), definition.FilterClass);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(3)->GetDirectSource(), definition.AddToScene);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(4)->GetDirectSource(), definition.ReuseMeshes);
    ScriptHelper::SetParamValue(behavior->GetInputParameter(5)->GetDirectSource(), definition.ReuseMaterials);
    ScriptHelper::SetParamValue(behavior->GetLocalParameter(0), definition.Dynamic);
    result.Status = Run(ObjectLoadSlot, 0);
    if (!result.Status)
        return result;

    result.m_BorrowedObjects = *static_cast<XObjectArray **>(behavior->GetOutputParameterWriteDataPtr(0));
    result.m_BorrowedMasterObject = behavior->GetOutputParameterObject(1);
    result.HasObjectArray = result.m_BorrowedObjects != nullptr;
    result.MasterObject = result.m_BorrowedMasterObject ? result.m_BorrowedMasterObject->GetID() : 0;

    if (result.m_BorrowedObjects) {
        result.Objects.reserve(static_cast<std::size_t>(result.m_BorrowedObjects->Size()));
        for (CK_ID *id = result.m_BorrowedObjects->Begin(); id != result.m_BorrowedObjects->End(); ++id)
            result.Objects.push_back(*id);
    }

    if (definition.Rename && result.m_BorrowedObjects) {
        const std::string suffix = "_BMLLoad_" + std::to_string(++m_LoadCount);
        for (CK_ID id : result.Objects) {
            CKObject *object = m_Context->GetObject(id);
            if (!object || !CKIsChildClassOf(object, CKCID_BEOBJECT))
                continue;
            const char *name = object->GetName();
            if (name)
                object->SetName((CKSTRING) (std::string(name) + suffix).c_str());
        }
    }

    return result;
}

std::pair<XObjectArray *, CKObject *> LegacyVirtoolsActions::LoadObjects(
    VirtoolsActions &actions, const BehaviorGraphRecipes::ObjectLoadDefinition &definition) {
    ObjectLoadResult result = actions.LoadObjects(definition);
    return result ? std::make_pair(result.m_BorrowedObjects, result.m_BorrowedMasterObject)
                  : std::make_pair(nullptr, nullptr);
}

} // namespace BML
