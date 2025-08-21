// dear imgui: Renderer Backend for Virtools

// Implemented features:
//  [X] Renderer: User texture binding.
//  [X] Renderer: Large meshes support (64k+ vertices) with 16-bit indices.
//  [X] Renderer: Texture updates support for dynamic font atlas (ImGuiBackendFlags_RendererHasTextures).

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2025-08-20: Virtools: Added support for ImGuiBackendFlags_RendererHasTextures, for dynamic font atlas.
//  2025-08-20: Virtools: Changed default texture sampler to Clamp instead of Repeat/Wrap.

#include "imgui.h"
#include "imgui_impl_ck2.h"

// Virtools
#include "CKContext.h"
#include "CKRenderManager.h"
#include "CKRenderContext.h"
#include "CKTexture.h"
#include "CKMaterial.h"

// CK2 data
struct ImGui_ImplCK2_Data
{
    CKContext *Context;
    CKRenderContext *RenderContext;
    CKTexture *FontTexture;

    ImGui_ImplCK2_Data() { memset(this, 0, sizeof(*this)); }
};

#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IMGUI_COL_TO_ARGB(_COL) (_COL)
#else
#define IMGUI_COL_TO_ARGB(_COL) (((_COL) & 0xFF00FF00) | (((_COL) & 0xFF0000) >> 16) | (((_COL) & 0xFF) << 16))
#endif

// Backend data stored in io.BackendPlatformUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplCK2_Data *ImGui_ImplCK2_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplCK2_Data *)ImGui::GetIO().BackendRendererUserData : NULL;
}

static void ImGui_ImplCK2_SetupRenderState(ImDrawData *draw_data)
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    CKRenderContext *dev = bd->RenderContext;

    // Setup viewport
    VxRect viewport(0, 0, draw_data->DisplaySize.x, draw_data->DisplaySize.y);
    dev->SetViewRect(viewport);

    // Setup render state: alpha-blending, no face culling, no depth testing, shade mode
    dev->SetState(VXRENDERSTATE_FILLMODE, VXFILL_SOLID);
    dev->SetState(VXRENDERSTATE_SHADEMODE, VXSHADE_GOURAUD);
    dev->SetState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);
    dev->SetState(VXRENDERSTATE_WRAP0, 0);
    dev->SetState(VXRENDERSTATE_SRCBLEND, VXBLEND_SRCALPHA);
    dev->SetState(VXRENDERSTATE_DESTBLEND, VXBLEND_INVSRCALPHA);
    dev->SetState(VXRENDERSTATE_ALPHATESTENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_ZWRITEENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_ZENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_ALPHABLENDENABLE, TRUE);
    dev->SetState(VXRENDERSTATE_BLENDOP, VXBLENDOP_ADD);
    dev->SetState(VXRENDERSTATE_FOGENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_SPECULARENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_STENCILENABLE, FALSE);
    dev->SetState(VXRENDERSTATE_CLIPPING, TRUE);
    dev->SetState(VXRENDERSTATE_LIGHTING, FALSE);

    // Setup texture stage states
    dev->SetTextureStageState(CKRST_TSS_ADDRESS, VXTEXTURE_ADDRESSCLAMP);
    dev->SetTextureStageState(CKRST_TSS_TEXTUREMAPBLEND, VXTEXTUREBLEND_MODULATEALPHA);
    dev->SetTextureStageState(CKRST_TSS_STAGEBLEND, 0, 1);
    dev->SetTextureStageState(CKRST_TSS_MINFILTER, VXTEXTUREFILTER_LINEAR);
    dev->SetTextureStageState(CKRST_TSS_MAGFILTER, VXTEXTUREFILTER_LINEAR);
}

// Copy texture region with optional format conversion
static void ImGui_ImplCK2_CopyTextureRegion(bool tex_use_colors, const ImU32 *src, int src_pitch, ImU32 *dst, int dst_pitch, int w, int h)
{

    for (int y = 0; y < h; y++)
    {
        const ImU32 *src_p = (const ImU32 *)(const void *)((const unsigned char *)src + src_pitch * y);
        ImU32 *dst_p = (ImU32 *)(void *)((unsigned char *)dst + dst_pitch * y);
#ifndef IMGUI_USE_BGRA_PACKED_COLOR
        if (tex_use_colors)
        {
            for (int x = w; x > 0; x--, src_p++, dst_p++) // Convert copy
                *dst_p = IMGUI_COL_TO_ARGB(*src_p);
        }
#else
        memcpy(dst_p, src_p, w * 4); // Raw copy
#endif
    }
}

