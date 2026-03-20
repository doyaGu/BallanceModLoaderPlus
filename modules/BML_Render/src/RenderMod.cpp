#define BML_LOADER_IMPLEMENTATION
#include "bml_hook_module.hpp"
#include "bml_config.hpp"
#include "bml_game_topics.h"
#include "bml_virtools.h"
#include "bml_virtools.hpp"

#include "RenderHook.h"
#include "CKTimeManager.h"

class RenderMod : public bml::HookModule {
    bool m_WidescreenFix = false;
    bool m_UnlockFrameRate = false;
    int32_t m_MaxFrameRate = 0;

    void EnsureDefaultConfig() {
        auto config = Services().Config();
        if (!config.GetBool("Graphics", "WidescreenFix").has_value()) {
            config.SetBool("Graphics", "WidescreenFix", false);
        }
        if (!config.GetBool("Graphics", "UnlockFrameRate").has_value()) {
            config.SetBool("Graphics", "UnlockFrameRate", false);
        }
        if (!config.GetInt("Graphics", "SetMaxFrameRate").has_value()) {
            config.SetInt("Graphics", "SetMaxFrameRate", 0);
        }
    }

    void RefreshConfig() {
        auto config = Services().Config();
        m_WidescreenFix = config.GetBool("Graphics", "WidescreenFix").value_or(false);
        m_UnlockFrameRate = config.GetBool("Graphics", "UnlockFrameRate").value_or(false);
        m_MaxFrameRate = config.GetInt("Graphics", "SetMaxFrameRate").value_or(0);
    }

    void ApplyConfig() {
        BML_Render::EnableWidescreenFix(m_WidescreenFix);

        auto *timeManager = bml::virtools::GetTimeManager(Services());
        if (!timeManager) {
            return;
        }

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
        RefreshConfig();
        ApplyConfig();
        return true;
    }

    void ShutdownHook() override {
        BML_Render::ShutdownRenderHook();
    }

    BML_Result OnModuleAttach(bml::ModuleServices &) override {
        EnsureDefaultConfig();
        RefreshConfig();
        ApplyConfig();

        m_Subs.Add(BML_TOPIC_ENGINE_END, [this](const bml::imc::Message &) {
            BML_Render::ShutdownRenderHook();
            m_HookReady = false;
        });

        m_Subs.Add(BML_TOPIC_GAME_MENU_POST_START, [this](const bml::imc::Message &) {
            RefreshConfig();
            ApplyConfig();
        });
        m_Subs.Add(BML_TOPIC_GAME_LEVEL_START, [this](const bml::imc::Message &) {
            RefreshConfig();
            ApplyConfig();
        });

        return BML_RESULT_OK;
    }
};

BML_DEFINE_MODULE(RenderMod)
