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

struct ImGui_ImplCK2_TextureLimits
{
    static constexpr int ProjectMaximum = 4096;
    static constexpr int ConservativeFallback = 2048;

    int Width;
    int Height;
    bool UsedFallback;
};

struct ImGui_ImplCK2_Diagnostics
{
    int TextureMaxWidth;
    int TextureMaxHeight;
    const char *LastTextureFailure;
    unsigned int LastTextureFailureCount;
};

inline ImGui_ImplCK2_TextureLimits ImGui_ImplCK2_SelectTextureLimits(unsigned int reported_width,
                                                                     unsigned int reported_height)
{
    if (reported_width == 0 || reported_height == 0)
        return {ImGui_ImplCK2_TextureLimits::ConservativeFallback,
                ImGui_ImplCK2_TextureLimits::ConservativeFallback, true};

    const unsigned int project_maximum = ImGui_ImplCK2_TextureLimits::ProjectMaximum;
    return {
        (int)(reported_width < project_maximum ? reported_width : project_maximum),
        (int)(reported_height < project_maximum ? reported_height : project_maximum),
        false,
    };
}

IMGUI_IMPL_API bool     ImGui_ImplCK2_Init(CKContext *context);
IMGUI_IMPL_API void     ImGui_ImplCK2_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplCK2_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplCK2_RenderDrawData(ImDrawData *draw_data);

// Use if you want to reset your rendering device without losing Dear ImGui state.
IMGUI_IMPL_API bool     ImGui_ImplCK2_CreateDeviceObjects();
IMGUI_IMPL_API void     ImGui_ImplCK2_InvalidateDeviceObjects();
IMGUI_IMPL_API bool     ImGui_ImplCK2_GetDiagnostics(ImGui_ImplCK2_Diagnostics *diagnostics);

// (Advanced) Use e.g. if you need to precisely control the timing of texture updates (e.g. for staged rendering), by setting ImDrawData::Textures = NULL to handle this manually.
IMGUI_IMPL_API void     ImGui_ImplCK2_UpdateTexture(ImTextureData* tex);
