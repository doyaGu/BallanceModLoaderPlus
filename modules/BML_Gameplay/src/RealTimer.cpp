#include "RealTimer.h"

#include <cstdio>

void RealTimer::Reset() {
    m_Accumulated = Duration::zero();
    m_StartTime = {};
    m_Running = false;
    m_Dirty = true;
}

void RealTimer::Start() {
    if (!m_Running) {
        m_StartTime = Clock::now();
        m_Running = true;
    }
}

void RealTimer::Pause() {
    if (m_Running) {
        m_Accumulated += GetElapsed();
        m_Running = false;
        m_Dirty = true;
    }
}

void RealTimer::Update(float /*deltaTime*/) {
    if (m_Running) {
        m_Dirty = true;
    }
}

RealTimer::Duration RealTimer::GetElapsed() const {
    return Clock::now() - m_StartTime;
}

float RealTimer::GetTime() const {
    auto total = m_Accumulated;
    if (m_Running)
        total += GetElapsed();
    return std::chrono::duration<float>(total).count();
}

bool RealTimer::IsRunning() const {
    return m_Running;
}

const char *RealTimer::GetFormattedTime() const {
    UpdateFormattedTime();
    return m_FormattedTime;
}

void RealTimer::UpdateFormattedTime() const {
    if (!m_Dirty) return;

    auto total = m_Accumulated;
    if (m_Running)
        total += GetElapsed();

    auto totalUs = std::chrono::duration_cast<std::chrono::microseconds>(total).count();

    int h = (int)(totalUs / 3600000000LL);
    totalUs %= 3600000000LL;
    int m = (int)(totalUs / 60000000LL);
    totalUs %= 60000000LL;
    int s = (int)(totalUs / 1000000LL);
    totalUs %= 1000000LL;
    int ms = (int)(totalUs / 1000LL);
    int us = (int)(totalUs % 1000LL);

    sprintf_s(m_FormattedTime, sizeof(m_FormattedTime), "  %02d:%02d:%02d.%03d%03d", h, m, s, ms, us);
    m_Dirty = false;
}
