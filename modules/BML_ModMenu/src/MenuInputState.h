#ifndef BML_MODMENU_INPUT_STATE_H
#define BML_MODMENU_INPUT_STATE_H

#include <cstdint>

namespace BML::ModMenu {

class MenuInputState {
public:
    void Reset() {
        m_EscapeDown = false;
        m_EnterDown = false;
        m_PendingKeyboardRelease = false;
    }

    void CancelPendingRelease() {
        m_PendingKeyboardRelease = false;
    }

    void OnKeyDown(uint32_t keyCode) {
        if (keyCode == kEscapeKey) {
            m_EscapeDown = true;
        } else if (keyCode == kEnterKey) {
            m_EnterDown = true;
        }
    }

    bool OnKeyUp(uint32_t keyCode) {
        if (keyCode == kEscapeKey) {
            m_EscapeDown = false;
        } else if (keyCode == kEnterKey) {
            m_EnterDown = false;
        }

        if (m_PendingKeyboardRelease && !m_EscapeDown && !m_EnterDown) {
            m_PendingKeyboardRelease = false;
            return true;
        }

        return false;
    }

    bool BeginRelease() {
        if (m_EscapeDown || m_EnterDown) {
            m_PendingKeyboardRelease = true;
            return false;
        }

        return true;
    }

private:
    static constexpr uint32_t kEscapeKey = 0x01;
    static constexpr uint32_t kEnterKey = 0x1C;

    bool m_EscapeDown = false;
    bool m_EnterDown = false;
    bool m_PendingKeyboardRelease = false;
};

} // namespace BML::ModMenu

#endif // BML_MODMENU_INPUT_STATE_H
