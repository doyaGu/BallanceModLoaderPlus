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

#include <algorithm>

#include "CKAll.h"
#include "BML/Guids/physics_RT.h"
#include "physics_RT.h"
#include "HookUtils.h"

// CKBaseManager VTable slot indices (declaration order, 0-based):
//   0=Destructor, 1=SaveData, 2=LoadData, 3=PreClearAll, 4=PostClearAll,
//   5=PreProcess, 6=PostProcess, ...
static constexpr size_t kPostProcessSlot = 6;

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
    void *m_OriginalPostProcess = nullptr;
    CKBEHAVIORFCT m_OriginalPhysicalize = nullptr;
    bml::imc::Topic m_TopicPostProcess;
    bml::imc::Topic m_TopicPhysicalize;
    bml::imc::Topic m_TopicUnphysicalize;

    const char *HookLogCategory() const override { return "BML_Physics"; }

    bool InitHook(CKContext *ctx) override {
        if (!ctx) return false;

        InitPhysicsAddresses();

        auto *imcBus = Services().Interfaces().ImcBus;
        auto owner = Services().Handle();
        m_TopicPostProcess = bml::imc::Topic(BML_TOPIC_PHYSICS_POST_PROCESS, imcBus, owner);
        m_TopicPhysicalize = bml::imc::Topic(BML_TOPIC_PHYSICS_PHYSICALIZE, imcBus, owner);
        m_TopicUnphysicalize = bml::imc::Topic(BML_TOPIC_PHYSICS_UNPHYSICALIZE, imcBus, owner);

        // Hook IpionManager VTable
        m_IpionManager = (CKIpionManager *)ctx->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148));
        if (m_IpionManager) {
            m_OriginalPostProcess = utils::HookVirtualMethod(
                m_IpionManager, &PostProcessThunk::Hook, kPostProcessSlot);
        }

        if (!m_IpionManager) {
            Services().Log().Warn("CKIpionManager not found — physics hooks unavailable");
            return false;
        }

        // Hook Physicalize BB
        CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(PHYSICS_RT_PHYSICALIZE);
        if (!proto) {
            Services().Log().Warn("Physicalize prototype not found — physics hooks unavailable");
            return false;
        }

        if (!m_OriginalPhysicalize)
            m_OriginalPhysicalize = proto->GetFunction();
        proto->SetFunction(&PhysicalizeCallback);

        return true;
    }

    void ShutdownHook() override {
        // Restore IpionManager VTable
        if (m_IpionManager && m_OriginalPostProcess)
            utils::HookVirtualMethod(m_IpionManager, m_OriginalPostProcess, kPostProcessSlot);

        // Restore Physicalize BB
        CKBehaviorPrototype *proto = CKGetPrototypeFromGuid(PHYSICS_RT_PHYSICALIZE);
        if (proto && m_OriginalPhysicalize)
            proto->SetFunction(m_OriginalPhysicalize);

        m_IpionManager = nullptr;
        m_OriginalPostProcess = nullptr;
        m_OriginalPhysicalize = nullptr;
        m_TopicPostProcess = {};
        m_TopicPhysicalize = {};
        m_TopicUnphysicalize = {};
    }

    // -------------------------------------------------------------------------
    // IpionManager PostProcess hook
    // -------------------------------------------------------------------------

    struct PostProcessThunk {
        CKERROR Hook() {
            auto *self = bml::GetModuleInstance<PhysicsMod>();
            if (self)
                return self->OnPostProcess(reinterpret_cast<CKIpionManager *>(this));
            return CK_OK;
        }
    };

    CKERROR OnPostProcess(CKIpionManager *mgr) {
        using Fn = CKERROR (__thiscall *)(CKIpionManager *);
        CKERROR result = reinterpret_cast<Fn>(m_OriginalPostProcess)(mgr);

        if (m_TopicPostProcess) {
            BML_PhysicsStepEvent event{};
            event.delta_time = mgr->GetDeltaTime();
            m_TopicPostProcess.Publish(event);
        }

        return result;
    }

    // -------------------------------------------------------------------------
    // Physicalize BB callback
    // -------------------------------------------------------------------------

    static int PhysicalizeCallback(const CKBehaviorContext &behcontext) {
        auto *self = bml::GetModuleInstance<PhysicsMod>();
        if (self)
            return self->HandlePhysicalize(behcontext);
        return CKBR_OK;
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

BML_DEFINE_MODULE(PhysicsMod)
