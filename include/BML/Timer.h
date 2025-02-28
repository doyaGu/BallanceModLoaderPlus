#ifndef BML_TIMER_H
#define BML_TIMER_H

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

#undef min
#undef max

namespace BML {
    class Timer : public std::enable_shared_from_this<Timer> {
    public:
        // Type definitions
        typedef uint32_t TimerId;
        static const TimerId InvalidId = 0;

        // Callback types
        typedef std::function<void(Timer &)> OnceCallback;
        typedef std::function<bool(Timer &)> LoopCallback;
        typedef std::function<void(Timer &, float)> ProgressCallback;

        // Timer execution type
        enum Type {
            ONCE,    // Executes once and terminates
            LOOP,    // Executes repeatedly until manually stopped or callback returns false
            REPEAT,  // Executes a specific number of times
            INTERVAL // Executes at regular intervals with optional termination time
        };

        // Time measurement basis
        enum TimeBase {
            TICK,    // Integer-based tick counter
            TIME,    // Float-based time in seconds
            REALTIME // System real time (ignores game pauses/scaling)
        };

        // Timer state
        enum State {
            IDLE,      // Not yet started
            RUNNING,   // Currently running
            PAUSED,    // Temporarily paused
            COMPLETED, // Finished execution
            CANCELLED  // Manually cancelled
        };

        // Easing function type
        enum Easing {
            LINEAR,     // Constant rate
            EASE_IN,    // Accelerating
            EASE_OUT,   // Decelerating
            EASE_IN_OUT // Accelerating then decelerating
        };

        /**
         * Builder class for creating Timer instances with a fluent interface.
         */
        class Builder {
            std::string m_Name;

            union {
                size_t tick;
                float time;
            } m_Delay;

            bool m_IsTickBased;
            OnceCallback m_OnceCallback;
            LoopCallback m_LoopCallback;
            Type m_Type;
            TimeBase m_TimeBase;
            int m_RepeatCount;
            Easing m_Easing;
            ProgressCallback m_ProgressCallback;
            int8_t m_Priority;
            std::vector<std::string> m_Groups;
            bool m_HasProgressCallback;
            bool m_HasOnceCallback;
            bool m_HasLoopCallback;

        public:
            /**
             * Initialize the builder with default values
             */
            Builder() : m_Delay(),
                        m_IsTickBased(true),
                        m_Type(ONCE),
                        m_TimeBase(TICK),
                        m_RepeatCount(1),
                        m_Easing(LINEAR),
                        m_Priority(0),
                        m_HasProgressCallback(false),
                        m_HasOnceCallback(false),
                        m_HasLoopCallback(false) {
            }

            /**
             * Set the timer name
             */
            Builder &WithName(const std::string &name) {
                m_Name = name;
                return *this;
            }

            /**
             * Set the delay in ticks
             */
            Builder &WithDelayTicks(size_t ticks) {
                m_Delay.tick = ticks;
                m_IsTickBased = true;
                m_TimeBase = TICK;
                return *this;
            }

            /**
             * Set the delay in seconds
             */
            Builder &WithDelaySeconds(float seconds) {
                m_Delay.time = seconds;
                m_IsTickBased = false;
                m_TimeBase = TIME;
                return *this;
            }

            /**
             * Set a one-time callback
             */
            Builder &WithOnceCallback(OnceCallback callback) {
                m_OnceCallback = std::move(callback);
                m_HasOnceCallback = true;
                m_HasLoopCallback = false;
                return *this;
            }

            /**
             * Set a repeating callback
             */
            Builder &WithLoopCallback(LoopCallback callback) {
                m_LoopCallback = std::move(callback);
                m_HasLoopCallback = true;
                m_HasOnceCallback = false;
                return *this;
            }

            /**
             * Set the timer type
             */
            Builder &WithType(Type type) {
                m_Type = type;
                return *this;
            }

            /**
             * Set the time measurement basis
             */
            Builder &WithTimeBase(TimeBase timeBase) {
                m_TimeBase = timeBase;
                return *this;
            }

            /**
             * Set the number of repetitions for REPEAT type
             */
            Builder &WithRepeatCount(int count) {
                m_RepeatCount = count;
                return *this;
            }

            /**
             * Set the easing function
             */
            Builder &WithEasing(Easing easing) {
                m_Easing = easing;
                return *this;
            }

