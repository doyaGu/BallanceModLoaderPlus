/**
 * @file PhysicsMod.cpp
 * @brief BML Physics Module - hooks into Virtools IpionManager and Physicalize behaviors.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_topics.h"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "PhysicsHook.h"

class PhysicsMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bool m_HookReady = false;

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        Services().Log().Info("Initializing BML Physics Module v0.4.0");

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &msg) {
            if (m_HookReady) return;

            auto *payload = bml::ValidateEnginePayload<BML_EngineInitEvent>(msg);
            if (!payload) {
                Services().Log().Warn("Engine/Init payload invalid for physics module");
                return;
            }

            if (BML_Physics::InitializePhysicsHook(payload->context, Services())) {
                m_HookReady = true;
                Services().Log().Info("Physics hooks initialized on Engine/Init event");
            } else {
                Services().Log().Warn("Physics hook initialization failed, will retry on next Engine/Init");
            }
        });

        if (m_Subs.Empty()) {
            Services().Log().Error("Failed to subscribe to Engine/Init events");
            return BML_RESULT_FAIL;
        }

        return BML_RESULT_OK;
    }

    void OnDetach() override {
        Services().Log().Info("Shutting down BML Physics Module");
        BML_Physics::ShutdownPhysicsHook();
    }
};

BML_DEFINE_MODULE(PhysicsMod)