void ImGui_ImplCK2_UpdateTexture(ImTextureData *tex)
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create and upload new texture to graphics system
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);

        CKTexture *ck_tex = (CKTexture *)bd->Context->CreateObject(CKCID_TEXTURE, (CKSTRING) "ImGuiDynamicTexture");
        if (!ck_tex)
        {
            IM_ASSERT(ck_tex && "Backend failed to create texture!");
            return;
        }

        // Set texture to not be saved or deleted automatically
        ck_tex->ModifyObjectFlags(CK_OBJECT_NOTTOBESAVED | CK_OBJECT_NOTTOBEDELETED, 0);

        // Create texture and check for success
        if (!ck_tex->Create(tex->Width, tex->Height))
        {
            bd->Context->DestroyObject(ck_tex);
            return;
        }

        // Lock texture surface and copy pixel data
        CKBYTE *ptr = ck_tex->LockSurfacePtr();
        if (!ptr)
        {
            bd->Context->DestroyObject(ck_tex);
            return;
        }

        // Copy pixel data
        ImGui_ImplCK2_CopyTextureRegion(tex->UseColors, (ImU32 *)tex->GetPixels(), tex->Width * 4, (ImU32 *)ptr, tex->Width * 4, tex->Width, tex->Height);
        ck_tex->ReleaseSurfacePtr();

        // Set optimal format for UI texture
        ck_tex->SetDesiredVideoFormat(_32_ARGB8888);

        // Force restore to video memory
        if (!ck_tex->SystemToVideoMemory(bd->RenderContext, TRUE))
        {
            bd->Context->DestroyObject(ck_tex);
            return;
        }

        // Only set identifiers after ALL operations succeeded
        tex->SetTexID((ImTextureID)ck_tex);
        // BackendUserData not used in this implementation
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        // Update selected blocks. We only ever write to textures regions which have never been used before!
        CKTexture *ck_tex = (CKTexture *)tex->TexID;

        // Lock texture surface for partial update
        CKBYTE *ptr = ck_tex->LockSurfacePtr();
        if (!ptr)
            return; // Failed to lock, don't mark as OK

        for (ImTextureRect &r : tex->Updates)
        {
            // Copy each update rectangle
            const ImU32 *src_data = (ImU32 *)tex->GetPixelsAt(r.x, r.y);
            ImU32 *dst_data = (ImU32 *)ptr + r.x + r.y * tex->Width;
            ImGui_ImplCK2_CopyTextureRegion(tex->UseColors, src_data, tex->Width * 4, dst_data, tex->Width * 4, r.w, r.h);
        }
        ck_tex->ReleaseSurfacePtr();

        // Force restore to video memory
        if (!ck_tex->SystemToVideoMemory(bd->RenderContext, TRUE))
            return; // Failed to upload, don't mark as OK

        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy)
    {
        CKTexture *ck_tex = (CKTexture *)tex->TexID;
        if (ck_tex == nullptr)
            return;
        IM_ASSERT(tex->TexID == (ImTextureID)ck_tex);

        bd->Context->DestroyObject(ck_tex);

        // Clear identifiers and mark as destroyed (in order to allow e.g. calling InvalidateDeviceObjects while running)
        tex->SetTexID(ImTextureID_Invalid);
        tex->BackendUserData = nullptr;
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

// Render function.
void ImGui_ImplCK2_RenderDrawData(ImDrawData *draw_data)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width == 0 || fb_height == 0)
        return;

    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    if (!bd || !bd->RenderContext)
        return;

    CKRenderContext *dev = bd->RenderContext;

    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (draw_data->Textures != nullptr)
        for (ImTextureData *tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplCK2_UpdateTexture(tex);

    // Setup desired render state
    ImGui_ImplCK2_SetupRenderState(draw_data);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList *cmd_list = draw_data->CmdLists[n];
        const ImDrawVert *vtx_buffer = cmd_list->VtxBuffer.Data;
        const ImDrawIdx *idx_buffer = cmd_list->IdxBuffer.Data;

        VxDrawPrimitiveData *data = NULL;
        bool use_large_mesh_approach = cmd_list->VtxBuffer.Size >= 0xFFFF;

        // For normal sized meshes, prepare all vertices at once
        if (!use_large_mesh_approach)
        {
            const ImDrawVert *vtx_src = vtx_buffer;
            int vtx_count = cmd_list->VtxBuffer.Size;

            data = dev->GetDrawPrimitiveStructure((CKRST_DPFLAGS)(CKRST_DP_CL_VCT | CKRST_DP_VBUFFER), vtx_count);
            if (!data)
            {
                data = dev->GetDrawPrimitiveStructure(CKRST_DP_CL_VCT, vtx_count);
                if (!data)
                    continue;
            }

            XPtrStrided<VxVector4> positions(data->PositionPtr, data->PositionStride);
            XPtrStrided<CKDWORD> colors(data->ColorPtr, data->ColorStride);
            XPtrStrided<VxUV> uvs(data->TexCoordPtr, data->TexCoordStride);

            for (int i = 0; i < vtx_count; i++)
            {
                positions->Set(vtx_src->pos.x, vtx_src->pos.y, 0.0f, 1.0f);
                *colors = IMGUI_COL_TO_ARGB(vtx_src->col);
                uvs->u = vtx_src->uv.x;
                uvs->v = vtx_src->uv.y;

                ++positions;
                ++colors;
                ++uvs;
                ++vtx_src;
            }
        }

        // Process command buffer
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];

            // Handle user callbacks
            if (pcmd->UserCallback)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplCK2_SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
                continue;
            }

            // Project scissor/clipping rectangles into framebuffer space
            ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
            ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

            // Apply scissor/clipping rectangle (Y is inverted in CK2)
            if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
            if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
            if (clip_max.x > (float)fb_width) { clip_max.x = (float)fb_width; }
            if (clip_max.y > (float)fb_height) { clip_max.y = (float)fb_height; }
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                continue;

            // Handle per-chunk vertex data for large meshes
            if (use_large_mesh_approach)
            {
                const ImDrawVert *vtx_src = vtx_buffer + pcmd->VtxOffset;
                int vtx_count = pcmd->ElemCount;

                data = dev->GetDrawPrimitiveStructure((CKRST_DPFLAGS)(CKRST_DP_CL_VCT | CKRST_DP_VBUFFER), vtx_count);
                if (!data)
                {
                    data = dev->GetDrawPrimitiveStructure(CKRST_DP_CL_VCT, vtx_count);
                    if (!data)
                        continue;
                }

                // Copy vertex data
                XPtrStrided<VxVector4> positions(data->PositionPtr, data->PositionStride);
                XPtrStrided<CKDWORD> colors(data->ColorPtr, data->ColorStride);
                XPtrStrided<VxUV> uvs(data->TexCoordPtr, data->TexCoordStride);

                for (int i = 0; i < vtx_count; i++)
                {
                    positions->Set(vtx_src[i].pos.x, vtx_src[i].pos.y, 0.0f, 1.0f);
                    *colors = IMGUI_COL_TO_ARGB(vtx_src[i].col);
                    uvs->u = vtx_src[i].uv.x;
                    uvs->v = vtx_src[i].uv.y;

                    ++positions;
                    ++colors;
                    ++uvs;
                }
            }

            // Set texture or material
            CKObject *obj = (CKObject *)pcmd->GetTexID();
            if (!obj)
                continue; // Skip if no texture/material

            if (obj->GetClassID() == CKCID_TEXTURE)
            {
                CKTexture *texture = (CKTexture *)obj;
                dev->SetTexture(texture);
                dev->DrawPrimitive(VX_TRIANGLELIST, (CKWORD *)(idx_buffer + pcmd->IdxOffset), pcmd->ElemCount, data);
            }
            else if (obj->GetClassID() == CKCID_MATERIAL)
            {
                ((CKMaterial *)obj)->SetAsCurrent(dev);
                dev->DrawPrimitive(VX_TRIANGLELIST, (CKWORD *)(idx_buffer + pcmd->IdxOffset), pcmd->ElemCount, data);
                ImGui_ImplCK2_SetupRenderState(draw_data);
            }
        }
    }
}

