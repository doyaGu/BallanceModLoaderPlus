#ifndef BML_UI_H
#define BML_UI_H

/**
 * @file bml_ui.h
 * @brief Draw registry and UI draw contribution types for BML_UI.
 *
 * C API for the BML_UI module.  Provides the draw registry interface
 * and draw descriptor/context types.
 *
 * For C++ RAII helpers (DrawRegistration, RegisterDraw), include bml_ui.hpp.
 * For the C++ ImGui wrapper, include bml_imgui.hpp (before imgui.h).
 */

#include "bml_imgui.h"
#include "bml_errors.h"
#include "bml_interface.h"

BML_BEGIN_CDECLS

/* ========================================================================
 * Interface ID
 * ======================================================================== */

#define BML_UI_DRAW_REGISTRY_INTERFACE_ID "bml.ui.draw_registry"

/* ========================================================================
 * Draw Context
 * ======================================================================== */

/**
 * @brief Per-callback context passed to draw contributions.
 *
 * The @c imgui pointer is the active BML_ImGuiApi vtable for this frame.
 * Pass it to @c BML_IMGUI_SCOPE_FROM_CONTEXT to use ImGui:: calls.
 */
typedef struct BML_UIDrawContext {
    size_t struct_size;
    const BML_ImGuiApi *imgui;
} BML_UIDrawContext;

/* ========================================================================
 * Draw Callback and Descriptor
 * ======================================================================== */

typedef void (*PFN_BML_UIDrawCallback)(const BML_UIDrawContext *ctx, void *user_data);

typedef struct BML_UIDrawDesc {
    size_t struct_size;
    const char *id;
    int32_t priority;
    PFN_BML_UIDrawCallback callback;
    void *user_data;
} BML_UIDrawDesc;

#define BML_UI_DRAW_DESC_INIT { sizeof(BML_UIDrawDesc), NULL, 0, NULL, NULL }
#define BML_UI_DRAW_CONTEXT_INIT { sizeof(BML_UIDrawContext), NULL }

/* ========================================================================
 * Draw Registry (interface vtable)
 * ======================================================================== */

typedef struct BML_UIDrawRegistry {
    BML_InterfaceHeader header;
    BML_Result (*Register)(const BML_UIDrawDesc *desc, BML_InterfaceRegistration *out_registration);
    BML_Result (*Unregister)(BML_InterfaceRegistration registration);
} BML_UIDrawRegistry;

BML_END_CDECLS

#endif /* BML_UI_H */
