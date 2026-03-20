#ifndef BML_UI_HPP
#define BML_UI_HPP

/**
 * @file bml_ui.hpp
 * @brief C++ RAII helpers for UI draw registration.
 *
 * Wraps the repetitive BML_UIDrawDesc setup + Register + error-check + cleanup
 * pattern into a single RAII handle returned by a one-line factory call.
 */

#include "bml_ui.h"
#include "bml_interface.hpp"

namespace bml {
namespace ui {

    /**
     * @brief RAII handle for a UI draw registration.
     *
     * Automatically unregisters the draw callback on destruction.
     * Move-only.
     */
    class DrawRegistration {
    public:
        DrawRegistration() = default;

        ~DrawRegistration() {
            (void) Reset();
        }

        DrawRegistration(const DrawRegistration &) = delete;
        DrawRegistration &operator=(const DrawRegistration &) = delete;

        DrawRegistration(DrawRegistration &&other) noexcept
            : m_Registry(std::move(other.m_Registry)),
              m_Token(other.m_Token) {
            other.m_Token = nullptr;
        }

        DrawRegistration &operator=(DrawRegistration &&other) noexcept {
            if (this != &other) {
                (void) Reset();
                m_Registry = std::move(other.m_Registry);
                m_Token = other.m_Token;
                other.m_Token = nullptr;
            }
            return *this;
        }

        explicit operator bool() const noexcept {
            return m_Token != nullptr && m_Registry;
        }

        BML_Result Reset() {
            if (m_Token && m_Registry) {
                BML_Result result = m_Registry->Unregister(m_Token);
                m_Token = nullptr;
                m_Registry.Reset();
                return result;
            }
            m_Token = nullptr;
            m_Registry.Reset();
            return BML_RESULT_OK;
        }

    private:
        bml::InterfaceLease<BML_UIDrawRegistry> m_Registry;
        BML_InterfaceRegistration m_Token = nullptr;

        friend DrawRegistration RegisterDraw(
            const char *, int32_t, PFN_BML_UIDrawCallback, void *);
    };

    /**
     * @brief Register a UI draw callback.
     *
     * @param id        Unique registration ID (e.g. "bml.console.window")
     * @param priority  Draw priority (lower = drawn earlier)
     * @param callback  Draw callback function
     * @param userData  User data passed to callback
     * @return DrawRegistration RAII handle (falsy on failure)
     */
    inline DrawRegistration RegisterDraw(
        const char *id, int32_t priority,
        PFN_BML_UIDrawCallback callback, void *userData) {

        DrawRegistration reg;
        reg.m_Registry = bml::Acquire<BML_UIDrawRegistry>();
        if (!reg.m_Registry) return {};

        BML_UIDrawDesc desc = BML_UI_DRAW_DESC_INIT;
        desc.id = id;
        desc.priority = priority;
        desc.callback = callback;
        desc.user_data = userData;

        if (reg.m_Registry->Register(&desc, &reg.m_Token) != BML_RESULT_OK) {
            reg.m_Registry.Reset();
            return {};
        }
        return reg;
    }

} // namespace ui
} // namespace bml

#endif /* BML_UI_HPP */
