#include "Timer.h"

#include <utility>

Timer::Timer(CKDWORD delay, std::function<void()> callback, CKDWORD tick, float time) {
    m_CallbackType = ONCE;
    m_Callback.once = std::move(callback);
    m_DelayType = TICK;
    m_Delay.tick = delay;
    m_Start.tick = tick;
}

Timer::Timer(CKDWORD delay, std::function<bool()> callback, CKDWORD tick, float time) {
    m_CallbackType = LOOP;
    m_Callback.loop = std::move(callback);
    m_DelayType = TICK;
    m_Delay.tick = delay;
    m_Start.tick = tick;
}

Timer::Timer(float delay, std::function<void()> callback, CKDWORD tick, float time) {
    m_CallbackType = ONCE;
    m_Callback.once = std::move(callback);
    m_DelayType = TIME;
    m_Delay.time = delay;
    m_Start.time = time;
}

Timer::Timer(float delay, std::function<bool()> callback, CKDWORD tick, float time) {
    m_CallbackType = LOOP;
    m_Callback.loop = std::move(callback);
    m_DelayType = TIME;
    m_Delay.time = delay;
    m_Start.time = time;
}

bool Timer::Process(CKDWORD tick, float time) {
    if (m_DelayType == TICK) {
        if (m_Start.tick + m_Delay.tick <= tick) {
            if (m_CallbackType == ONCE) {
                m_Callback.once();
                return false;
            } else {
                m_Start.tick = tick;
                return m_Callback.loop();
            }
        }
    } else {
        if (m_Start.time + m_Delay.time <= time) {
            if (m_CallbackType == ONCE) {
                m_Callback.once();
                return false;
            } else {
                m_Start.time = time;
                return m_Callback.loop();
            }
        }
    }
    return true;
}