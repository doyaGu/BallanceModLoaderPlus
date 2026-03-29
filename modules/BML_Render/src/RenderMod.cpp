/**
 * @file RenderMod.cpp
 * @brief BML_Render module entry point
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_hook.hpp"
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
    bml::HookRegistration m_RenderHookReg;
    bml::HookRegistration m_ProjectionHookReg;

    void ApplyConfig() {
        BML_Render::EnableWidescreenFix(m_WidescreenFix);

        auto *tm = bml::virtools::GetTimeManager(Services());
        if (!tm) return;

        if (m_UnlockFrameRate) {
            tm->ChangeLimitOptions(CK_FRAMERATE_FREE);
        } else if (m_MaxFrameRate > 0) {
            tm->ChangeLimitOptions(CK_FRAMERATE_LIMIT);
            tm->SetFrameRateLimit(static_cast<float>(m_MaxFrameRate));
        } else {
            tm->ChangeLimitOptions(CK_FRAMERATE_SYNC);
        }
    }

    const char *HookLogCategory() const override { return "BML_Render"; }

    bool InitHook(CKContext *) override {
        if (!BML_Render::InitRenderHook(Services())) return false;

        auto hooks = Services().Hooks();
        if (hooks) {
            auto addrs = BML_Render::GetHookAddresses();
            if (addrs.render)
                m_RenderHookReg = hooks.Register("CKRenderContext::Render", addrs.render);
            if (addrs.updateProjection)
                m_ProjectionHookReg = hooks.Register("CKRenderContext::UpdateProjection", addrs.updateProjection);
        }

        m_Cfg.Refresh(Services().Config());
        ApplyConfig();
        return true;
    }

    void ShutdownHook() override {
        m_RenderHookReg.Unregister();
        m_ProjectionHookReg.Unregister();
        BML_Render::ShutdownRenderHook();
    }

    BML_Result OnModuleAttach() override {
        m_Cfg.Bind("Graphics", "WidescreenFix",  m_WidescreenFix,  false);
        m_Cfg.Bind("Graphics", "UnlockFrameRate", m_UnlockFrameRate, false);
        m_Cfg.Bind("Graphics", "SetMaxFrameRate", m_MaxFrameRate,    0);
        m_Cfg.Sync(Services().Config());
        ApplyConfig();

        auto refreshCfg = [this](const bml::imc::Message &) {
            m_Cfg.Refresh(Services().Config());
            ApplyConfig();
        };
        m_Subs.Add(BML_TOPIC_GAME_MENU_POST_START, refreshCfg);
        m_Subs.Add(BML_TOPIC_GAME_LEVEL_START, refreshCfg);

        return BML_RESULT_OK;
    }
};

BML_DEFINE_MODULE(RenderMod)
