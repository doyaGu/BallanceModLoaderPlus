#include "BML/Guids/physics_RT.h"

#include "Hooks/BehaviorFunctionPatch.h"
#include "Hooks/HookLifecycle.h"
#include "Hooks/VTablePatch.h"
#include "Hooks/VTables.h"
#include "HookUtils.h"
#include "Loader/ModContext.h"

namespace {

class CKIpionManager : public CKBaseManager {
public:
    virtual void Reset() = 0;
};

struct CP_CLASS_VTABLE_NAME(CKIpionManager) : public CP_CLASS_VTABLE_NAME(CKBaseManager)<CKIpionManager> {
    CP_DECLARE_METHOD_PTR(CKIpionManager, void, Reset, ());
};

struct PhysicsHook {
    static CKIpionManager *s_IpionManager;
    static CP_CLASS_VTABLE_NAME(CKIpionManager) s_VTable;
    static VTablePatch s_Patch;

    static bool Hook(CKIpionManager *im) {
        if (!im)
            return false;
        if (s_Patch.IsInstalled())
            return s_IpionManager == im;

        const std::size_t postProcessSlot =
            offsetof(CP_CLASS_VTABLE_NAME(CKIpionManager), PostProcess) / sizeof(void *);
        const VTablePatch::Request request = {
            postProcessSlot,
            utils::TypeErase(&PhysicsHook::CP_FUNC_HOOK_NAME(PostProcess)),
        };
        const VTablePatchResult result = s_Patch.Install(im, &request, 1);
        if (!result) {
            utils::OutputDebugA("BML PhysicsHook install failed: %s (entry %zu)\n",
                                VTablePatch::GetErrorName(result.Code), result.EntryIndex);
            return false;
        }

        s_VTable.PostProcess = utils::ForceReinterpretCast<decltype(s_VTable.PostProcess)>(
            s_Patch.GetOriginal(postProcessSlot));
        s_IpionManager = im;
        return true;
    }

    static bool Unhook() {
        const VTablePatchResult result = s_Patch.Remove();
        if (!result) {
            utils::OutputDebugA("BML PhysicsHook removal warning: %s (entry %zu)\n",
                                VTablePatch::GetErrorName(result.Code), result.EntryIndex);
        }

        if (!s_Patch.IsInstalled()) {
            s_IpionManager = nullptr;
            s_VTable = {};
            return true;
        }
        return false;
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, PostProcess, ()) { return CK_OK; }

    static CKERROR PostProcessOriginal() {
        if (!s_Patch.IsInstalled() || !s_IpionManager || !s_VTable.PostProcess)
            return CK_OK;
        return CP_CALL_METHOD_PTR(s_IpionManager, s_VTable.PostProcess);
    }

    static bool IsInstalled() { return s_Patch.IsInstalled(); }
};

CKIpionManager *PhysicsHook::s_IpionManager = nullptr;
CP_CLASS_VTABLE_NAME(CKIpionManager) PhysicsHook::s_VTable = {};
VTablePatch PhysicsHook::s_Patch;

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

int Physicalize(const CKBehaviorContext &behcontext) {
    const CKBEHAVIORFCT original = g_PhysicalizePatch.Original();
    if (!original)
        return CKBR_BEHAVIORERROR;

    CKBehavior *beh = behcontext.Behavior;
    bool physicalize = beh->IsInputActive(0);
    auto *target = (CK3dEntity *) beh->GetTarget();
    ModContext *modContext = BML_GetModContext();
    if (!modContext)
        return original(behcontext);

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

        modContext->BroadcastCallback(&IMod::OnPhysicalize, target,
                                               fixed, friction, elasticity, mass,
                                               collisionGroup, startFrozen, enableCollision,
                                               autoCalcMassCenter, linearSpeedDampening,
                                               rotSpeedDampening,
                                               collisionSurface, shiftMassCenter, convexCount,
                                               convexMesh, ballCount, ballCenter,
                                               ballRadius, concaveCount, concaveMesh);
        delete[] convexMesh;
        delete[] ballCenter;
        delete[] ballRadius;
        delete[] concaveMesh;
    } else {
        modContext->BroadcastCallback(&IMod::OnUnphysicalize, target);
    }

    return original(behcontext);
}

} // namespace

void RunPhysicsPostProcess() {
    if (!PhysicsHook::IsInstalled())
        return;
    PhysicsHook::PostProcessOriginal();
}

bool HookPhysicalize() {
    auto *im = (CKIpionManager *) BML_GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
    if (!PhysicsHook::Hook(im))
        utils::OutputDebugA("BML physics scheduling redirection is unavailable; CK2 will keep its normal order\n");

    const BehaviorFunctionPatchResult result = g_PhysicalizePatch.Install(
        PHYSICS_RT_PHYSICALIZE, &Physicalize);
    if (!result) {
        utils::OutputDebugA("BML Physicalize hook installation failed: %s\n",
                            BehaviorFunctionPatch::GetErrorName(result.Code));
        PhysicsHook::Unhook();
        return false;
    }
    return true;
}

bool UnhookPhysicalize() {
    const BehaviorFunctionPatchResult behaviorResult = g_PhysicalizePatch.Remove();
    bool behaviorRemoved = static_cast<bool>(behaviorResult);
    if (behaviorResult.Code == BehaviorFunctionPatchError::OwnershipLost) {
        utils::OutputDebugA("BML Physicalize hook ownership changed; preserving the current function\n");
        behaviorRemoved = true;
    }

    return PhysicsHook::Unhook() && behaviorRemoved;
}

bool IsPhysicsPostProcessHookInstalled() {
    return PhysicsHook::IsInstalled();
}

bool IsPhysicalizeHookInstalled() {
    return g_PhysicalizePatch.IsInstalled();
}
