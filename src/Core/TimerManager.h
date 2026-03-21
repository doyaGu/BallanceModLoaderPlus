#ifndef BML_CORE_TIMER_MANAGER_H
#define BML_CORE_TIMER_MANAGER_H

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "bml_timer.h"

namespace BML::Core {

    class TimerManager {
    public:
        /**
         * @brief Schedule a one-shot millisecond timer.
         * @param owner_id Module ID of the owning module
         * @param delay_ms Delay in milliseconds
         * @param callback Timer callback
         * @param user_data Opaque user pointer
         * @param out_timer Receives the timer handle
         */
        BML_Result ScheduleOnce(const std::string &owner_id,
                                uint32_t delay_ms,
                                BML_TimerCallback callback,
                                void *user_data,
                                BML_Timer *out_timer);

        /**
         * @brief Schedule a repeating millisecond timer.
         * @param owner_id Module ID of the owning module
         * @param interval_ms Interval in milliseconds (must be > 0)
         * @param callback Timer callback
         * @param user_data Opaque user pointer
         * @param out_timer Receives the timer handle
         */
        BML_Result ScheduleRepeat(const std::string &owner_id,
                                  uint32_t interval_ms,
                                  BML_TimerCallback callback,
                                  void *user_data,
                                  BML_Timer *out_timer);

        /**
         * @brief Schedule a frame-counted timer.
         * @param owner_id Module ID of the owning module
         * @param frame_count Frames to wait
         * @param callback Timer callback
         * @param user_data Opaque user pointer
         * @param out_timer Receives the timer handle
         */
        BML_Result ScheduleFrames(const std::string &owner_id,
                                  uint32_t frame_count,
                                  BML_TimerCallback callback,
                                  void *user_data,
                                  BML_Timer *out_timer);

        BML_Result Cancel(BML_Timer timer);
        BML_Result IsActive(BML_Timer timer, BML_Bool *out_active);

        /** @brief Cancel all timers owned by a specific module */
        BML_Result CancelAllForModule(const std::string &owner_id);

        /**
         * @brief Tick all timers. Called from UpdateMicrokernel() on the main thread.
         *
         * Fires elapsed millisecond timers and decrements frame counters.
         */
        void Tick();

        /** @brief Cancel all timers and reset state (called during shutdown) */
        void Shutdown();

        TimerManager() = default;

    private:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        enum class TimerKind : uint8_t {
            OnceMs,
            RepeatMs,
            OnceFrames,
        };

        struct TimerEntry {
            uint32_t id = 0;
            TimerKind kind = TimerKind::OnceMs;
            bool active = true;

            // Millisecond timers
            TimePoint fire_at{};
            uint32_t interval_ms = 0;   // 0 for one-shot

            // Frame-based timers
            uint32_t frames_remaining = 0;

            BML_TimerCallback callback = nullptr;
            void *user_data = nullptr;
            std::string owner_id;
        };

        TimerEntry *FindEntry(BML_Timer timer);

        std::mutex m_Mutex;
        std::vector<TimerEntry> m_Timers;
        uint32_t m_NextId = 1;
    };

} // namespace BML::Core

#endif // BML_CORE_TIMER_MANAGER_H
