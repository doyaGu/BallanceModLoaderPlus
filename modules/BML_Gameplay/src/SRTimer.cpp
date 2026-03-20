#include "SRTimer.h"
#include <cstdio>

SRTimer::SRTimer() : m_Time(0.0f), m_Running(false) {}

void SRTimer::Reset() {
    m_Time = 0.0f;
    m_Dirty = true;
}

void SRTimer::Start() {
    m_Running = true;
}

void SRTimer::Pause() {
    m_Running = false;
}

void SRTimer::Update(float deltaTime) {
    if (m_Running) {
        m_Time += deltaTime * 1000.0f; // Convert to milliseconds
        m_Dirty = true;
    }
}

float SRTimer::GetTime() const {
    return m_Time / 1000.0f; // Return in seconds
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

    float totalSeconds = m_Time / 1000.0f;
    int hours = (int)(totalSeconds / 3600.0f);
    int minutes = (int)((totalSeconds - hours * 3600.0f) / 60.0f);
    float seconds = totalSeconds - hours * 3600.0f - minutes * 60.0f;
    int milliseconds = (int)((seconds - (int)seconds) * 1000.0f);

    sprintf_s(m_FormattedTime, sizeof(m_FormattedTime), "  %02d:%02d:%02d.%03d", hours, minutes, (int)seconds, milliseconds);
    m_Dirty = false;
}
