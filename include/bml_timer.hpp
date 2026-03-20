/**
 * @file bml_timer.hpp
 * @brief C++ wrapper for the BML Timer API
 */

#ifndef BML_TIMER_HPP
#define BML_TIMER_HPP

#include "bml_builtin_interfaces.h"

namespace bml {

    class TimerService {
    public:
        TimerService() = default;
        explicit TimerService(const BML_CoreTimerInterface *iface) noexcept
            : m_Interface(iface) {}

        BML_Timer ScheduleOnce(uint32_t delay_ms, BML_TimerCallback cb, void *ud = nullptr) const {
            BML_Timer t = nullptr;
            if (m_Interface && m_Interface->ScheduleOnce)
                m_Interface->ScheduleOnce(delay_ms, cb, ud, &t);
            return t;
        }

        BML_Timer ScheduleRepeat(uint32_t interval_ms, BML_TimerCallback cb, void *ud = nullptr) const {
            BML_Timer t = nullptr;
            if (m_Interface && m_Interface->ScheduleRepeat)
                m_Interface->ScheduleRepeat(interval_ms, cb, ud, &t);
            return t;
        }

        BML_Timer ScheduleFrames(uint32_t frame_count, BML_TimerCallback cb, void *ud = nullptr) const {
            BML_Timer t = nullptr;
            if (m_Interface && m_Interface->ScheduleFrames)
                m_Interface->ScheduleFrames(frame_count, cb, ud, &t);
            return t;
        }

        BML_Result Cancel(BML_Timer timer) const {
            return (m_Interface && m_Interface->Cancel)
                ? m_Interface->Cancel(timer) : BML_RESULT_NOT_INITIALIZED;
        }

        bool IsActive(BML_Timer timer) const {
            BML_Bool active = BML_FALSE;
            if (m_Interface && m_Interface->IsActive)
                m_Interface->IsActive(timer, &active);
            return active == BML_TRUE;
        }

        BML_Result CancelAll() const {
            return (m_Interface && m_Interface->CancelAll)
                ? m_Interface->CancelAll() : BML_RESULT_NOT_INITIALIZED;
        }

        explicit operator bool() const noexcept { return m_Interface != nullptr; }

    private:
        const BML_CoreTimerInterface *m_Interface = nullptr;
    };

} // namespace bml

#endif /* BML_TIMER_HPP */
