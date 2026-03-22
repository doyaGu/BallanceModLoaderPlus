/**
 * @file bml_timer.hpp
 * @brief C++ wrapper for the BML Timer API
 */

#ifndef BML_TIMER_HPP
#define BML_TIMER_HPP

#include "bml_builtin_interfaces.h"
#include <cstddef>

namespace bml {
    namespace detail {
        template <typename MemberT>
        constexpr bool HasTimerMember(const BML_CoreTimerInterface *iface, size_t offset) noexcept {
            return iface != nullptr && iface->header.struct_size >= offset + sizeof(MemberT);
        }
    } // namespace detail

#define BML_CORE_TIMER_HAS_MEMBER(iface, member) \
    (::bml::detail::HasTimerMember<decltype(((BML_CoreTimerInterface *) 0)->member)>( \
        (iface), offsetof(BML_CoreTimerInterface, member)) && (iface)->member != nullptr)

    class TimerService {
    public:
        TimerService() = default;
        explicit TimerService(const BML_CoreTimerInterface *iface) noexcept
            : m_Interface(iface) {}
        TimerService(const BML_CoreTimerInterface *iface, BML_Mod owner) noexcept
            : m_Interface(iface), m_Owner(owner) {}

        BML_Timer ScheduleOnce(uint32_t delay_ms, BML_TimerCallback cb, void *ud = nullptr) const {
            BML_Timer t = nullptr;
            if (m_Interface && m_Owner && BML_CORE_TIMER_HAS_MEMBER(m_Interface, ScheduleOnceOwned))
                m_Interface->ScheduleOnceOwned(m_Owner, delay_ms, cb, ud, &t);
            else if (m_Interface && m_Interface->ScheduleOnce)
                m_Interface->ScheduleOnce(delay_ms, cb, ud, &t);
            return t;
        }

        BML_Timer ScheduleRepeat(uint32_t interval_ms, BML_TimerCallback cb, void *ud = nullptr) const {
            BML_Timer t = nullptr;
            if (m_Interface && m_Owner && BML_CORE_TIMER_HAS_MEMBER(m_Interface, ScheduleRepeatOwned))
                m_Interface->ScheduleRepeatOwned(m_Owner, interval_ms, cb, ud, &t);
            else if (m_Interface && m_Interface->ScheduleRepeat)
                m_Interface->ScheduleRepeat(interval_ms, cb, ud, &t);
            return t;
        }

        BML_Timer ScheduleFrames(uint32_t frame_count, BML_TimerCallback cb, void *ud = nullptr) const {
            BML_Timer t = nullptr;
            if (m_Interface && m_Owner && BML_CORE_TIMER_HAS_MEMBER(m_Interface, ScheduleFramesOwned))
                m_Interface->ScheduleFramesOwned(m_Owner, frame_count, cb, ud, &t);
            else if (m_Interface && m_Interface->ScheduleFrames)
                m_Interface->ScheduleFrames(frame_count, cb, ud, &t);
            return t;
        }

        BML_Result Cancel(BML_Timer timer) const {
            if (!m_Interface) {
                return BML_RESULT_NOT_INITIALIZED;
            }
            if (m_Owner && BML_CORE_TIMER_HAS_MEMBER(m_Interface, CancelOwned)) {
                return m_Interface->CancelOwned(m_Owner, timer);
            }
            return m_Interface->Cancel ? m_Interface->Cancel(timer) : BML_RESULT_NOT_INITIALIZED;
        }

        bool IsActive(BML_Timer timer) const {
            BML_Bool active = BML_FALSE;
            if (m_Interface && m_Owner && BML_CORE_TIMER_HAS_MEMBER(m_Interface, IsActiveOwned))
                m_Interface->IsActiveOwned(m_Owner, timer, &active);
            else if (m_Interface && m_Interface->IsActive)
                m_Interface->IsActive(timer, &active);
            return active == BML_TRUE;
        }

        BML_Result CancelAll() const {
            if (!m_Interface) {
                return BML_RESULT_NOT_INITIALIZED;
            }
            if (m_Owner && BML_CORE_TIMER_HAS_MEMBER(m_Interface, CancelAllOwned)) {
                return m_Interface->CancelAllOwned(m_Owner);
            }
            return m_Interface->CancelAll ? m_Interface->CancelAll() : BML_RESULT_NOT_INITIALIZED;
        }

        explicit operator bool() const noexcept { return m_Interface != nullptr; }

    private:
        const BML_CoreTimerInterface *m_Interface = nullptr;
        BML_Mod m_Owner = nullptr;
    };

#undef BML_CORE_TIMER_HAS_MEMBER

} // namespace bml

#endif /* BML_TIMER_HPP */
