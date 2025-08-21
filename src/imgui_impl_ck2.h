// dear imgui: Renderer Backend for Virtools

// Implemented features:
//  [X] Renderer: User texture binding.
//  [X] Renderer: Large meshes support (64k+ vertices) with 16-bit indices.
//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#pragma once
#include "imgui.h"      // IMGUI_IMPL_API

class CKContext;

IMGUI_IMPL_API bool     ImGui_ImplCK2_Init(CKContext *context);
IMGUI_IMPL_API void     ImGui_ImplCK2_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplCK2_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplCK2_RenderDrawData(ImDrawData *draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API bool     ImGui_ImplCK2_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplCK2_InvalidateDeviceObjects();

// (Advanced) Use e.g. if you need to precisely control the timing of texture updates (e.g. for staged rendering), by setting ImDrawData::Textures = NULL to handle this manually.
IMGUI_IMPL_API void     ImGui_ImplCK2_UpdateTexture(ImTextureData* tex);