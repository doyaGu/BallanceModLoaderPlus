#ifndef BML_TIMER_H
#define BML_TIMER_H

#include <functional>

#include "BML/Defines.h"

class Timer {
public:
    Timer(CKDWORD delay, std::function<void()> callback, CKDWORD tick, float time);
    Timer(CKDWORD delay, std::function<bool()> callback, CKDWORD tick, float time);
    Timer(float delay, std::function<void()> callback, CKDWORD tick, float time);
    Timer(float delay, std::function<bool()> callback, CKDWORD tick, float time);

    bool Process(CKDWORD tick, float time);

private:
    struct {
        std::function<void()> once;
        std::function<bool()> loop;
    } m_Callback;

    enum {
        TICK,
        TIME,
        ONCE,
        LOOP
    } m_DelayType, m_CallbackType;

    union {
        CKDWORD tick;
        float time;
    } m_Delay, m_Start;
};

#endif // BML_TIMER_H