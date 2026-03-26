/**
 * TimerMod - BML v0.4 Timer & IMC Publishing Example
 *
 * Demonstrates:
 * - Creating a 1-second repeating timer via Services().Timer()
 * - Logging "Tick N" on each timer callback
 * - Publishing the tick count to a custom IMC topic
 * - Cancelling the timer and reporting total ticks on detach
 */

#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_topics.h"

// Custom topic for this module's tick events
static constexpr const char *TOPIC_TIMER_TICK = "Example/Timer/Tick";

class TimerMod : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    BML_Timer   m_Timer     = nullptr;
    uint32_t    m_TickCount = 0;

    // IMC topic handle for publishing tick events
    bml::imc::Topic m_TickTopic;

public:
    BML_Result OnAttach(bml::ModuleServices &services) override {
        m_Subs = services.CreateSubscriptions();

        // Resolve the custom topic for publishing
        m_TickTopic = bml::imc::Topic(
            TOPIC_TIMER_TICK,
            services.Interfaces().ImcBus,
            Handle());

        // Schedule a repeating timer that fires every 1000 ms.
        // The callback is a plain C function; we pass `this` as user_data.
        m_Timer = Services().Timer().ScheduleRepeat(1000, &TimerMod::OnTimerTick, this);
        if (!m_Timer) {
            Services().Log().Error("Failed to create repeating timer");
            return BML_RESULT_FAIL;
        }

        Services().Log().Info("1-second repeating timer started");

        // Also subscribe to our own topic to prove the round-trip works
        m_Subs.Add(TOPIC_TIMER_TICK, [this](const bml::imc::Message &msg) {
            auto *count = msg.As<uint32_t>();
            if (count && *count % 10 == 0) {
                Services().Log().Info("[Subscriber] Received tick milestone: %u", *count);
            }
        });

        return BML_RESULT_OK;
    }

    void OnDetach() override {
        // Cancel the timer (also done automatically on module unload,
        // but explicit cancellation is good practice).
        if (m_Timer) {
            Services().Timer().Cancel(m_Timer);
            m_Timer = nullptr;
        }

        Services().Log().Info("Timer stopped. Total ticks: %u", m_TickCount);
    }

private:
    /**
     * Timer callback -- plain C function signature required by BML_TimerCallback.
     * We recover the module instance from user_data.
     */
    static void OnTimerTick(BML_Context /*ctx*/, BML_Timer /*timer*/, void *user_data) {
        auto *self = static_cast<TimerMod *>(user_data);
        self->m_TickCount++;
        self->Services().Log().Info("Tick %u", self->m_TickCount);

        // Publish the tick count so other modules can react
        self->m_TickTopic.Publish(self->m_TickCount);
    }
};

BML_DEFINE_MODULE(TimerMod)
