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
#include <mutex>

#undef max
#undef min

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
        typedef std::function<void()> SimpleCallback;

        // Timer execution type
        enum Type {
            ONCE,     // Executes once and terminates
            LOOP,     // Executes repeatedly until manually stopped or callback returns false
            REPEAT,   // Executes a specific number of times
            INTERVAL, // Executes at regular intervals with optional termination time
            DEBOUNCE, // Executes only after a quiet period
            THROTTLE  // Limits execution frequency
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

        class TimeValue {
        public:
            TimeValue() : m_IsTickBased(true) { m_Value.tick = 0; }
            explicit TimeValue(size_t ticks) : m_IsTickBased(true) { m_Value.tick = ticks; }
            explicit TimeValue(float seconds) : m_IsTickBased(false) { m_Value.time = seconds; }

            void SetTicks(size_t ticks) {
                m_IsTickBased = true;
                m_Value.tick = ticks;
            }

            void SetSeconds(float seconds) {
                m_IsTickBased = false;
                m_Value.time = seconds;
            }

            bool IsTickBased() const { return m_IsTickBased; }
            size_t GetTicks() const { return m_IsTickBased ? m_Value.tick : static_cast<size_t>(m_Value.time); }
            float GetSeconds() const { return m_IsTickBased ? static_cast<float>(m_Value.tick) : m_Value.time; }

        private:
            union {
                size_t tick;
                float time;
            } m_Value = {};

            bool m_IsTickBased;
        };

        /**
         * Builder class for creating Timer instances with a fluent interface.
         */
        class Builder {
        public:
            Builder() :
                m_Delay(TimeValue(static_cast<size_t>(0))),
                m_Type(ONCE),
                m_TimeBase(TICK),
                m_RepeatCount(1),
                m_Easing(LINEAR),
                m_Priority(0),
                m_HasProgressCallback(false),
                m_HasOnceCallback(false),
                m_HasLoopCallback(false),
                m_HasSimpleCallback(false),
                m_HasChainedTimer(false) {
            }

            Builder &WithName(const std::string &name) {
                m_Name = name;
                return *this;
            }

            Builder &WithDelayTicks(size_t ticks) {
                m_Delay = TimeValue(ticks);
                m_TimeBase = TICK;
                return *this;
            }

            Builder &WithDelaySeconds(float seconds) {
                m_Delay = TimeValue(seconds);
                m_TimeBase = TIME;
                return *this;
            }

            Builder &WithOnceCallback(OnceCallback callback) {
                m_OnceCallback = std::move(callback);
                m_HasOnceCallback = true;
                m_HasLoopCallback = false;
                m_HasSimpleCallback = false;
                return *this;
            }

            Builder &WithLoopCallback(LoopCallback callback) {
                m_LoopCallback = std::move(callback);
                m_HasLoopCallback = true;
                m_HasOnceCallback = false;
                m_HasSimpleCallback = false;
                return *this;
            }

            Builder &WithSimpleCallback(SimpleCallback callback) {
                m_SimpleCallback = std::move(callback);
                m_HasSimpleCallback = true;
                m_HasOnceCallback = false;
                m_HasLoopCallback = false;
                return *this;
            }

            Builder &WithType(Type type) {
                m_Type = type;
                return *this;
            }

            Builder &WithTimeBase(TimeBase timeBase) {
                m_TimeBase = timeBase;
                return *this;
            }

            Builder &WithRepeatCount(int count) {
                m_RepeatCount = count > 0 ? count : 1;
                return *this;
            }

            Builder &WithEasing(Easing easing) {
                m_Easing = easing;
                return *this;
            }

            Builder &WithProgressCallback(ProgressCallback callback) {
                m_ProgressCallback = std::move(callback);
                m_HasProgressCallback = true;
                return *this;
            }

            Builder &WithPriority(int8_t priority) {
                m_Priority = priority;
                return *this;
            }

            Builder &AddToGroup(const std::string &group) {
                if (!group.empty()) {
                    m_Groups.push_back(group);
                }
                return *this;
            }

            Builder &AsDebounced(size_t delayTicks) {
                m_Type = DEBOUNCE;
                m_Delay = TimeValue(delayTicks);
                m_TimeBase = TICK;
                return *this;
            }

            Builder &AsThrottled(size_t delayTicks) {
                m_Type = THROTTLE;
                m_Delay = TimeValue(delayTicks);
                m_TimeBase = TICK;
                return *this;
            }

            Builder &WithChainedTimer(std::function<Builder()> chainedTimerBuilder) {
                m_ChainedTimerBuilder = std::move(chainedTimerBuilder);
                m_HasChainedTimer = true;
                return *this;
            }

            std::shared_ptr<Timer> Build(size_t tick, float time) {
                std::shared_ptr<Timer> timer(new Timer(*this, tick, time));
                std::lock_guard<std::mutex> lock(GetMutex());
                GetTimersMap()[timer->m_Id] = timer;

                for (const auto &group : timer->m_Groups) {
                    GetTimerGroupsMap()[group].push_back(timer->m_Id);
                }

                return timer;
            }

        private:
            friend class Timer;

            std::string m_Name;
            TimeValue m_Delay;
            OnceCallback m_OnceCallback;
            LoopCallback m_LoopCallback;
            SimpleCallback m_SimpleCallback;
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
            bool m_HasSimpleCallback;
            std::function<Builder()> m_ChainedTimerBuilder;
            bool m_HasChainedTimer;
        };

        ~Timer() {
            // Only clean up group membership - don't try to start chained timers here
            std::lock_guard<std::mutex> lock(GetMutex());

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
        }

        bool Process(size_t tick, float time) {
            if (m_State != RUNNING) {
                return m_State != COMPLETED && m_State != CANCELLED;
            }

            float progress = CalculateProgress(tick, time);
            if (m_HasProgressCallback && m_ProgressCallback) {
                m_ProgressCallback(*this, progress);
            }

            if (IsTimeToExecute(tick, time)) {
                bool continueTimer = ExecuteCallback();

                // Update last execution time for THROTTLE
                if (m_Type == THROTTLE) {
                    m_LastExecutionTick = tick;
                    m_LastExecutionTime = time;
                }

                if (m_Type == REPEAT && m_RemainingIterations > 0) {
                    m_CompletedIterations++;
                    m_RemainingIterations--;
                }

                // Handle timer type specific behavior
                if (m_Type == DEBOUNCE) {
                    m_State = COMPLETED;
                    return false;
                } else if (m_Type == THROTTLE) {
                    return true; // Throttle timers always continue
                } else if (m_Type == REPEAT) {
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
            } else if (m_Type == DEBOUNCE) {
                // For DEBOUNCE, reset the start time if called before quiet period ends
                UpdateStartTime(tick, time);
            }

            return true;
        }

        // Timer control methods
        void Pause() {
            if (m_State == RUNNING) {
                m_State = PAUSED;
                // Store the current pause time
                if (m_TimeBase == TICK) {
                    m_Pause = TimeValue(m_Start.GetTicks());
                } else {
                    m_Pause = TimeValue(m_Start.GetSeconds());
                }
            }
        }

        void Resume() {
            if (m_State == PAUSED) {
                m_State = RUNNING;
            }
        }

        void Reset(size_t tick, float time) {
            if (m_TimeBase == TICK) {
                m_Start = TimeValue(tick);
            } else {
                m_Start = TimeValue(time);
            }

            m_State = RUNNING;
            m_RemainingIterations = m_TotalIterations;
            m_CompletedIterations = 0;
        }

        void Cancel() {
            m_State = CANCELLED;
        }

        // Delay modification methods
        void SetDelay(size_t ticks) {
            m_Delay = TimeValue(ticks);
            m_TimeBase = TICK;
        }

        void SetDelay(float seconds) {
            m_Delay = TimeValue(seconds);
            m_TimeBase = TIME;
        }

        // Callback modification methods
        void SetCallback(OnceCallback callback) {
            m_OnceCallback = std::move(callback);
            m_HasOnceCallback = true;
            m_HasLoopCallback = false;
            m_HasSimpleCallback = false;
            m_Type = ONCE;
        }

        void SetCallback(LoopCallback callback) {
            m_LoopCallback = std::move(callback);
            m_HasLoopCallback = true;
            m_HasOnceCallback = false;
            m_HasSimpleCallback = false;
            m_Type = (m_Type == REPEAT) ? REPEAT : LOOP;
        }

        void SetCallback(SimpleCallback callback) {
            m_SimpleCallback = std::move(callback);
            m_HasSimpleCallback = true;
            m_HasOnceCallback = false;
            m_HasLoopCallback = false;
        }

        void SetProgressCallback(ProgressCallback callback) {
            m_ProgressCallback = std::move(callback);
            m_HasProgressCallback = true;
        }

        // Getter methods
        TimerId GetId() const { return m_Id; }
        const std::string &GetName() const { return m_Name; }
        State GetState() const { return m_State; }
        Type GetType() const { return m_Type; }
        TimeBase GetTimeBase() const { return m_TimeBase; }
        int8_t GetPriority() const { return m_Priority; }
        int GetRemainingIterations() const { return m_RemainingIterations; }
        int GetCompletedIterations() const { return m_CompletedIterations; }

        float GetProgress() const {
            if (m_State == COMPLETED) {
                return 1.0f;
            } else if (m_State == IDLE || m_State == CANCELLED) {
                return 0.0f;
            }

            // For REPEAT type, use iteration count as an approximation
            if (m_Type == REPEAT && m_TotalIterations > 0) {
                return static_cast<float>(m_CompletedIterations) /
                    static_cast<float>(m_TotalIterations);
            }

            return 0.5f; // Default when we don't have current time/tick
        }

        // Timer grouping methods
        void AddToGroup(const std::string &group) {
            if (group.empty()) {
                return;
            }

            std::lock_guard<std::mutex> lock(GetMutex());

            if (std::find(m_Groups.begin(), m_Groups.end(), group) == m_Groups.end()) {
                m_Groups.push_back(group);
                GetTimerGroupsMap()[group].push_back(m_Id);
            }
        }

        void RemoveFromGroup(const std::string &group) {
            std::lock_guard<std::mutex> lock(GetMutex());

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

        // Static timer management methods
        static std::shared_ptr<Timer> FindById(TimerId id) {
            std::lock_guard<std::mutex> lock(GetMutex());

            auto it = GetTimersMap().find(id);
            return (it != GetTimersMap().end()) ? it->second : nullptr;
        }

        static std::shared_ptr<Timer> FindByName(const std::string &name) {
            std::lock_guard<std::mutex> lock(GetMutex());

            for (const auto &pair : GetTimersMap()) {
                if (pair.second->GetName() == name) {
                    return pair.second;
                }
            }
            return nullptr;
        }

        static std::vector<std::shared_ptr<Timer>> FindByGroup(const std::string &group) {
            std::lock_guard<std::mutex> lock(GetMutex());

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

        static void CancelAll() {
            std::lock_guard<std::mutex> lock(GetMutex());

            for (auto &pair : GetTimersMap()) {
                pair.second->Cancel();
            }
        }

        static void PauseAll() {
            std::lock_guard<std::mutex> lock(GetMutex());

            for (auto &pair : GetTimersMap()) {
                pair.second->Pause();
            }
        }

        static void ResumeAll() {
            std::lock_guard<std::mutex> lock(GetMutex());

            for (auto &pair : GetTimersMap()) {
                pair.second->Resume();
            }
        }

        static size_t ProcessAll(size_t tick, float time) {
            // First, make a copy of all timers to process safely
            std::vector<std::shared_ptr<Timer>> timersToProcess;
            std::vector<TimerId> toRemove;

            // Collect all timers under lock
            {
                std::lock_guard<std::mutex> lock(GetMutex());
                timersToProcess.reserve(GetTimersMap().size());
                for (const auto &pair : GetTimersMap()) {
                    timersToProcess.push_back(pair.second);
                }
            }

            // Process each timer without holding the lock
            for (auto &timer : timersToProcess) {
                bool keepTimer = true;
                try {
                    keepTimer = timer->Process(tick, time);
                } catch (...) {
                    // If any exception occurs, mark timer for removal
                    keepTimer = false;
                }

                if (!keepTimer) {
                    toRemove.push_back(timer->GetId());
                }
            }

            // Phase 2: Remove completed timers and handle chaining
            if (!toRemove.empty()) {
                std::vector<std::shared_ptr<Timer>> timersToStart;

                // Remove timers under lock
                {
                    std::lock_guard<std::mutex> lock(GetMutex());
                    for (const auto &id : toRemove) {
                        auto it = GetTimersMap().find(id);
                        if (it != GetTimersMap().end()) {
                            // Check if there's a chained timer to start
                            auto timer = it->second;
                            if (timer->m_State == COMPLETED && timer->m_HasChainedTimer) {
                                if (auto nextTimer = timer->m_NextTimer.lock()) {
                                    timersToStart.push_back(nextTimer);
                                } else if (timer->m_ChainedTimerBuilder) {
                                    // We'll handle this outside the lock
                                }
                            }

                            // Now safe to remove
                            GetTimersMap().erase(id);
                        }
                    }
                }

                // Start any chained timers outside the lock
                for (auto &timer : timersToStart) {
                    timer->m_State = RUNNING;
                    if (timer->m_TimeBase == TICK) {
                        timer->m_Start = TimeValue(tick);
                    } else {
                        timer->m_Start = TimeValue(time);
                    }
                }

                // Handle any builders that need to create new timers
                for (auto &id : toRemove) {
                    auto timerIt = std::find_if(timersToProcess.begin(), timersToProcess.end(),
                                                [id](const std::shared_ptr<Timer> &t) { return t->GetId() == id; });

                    if (timerIt != timersToProcess.end() &&
                        (*timerIt)->m_State == COMPLETED &&
                        (*timerIt)->m_HasChainedTimer &&
                        (*timerIt)->m_NextTimer.expired() &&
                        (*timerIt)->m_ChainedTimerBuilder) {
                        try {
                            auto builder = (*timerIt)->m_ChainedTimerBuilder();
                            builder.Build(tick, time);
                        } catch (...) {
                            // Ignore exceptions from builder
                        }
                    }
                }
            }

            // Return the count of remaining timers
            std::lock_guard<std::mutex> lock(GetMutex());
            return GetTimersMap().size();
        }

        // Global time scaling methods
        static void SetTimeScale(float scale) {
            GetTimeScale() = (scale > 0.0f) ? scale : 1.0f;
        }

        static float &GetTimeScale() {
            static float instance = 1.0f;
            return instance;
        }

        static std::shared_ptr<Timer> Chain(std::shared_ptr<Timer> first, std::shared_ptr<Timer> second) {
            if (first && second) {
                first->m_ChainedTimerBuilder = [second]() {
                    Builder builder;
                    // The builder is just a placeholder - we'll use the existing second timer
                    return builder;
                };
                first->m_HasChainedTimer = true;
                first->m_NextTimer = second;
            }
            return second;
        }

    private:
        // Private constructor - use Builder to create timers
        Timer(const Builder &builder, size_t tick, float time)
            : m_Id(GetNextTimerId().fetch_add(1)),
              m_Name(builder.m_Name.empty() ? "Timer_" + std::to_string(m_Id) : builder.m_Name),
              m_Type(builder.m_Type),
              m_TimeBase(builder.m_TimeBase),
              m_State(RUNNING),
              m_Priority(builder.m_Priority),
              m_Easing(builder.m_Easing),
              m_HasOnceCallback(builder.m_HasOnceCallback),
              m_HasLoopCallback(builder.m_HasLoopCallback),
              m_HasSimpleCallback(builder.m_HasSimpleCallback),
              m_HasProgressCallback(builder.m_HasProgressCallback),
              m_Groups(builder.m_Groups),
              m_ChainedTimerBuilder(builder.m_ChainedTimerBuilder),
              m_HasChainedTimer(builder.m_HasChainedTimer),
              // Initialize with 0 for throttle to indicate first execution
              m_LastExecutionTick(0),
              m_LastExecutionTime(0.0f) {
            // Set delay based on time base
            m_Delay = builder.m_Delay;

            // Set start time
            if (m_TimeBase == TICK) {
                m_Start = TimeValue(tick);
            } else {
                m_Start = TimeValue(time);
            }

            // Set callbacks
            if (builder.m_HasOnceCallback) {
                m_OnceCallback = builder.m_OnceCallback;
            }

            if (builder.m_HasLoopCallback) {
                m_LoopCallback = builder.m_LoopCallback;
            }

            if (builder.m_HasSimpleCallback) {
                m_SimpleCallback = builder.m_SimpleCallback;
            }

            if (builder.m_HasProgressCallback) {
                m_ProgressCallback = builder.m_ProgressCallback;
            }

            // Set iteration counts based on timer type
            if (m_Type == REPEAT) {
                m_TotalIterations = builder.m_RepeatCount;
                m_RemainingIterations = builder.m_RepeatCount;
            } else if (m_Type == ONCE || m_Type == DEBOUNCE) {
                m_TotalIterations = 1;
                m_RemainingIterations = 1;
            } else {
                m_TotalIterations = -1; // Infinite
                m_RemainingIterations = -1;
            }

            m_CompletedIterations = 0;
        }

        // Helper methods
        float CalculateProgress(size_t tick, float time) const {
            if (m_TimeBase == TICK) {
                size_t start = m_Start.GetTicks();
                size_t delay = m_Delay.GetTicks();
                size_t elapsed = tick >= start ? tick - start : 0;
                float linearProgress = delay > 0
                                           ? static_cast<float>(elapsed) / static_cast<float>(delay)
                                           : 1.0f;
                return ApplyEasing(std::max(0.0f, std::min(linearProgress, 1.0f)));
            } else {
                float start = m_Start.GetSeconds();
                float delay = m_Delay.GetSeconds();
                float elapsed = time >= start ? (time - start) * GetTimeScale() : 0.0f;
                float linearProgress = delay > 0.0f ? elapsed / delay : 1.0f;
                return ApplyEasing(std::max(0.0f, std::min(linearProgress, 1.0f)));
            }
        }

        float ApplyEasing(float linearProgress) const {
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

        bool IsTimeToExecute(size_t tick, float time) const {
            if (m_Type == THROTTLE) {
                // For throttle, special handling:
                // 1. First execution happens immediately only once
                // 2. Subsequent executions respect the throttle period

                if (m_TimeBase == TICK) {
                    // If this is the first call (lastExecution equals start time)
                    if (m_LastExecutionTick == 0) {
                        return true;
                    }

                    // Otherwise, check if enough time has passed since last execution
                    return (tick - m_LastExecutionTick) >= m_Delay.GetTicks();
                } else {
                    // Time-based version
                    if (m_LastExecutionTime == 0.0f) {
                        return true;
                    }

                    return (time - m_LastExecutionTime) * GetTimeScale() >= m_Delay.GetSeconds();
                }
            } else if (m_Type == DEBOUNCE) {
                // Debounce implementation...
                if (m_TimeBase == TICK) {
                    return (tick - m_Start.GetTicks()) >= m_Delay.GetTicks();
                } else {
                    return (time - m_Start.GetSeconds()) * GetTimeScale() >= m_Delay.GetSeconds();
                }
            } else {
                // Normal timer behavior
                if (m_TimeBase == TICK) {
                    return m_Start.GetTicks() + m_Delay.GetTicks() <= tick;
                } else {
                    float adjustedDelay = m_Delay.GetSeconds() / GetTimeScale();
                    return m_Start.GetSeconds() + adjustedDelay <= time;
                }
            }
        }

        void UpdateStartTime(size_t tick, float time) {
            if (m_TimeBase == TICK) {
                m_Start = TimeValue(tick);
            } else {
                m_Start = TimeValue(time);
            }
        }

        bool ExecuteCallback() {
            bool continueTimer = false;

            if (m_Type == ONCE || m_Type == REPEAT || m_Type == DEBOUNCE) {
                if (m_HasOnceCallback && m_OnceCallback) {
                    m_OnceCallback(*this);
                } else if (m_HasSimpleCallback && m_SimpleCallback) {
                    m_SimpleCallback();
                }
                // These types don't continue unless explicitly set up as REPEAT
                continueTimer = (m_Type == REPEAT && m_RemainingIterations > 1);
            } else if (m_Type == LOOP || m_Type == INTERVAL || m_Type == THROTTLE) {
                if (m_HasLoopCallback && m_LoopCallback) {
                    continueTimer = m_LoopCallback(*this);
                } else if (m_HasSimpleCallback && m_SimpleCallback) {
                    m_SimpleCallback();
                    continueTimer = true; // Simple callbacks in loop timers continue by default
                }
            }

            return continueTimer;
        }

        // Static helper methods
        static std::atomic<TimerId> &GetNextTimerId() {
            static std::atomic<TimerId> instance(1);
            return instance;
        }

        static std::unordered_map<TimerId, std::shared_ptr<Timer>> &GetTimersMap() {
            static std::unordered_map<TimerId, std::shared_ptr<Timer>> instance;
            return instance;
        }

        static std::unordered_map<std::string, std::vector<TimerId>> &GetTimerGroupsMap() {
            static std::unordered_map<std::string, std::vector<TimerId>> instance;
            return instance;
        }

        static std::mutex &GetMutex() {
            static std::mutex instance;
            return instance;
        }

        // Timer identity
        TimerId m_Id;
        std::string m_Name;

        // Timer configuration
        Type m_Type;
        TimeBase m_TimeBase;
        State m_State;
        int8_t m_Priority;
        Easing m_Easing;

        // Time tracking
        TimeValue m_Delay;
        TimeValue m_Start;
        TimeValue m_Pause;
        size_t m_LastExecutionTick;
        float m_LastExecutionTime;

        // Callbacks
        OnceCallback m_OnceCallback;
        LoopCallback m_LoopCallback;
        SimpleCallback m_SimpleCallback;
        ProgressCallback m_ProgressCallback;
        bool m_HasOnceCallback;
        bool m_HasLoopCallback;
        bool m_HasSimpleCallback;
        bool m_HasProgressCallback;

        // Iteration tracking
        int m_TotalIterations;
        int m_RemainingIterations;
        int m_CompletedIterations;

        // Timer groups
        std::vector<std::string> m_Groups;

        // Timer chaining
        std::function<Builder()> m_ChainedTimerBuilder;
        bool m_HasChainedTimer;
        std::weak_ptr<Timer> m_NextTimer;
    };

    inline std::shared_ptr<Timer> Delay(size_t ticks, std::function<void()> callback, size_t currentTick) {
        return Timer::Builder()
               .WithDelayTicks(ticks)
               .WithSimpleCallback(std::move(callback))
               .WithType(Timer::ONCE)
               .Build(currentTick, 0.0f);
    }

    inline std::shared_ptr<Timer> Delay(float seconds, std::function<void()> callback, float currentTime) {
        return Timer::Builder()
               .WithDelaySeconds(seconds)
               .WithSimpleCallback(std::move(callback))
               .WithType(Timer::ONCE)
               .Build(0, currentTime);
    }

    inline std::shared_ptr<Timer> Interval(size_t ticks, std::function<bool()> callback, size_t currentTick) {
        return Timer::Builder()
               .WithDelayTicks(ticks)
               .WithLoopCallback([cb = std::move(callback)](Timer &) {
                   return cb();
               })
               .WithType(Timer::LOOP)
               .Build(currentTick, 0.0f);
    }

    inline std::shared_ptr<Timer> Interval(float seconds, std::function<bool()> callback, float currentTime) {
        return Timer::Builder()
               .WithDelaySeconds(seconds)
               .WithLoopCallback([cb = std::move(callback)](Timer &) {
                   return cb();
               })
               .WithType(Timer::LOOP)
               .Build(0, currentTime);
    }

    inline std::shared_ptr<Timer> Repeat(size_t ticks, int count, std::function<void()> callback, size_t currentTick) {
        return Timer::Builder()
               .WithDelayTicks(ticks)
               .WithSimpleCallback(std::move(callback))
               .WithType(Timer::REPEAT)
               .WithRepeatCount(count)
               .Build(currentTick, 0.0f);
    }

    inline std::shared_ptr<Timer> Repeat(float seconds, int count, std::function<void()> callback, float currentTime) {
        return Timer::Builder()
               .WithDelaySeconds(seconds)
               .WithSimpleCallback(std::move(callback))
               .WithType(Timer::REPEAT)
               .WithRepeatCount(count)
               .Build(0, currentTime);
    }

    inline std::shared_ptr<Timer> RepeatUntil(size_t ticks, int count, std::function<bool()> callback,
                                              size_t currentTick) {
        return Timer::Builder()
               .WithDelayTicks(ticks)
               .WithLoopCallback([cb = std::move(callback)](Timer &) {
                   return cb(); // Return callback result to determine if we continue
               })
               .WithType(Timer::LOOP)
               .WithRepeatCount(count)
               .Build(currentTick, 0.0f);
    }

    inline std::shared_ptr<Timer> RepeatUntil(float seconds, int count, std::function<bool()> callback,
                                              float currentTime) {
        return Timer::Builder()
               .WithDelaySeconds(seconds)
               .WithLoopCallback([cb = std::move(callback)](Timer &) {
                   return cb(); // Return callback result to determine if we continue
               })
               .WithType(Timer::LOOP)
               .WithRepeatCount(count)
               .Build(0, currentTime);
    }

    inline std::shared_ptr<Timer> Debounce(size_t ticks, std::function<void()> callback, size_t currentTick) {
        return Timer::Builder()
               .WithDelayTicks(ticks)
               .WithSimpleCallback(std::move(callback))
               .WithType(Timer::DEBOUNCE)
               .Build(currentTick, 0.0f);
    }

    inline std::shared_ptr<Timer> Throttle(size_t ticks, std::function<void()> callback, size_t currentTick) {
        return Timer::Builder()
               .WithDelayTicks(ticks)
               .WithSimpleCallback(std::move(callback))
               .WithType(Timer::THROTTLE)
               .Build(currentTick, 0.0f);
    }

    namespace Timers {
        inline std::shared_ptr<Timer> After(size_t ticks, std::function<void()> callback, size_t currentTick) {
            return Delay(ticks, std::move(callback), currentTick);
        }

        inline std::shared_ptr<Timer> After(float seconds, std::function<void()> callback, float currentTime) {
            return Delay(seconds, std::move(callback), currentTime);
        }

        inline std::shared_ptr<Timer> Every(size_t ticks, std::function<void()> callback, size_t currentTick) {
            return Interval(ticks, [cb = std::move(callback)]() {
                cb();
                return true;
            }, currentTick);
        }

        inline std::shared_ptr<Timer> Every(float seconds, std::function<void()> callback, float currentTime) {
            return Interval(seconds, [cb = std::move(callback)]() {
                cb();
                return true;
            }, currentTime);
        }

        inline std::shared_ptr<Timer> Chain(std::shared_ptr<Timer> first, std::shared_ptr<Timer> second) {
            return Timer::Chain(std::move(first), std::move(second));
        }
    }
} // namespace BML

#endif // BML_TIMER_H
