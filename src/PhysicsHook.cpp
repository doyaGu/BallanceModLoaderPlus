#include "ModManager.h"
#include "HookUtils.h"
#include "VTables.h"

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

    static void Hook(CKIpionManager *im) {
        if (!im)
            return;

        s_IpionManager = im;
        utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKIpionManager)>(s_IpionManager, s_VTable);

#define HOOK_PHYSICS_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &PhysicsHook::CP_FUNC_HOOK_NAME(Name), (offsetof(CP_CLASS_VTABLE_NAME(CKIpionManager), Name) / sizeof(void*)))

        HOOK_PHYSICS_VIRTUAL_METHOD(s_IpionManager, PostProcess);

#undef HOOK_PHYSICS_VIRTUAL_METHOD
    }

    static void Unhook() {
        if (s_IpionManager)
            utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKIpionManager)>(s_IpionManager, s_VTable);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, PostProcess, ()) { return CK_OK; }

    static CKERROR PostProcessOriginal() {
        return CP_CALL_METHOD_PTR(s_IpionManager, s_VTable.PostProcess);
    }
};

CKIpionManager *PhysicsHook::s_IpionManager = nullptr;
CP_CLASS_VTABLE_NAME(CKIpionManager) PhysicsHook::s_VTable = {};

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

static CKBEHAVIORFCT g_Physicalize = nullptr;

int Physicalize(const CKBehaviorContext &behcontext) {
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

        BML_GetModManager()->BroadcastCallback(&IMod::OnPhysicalize, target,
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
        BML_GetModManager()->BroadcastCallback(&IMod::OnUnphysicalize, target);
    }

    return g_Physicalize(behcontext);
}

void PhysicsPostProcess() {
    PhysicsHook::PostProcessOriginal();
}

bool HookPhysicalize() {
    auto *im = (CKIpionManager *) BML_GetCKContext()->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
    PhysicsHook::Hook(im);

    CKBehaviorPrototype *physicalizeProto = CKGetPrototypeFromGuid(PHYSICS_RT_PHYSICALIZE);
    if (!physicalizeProto) return false;
    if (!g_Physicalize) g_Physicalize = physicalizeProto->GetFunction();
    physicalizeProto->SetFunction(&Physicalize);
    return true;
}

bool UnhookPhysicalize() {
    PhysicsHook::Unhook();

    CKBehaviorPrototype *physicalizeProto = CKGetPrototypeFromGuid(PHYSICS_RT_PHYSICALIZE);
    if (!physicalizeProto) return false;
    physicalizeProto->SetFunction(g_Physicalize);
    return true;
}