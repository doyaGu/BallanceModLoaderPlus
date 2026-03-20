/**
 * @file ObjectLoadMod.cpp
 * @brief BML ObjectLoad Module - intercepts ObjectLoad behavior to notify mods.
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_virtools.h"
#include "bml_virtools.hpp"
#include "ObjectLoadHook.h"

#include "CKContext.h"

class ObjectLoadMod : public bml::HookModule {
    const char *HookLogCategory() const override { return "BML_ObjectLoad"; }

    bool InitHook(CKContext *ctx, const BML_HookContext *hctx) override {
        return BML_ObjectLoad::InitializeObjectLoadHook(ctx, hctx);
    }

    void ShutdownHook() override {
        BML_ObjectLoad::ShutdownObjectLoadHook();
    }

    BML_Result OnModuleAttach(bml::ModuleServices &) override {
        // Eager init: try immediately if CKContext is already available
        CKContext *ctx = bml::virtools::GetCKContext(Services());
        TryInitHook(ctx);
        if (m_HookReady) {
            BML_ObjectLoad::PublishInitialLoadSnapshot(ctx);
        }

        // Retry on Engine/Play as well
        m_Subs.Add(BML_TOPIC_ENGINE_PLAY, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EnginePlayEvent>(msg);
            if (!payload) return;
            TryInitHook(payload->context);
            if (m_HookReady) {
                BML_ObjectLoad::PublishInitialLoadSnapshot(payload->context);
            }
        });

        return BML_RESULT_OK;
    }
};

BML_DEFINE_MODULE(ObjectLoadMod)