            /**
             * Set a progress callback
             */
            Builder &WithProgressCallback(ProgressCallback callback) {
                m_ProgressCallback = std::move(callback);
                m_HasProgressCallback = true;
                return *this;
            }

            /**
             * Set the timer priority
             */
            Builder &WithPriority(int8_t priority) {
                m_Priority = priority;
                return *this;
            }

            /**
             * Add the timer to a group
             */
            Builder &AddToGroup(const std::string &group) {
                m_Groups.push_back(group);
                return *this;
            }

            /**
             * Build the timer with current settings
             */
            std::shared_ptr<Timer> Build(size_t tick, float time);

            // Grant Timer access to private members
            friend class Timer;
        };

    private:
        // Timer identity
        TimerId m_Id;
        std::string m_Name;

        // Timer configuration
        Type m_Type;
        TimeBase m_TimeBase;
        State m_State;
        int8_t m_Priority;
        Easing m_Easing;

        // Time measurement union
        union TimeValue {
            size_t tick;
            float time;

            TimeValue() : tick(0) {
            }

            explicit TimeValue(size_t t) : tick(t) {
            }

            explicit TimeValue(float t) : time(t) {
            }
        };

        // Time tracking
        TimeValue m_Delay;
        TimeValue m_Start;
        TimeValue m_Pause;

        // Callbacks
        OnceCallback m_OnceCallback;
        LoopCallback m_LoopCallback;
        ProgressCallback m_ProgressCallback;
        bool m_HasOnceCallback;
        bool m_HasLoopCallback;
        bool m_HasProgressCallback;

        // Iteration tracking
        int m_TotalIterations;
        int m_RemainingIterations;
        int m_CompletedIterations;

        // Timer groups
        std::vector<std::string> m_Groups;

        // Private constructor - use Builder to create timers
        Timer(const Builder &builder, size_t tick, float time);

        // Helper methods
        float CalculateProgress(size_t tick, float time) const;
        float ApplyEasing(float linearProgress) const;
        void ExecuteProgressCallback(float progress);
        bool IsTimeToExecute(size_t tick, float time) const;
        void UpdateStartTime(size_t tick, float time);
        bool ExecuteCallback();

    public:
        // Destructor
        ~Timer();

        // Core timer processing
        bool Process(size_t tick, float time);

        // Timer control
        void Pause();
        void Resume();
        void Reset(size_t tick, float time);
        void Cancel();

        // Delay modification
        void SetDelay(size_t ticks);
        void SetDelay(float seconds);

        // Callback modification
        void SetCallback(OnceCallback callback);
        void SetCallback(LoopCallback callback);
        void SetProgressCallback(ProgressCallback callback);

        // Getters
        TimerId GetId() const { return m_Id; }
        const std::string &GetName() const { return m_Name; }
        State GetState() const { return m_State; }
        Type GetType() const { return m_Type; }
        TimeBase GetTimeBase() const { return m_TimeBase; }
        float GetProgress() const;
        int8_t GetPriority() const { return m_Priority; }
        int GetRemainingIterations() const { return m_RemainingIterations; }
        int GetCompletedIterations() const { return m_CompletedIterations; }

        static std::atomic<TimerId> &GetNextTimerId();
        static std::unordered_map<TimerId, std::shared_ptr<Timer>> &GetTimersMap();
        static std::unordered_map<std::string, std::vector<TimerId>> &GetTimerGroupsMap();

        static std::shared_ptr<Timer> FindById(TimerId id);
        static std::shared_ptr<Timer> FindByName(const std::string &name);
        static std::vector<std::shared_ptr<Timer>> FindByGroup(const std::string &group);
        static void CancelAll();
        static void PauseAll();
        static void ResumeAll();
        static size_t ProcessAll(size_t tick, float time);

        // Timer grouping
        void AddToGroup(const std::string &group);
        void RemoveFromGroup(const std::string &group);

        // Global time scaling
        static void SetTimeScale(float scale);
        static float &GetTimeScale();
    };

    // Convenience functions
    std::shared_ptr<Timer> Delay(size_t ticks, std::function<void()> callback, size_t currentTick);
    std::shared_ptr<Timer> Delay(float seconds, std::function<void()> callback, float currentTime);
    std::shared_ptr<Timer> Interval(size_t ticks, std::function<bool()> callback, size_t currentTick);
    std::shared_ptr<Timer> Interval(float seconds, std::function<bool()> callback, float currentTime);
    std::shared_ptr<Timer> Repeat(size_t ticks, int count, std::function<void()> callback, size_t currentTick);
    std::shared_ptr<Timer> Repeat(float seconds, int count, std::function<void()> callback, float currentTime);

