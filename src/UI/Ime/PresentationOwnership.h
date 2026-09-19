#ifndef BML_UI_IME_PRESENTATIONOWNERSHIP_H
#define BML_UI_IME_PRESENTATIONOWNERSHIP_H

#include <atomic>

namespace Overlay::Ime {
    class PresentationOwnership {
    public:
        void Attach(bool focused) noexcept {
            m_Visible.store(false, std::memory_order_release);
            m_Focused.store(focused, std::memory_order_release);
            m_Attached.store(true, std::memory_order_release);
        }

        void Detach() noexcept {
            m_Attached.store(false, std::memory_order_release);
            m_Focused.store(false, std::memory_order_release);
            m_Visible.store(false, std::memory_order_release);
        }

        bool SetFocused(bool focused) noexcept {
            focused = m_Attached.load(std::memory_order_acquire) && focused;
            const bool previous = m_Focused.exchange(focused, std::memory_order_acq_rel);
            if (!focused)
                m_Visible.store(false, std::memory_order_release);
            return previous != focused;
        }

        bool ExchangeVisible(bool visible) noexcept {
            visible = m_Attached.load(std::memory_order_acquire) &&
                      m_Focused.load(std::memory_order_acquire) && visible;
            return m_Visible.exchange(visible, std::memory_order_acq_rel);
        }

        bool IsFocused() const noexcept {
            return m_Attached.load(std::memory_order_acquire) &&
                   m_Focused.load(std::memory_order_acquire);
        }

        bool IsVisible() const noexcept {
            return IsFocused() && m_Visible.load(std::memory_order_acquire);
        }

    private:
        std::atomic_bool m_Attached{false};
        std::atomic_bool m_Focused{false};
        std::atomic_bool m_Visible{false};
    };
}

#endif // BML_UI_IME_PRESENTATIONOWNERSHIP_H
