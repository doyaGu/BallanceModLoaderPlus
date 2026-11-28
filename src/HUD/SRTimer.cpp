#include "HUD/SRTimer.h"

#include <cstdio>

void SRTimer::Reset() {
    m_Time = 0.0f;
    Pause();
    m_Dirty = true;
}

void SRTimer::Start(uint32_t gameTick) {
    m_Running = true;
    m_ManualClock = true;
    m_PlayTick = gameTick;
}

void SRTimer::Pause() {
    m_Running = false;
    m_PlayTick.reset();
}

void SRTimer::Update(uint32_t gameTick, float deltaTimeMs) {
    // Retail TT_Timer returns on the Play frame without adding its delta.
    if (!m_Running || m_PlayTick == gameTick)
        return;
    m_PlayTick.reset();
    Accumulate(deltaTimeMs);
}

void SRTimer::UpdateGameplay(unsigned inputs, float deltaTimeMs) {
    if (inputs != 0) {
        m_ManualClock = false;
        m_PlayTick.reset();
    } else if (m_ManualClock) {
        return;
    }

    // TT_Timer handles Reset first, then takes at most one of Pause, Play or Off.
    if (inputs & OnReset) {
        Reset();
        m_Running = true;
    }
    if (inputs & OnPause) {
        Pause();
    } else if (inputs & OnPlay) {
        m_Running = true;
    } else if (inputs & OnOff) {
        Reset();
        // Off stops the Behavior, while the native manager's Reset enables timing.
        m_Running = true;
    } else if (m_Running) {
        Accumulate(deltaTimeMs);
    }
}

void SRTimer::Accumulate(float deltaTimeMs) {
    m_Time += deltaTimeMs;
    m_Dirty = true;
}

float SRTimer::GetTime() const {
    return m_Time;
}

bool SRTimer::IsRunning() const {
    return m_Running;
}

const char *SRTimer::GetFormattedTime() const {
    UpdateFormattedTime();
    return m_FormattedTime;
}

void SRTimer::UpdateFormattedTime() const {
    if (!m_Dirty) return;

    int64_t totalMs = static_cast<int64_t>(GetTime());

    int h = (int)(totalMs / 3600000);
    totalMs %= 3600000;
    int m = (int)(totalMs / 60000);
    totalMs %= 60000;
    int s = (int)(totalMs / 1000);
    int ms = (int)(totalMs % 1000);

    std::snprintf(m_FormattedTime, sizeof(m_FormattedTime), "  %02d:%02d:%02d.%03d", h, m, s, ms);
    m_Dirty = false;
}
