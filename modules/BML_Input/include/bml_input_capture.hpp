/**
 * @file bml_input_capture.hpp
 * @brief RAII guard for input capture token management
 *
 * Wraps the BML_InputCaptureInterface acquire/release pattern into a
 * move-only guard that auto-releases on destruction.
 */

#ifndef BML_INPUT_CAPTURE_HPP
#define BML_INPUT_CAPTURE_HPP

#include "bml_input_control.h"

namespace bml {

    /**
     * @brief RAII guard for an input capture token.
     *
     * Automatically releases the capture token on destruction or move.
     * Move-only.
     */
    class InputCaptureGuard {
    public:
        explicit InputCaptureGuard(const BML_InputCaptureInterface *svc = nullptr) noexcept
            : m_Service(svc) {}

        ~InputCaptureGuard() {
            Release();
        }

        InputCaptureGuard(const InputCaptureGuard &) = delete;
        InputCaptureGuard &operator=(const InputCaptureGuard &) = delete;

        InputCaptureGuard(InputCaptureGuard &&other) noexcept
            : m_Service(other.m_Service),
              m_Token(other.m_Token) {
            other.m_Token = nullptr;
            other.m_Service = nullptr;
        }

        InputCaptureGuard &operator=(InputCaptureGuard &&other) noexcept {
            if (this != &other) {
                Release();
                m_Service = other.m_Service;
                m_Token = other.m_Token;
                other.m_Token = nullptr;
                other.m_Service = nullptr;
            }
            return *this;
        }

        void SetService(const BML_InputCaptureInterface *svc) noexcept {
            m_Service = svc;
        }

        BML_Result Acquire(uint32_t flags, int cursorVisible = -1, int32_t priority = 0) {
            if (!m_Service || !m_Service->AcquireCapture) {
                return BML_RESULT_NOT_SUPPORTED;
            }
            if (m_Token) {
                return BML_RESULT_OK;
            }
            BML_InputCaptureDesc desc = BML_INPUT_CAPTURE_DESC_INIT;
            desc.flags = flags;
            desc.cursor_visible = cursorVisible;
            desc.priority = priority;
            return m_Service->AcquireCapture(&desc, &m_Token);
        }

        void Release() {
            if (m_Token && m_Service && m_Service->ReleaseCapture) {
                m_Service->ReleaseCapture(m_Token);
                m_Token = nullptr;
            }
        }

        bool IsHeld() const noexcept {
            return m_Token != nullptr;
        }

        explicit operator bool() const noexcept { return IsHeld(); }

    private:
        const BML_InputCaptureInterface *m_Service = nullptr;
        BML_InputCaptureToken m_Token = nullptr;
    };

} // namespace bml

#endif /* BML_INPUT_CAPTURE_HPP */
