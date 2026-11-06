#include "BML/Guids/physics_RT.h"

#include "Hooks/BehaviorFunctionPatch.h"
#include "Hooks/HookLifecycle.h"
#include "HookUtils.h"
#include "Loader/ModContext.h"

namespace {


#define FIXED 0
#define FRICTION 1
#define ELASTICITY 2
#define MASS 3
#define COLLISION_GROUP 4
#define START_FROZEN 5
#define ENABLE_COLLISION 6
#define AUTOMATIC_CALCULATE_MASS_CENTER 7
#define LINEAR_SPEED_DAMPENING 8
#define ROT_SPEED_DAMPENING 9
#define COLLISION_SURFACE 10
#define CONVEX 11

BehaviorFunctionPatch g_PhysicalizePatch;

} // namespace

HookLifecycle *HookLifecycle::s_PhysicsOwner = nullptr;

int HookLifecycle::Physicalize(const CKBehaviorContext &behcontext) {
    const CKBEHAVIORFCT original = g_PhysicalizePatch.Original();
    if (!original)
        return CKBR_BEHAVIORERROR;

    ModContext *modContext = s_PhysicsOwner ? &s_PhysicsOwner->m_Context : nullptr;
    if (!modContext || behcontext.Context != modContext->GetCKContext())
        return original(behcontext);

    CKBehavior *beh = behcontext.Behavior;
    bool physicalize = beh->IsInputActive(0);
    auto *target = (CK3dEntity *) beh->GetTarget();

    if (physicalize) {
        CKBOOL fixed = FALSE;
        beh->GetInputParameterValue(FIXED, &fixed);

        float friction = 0.4f;
        beh->GetInputParameterValue(FRICTION, &friction);

        float elasticity = 0.5f;
        beh->GetInputParameterValue(ELASTICITY, &elasticity);

        float mass = 1.0f;
        beh->GetInputParameterValue(MASS, &mass);

        CKSTRING collisionGroup = (CKSTRING) beh->GetInputParameterReadDataPtr(COLLISION_GROUP);

        CKBOOL startFrozen = FALSE;
        beh->GetInputParameterValue(START_FROZEN, &startFrozen);

        CKBOOL enableCollision = TRUE;
        beh->GetInputParameterValue(ENABLE_COLLISION, &enableCollision);

        CKBOOL autoCalcMassCenter = TRUE;
        beh->GetInputParameterValue(AUTOMATIC_CALCULATE_MASS_CENTER, &autoCalcMassCenter);

        float linearSpeedDampening = 0.1f;
        beh->GetInputParameterValue(LINEAR_SPEED_DAMPENING, &linearSpeedDampening);

        float rotSpeedDampening = 0.1f;
        beh->GetInputParameterValue(ROT_SPEED_DAMPENING, &rotSpeedDampening);

        auto collisionSurface = (CKSTRING) beh->GetInputParameterReadDataPtr(COLLISION_SURFACE);

        int convexCount = 1;
        beh->GetLocalParameterValue(0, &convexCount);

        int ballCount = 0;
        beh->GetLocalParameterValue(1, &ballCount);

        int concaveCount = 0;
        beh->GetLocalParameterValue(2, &concaveCount);

        int pos = CONVEX;
        CKMesh **convexMesh = (convexCount > 0) ? new CKMesh *[convexCount] : nullptr;
        VxVector *ballCenter = (ballCount > 0) ? new VxVector[ballCount] : nullptr;
        float *ballRadius = (ballCount > 0) ? new float[ballCount] : nullptr;
        CKMesh **concaveMesh = (concaveCount > 0) ? new CKMesh *[concaveCount] : nullptr;

        if (convexMesh) {
            for (int i = 0; i < convexCount; ++i)
                convexMesh[i] = (CKMesh *) beh->GetInputParameterObject(pos + i);
        }
        pos += convexCount;

        for (int j = 0; j < ballCount; ++j) {
            beh->GetInputParameterValue(pos + 2 * j, &ballCenter[j]);
            beh->GetInputParameterValue(pos + 2 * j + 1, &ballRadius[j]);
        }
        pos += ballCount * 2;

        if (concaveMesh) {
            for (int k = 0; k < concaveCount; ++k)
                concaveMesh[k] = (CKMesh *) beh->GetInputParameterObject(pos + k);
        }
        pos += concaveCount;

        VxVector shiftMassCenter;
        beh->GetLocalParameterValue(3, &shiftMassCenter);

        modContext->GetModLoader().BroadcastCallback<ModEvent::OnPhysicalize>(
            target, fixed, friction, elasticity, mass, collisionGroup, startFrozen, enableCollision,
            autoCalcMassCenter, linearSpeedDampening, rotSpeedDampening, collisionSurface,
            shiftMassCenter, convexCount, convexMesh, ballCount, ballCenter, ballRadius,
            concaveCount, concaveMesh);
        delete[] convexMesh;
        delete[] ballCenter;
        delete[] ballRadius;
        delete[] concaveMesh;
    } else {
        modContext->GetModLoader().BroadcastCallback<ModEvent::OnUnphysicalize>(target);
    }

    return original(behcontext);
}


bool HookLifecycle::AttachPhysicalize() {
    if (s_PhysicsOwner)
        return s_PhysicsOwner == this && g_PhysicalizePatch.IsInstalled();
    if (g_PhysicalizePatch.IsInstalled()
    )
        return false;

    s_PhysicsOwner = this;

    const BehaviorFunctionPatchResult result = g_PhysicalizePatch.Install(
        PHYSICS_RT_PHYSICALIZE, &HookLifecycle::Physicalize);
    if (!result) {
        utils::OutputDebugA("BML Physicalize hook installation failed: %s\n",
                            BehaviorFunctionPatch::GetErrorName(result.Code));
            s_PhysicsOwner = nullptr;
        return false;
    }
    return true;
}

bool HookLifecycle::DetachPhysicalize() {
    if (s_PhysicsOwner != this)
        return true;

    const BehaviorFunctionPatchResult behaviorResult = g_PhysicalizePatch.Remove();
    bool behaviorRemoved = static_cast<bool>(behaviorResult);
    if (behaviorResult.Code == BehaviorFunctionPatchError::OwnershipLost) {
        utils::OutputDebugA("BML Physicalize hook ownership changed; preserving the current function\n");
        behaviorRemoved = true;
    }

    if (behaviorRemoved)
        s_PhysicsOwner = nullptr;
    return behaviorRemoved;
}


bool HookLifecycle::OwnsPhysicalize() const {
    return s_PhysicsOwner == this && g_PhysicalizePatch.IsInstalled();
}
