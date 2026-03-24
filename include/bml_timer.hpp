/**
 * @file bml_timer.hpp
 * @brief C++ wrapper for the BML Timer API
 */

#ifndef BML_TIMER_HPP
#define BML_TIMER_HPP

#include "bml_timer.h"
#include "bml_assert.hpp"
#include <cstddef>

namespace bml {

    class TimerService {
    public:
        TimerService() = default;
        explicit TimerService(const BML_CoreTimerInterface *iface) noexcept
            : m_Interface(iface) {}
        TimerService(const BML_CoreTimerInterface *iface, BML_Mod owner) noexcept
            : m_Interface(iface), m_Owner(owner) {}

        BML_Timer ScheduleOnce(uint32_t delay_ms, BML_TimerCallback cb, void *ud = nullptr) const {
            BML_Timer t = nullptr;
            m_Interface->ScheduleOnce(m_Owner, delay_ms, cb, ud, &t);
            return t;
        }

        BML_Timer ScheduleRepeat(uint32_t interval_ms, BML_TimerCallback cb, void *ud = nullptr) const {
            BML_Timer t = nullptr;
            m_Interface->ScheduleRepeat(m_Owner, interval_ms, cb, ud, &t);
            return t;
        }

        BML_Timer ScheduleFrames(uint32_t frame_count, BML_TimerCallback cb, void *ud = nullptr) const {
            BML_Timer t = nullptr;
            m_Interface->ScheduleFrames(m_Owner, frame_count, cb, ud, &t);
            return t;
        }

        BML_Result Cancel(BML_Timer timer) const {
            return m_Interface->Cancel(m_Owner, timer);
        }

        bool IsActive(BML_Timer timer) const {
            BML_Bool active = BML_FALSE;
            m_Interface->IsActive(m_Owner, timer, &active);
            return active == BML_TRUE;
        }

        BML_Result CancelAll() const {
            return m_Interface->CancelAll(m_Owner);
        }

        explicit operator bool() const noexcept { return m_Interface != nullptr; }

    private:
        const BML_CoreTimerInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

} // namespace bml

#endif /* BML_TIMER_HPP */
