#ifndef BML_FPSCOUNTER_H
#define BML_FPSCOUNTER_H

#include <cstdint>
#include <array>

class FpsCounter {
public:
    explicit FpsCounter(uint32_t sampleCount = 60);
    ~FpsCounter() = default;

    void Update(float frameTime);

    float GetAverageFps() const;
    const char *GetFormattedFps() const;
    bool IsDirty() const { return m_Dirty; }
    void ClearDirty() { m_Dirty = false; }

    void SetUpdateFrequency(uint32_t frames);
    uint32_t GetUpdateFrequency() const;

private:
    std::array<float, 120> m_FrameTimes = {}; // Circular buffer of frame times
    uint32_t m_SampleCount;                   // Number of samples to average
    uint32_t m_CurrentIndex;                  // Current index in circular buffer
    uint32_t m_FrameCounter;                  // Frame counter for update frequency
    uint32_t m_UpdateFrequency;               // How often to update the average
    float m_CurrentAverageFps;                // Current average FPS
    mutable char m_FormattedFps[16] = {};     // Formatted FPS string
    int m_LastFpsInt = 60;                    // Last integer FPS used for string
    bool m_Dirty = true;                      // Whether formatted FPS changed since last clear

    void RecalculateAverage();
};

#endif // BML_FPSCOUNTER_H
