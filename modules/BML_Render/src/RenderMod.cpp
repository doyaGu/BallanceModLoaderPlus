#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_builtin_interfaces.h"
#include "bml_config.hpp"
#include "bml_engine_events.h"
#include "bml_engine_events.hpp"
#include "bml_game_topics.h"
#include "bml_topics.h"
#include "bml_virtools.h"
#include "bml_virtools.hpp"

#include "RenderHook.h"
#include "CKTimeManager.h"

class RenderMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bool m_HookReady = false;
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

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        Services().Log().Info("Initializing BML Render Module v0.4.0");

        EnsureDefaultConfig();
        RefreshConfig();
        ApplyConfig();

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &msg) {
            auto *payload = bml::ValidateEnginePayload<BML_EngineInitEvent>(msg);
            if (!payload) {
                Services().Log().Warn("Engine/Init payload invalid for render module");
                return;
            }

            if (!m_HookReady && BML_Render::InitRenderHook(Services())) {
                m_HookReady = true;
                RefreshConfig();
                ApplyConfig();
                Services().Log().Info("Render hooks initialized on Engine/Init event");
            }
        });

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

        if (m_Subs.Count() < 4) {
            Services().Log().Error("Failed to subscribe to engine lifecycle events");
            return BML_RESULT_FAIL;
        }

        Services().Log().Info("BML Render Module initialized - waiting for Engine/Init");
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        Services().Log().Info("Shutting down BML Render Module");
        BML_Render::ShutdownRenderHook();
        m_HookReady = false;
    }
};

BML_DEFINE_MODULE(RenderMod)
