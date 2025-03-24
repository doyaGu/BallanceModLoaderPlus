// dear imgui: Renderer Backend for CKRasterizer
// This needs to be used along with a Platform Backend (e.g. Win32)

// Implemented features:
//  [X] Renderer: User texture binding. Use 'CKDWORD' as ImTextureID. Read the FAQ about ImTextureID!
//  [X] Renderer: Large meshes support (64k+ vertices) with 16-bit indices.
//  [X] Renderer: Multi-viewport support (multiple windows). Enable with 'io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable'.

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API

// Forward declarations
class CKRasterizer;
class CKRasterizerContext;

// Main functions
IMGUI_IMPL_API bool     ImGui_ImplCKRasterizer_Init(CKRasterizer* rasterizer, CKRasterizerContext* context);
IMGUI_IMPL_API void     ImGui_ImplCKRasterizer_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplCKRasterizer_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplCKRasterizer_RenderDrawData(ImDrawData* draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API bool     ImGui_ImplCKRasterizer_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplCKRasterizer_InvalidateDeviceObjects();
