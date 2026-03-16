/**
 * @file ObjectLoadMod.cpp
 * @brief BML ObjectLoad Module - intercepts ObjectLoad behavior to notify mods.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_topics.h"
#include "bml_virtools.h"
#include "bml_virtools.hpp"
#include "ObjectLoadHook.h"

#include "CKContext.h"

class ObjectLoadMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bool m_HookInitialized = false;

public:
    bool EnsureHookReady(CKContext *context, const char *source) {
        if (!context) {
            return false;
        }

        if (!m_HookInitialized) {
            if (!BML_ObjectLoad::InitializeObjectLoadHook(context, Services())) {
                Services().Log().Warn("ObjectLoad hook initialization failed from %s", source ? source : "");
                return false;
            }

            m_HookInitialized = true;
            Services().Log().Info("ObjectLoad hook initialized from %s", source ? source : "");
        }

        BML_ObjectLoad::PublishInitialLoadSnapshot(context);
        return true;
    }

    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        Services().Log().Info("Initializing BML ObjectLoad Module");

        CKContext *context = bml::virtools::GetCKContext(Services());
        EnsureHookReady(context, "OnAttach");

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EngineInitEvent>(msg);
            if (!payload) {
                Services().Log().Warn("Engine/Init payload invalid for ObjectLoad module");
                return;
            }
            EnsureHookReady(payload->context, "Engine/Init");
        });

        m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EnginePlayEvent>(msg);
            if (!payload) return;
            EnsureHookReady(payload->context, "Engine/Play");
        });

        if (m_Subs.Empty()) {
            Services().Log().Error("Failed to subscribe to engine lifecycle events");
            return BML_RESULT_FAIL;
        }

        return BML_RESULT_OK;
    }

    void OnDetach() override {
        Services().Log().Info("Shutting down BML ObjectLoad Module");
        BML_ObjectLoad::ShutdownObjectLoadHook();
        m_HookInitialized = false;
    }
};

BML_DEFINE_MODULE(ObjectLoadMod)
