#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_config_bind.hpp"
#include "bml_game_topics.h"
#include "bml_virtools.h"
#include "bml_virtools.hpp"

#include "RenderHook.h"
#include "CKTimeManager.h"

class RenderMod : public bml::HookModule {
    bool m_WidescreenFix = false;
    bool m_UnlockFrameRate = false;
    int32_t m_MaxFrameRate = 0;
    bml::ConfigBindings m_Cfg;

    void ApplyConfig() {
        BML_Render::EnableWidescreenFix(m_WidescreenFix);

        auto *timeManager = bml::virtools::GetTimeManager(Services());
        if (!timeManager) return;

        if (m_UnlockFrameRate) {
            timeManager->ChangeLimitOptions(CK_FRAMERATE_FREE);
            return;
        }

        if (m_MaxFrameRate > 0) {
            timeManager->ChangeLimitOptions(CK_FRAMERATE_LIMIT);
            timeManager->SetFrameRateLimit(static_cast<float>(m_MaxFrameRate));
            return;
        }

        timeManager->ChangeLimitOptions(CK_FRAMERATE_SYNC);
    }

    const char *HookLogCategory() const override { return "BML_Render"; }

    bool InitHook(CKContext *, const BML_HookContext *hctx) override {
        if (!BML_Render::InitRenderHook(hctx)) return false;
        m_Cfg.Refresh(Services().Config());
        ApplyConfig();
        return true;
    }

    void ShutdownHook() override {
        BML_Render::ShutdownRenderHook();
    }

    BML_Result OnModuleAttach(bml::ModuleServices &) override {
        m_Cfg.Bind("Graphics", "WidescreenFix",   m_WidescreenFix,  false);
        m_Cfg.Bind("Graphics", "UnlockFrameRate",  m_UnlockFrameRate, false);
        m_Cfg.Bind("Graphics", "SetMaxFrameRate",  m_MaxFrameRate,    0);
        m_Cfg.Sync(Services().Config());
        ApplyConfig();

        m_Subs.Add(BML_TOPIC_ENGINE_END, [this](const bml::imc::Message &) {
            BML_Render::ShutdownRenderHook();
            m_HookReady = false;
        });

        m_Subs.Add(BML_TOPIC_GAME_MENU_POST_START, [this](const bml::imc::Message &) {
            m_Cfg.Refresh(Services().Config());
            ApplyConfig();
        });
        m_Subs.Add(BML_TOPIC_GAME_LEVEL_START, [this](const bml::imc::Message &) {
            m_Cfg.Refresh(Services().Config());
            ApplyConfig();
        });

        return BML_RESULT_OK;
    }
};

BML_DEFINE_MODULE(RenderMod)
