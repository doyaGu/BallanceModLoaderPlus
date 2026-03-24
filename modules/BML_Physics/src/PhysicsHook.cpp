/**
 * @file PhysicsHook.cpp
 * @brief Physics system hook implementation
 */

#include "PhysicsHook.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdlib>
#include <cstring>

#include "CKAll.h"
#include "BML/Guids/physics_RT.h"
#include "VTables.h"
#include "HookUtils.h"

#include "bml_virtools_payloads.h"

namespace BML_Physics {

namespace {
BML_HookContext s_Hook = BML_HOOK_CONTEXT_INIT;
}

//-----------------------------------------------------------------------------
// CKIpionManager interface (physics manager)
//-----------------------------------------------------------------------------

class CKIpionManager : public CKBaseManager {
public:
    virtual void Reset() = 0;
};

struct CP_CLASS_VTABLE_NAME(CKIpionManager) : public CP_CLASS_VTABLE_NAME(CKBaseManager)<CKIpionManager> {
    CP_DECLARE_METHOD_PTR(CKIpionManager, void, Reset, ());
};

//-----------------------------------------------------------------------------
// Physics Hook State
//-----------------------------------------------------------------------------

struct PhysicsHookState {
    static CKIpionManager *s_IpionManager;
    static CP_CLASS_VTABLE_NAME(CKIpionManager) s_VTable;
    static CKContext *s_Context;

    // Cached topic IDs
    static BML_TopicId s_TopicPhysicalize;
    static BML_TopicId s_TopicUnphysicalize;

    static void Hook(CKIpionManager *im) {
        if (!im)
            return;

        s_IpionManager = im;
        utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKIpionManager)>(s_IpionManager, s_VTable);

#define HOOK_PHYSICS_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &PhysicsHookState::CP_FUNC_HOOK_NAME(Name), (offsetof(CP_CLASS_VTABLE_NAME(CKIpionManager), Name) / sizeof(void*)))

        HOOK_PHYSICS_VIRTUAL_METHOD(s_IpionManager, PostProcess);

#undef HOOK_PHYSICS_VIRTUAL_METHOD
    }

    static void Unhook() {
        if (s_IpionManager)
            utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKIpionManager)>(s_IpionManager, s_VTable);
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, PostProcess, ()) { return PostProcessOriginal(); }

    static CKERROR PostProcessOriginal() {
        return CP_CALL_METHOD_PTR(s_IpionManager, s_VTable.PostProcess);
    }
};

// Static member definitions
CKIpionManager *PhysicsHookState::s_IpionManager = nullptr;
CP_CLASS_VTABLE_NAME(CKIpionManager) PhysicsHookState::s_VTable = {};
CKContext *PhysicsHookState::s_Context = nullptr;
BML_TopicId PhysicsHookState::s_TopicPhysicalize = 0;
BML_TopicId PhysicsHookState::s_TopicUnphysicalize = 0;

//-----------------------------------------------------------------------------
// Physicalize Behavior Hook
//-----------------------------------------------------------------------------

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

static CKBEHAVIORFCT g_OriginalPhysicalize = nullptr;

// (DuplicateCString and CleanupPhysicalizePayload removed -- events are now
// stack-local POD published via Publish(), not heap-allocated PublishBuffer())

/**
 * @brief Physicalize behavior hook
 * 
 * Intercepts the Physicalize BB to publish IMC events before/after
 * physics objects are created.
 */
