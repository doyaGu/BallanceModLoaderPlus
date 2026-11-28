#include "HUD/FpsCounter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr auto SampleWindow = std::chrono::milliseconds(500);
constexpr auto MaxUpdateInterval = std::chrono::milliseconds(250);
}

FpsCounter::FpsCounter(uint32_t sampleCount)
    : m_Capacity(std::clamp(sampleCount, 1u, static_cast<uint32_t>(m_FrameTimes.size()))) {
}

void FpsCounter::RecordFrame(Clock::time_point now) {
    if (!m_LastFrame) {
        m_LastFrame = now;
        return;
    }
    if (now <= *m_LastFrame)
        return;

    const Clock::duration frameTime = now - *m_LastFrame;
    m_LastFrame = now;
    // Keep at least the last complete interval, even when one frame takes
    // longer than the smoothing window.
    while (m_Samples != 0 &&
           (m_Samples == m_Capacity || m_WindowDuration + frameTime > SampleWindow))
        DropOldestSample();
    m_FrameTimes[(m_FirstSample + m_Samples) % m_Capacity] = frameTime;
    ++m_Samples;
    m_WindowDuration += frameTime;

    ++m_FramesSinceUpdate;
    m_TimeSinceUpdate += frameTime;
    if (m_FramesSinceUpdate >= m_UpdateFrequency || m_TimeSinceUpdate >= MaxUpdateInterval)
        Publish();
}

void FpsCounter::Reset() {
    m_LastFrame.reset();
    m_WindowDuration = {};
    m_TimeSinceUpdate = {};
    m_FirstSample = 0;
    m_Samples = 0;
    m_FramesSinceUpdate = 0;
    m_CurrentAverageFps = 0.0f;
    m_LastFpsInt = -1;
    std::snprintf(m_FormattedFps, sizeof(m_FormattedFps), "FPS: --");
    m_Dirty = true;
}

void FpsCounter::DropOldestSample() {
    m_WindowDuration -= m_FrameTimes[m_FirstSample];
    m_FirstSample = (m_FirstSample + 1) % m_Capacity;
    --m_Samples;
}

void FpsCounter::Publish() {
    m_FramesSinceUpdate = 0;
    m_TimeSinceUpdate = {};
    const double seconds = std::chrono::duration<double>(m_WindowDuration).count();
    const double fps = m_Samples / seconds;
    m_CurrentAverageFps = static_cast<float>(fps);
    const int currentFps = static_cast<int>(std::lround(fps));
    if (currentFps == m_LastFpsInt)
        return;

    m_LastFpsInt = currentFps;
    std::snprintf(m_FormattedFps, sizeof(m_FormattedFps), "FPS: %d", currentFps);
    m_Dirty = true;
}

float FpsCounter::GetAverageFps() const {
    return m_CurrentAverageFps;
}

const char *FpsCounter::GetFormattedFps() const {
    return m_FormattedFps;
}

void FpsCounter::SetUpdateFrequency(uint32_t frames) {
    m_UpdateFrequency = (frames > 0) ? frames : 1;
}

uint32_t FpsCounter::GetUpdateFrequency() const {
    return m_UpdateFrequency;
}
