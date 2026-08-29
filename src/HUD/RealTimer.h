#ifndef BML_REALTIMER_H
#define BML_REALTIMER_H

#include <cstdint>
#include <chrono>

class RealTimer {
public:
    RealTimer() = default;
    ~RealTimer() = default;

    void Reset();
    void Start();
    void Pause();

    void Update(float deltaTime);

    float GetTime() const;
    const char *GetFormattedTime() const;

    bool IsRunning() const;
    bool IsDirty() const { return m_Dirty; }
    void ClearDirty() { m_Dirty = false; }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::nanoseconds;

    TimePoint m_StartTime;
    Duration m_Accumulated = Duration::zero();
    bool m_Running = false;
    mutable char m_FormattedTime[32] = {};
    mutable bool m_Dirty = true;

    Duration GetElapsed() const;
    void UpdateFormattedTime() const;
};

#endif // BML_REALTIMER_H