int PhysicalizeHook(const CKBehaviorContext &behcontext) {
    CKBehavior *beh = behcontext.Behavior;
    bool physicalize = beh->IsInputActive(0);
    auto *target = (CK3dEntity *) beh->GetTarget();

    if (physicalize) {
        // Publish OnPhysicalize event via IMC
        if (s_Hook.imc_bus && s_Hook.imc_bus->Publish && PhysicsHookState::s_TopicPhysicalize) {
            auto *imcBus = s_Hook.imc_bus;
            BML_PhysicalizeEvent event{};
            event.target = target;

            beh->GetInputParameterValue(FIXED, &event.fixed);
            beh->GetInputParameterValue(FRICTION, &event.friction);
            beh->GetInputParameterValue(ELASTICITY, &event.elasticity);
            beh->GetInputParameterValue(MASS, &event.mass);
            event.collision_group = (CKSTRING) beh->GetInputParameterReadDataPtr(COLLISION_GROUP);
            beh->GetInputParameterValue(START_FROZEN, &event.start_frozen);
            beh->GetInputParameterValue(ENABLE_COLLISION, &event.enable_collision);
            beh->GetInputParameterValue(AUTOMATIC_CALCULATE_MASS_CENTER, &event.auto_calc_mass_center);
            beh->GetInputParameterValue(LINEAR_SPEED_DAMPENING, &event.linear_speed_dampening);
            beh->GetInputParameterValue(ROT_SPEED_DAMPENING, &event.rot_speed_dampening);
            event.collision_surface = (CKSTRING) beh->GetInputParameterReadDataPtr(COLLISION_SURFACE);

            beh->GetLocalParameterValue(0, &event.convex_count);
            beh->GetLocalParameterValue(1, &event.ball_count);
            beh->GetLocalParameterValue(2, &event.concave_count);

            // Stack-local arrays for borrowed CK pointers
            constexpr int kMaxConvex = 32, kMaxBall = 32, kMaxConcave = 32;
            CKMesh *convexMeshes[kMaxConvex]{};
            VxVector ballCenters[kMaxBall]{};
            float ballRadii[kMaxBall]{};
            CKMesh *concaveMeshes[kMaxConcave]{};

            event.convex_count = (std::min)(event.convex_count, kMaxConvex);
            event.ball_count = (std::min)(event.ball_count, kMaxBall);
            event.concave_count = (std::min)(event.concave_count, kMaxConcave);

            int pos = CONVEX;
            for (int i = 0; i < event.convex_count; ++i)
                convexMeshes[i] = (CKMesh *) beh->GetInputParameterObject(pos + i);
            pos += event.convex_count;

            for (int i = 0; i < event.ball_count; ++i) {
                beh->GetInputParameterValue(pos + 2 * i, &ballCenters[i]);
                beh->GetInputParameterValue(pos + 2 * i + 1, &ballRadii[i]);
            }
            pos += event.ball_count * 2;

            for (int i = 0; i < event.concave_count; ++i)
                concaveMeshes[i] = (CKMesh *) beh->GetInputParameterObject(pos + i);

            event.convex_meshes = convexMeshes;
            event.ball_centers = ballCenters;
            event.ball_radii = ballRadii;
            event.concave_meshes = concaveMeshes;

            beh->GetLocalParameterValue(3, &event.mass_center);

            imcBus->Publish(s_Hook.owner,
                            PhysicsHookState::s_TopicPhysicalize,
                            &event,
                            sizeof(event));
        }
    } else {
        // Publish OnUnphysicalize event via IMC
        if (s_Hook.imc_bus && s_Hook.imc_bus->Publish && s_Hook.owner &&
            PhysicsHookState::s_TopicUnphysicalize) {
            s_Hook.imc_bus->Publish(
                s_Hook.owner,
                PhysicsHookState::s_TopicUnphysicalize,
                &target,
                sizeof(target)
            );
        }
    }

    // Call original behavior
    return g_OriginalPhysicalize(behcontext);
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------

bool InitializePhysicsHook(CKContext *context, const BML_HookContext *ctx) {
    if (!context)
        return false;

    if (ctx) s_Hook = *ctx;
    PhysicsHookState::s_Context = context;

    // Register topics through hook context
    auto *imcBus = s_Hook.imc_bus;
    if (imcBus && imcBus->GetTopicId) {
        imcBus->GetTopicId(
            imcBus->Context, "Physics/Physicalize", &PhysicsHookState::s_TopicPhysicalize);
        imcBus->GetTopicId(
            imcBus->Context, "Physics/Unphysicalize", &PhysicsHookState::s_TopicUnphysicalize);
    }

    // Hook IpionManager
    auto *im = (CKIpionManager *) context->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
    PhysicsHookState::Hook(im);

    // Hook Physicalize BB
    CKBehaviorPrototype *physicalizeProto = CKGetPrototypeFromGuid(PHYSICS_RT_PHYSICALIZE);
    if (physicalizeProto) {
        if (!g_OriginalPhysicalize)
            g_OriginalPhysicalize = physicalizeProto->GetFunction();
        physicalizeProto->SetFunction(&PhysicalizeHook);
    }

    return true;
}

void ShutdownPhysicsHook() {
    PhysicsHookState::Unhook();
    PhysicsHookState::s_Context = nullptr;
    PhysicsHookState::s_TopicPhysicalize = 0;
    PhysicsHookState::s_TopicUnphysicalize = 0;

    // Restore Physicalize BB
    CKBehaviorPrototype *physicalizeProto = CKGetPrototypeFromGuid(PHYSICS_RT_PHYSICALIZE);
    if (physicalizeProto && g_OriginalPhysicalize) {
        physicalizeProto->SetFunction(g_OriginalPhysicalize);
    }

    s_Hook = {};
}

void PhysicsPostProcess() {
    PhysicsHookState::PostProcessOriginal();
}

} // namespace BML_Physics