    //----------------------------------------------------------------------
    // Implementation section - still in header but separated for clarity
    //----------------------------------------------------------------------

    //----------------------------------------------------------------------
    // Timer implementation from Builder
    //----------------------------------------------------------------------

    inline Timer::Timer(const Builder &builder, size_t tick, float time)
        : m_Id(GetNextTimerId().fetch_add(1)),
          m_Name(builder.m_Name.empty() ? "Timer_" + std::to_string(m_Id) : builder.m_Name),
          m_Type(builder.m_Type),
          m_TimeBase(builder.m_TimeBase),
          m_State(RUNNING),
          m_Priority(builder.m_Priority),
          m_Easing(builder.m_Easing),
          m_HasOnceCallback(builder.m_HasOnceCallback),
          m_HasLoopCallback(builder.m_HasLoopCallback),
          m_HasProgressCallback(builder.m_HasProgressCallback),
          m_Groups(builder.m_Groups) {
        // Set delay based on time base
        if (builder.m_IsTickBased) {
            m_Delay = TimeValue(builder.m_Delay.tick);
            m_Start = TimeValue(tick);
        } else {
            m_Delay = TimeValue(builder.m_Delay.time);
            m_Start = TimeValue(time);
        }

        // Set callbacks
        if (builder.m_HasOnceCallback) {
            m_OnceCallback = builder.m_OnceCallback;
        }

        if (builder.m_HasLoopCallback) {
            m_LoopCallback = builder.m_LoopCallback;
        }

        if (builder.m_HasProgressCallback) {
            m_ProgressCallback = builder.m_ProgressCallback;
        }

        // Set iteration counts based on timer type
        if (m_Type == REPEAT) {
            m_TotalIterations = builder.m_RepeatCount;
            m_RemainingIterations = builder.m_RepeatCount;
        } else if (m_Type == ONCE) {
            m_TotalIterations = 1;
            m_RemainingIterations = 1;
        } else {
            m_TotalIterations = -1; // Infinite
            m_RemainingIterations = -1;
        }

        m_CompletedIterations = 0;
    }

    inline std::shared_ptr<Timer> Timer::Builder::Build(size_t tick, float time) {
        std::shared_ptr<Timer> timer(new Timer(*this, tick, time));

        // Register the timer after it's been fully constructed
        GetTimersMap()[timer->m_Id] = timer;

        // Register with groups after construction
        for (const auto &group : timer->m_Groups) {
            GetTimerGroupsMap()[group].push_back(timer->m_Id);
        }

        return timer;
    }

    inline Timer::~Timer() {
        // Remove from all groups
        for (const auto &group : m_Groups) {
            auto &groupTimers = GetTimerGroupsMap()[group];
            groupTimers.erase(
                std::remove(groupTimers.begin(), groupTimers.end(), m_Id),
                groupTimers.end());

            // Remove empty groups
            if (groupTimers.empty()) {
                GetTimerGroupsMap().erase(group);
            }
        }

        // Remove from global timers map
        GetTimersMap().erase(m_Id);
    }

    //----------------------------------------------------------------------
    // Helper methods implementation
    //----------------------------------------------------------------------

    inline float Timer::CalculateProgress(size_t tick, float time) const {
        if (m_TimeBase == TICK) {
            size_t start = m_Start.tick;
            size_t delay = m_Delay.tick;
            size_t elapsed = tick - start;
            float linearProgress = static_cast<float>(elapsed) / static_cast<float>(delay);
            return ApplyEasing(std::max(0.0f, std::min(linearProgress, 1.0f)));
        } else {
            float start = m_Start.time;
            float delay = m_Delay.time;
            float elapsed = (time - start) * GetTimeScale();
            float linearProgress = elapsed / delay;
            return ApplyEasing(std::max(0.0f, std::min(linearProgress, 1.0f)));
        }
    }

    inline float Timer::ApplyEasing(float linearProgress) const {
        switch (m_Easing) {
        case LINEAR:
            return linearProgress;
        case EASE_IN:
            return linearProgress * linearProgress;
        case EASE_OUT:
            return linearProgress * (2.0f - linearProgress);
        case EASE_IN_OUT:
            return linearProgress < 0.5f
                       ? 2.0f * linearProgress * linearProgress
                       : -1.0f + (4.0f - 2.0f * linearProgress) * linearProgress;
        default:
            return linearProgress;
        }
    }

