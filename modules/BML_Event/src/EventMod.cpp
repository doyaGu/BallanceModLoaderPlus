#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_topics.h"
#include "bml_virtools.h"
#include "bml_virtools.hpp"
#include "bml_virtools_payloads.h"
#include "EventTopics.h"
#include "EventHook.h"

class EventMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bool m_ScanCompleted = false;

    void EnsureHooksReady(CKContext *context, const char *source) {
        if (!context) {
            return;
        }

        if (!BML_Event::InitEventHooks(context, Services())) {
            Services().Log().Warn("Failed to initialize event hooks from %s", source ? source : "");
            return;
        }

        if (!m_ScanCompleted) {
            const int scanned = BML_Event::ScanLoadedScripts(context);
            if (scanned > 0) {
                m_ScanCompleted = true;
            }
        }
    }

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        Services().Log().Info("Initializing BML Event Module v0.4.0");

        CKContext *context = bml::virtools::GetCKContext(Services());
        EnsureHooksReady(context, "OnAttach");

        m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, [this](const bml::imc::Message &msg) {
            auto *payload = msg.As<BML_LegacyScriptLoadPayload>();
            if (!payload || !payload->script) return;

            CKContext *context = payload->script->GetCKContext();
            if (!context) return;
            BML_Event::InitEventHooks(context, Services());
            BML_Event::OnScriptLoaded(payload->filename, payload->script);
        });

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EngineInitEvent>(msg);
            if (!payload) return;
            EnsureHooksReady(payload->context, "Engine/Init");
        });

        m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EnginePlayEvent>(msg);
            if (!payload) return;
            EnsureHooksReady(payload->context, "Engine/Play");
        });

        if (m_Subs.Empty()) {
            Services().Log().Warn("Failed to subscribe to lifecycle events, event hooks may not work");
        }

        Services().Log().Info("BML Event Module initialized");
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        Services().Log().Info("Shutting down BML Event Module");
        m_ScanCompleted = false;
        BML_Event::ShutdownEventHooks();
    }
};

BML_DEFINE_MODULE(EventMod)
