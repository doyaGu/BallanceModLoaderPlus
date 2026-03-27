/**
 * EventLogger - Example BML v0.4 Module
 *
 * Demonstrates:
 * - Multi-topic IMC event subscription using SubscriptionManager
 * - Typed message access with bml::imc::Message::As<T>()
 * - Proper module lifecycle via bml::Module
 * - Logging to per-module log file
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_topics.h"
#include "bml_input.h"

class EventLoggerMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    uint32_t m_FrameCount = 0;
    uint32_t m_KeyDownCount = 0;

public:
    BML_Result OnAttach() override {
        m_Subs = Services().CreateSubscriptions();
        Services().Log().Info("=== EventLogger v1.0.0 Starting ===");

        m_Subs.Add(BML_TOPIC_ENGINE_INIT, [this](const bml::imc::Message &) {
            Services().Log().Info("[Event] Engine Init - Engine bridge is ready!");
            m_FrameCount = 0;
            m_KeyDownCount = 0;
        });

        m_Subs.Add(BML_TOPIC_ENGINE_POST_PROCESS, [this](const bml::imc::Message &msg) {
            m_FrameCount++;
            auto *dt = msg.As<float>();
            if (dt && m_FrameCount % 60 == 0) {
                Services().Log().Info("[Event] Frame %u - DeltaTime: %.4f ms",
                                      m_FrameCount,
                                      *dt * 1000.0f);
            }
        });

        // Render events (optional, non-fatal if missing)
        m_Subs.Add(BML_TOPIC_ENGINE_PRE_RENDER, [this](const bml::imc::Message &) {
            if (m_FrameCount <= 3) {
                Services().Log().Info("[Event] PreRender - Frame %u", m_FrameCount);
            }
        });

        m_Subs.Add(BML_TOPIC_ENGINE_POST_RENDER, [this](const bml::imc::Message &) {
            if (m_FrameCount <= 3) {
                Services().Log().Info("[Event] PostRender - Frame %u", m_FrameCount);
            }
        });

        // Input events
        m_Subs.Add(BML_TOPIC_INPUT_KEY_DOWN, [this](const bml::imc::Message &msg) {
            auto *event = msg.As<BML_KeyDownEvent>();
            if (event) {
                m_KeyDownCount++;
                Services().Log().Info("[Event] KeyDown - Code: 0x%02X (Count: %u)",
                                      event->key_code,
                                      m_KeyDownCount);
            }
        });

        if (m_Subs.Count() < 2) return BML_RESULT_FAIL;

        Services().Log().Info("Successfully subscribed to all events");
        return BML_RESULT_OK;
    }

    void OnDetach() override {
        Services().Log().Info("=== EventLogger Statistics ===");
        Services().Log().Info("Total frames processed: %u", m_FrameCount);
        Services().Log().Info("Total key presses logged: %u", m_KeyDownCount);
        Services().Log().Info("=== EventLogger Shutting Down ===");
    }
};

BML_DEFINE_MODULE(EventLoggerMod)