    inline void Timer::ExecuteProgressCallback(float progress) {
        if (m_HasProgressCallback) {
            m_ProgressCallback(*this, progress);
        }
    }

    inline bool Timer::IsTimeToExecute(size_t tick, float time) const {
        if (m_TimeBase == TICK) {
            return m_Start.tick + m_Delay.tick <= tick;
        } else {
            float adjustedDelay = m_Delay.time / GetTimeScale();
            return m_Start.time + adjustedDelay <= time;
        }
    }

    inline void Timer::UpdateStartTime(size_t tick, float time) {
        if (m_TimeBase == TICK) {
            m_Start.tick = tick;
        } else {
            m_Start.time = time;
        }
    }

    inline bool Timer::ExecuteCallback() {
        if (m_Type == ONCE || m_Type == REPEAT) {
            if (m_HasOnceCallback) {
                m_OnceCallback(*this);
                return false; // Once callbacks always complete the timer
            }
        } else if (m_Type == LOOP || m_Type == INTERVAL) {
            if (m_HasLoopCallback) {
                return m_LoopCallback(*this); // Loop callbacks determine if timer continues
            }
        }
        return false; // Default is to complete the timer
    }

    //----------------------------------------------------------------------
    // Core Timer process method
    //----------------------------------------------------------------------

    inline bool Timer::Process(size_t tick, float time) {
        if (m_State != RUNNING) {
            return m_State != COMPLETED && m_State != CANCELLED;
        }

        float progress = CalculateProgress(tick, time);
        if (m_HasProgressCallback) {
            ExecuteProgressCallback(progress);
        }

        if (IsTimeToExecute(tick, time)) {
            bool continueTimer = ExecuteCallback();

            if (m_Type == REPEAT && m_RemainingIterations > 0) {
                m_CompletedIterations++;
                m_RemainingIterations--;
            }

            // Handle continuation based on timer type
            if (m_Type == REPEAT) {
                if (m_RemainingIterations > 0) {
                    UpdateStartTime(tick, time);
                    return true;
                } else {
                    m_State = COMPLETED;
                    return false;
                }
            } else {
                if (continueTimer) {
                    UpdateStartTime(tick, time);
                    return true;
                } else {
                    m_State = COMPLETED;
                    return false;
                }
            }
        }

        return true;
    }

    //----------------------------------------------------------------------
    // Timer control methods
    //----------------------------------------------------------------------

    inline void Timer::Pause() {
        if (m_State == RUNNING) {
            m_State = PAUSED;
            // Store the current pause time
            if (m_TimeBase == TICK) {
                m_Pause.tick = m_Start.tick;
            } else {
                m_Pause.time = m_Start.time;
            }
        }
    }

    inline void Timer::Resume() {
        if (m_State == PAUSED) {
            m_State = RUNNING;
        }
    }

    inline void Timer::Reset(size_t tick, float time) {
        if (m_TimeBase == TICK) {
            m_Start.tick = tick;
        } else {
            m_Start.time = time;
        }

        m_State = RUNNING;
        m_RemainingIterations = m_TotalIterations;
        m_CompletedIterations = 0;
    }

    inline void Timer::Cancel() {
        m_State = CANCELLED;
    }

    //----------------------------------------------------------------------
    // Delay modification methods
    //----------------------------------------------------------------------

    inline void Timer::SetDelay(size_t ticks) {
        m_Delay.tick = ticks;
        m_TimeBase = TICK;
    }

    inline void Timer::SetDelay(float seconds) {
        m_Delay.time = seconds;
        m_TimeBase = TIME;
    }

    //----------------------------------------------------------------------
    // Callback modification methods
    //----------------------------------------------------------------------

    inline void Timer::SetCallback(OnceCallback callback) {
        m_OnceCallback = std::move(callback);
        m_HasOnceCallback = true;
        m_HasLoopCallback = false;
        m_Type = ONCE;
    }

    inline void Timer::SetCallback(LoopCallback callback) {
        m_LoopCallback = std::move(callback);
        m_HasLoopCallback = true;
        m_HasOnceCallback = false;
        m_Type = (m_Type == REPEAT) ? REPEAT : LOOP;
    }

