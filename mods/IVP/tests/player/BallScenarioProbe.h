#pragma once

#include <BML/IBML.h>

#include "GameplayInputDriver.h"
#include "PlayerBallLocator.h"

#include "CKAll.h"

#include <memory>
#include <utility>

namespace BML::PlayerTest {

// Hosts one Ball* scenario inside a probe Mod. Every IVP gameplay probe needs
// the same three things: the retail ball, gameplay input it can drive, and one
// Advance per frame until the scenario reports. Only the result formatting
// differs, so that stays in the probe Mod.
template <class Scenario>
class BallScenarioProbe {
public:
    using ResultType = decltype(std::declval<const Scenario &>().Result());

    enum class Step {
        // The scenario is still running. Nothing to report yet.
        Running,
        Done,
        // The level did not offer what the scenario needs.
        Failed,
    };

    // Replaces the input source the shipped Ball Navigation reads. Returns
    // false when the input manager is missing, which the probe reports instead
    // of silently running without control.
    bool Attach(IBML *bml) {
        CKContext *context = bml ? bml->GetCKContext() : nullptr;
        auto *input = context ? static_cast<CKInputManager *>(
            context->GetManagerByGuid(INPUT_MANAGER_GUID)) : nullptr;
        m_InputReady = m_Input.Attach(input);
        return m_InputReady;
    }

    void Detach() {
        m_Scenario.reset();
        m_Input.Detach();
    }

    Step Advance(IBML *bml) {
        if (!m_Scenario) {
            CK3dEntity *ball = ResolveRetailBall(bml);
            if (!ball || !m_InputReady)
                return Step::Failed;
            m_Input.SetEnabled(true);
            m_Scenario = std::make_unique<Scenario>(ball);
        }

        m_Scenario->Advance();
        m_Input.SetMask(m_Scenario->RequestedInputMask());
        if (!m_Scenario->Done())
            return Step::Running;

        m_Result = m_Scenario->Result();
        m_Scenario.reset();
        m_Input.SetMask(GameplayKeyNone);
        return Step::Done;
    }

    [[nodiscard]] const ResultType &Result() const { return m_Result; }

private:
    GameplayInputDriver m_Input;
    std::unique_ptr<Scenario> m_Scenario;
    ResultType m_Result;
    bool m_InputReady = false;
};

} // namespace BML::PlayerTest
