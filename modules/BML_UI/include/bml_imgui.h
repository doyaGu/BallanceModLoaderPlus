#ifndef BML_IMGUI_H
#define BML_IMGUI_H

/**
 * @file bml_imgui.h
 * @brief Forward declarations and common types for the BML ImGui API.
 *
 * Lightweight C header that does NOT pull in imgui.h.  Provides:
 * - @c BML_UI_IMGUI_INTERFACE_ID
 * - @c BML_ImVec2
 * - Forward declaration of @c BML_ImGuiApi
 *
 * For the full @c BML_ImGuiApi struct definition (requires imgui.h),
 * include bml_imgui_api.h.  For the C++ vtable wrapper, include
 * bml_imgui.hpp.
 */

#include "bml_types.h"

BML_BEGIN_CDECLS

#define BML_UI_IMGUI_INTERFACE_ID "bml.ui.imgui"

typedef struct BML_ImVec2 {
    float x;
    float y;
} BML_ImVec2;

typedef struct BML_ImGuiApi BML_ImGuiApi;

BML_END_CDECLS

#endif /* BML_IMGUI_H */