    inline void Timer::SetProgressCallback(ProgressCallback callback) {
        m_ProgressCallback = std::move(callback);
        m_HasProgressCallback = true;
    }

    //----------------------------------------------------------------------
    // Getter methods
    //----------------------------------------------------------------------

    inline float Timer::GetProgress() const {
        if (m_State == COMPLETED) {
            return 1.0f;
        } else if (m_State == IDLE || m_State == CANCELLED) {
            return 0.0f;
        }

        // We don't have current time/tick values here
        // Return a reasonable approximation based on iterations
        if (m_Type == REPEAT && m_TotalIterations > 0) {
            return static_cast<float>(m_CompletedIterations) /
                static_cast<float>(m_TotalIterations);
        }

        return 0.5f; // Reasonable default without current time/tick
    }

    //----------------------------------------------------------------------
    // Static timer management methods
    //----------------------------------------------------------------------

    inline std::atomic<Timer::TimerId> &Timer::GetNextTimerId() {
        static std::atomic<TimerId> instance(1);
        return instance;
    }

    inline std::unordered_map<Timer::TimerId, std::shared_ptr<Timer>> & Timer::GetTimersMap() {
        static std::unordered_map<Timer::TimerId, std::shared_ptr<Timer>> instance;
        return instance;
    }

    inline std::unordered_map<std::string, std::vector<Timer::TimerId>> &Timer::GetTimerGroupsMap() {
        static std::unordered_map<std::string, std::vector<TimerId>> instance;
        return instance;
    }

    inline std::shared_ptr<Timer> Timer::FindById(TimerId id) {
        auto it = GetTimersMap().find(id);
        return (it != GetTimersMap().end()) ? it->second : nullptr;
    }

    inline std::shared_ptr<Timer> Timer::FindByName(const std::string &name) {
        for (const auto &pair : GetTimersMap()) {
            if (pair.second->GetName() == name) {
                return pair.second;
            }
        }
        return nullptr;
    }

    inline std::vector<std::shared_ptr<Timer>> Timer::FindByGroup(const std::string &group) {
        std::vector<std::shared_ptr<Timer>> result;
        auto it = GetTimerGroupsMap().find(group);

        if (it != GetTimerGroupsMap().end()) {
            result.reserve(it->second.size());
            for (TimerId id : it->second) {
                auto timerIt = GetTimersMap().find(id);
                if (timerIt != GetTimersMap().end()) {
                    result.push_back(timerIt->second);
                }
            }
        }

        return result;
    }

    inline void Timer::CancelAll() {
        for (auto &pair : GetTimersMap()) {
            pair.second->Cancel();
        }
    }

    inline void Timer::PauseAll() {
        for (auto &pair : GetTimersMap()) {
            pair.second->Pause();
        }
    }

    inline void Timer::ResumeAll() {
        for (auto &pair : GetTimersMap()) {
            pair.second->Resume();
        }
    }

    inline size_t Timer::ProcessAll(size_t tick, float time) {
        std::vector<TimerId> toRemove;
        toRemove.reserve(GetTimersMap().size() / 4); // Reasonable size assumption

        for (auto &pair : GetTimersMap()) {
            if (!pair.second->Process(tick, time)) {
                toRemove.push_back(pair.first);
            }
        }

        for (const auto id : toRemove) {
            GetTimersMap().erase(id);
        }

        return GetTimersMap().size();
    }

    //----------------------------------------------------------------------
    // Timer grouping methods
    //----------------------------------------------------------------------

    inline void Timer::AddToGroup(const std::string &group) {
        if (std::find(m_Groups.begin(), m_Groups.end(), group) == m_Groups.end()) {
            m_Groups.push_back(group);
            GetTimerGroupsMap()[group].push_back(m_Id);
        }
    }

    inline void Timer::RemoveFromGroup(const std::string &group) {
        auto it = std::find(m_Groups.begin(), m_Groups.end(), group);
        if (it != m_Groups.end()) {
            m_Groups.erase(it);

            auto &groupTimers = GetTimerGroupsMap()[group];
            groupTimers.erase(
                std::remove(groupTimers.begin(), groupTimers.end(), m_Id),
                groupTimers.end());

            // Remove empty groups
            if (groupTimers.empty()) {
                GetTimerGroupsMap().erase(group);
            }
        }
    }

