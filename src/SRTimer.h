#ifndef BML_SRTIMER_H
#define BML_SRTIMER_H

class SRTimer {
public:
    SRTimer();
    ~SRTimer() = default;

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
    float m_Time;                          // Time in milliseconds
    bool m_Running;                        // Is timer running?
    mutable char m_FormattedTime[32] = {}; // Formatted time string
    mutable bool m_Dirty = true;           // Whether formatted string changed since last clear

    void UpdateFormattedTime() const;
};

#endif // BML_SRTIMER_H