bool ImGui_ImplCK2_Init(CKContext *context)
{
    ImGuiIO &io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");

    if (!context)
        return false;

    // Get render context
    CKRenderContext *render_context = context->GetPlayerRenderContext();
    if (!render_context)
        return false;

    // Setup backend capabilities flags
    ImGui_ImplCK2_Data *bd = IM_NEW(ImGui_ImplCK2_Data)();
    io.BackendRendererUserData = (void *)bd;
    io.BackendRendererName = "imgui_impl_ck2";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // We can honor ImGuiPlatformIO::Textures[] requests during render.

    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_TextureMaxWidth = platform_io.Renderer_TextureMaxHeight = 4096;

    bd->Context = context;
    bd->RenderContext = render_context;

    return true;
}

void ImGui_ImplCK2_Shutdown()
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    IM_ASSERT(bd != NULL && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO &io = ImGui::GetIO();

    ImGui_ImplCK2_InvalidateDeviceObjects();

    io.BackendRendererName = NULL;
    io.BackendRendererUserData = NULL;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    IM_DELETE(bd);
}

bool ImGui_ImplCK2_CreateDeviceObjects()
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    if (!bd || !bd->Context)
        return false;
    return true;
}

void ImGui_ImplCK2_InvalidateDeviceObjects()
{
    // Destroy all textures
    for (ImTextureData *tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
        {
            tex->SetStatus(ImTextureStatus_WantDestroy);
            ImGui_ImplCK2_UpdateTexture(tex);
        }
}

void ImGui_ImplCK2_NewFrame()
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplCK2_Init()?");
}