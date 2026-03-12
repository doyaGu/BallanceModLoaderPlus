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

// BML Core APIs (via loader)
#include "bml_loader.h"

namespace BML_Physics {

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

static char *DuplicateCString(const char *value) {
    if (!value) {
        return nullptr;
    }

    const size_t length = std::strlen(value) + 1;
    auto *copy = static_cast<char *>(std::malloc(length));
    if (copy) {
        std::memcpy(copy, value, length);
    }
    return copy;
}

static void CleanupPhysicalizePayload(const void *data, size_t size, void *user_data) {
    (void)size;
    (void)user_data;

    auto *payload = static_cast<BML_LegacyPhysicalizePayload *>(const_cast<void *>(data));
    if (!payload) {
        return;
    }

    std::free(const_cast<char *>(payload->collision_group));
    std::free(const_cast<char *>(payload->collision_surface));
    delete[] payload->convex_meshes;
    delete[] payload->ball_centers;
    delete[] payload->ball_radii;
    delete[] payload->concave_meshes;
    delete payload;
}

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
        if (bmlImcPublishBuffer && PhysicsHookState::s_TopicPhysicalize) {
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

            auto *payload = new BML_LegacyPhysicalizePayload{};
            payload->struct_size = sizeof(BML_LegacyPhysicalizePayload);
            payload->target = target;
            payload->fixed = fixed;
            payload->friction = friction;
            payload->elasticity = elasticity;
            payload->mass = mass;
            payload->collision_group = DuplicateCString(collisionGroup);
            payload->start_frozen = startFrozen;
            payload->enable_collision = enableCollision;
            payload->auto_calc_mass_center = autoCalcMassCenter;
            payload->linear_speed_dampening = linearSpeedDampening;
            payload->rot_speed_dampening = rotSpeedDampening;
            payload->collision_surface = DuplicateCString(collisionSurface);
            payload->convex_count = convexCount;
            payload->ball_count = ballCount;
            payload->concave_count = concaveCount;
            payload->convex_meshes = convexCount > 0 ? new CKMesh *[convexCount] : nullptr;
            payload->ball_centers = ballCount > 0 ? new VxVector[ballCount] : nullptr;
            payload->ball_radii = ballCount > 0 ? new float[ballCount] : nullptr;
            payload->concave_meshes = concaveCount > 0 ? new CKMesh *[concaveCount] : nullptr;

            int pos = CONVEX;
            for (int index = 0; index < convexCount; ++index) {
                payload->convex_meshes[index] = (CKMesh *) beh->GetInputParameterObject(pos + index);
            }
            pos += convexCount;

            for (int index = 0; index < ballCount; ++index) {
                beh->GetInputParameterValue(pos + 2 * index, &payload->ball_centers[index]);
                beh->GetInputParameterValue(pos + 2 * index + 1, &payload->ball_radii[index]);
            }
            pos += ballCount * 2;

            for (int index = 0; index < concaveCount; ++index) {
                payload->concave_meshes[index] = (CKMesh *) beh->GetInputParameterObject(pos + index);
            }

            beh->GetLocalParameterValue(3, &payload->mass_center);

            BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
            buffer.data = payload;
            buffer.size = sizeof(BML_LegacyPhysicalizePayload);
            buffer.cleanup = &CleanupPhysicalizePayload;
            bmlImcPublishBuffer(PhysicsHookState::s_TopicPhysicalize, &buffer);
        }
    } else {
        // Publish OnUnphysicalize event via IMC
        if (bmlImcPublish && PhysicsHookState::s_TopicUnphysicalize) {
            bmlImcPublish(
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

bool InitializePhysicsHook(CKContext *context) {
    if (!context)
        return false;
        
    PhysicsHookState::s_Context = context;
    
    // Register topics using loader API
    if (bmlImcGetTopicId) {
        bmlImcGetTopicId("Physics/Physicalize", &PhysicsHookState::s_TopicPhysicalize);
        bmlImcGetTopicId("Physics/Unphysicalize", &PhysicsHookState::s_TopicUnphysicalize);
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

    // Restore Physicalize BB
    CKBehaviorPrototype *physicalizeProto = CKGetPrototypeFromGuid(PHYSICS_RT_PHYSICALIZE);
    if (physicalizeProto && g_OriginalPhysicalize) {
        physicalizeProto->SetFunction(g_OriginalPhysicalize);
    }
}

void PhysicsPostProcess() {
    PhysicsHookState::PostProcessOriginal();
}

} // namespace BML_Physics
