#include "SRTimer.h"

#include <cstdio>

void SRTimer::Reset() {
    m_Time = 0.0;
    m_Running = false;
    m_Dirty = true;
}

void SRTimer::Start() {
    m_Running = true;
}

void SRTimer::Pause() {
    m_Running = false;
}

void SRTimer::Update(float deltaTimeMs) {
    if (m_Running) {
        m_Time += (double)deltaTimeMs;
        m_Dirty = true;
    }
}

float SRTimer::GetTime() const {
    return (float)m_Time;
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

    int64_t totalMs = (int64_t)m_Time;

    int h = (int)(totalMs / 3600000);
    totalMs %= 3600000;
    int m = (int)(totalMs / 60000);
    totalMs %= 60000;
    int s = (int)(totalMs / 1000);
    int ms = (int)(totalMs % 1000);

    sprintf_s(m_FormattedTime, sizeof(m_FormattedTime), "  %02d:%02d:%02d.%03d", h, m, s, ms);
    m_Dirty = false;
}
