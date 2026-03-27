#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_virtools.h"
#include "bml_virtools.hpp"
#include "bml_virtools_payloads.h"
#include "EventTopics.h"
#include "EventHook.h"

class EventMod : public bml::HookModule {
    bool m_ScanCompleted = false;

    const char *HookLogCategory() const override { return "BML_Event"; }

    void EnsureScanned(CKContext *ctx) {
        if (m_ScanCompleted || !ctx) return;
        if (BML_Event::ScanLoadedScripts(ctx) > 0) {
            m_ScanCompleted = true;
        }
    }

    bool InitHook(CKContext *ctx) override {
        if (!BML_Event::InitEventHooks(ctx, Services())) return false;
        EnsureScanned(ctx);
        return true;
    }

    void ShutdownHook() override {
        BML_Event::ShutdownEventHooks();
    }

    BML_Result OnModuleAttach() override {
        // Eager init
        CKContext *ctx = bml::virtools::GetCKContext(Services());
        TryInitHook(ctx);

        m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, [this](const bml::imc::Message &msg) {
            auto *payload = msg.As<BML_ScriptLoadEvent>();
            if (!payload || !payload->script) return;

            CKContext *context = payload->script->GetCKContext();
            if (!context) return;
            TryInitHook(context);
            BML_Event::OnScriptLoaded(payload->filename, payload->script);
        });

        m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EnginePlayEvent>(msg);
            if (!payload) return;
            TryInitHook(payload->context);
        });

        return BML_RESULT_OK;
    }

    void OnModuleDetach() override {
        m_ScanCompleted = false;
    }
};

BML_DEFINE_MODULE(EventMod)
