#include "FpsCounter.h"
#include <algorithm>
#include <cstdio>

FpsCounter::FpsCounter(uint32_t sampleCount)
    : m_SampleCount(std::min(sampleCount, (uint32_t) m_FrameTimes.size())),
      m_CurrentIndex(0),
      m_FrameCounter(0),
      m_UpdateFrequency(1),
      m_CurrentAverageFps(60.0f) {
}

void FpsCounter::Update(float frameTime) {
    m_FrameTimes[m_CurrentIndex] = frameTime;
    m_CurrentIndex = (m_CurrentIndex + 1) % m_SampleCount;
    m_FrameCounter++;

    if (m_FrameCounter >= m_UpdateFrequency) {
        m_FrameCounter = 0;
        RecalculateAverage();
        m_Dirty = true;
    }
}

void FpsCounter::RecalculateAverage() {
    float totalTime = 0.0f;
    for (uint32_t i = 0; i < m_SampleCount; ++i) {
        totalTime += m_FrameTimes[i];
    }
    m_CurrentAverageFps = m_SampleCount / totalTime;
}

float FpsCounter::GetAverageFps() const {
    return m_CurrentAverageFps;
}

const char *FpsCounter::GetFormattedFps() const {
    int currentFps = (int) m_CurrentAverageFps;
    if (currentFps != m_LastFpsInt) {
        const_cast<int &>(m_LastFpsInt) = currentFps;
        sprintf_s(m_FormattedFps, sizeof(m_FormattedFps), "FPS: %d", currentFps);
    }
    return m_FormattedFps;
}

void FpsCounter::SetUpdateFrequency(uint32_t frames) {
    m_UpdateFrequency = (frames > 0) ? frames : 1;
}

uint32_t FpsCounter::GetUpdateFrequency() const {
    return m_UpdateFrequency;
}
