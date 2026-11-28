#ifndef BML_SRTIMER_H
#define BML_SRTIMER_H

#include <cstdint>
#include <optional>

class SRTimer {
public:
    enum GameplayInput : unsigned {
        OnReset = 1u << 0,
        OnPause = 1u << 1,
        OnPlay = 1u << 2,
        OnOff = 1u << 3,
    };

    SRTimer() = default;
    ~SRTimer() = default;

    void Reset();
    void Start(uint32_t gameTick);
    void Pause();

    void Update(uint32_t gameTick, float deltaTimeMs);
    void UpdateGameplay(unsigned inputs, float deltaTimeMs);

    float GetTime() const;
    const char *GetFormattedTime() const;

    bool IsRunning() const;
    bool UsesManualClock() const { return m_ManualClock; }
    bool IsDirty() const { return m_Dirty; }

private:
    float m_Time = 0.0f;                 // Accumulated time in milliseconds
    bool m_Running = false;
    bool m_ManualClock = false;
    std::optional<uint32_t> m_PlayTick;
    mutable char m_FormattedTime[32] = {};
    mutable bool m_Dirty = true;

    void Accumulate(float deltaTimeMs);
    void UpdateFormattedTime() const;
};

#endif // BML_SRTIMER_H
