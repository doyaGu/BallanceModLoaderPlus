/**
 * @file PhysicsMod.cpp
 * @brief BML Physics Module - hooks IpionManager and Physicalize behavior.
 *
 * Intercepts the Virtools physics engine to publish IMC events when
 * objects are physicalized or unphysicalized.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_imc_topic.hpp"
#include "bml_virtools_payloads.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "CKAll.h"
#include "BML/Guids/physics_RT.h"
#include "VTables.h"
#include "HookUtils.h"

// -------------------------------------------------------------------------
// CKIpionManager interface (physics manager)
// -------------------------------------------------------------------------

class CKIpionManager : public CKBaseManager {
public:
    virtual void Reset() = 0;
};

struct CP_CLASS_VTABLE_NAME(CKIpionManager) : public CP_CLASS_VTABLE_NAME(CKBaseManager)<CKIpionManager> {
    CP_DECLARE_METHOD_PTR(CKIpionManager, void, Reset, ());
};

// -------------------------------------------------------------------------
// Physicalize BB input indices
// -------------------------------------------------------------------------

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

// -------------------------------------------------------------------------
// Module
// -------------------------------------------------------------------------

class PhysicsMod : public bml::HookModule {
    CKIpionManager *m_IpionManager = nullptr;
    CP_CLASS_VTABLE_NAME(CKIpionManager) m_IpionVTable = {};
    CKBEHAVIORFCT m_OriginalPhysicalize = nullptr;
    bml::imc::Topic m_TopicPhysicalize;
    bml::imc::Topic m_TopicUnphysicalize;

    static PhysicsMod *s_Instance;

    const char *HookLogCategory() const override { return "BML_Physics"; }

    bool InitHook(CKContext *ctx) override {
        if (!ctx) return false;

        auto *imcBus = Services().Interfaces().ImcBus;
        auto owner = Services().Handle();
        m_TopicPhysicalize = bml::imc::Topic("Physics/Physicalize", imcBus, owner);
        m_TopicUnphysicalize = bml::imc::Topic("Physics/Unphysicalize", imcBus, owner);

        // Hook IpionManager VTable
        m_IpionManager = (CKIpionManager *)ctx->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
        if (m_IpionManager) {
            utils::LoadVTable<CP_CLASS_VTABLE_NAME(CKIpionManager)>(m_IpionManager, m_IpionVTable);

#define HOOK_PHYSICS_VIRTUAL_METHOD(Instance, Name) \
    utils::HookVirtualMethod(Instance, &PhysicsVTableHook::CP_FUNC_HOOK_NAME(Name), \
        (offsetof(CP_CLASS_VTABLE_NAME(CKIpionManager), Name) / sizeof(void*)))

            HOOK_PHYSICS_VIRTUAL_METHOD(m_IpionManager, PostProcess);

#undef HOOK_PHYSICS_VIRTUAL_METHOD
        }

        // Hook Physicalize BB
        CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(PHYSICS_RT_PHYSICALIZE);
        if (proto) {
            if (!m_OriginalPhysicalize)
                m_OriginalPhysicalize = proto->GetFunction();
            s_Instance = this;
            proto->SetFunction(&PhysicalizeCallback);
        }

        return true;
    }

    void ShutdownHook() override {
        // Restore IpionManager VTable
        if (m_IpionManager)
            utils::SaveVTable<CP_CLASS_VTABLE_NAME(CKIpionManager)>(m_IpionManager, m_IpionVTable);

        // Restore Physicalize BB
        CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(PHYSICS_RT_PHYSICALIZE);
        if (proto && m_OriginalPhysicalize)
            proto->SetFunction(m_OriginalPhysicalize);

        s_Instance = nullptr;
        m_IpionManager = nullptr;
        m_IpionVTable = {};
        m_OriginalPhysicalize = nullptr;
        m_TopicPhysicalize = {};
        m_TopicUnphysicalize = {};
    }

    // -------------------------------------------------------------------------
    // IpionManager VTable hook
    // -------------------------------------------------------------------------

    struct PhysicsVTableHook {
        CP_DECLARE_METHOD_HOOK(CKERROR, PostProcess, ()) { return CK_OK; }
    };

    CKERROR PostProcessOriginal() {
        return CP_CALL_METHOD_PTR(m_IpionManager, m_IpionVTable.PostProcess);
    }

    // -------------------------------------------------------------------------
    // Physicalize BB callback
    // -------------------------------------------------------------------------

    static int PhysicalizeCallback(const CKBehaviorContext &behcontext) {
        if (s_Instance)
            return s_Instance->HandlePhysicalize(behcontext);
        return s_Instance->m_OriginalPhysicalize
            ? s_Instance->m_OriginalPhysicalize(behcontext) : CKBR_OK;
    }

    int HandlePhysicalize(const CKBehaviorContext &behcontext) {
        CKBehavior *beh = behcontext.Behavior;
        bool physicalize = beh->IsInputActive(0);
        auto *target = (CK3dEntity *)beh->GetTarget();

        if (physicalize) {
            if (m_TopicPhysicalize) {
                BML_PhysicalizeEvent event{};
                event.target = target;

                beh->GetInputParameterValue(FIXED, &event.fixed);
                beh->GetInputParameterValue(FRICTION, &event.friction);
                beh->GetInputParameterValue(ELASTICITY, &event.elasticity);
                beh->GetInputParameterValue(MASS, &event.mass);
                event.collision_group = (CKSTRING)beh->GetInputParameterReadDataPtr(COLLISION_GROUP);
                beh->GetInputParameterValue(START_FROZEN, &event.start_frozen);
                beh->GetInputParameterValue(ENABLE_COLLISION, &event.enable_collision);
                beh->GetInputParameterValue(AUTOMATIC_CALCULATE_MASS_CENTER, &event.auto_calc_mass_center);
                beh->GetInputParameterValue(LINEAR_SPEED_DAMPENING, &event.linear_speed_dampening);
                beh->GetInputParameterValue(ROT_SPEED_DAMPENING, &event.rot_speed_dampening);
                event.collision_surface = (CKSTRING)beh->GetInputParameterReadDataPtr(COLLISION_SURFACE);

                beh->GetLocalParameterValue(0, &event.convex_count);
                beh->GetLocalParameterValue(1, &event.ball_count);
                beh->GetLocalParameterValue(2, &event.concave_count);

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
                    convexMeshes[i] = (CKMesh *)beh->GetInputParameterObject(pos + i);
                pos += event.convex_count;

                for (int i = 0; i < event.ball_count; ++i) {
                    beh->GetInputParameterValue(pos + 2 * i, &ballCenters[i]);
                    beh->GetInputParameterValue(pos + 2 * i + 1, &ballRadii[i]);
                }
                pos += event.ball_count * 2;

                for (int i = 0; i < event.concave_count; ++i)
                    concaveMeshes[i] = (CKMesh *)beh->GetInputParameterObject(pos + i);

                event.convex_meshes = convexMeshes;
                event.ball_centers = ballCenters;
                event.ball_radii = ballRadii;
                event.concave_meshes = concaveMeshes;

                beh->GetLocalParameterValue(3, &event.mass_center);

                m_TopicPhysicalize.Publish(event);
            }
        } else {
            if (m_TopicUnphysicalize)
                m_TopicUnphysicalize.Publish(target);
        }

        return m_OriginalPhysicalize(behcontext);
    }
};

PhysicsMod *PhysicsMod::s_Instance = nullptr;

BML_DEFINE_MODULE(PhysicsMod)
