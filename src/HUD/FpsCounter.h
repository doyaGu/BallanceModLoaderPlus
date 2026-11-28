#ifndef BML_FPSCOUNTER_H
#define BML_FPSCOUNTER_H

#include <cstdint>
#include <array>
#include <chrono>
#include <optional>

class FpsCounter {
public:
    using Clock = std::chrono::steady_clock;

    explicit FpsCounter(uint32_t sampleCount = 60);

    void RecordFrame(Clock::time_point now);
    void Reset();

    float GetAverageFps() const;
    const char *GetFormattedFps() const;
    bool IsDirty() const { return m_Dirty; }
    void ClearDirty() { m_Dirty = false; }

    void SetUpdateFrequency(uint32_t frames);
    uint32_t GetUpdateFrequency() const;

private:
    std::array<Clock::duration, 120> m_FrameTimes{};
    const uint32_t m_Capacity;
    std::optional<Clock::time_point> m_LastFrame;
    Clock::duration m_WindowDuration{};
    Clock::duration m_TimeSinceUpdate{};
    uint32_t m_FirstSample = 0;
    uint32_t m_Samples = 0;
    uint32_t m_FramesSinceUpdate = 0;
    uint32_t m_UpdateFrequency = 1;
    float m_CurrentAverageFps = 0.0f;
    char m_FormattedFps[16] = "FPS: --";
    int m_LastFpsInt = -1;
    bool m_Dirty = true;

    void DropOldestSample();
    void Publish();
};

#endif // BML_FPSCOUNTER_H
