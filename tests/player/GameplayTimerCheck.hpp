#pragma once

#include <BML/Engine.hpp>
#include <BML/Guids/TT_Toolbox_RT.h>
#include <BML/Speedrun.h>
#include <bit>
#include <stdexcept>

inline void CheckNativeGameplayTimer(const BML::Mod::Context &ctx, CKContext &engine) {
    namespace Speedrun = BML::Speedrun;
    auto *script = CKBehavior::Cast(engine.GetObjectByNameAndClass(
        const_cast<char *>("Gameplay_Energy"), CKCID_BEHAVIOR));
    if (!script) throw std::runtime_error("Gameplay_Energy missing");
    CKBehavior *timer = nullptr;
    for (int index = 0; index < script->GetSubBehaviorCount(); ++index) {
        auto *child = script->GetSubBehavior(index);
        if (child && child->GetPrototypeGuid() == TT_TOOLBOX_RT_TT_TIMER)
            timer = child;
    }
    if (!timer || !timer->GetFunction()) throw std::runtime_error("native gameplay timer missing");
    auto *reader = CKBehavior::Cast(engine.CreateObject(CKCID_BEHAVIOR));
    if (!reader || reader->InitFromPrototype(CKGetPrototypeFromGuid(TT_TOOLBOX_RT_TT_GETTIME)) != CK_OK ||
        !reader->GetFunction())
        throw std::runtime_error("cannot create native TT_GetTime");
    CKBehaviorContext timerFrame{};
    timerFrame.Context = &engine;
    timerFrame.TimeManager = engine.GetTimeManager();
    timerFrame.Behavior = timer;
    CKBehaviorContext readFrame = timerFrame;
    readFrame.Behavior = reader;

    const auto step = [&](unsigned inputs, float delta) {
        for (int index = 0; index < 4; ++index) {
            if (inputs & (1u << index))
                timer->ActivateInput(index);
        }
        timerFrame.DeltaTime = delta;
        timer->GetFunction()(timerFrame);
        reader->GetFunction()(readFrame);
        float nativeTime = 0.0f;
        Speedrun::TimerState actual{};
        if (reader->GetOutputParameterValue(0, &nativeTime) != CK_OK ||
            Speedrun::ReadTimerState(actual) != BML_OK ||
            std::bit_cast<uint32_t>(actual.ElapsedTime) != std::bit_cast<uint32_t>(nativeTime)) {
            throw std::runtime_error("TT_GetTime mismatch for input " + std::to_string(inputs) +
                                     " at " + std::to_string(nativeTime) + " ms, BML " +
                                     std::to_string(actual.ElapsedTime) + " ms");
        }
    };
    step(1u | 2u, 500.0f);  // On/Reset and Pause
    step(4u, 100.0f);       // Play
    for (int i = 0; i < 36000; ++i)
        step(0u, 16.67f);
    step(2u | 4u, 80.0f);  // Pause takes precedence, leaving Play pending
    step(0u, 90.0f);       // pending Play still leaves elapsed unchanged
    step(0u, 0.125f);
    step(8u, 90.0f);       // Off
    step(0u, 0.25f);      // reactivated TT_Timer reads the manager's Reset state
    step(1u, 0.75f);       // Reset alone includes its delta
    step(1u | 2u, 25.0f);

    for (int index = 0; index < 4; ++index)
        timer->ActivateOutput(index, FALSE);
    engine.DestroyObject(reader);
    (void)ctx.LogInfo("SRTimer PASS 36009 native TT_GetTime comparisons");
}
