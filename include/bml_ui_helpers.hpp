/**
 * @file bml_ui_helpers.hpp
 * @brief RAII helpers for UI draw registration
 *
 * Wraps the repetitive BML_UIDrawDesc setup + Register + error-check + cleanup
 * pattern into a single RAII handle returned by a one-line factory call.
 */

#ifndef BML_UI_HELPERS_HPP
#define BML_UI_HELPERS_HPP

#include "bml_ui_host.h"
#include "bml_interface.hpp"

namespace bml {
namespace ui {

    /**
     * @brief RAII handle for a UI draw registration.
     *
     * Automatically unregisters the draw callback on destruction.
     * Move-only.
     */
    struct DrawRegistration {
        bml::InterfaceLease<BML_UIDrawRegistry> registry;
        BML_InterfaceRegistration token = nullptr;

        DrawRegistration() = default;

        ~DrawRegistration() {
            (void) Reset();
        }

        DrawRegistration(const DrawRegistration &) = delete;
        DrawRegistration &operator=(const DrawRegistration &) = delete;

        DrawRegistration(DrawRegistration &&other) noexcept
            : registry(std::move(other.registry)),
              token(other.token) {
            other.token = nullptr;
        }

        DrawRegistration &operator=(DrawRegistration &&other) noexcept {
            if (this != &other) {
                (void) Reset();
                registry = std::move(other.registry);
                token = other.token;
                other.token = nullptr;
            }
            return *this;
        }

        explicit operator bool() const noexcept {
            return token != nullptr && registry;
        }

        BML_Result Reset() {
            if (token && registry) {
                BML_Result result = registry->Unregister(token);
                token = nullptr;
                registry.Reset();
                return result;
            }
            token = nullptr;
            registry.Reset();
            return BML_RESULT_OK;
        }
    };

    /**
     * @brief Register a UI draw callback with the window registry.
     *
     * @param id        Unique registration ID (e.g. "bml.console.window")
     * @param priority  Draw priority (lower = earlier)
     * @param callback  Draw callback function
     * @param userData  User data passed to callback
     * @param layer     UI layer (default: BML_UI_LAYER_WINDOW)
     * @return DrawRegistration RAII handle (falsy on failure)
     */
    inline DrawRegistration RegisterWindowDraw(
        const char *id, int32_t priority,
        PFN_BML_UIDrawCallback callback, void *userData,
        BML_UILayer layer = BML_UI_LAYER_WINDOW) {

        DrawRegistration reg;
        reg.registry = bml::AcquireInterface<BML_UIDrawRegistry>(
            BML_UI_WINDOW_REGISTRY_INTERFACE_ID, 1, 0, 0);
        if (!reg.registry) return {};

        BML_UIDrawDesc desc = BML_UI_DRAW_DESC_INIT;
        desc.id = id;
        desc.layer = layer;
        desc.priority = priority;
        desc.callback = callback;
        desc.user_data = userData;

        if (reg.registry->Register(&desc, &reg.token) != BML_RESULT_OK) {
            reg.registry.Reset();
            return {};
        }
        return reg;
    }

    /**
     * @brief Register a UI draw callback with the overlay registry.
     */
    inline DrawRegistration RegisterOverlayDraw(
        const char *id, int32_t priority,
        PFN_BML_UIDrawCallback callback, void *userData) {

        DrawRegistration reg;
        reg.registry = bml::AcquireInterface<BML_UIDrawRegistry>(
            BML_UI_OVERLAY_REGISTRY_INTERFACE_ID, 1, 0, 0);
        if (!reg.registry) return {};

        BML_UIDrawDesc desc = BML_UI_DRAW_DESC_INIT;
        desc.id = id;
        desc.layer = BML_UI_LAYER_OVERLAY;
        desc.priority = priority;
        desc.callback = callback;
        desc.user_data = userData;

        if (reg.registry->Register(&desc, &reg.token) != BML_RESULT_OK) {
            reg.registry.Reset();
            return {};
        }
        return reg;
    }

} // namespace ui
} // namespace bml

#endif /* BML_UI_HELPERS_HPP */