    //----------------------------------------------------------------------
    // Global time scaling methods
    //----------------------------------------------------------------------

    inline void Timer::SetTimeScale(float scale) {
        GetTimeScale() = (scale > 0.0f) ? scale : 1.0f;
    }

    inline float &Timer::GetTimeScale() {
        static float instance = 1.0f;
        return instance;
    }

    //----------------------------------------------------------------------
    // Convenience functions implementation
    //----------------------------------------------------------------------

    inline std::shared_ptr<Timer> Delay(size_t ticks, std::function<void()> callback, size_t currentTick) {
        Timer::OnceCallback onceCb = [cb = std::move(callback)](Timer &) { cb(); };
        return Timer::Builder()
               .WithDelayTicks(ticks)
               .WithOnceCallback(onceCb)
               .WithType(Timer::ONCE)
               .WithTimeBase(Timer::TICK)
               .Build(currentTick, 0.0f);
    }

    inline std::shared_ptr<Timer> Delay(float seconds, std::function<void()> callback, float currentTime) {
        Timer::OnceCallback onceCb = [cb = std::move(callback)](Timer &) { cb(); };
        return Timer::Builder()
               .WithDelaySeconds(seconds)
               .WithOnceCallback(onceCb)
               .WithType(Timer::ONCE)
               .WithTimeBase(Timer::TIME)
               .Build(0, currentTime);
    }

    inline std::shared_ptr<Timer> Interval(size_t ticks, std::function<bool()> callback, size_t currentTick) {
        Timer::LoopCallback loopCb = [cb = std::move(callback)](Timer &) { return cb(); };
        return Timer::Builder()
               .WithDelayTicks(ticks)
               .WithLoopCallback(loopCb)
               .WithType(Timer::LOOP)
               .WithTimeBase(Timer::TICK)
               .Build(currentTick, 0.0f);
    }

    inline std::shared_ptr<Timer> Interval(float seconds, std::function<bool()> callback, float currentTime) {
        Timer::LoopCallback loopCb = [cb = std::move(callback)](Timer &) { return cb(); };
        return Timer::Builder()
               .WithDelaySeconds(seconds)
               .WithLoopCallback(loopCb)
               .WithType(Timer::LOOP)
               .WithTimeBase(Timer::TIME)
               .Build(0, currentTime);
    }

    inline std::shared_ptr<Timer> Repeat(size_t ticks, int count, std::function<void()> callback, size_t currentTick) {
        Timer::OnceCallback onceCb = [cb = std::move(callback)](Timer &) { cb(); };
        return Timer::Builder()
               .WithDelayTicks(ticks)
               .WithOnceCallback(onceCb)
               .WithType(Timer::REPEAT)
               .WithTimeBase(Timer::TICK)
               .WithRepeatCount(count)
               .Build(currentTick, 0.0f);
    }

    inline std::shared_ptr<Timer> Repeat(float seconds, int count, std::function<void()> callback, float currentTime) {
        Timer::OnceCallback onceCb = [cb = std::move(callback)](Timer &) { cb(); };
        return Timer::Builder()
               .WithDelaySeconds(seconds)
               .WithOnceCallback(onceCb)
               .WithType(Timer::REPEAT)
               .WithTimeBase(Timer::TIME)
               .WithRepeatCount(count)
               .Build(0, currentTime);
    }

    inline std::shared_ptr<Timer> RepeatUntil(size_t ticks, int count, std::function<bool()> callback, size_t currentTick) {
        Timer::LoopCallback loopCb = [cb = std::move(callback)](Timer&) {
            return cb();
        };

        return Timer::Builder()
            .WithDelayTicks(ticks)
            .WithLoopCallback(loopCb)
            .WithType(Timer::LOOP)
            .WithTimeBase(Timer::TICK)
            .WithRepeatCount(count)
            .Build(currentTick, 0.0f);
    }

    inline std::shared_ptr<Timer> RepeatUntil(float seconds, int count, std::function<bool()> callback, float currentTime) {
        Timer::LoopCallback loopCb = [cb = std::move(callback)](Timer&) {
            return cb();
        };

        return Timer::Builder()
            .WithDelaySeconds(seconds)
            .WithLoopCallback(loopCb)
            .WithType(Timer::LOOP)
            .WithTimeBase(Timer::TIME)
            .WithRepeatCount(count)
            .Build(0, currentTime);
    }
}

#endif // BML_TIMER_H
