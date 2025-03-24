// dear imgui: Renderer Backend for Virtools

// Implemented features:
//  [X] Renderer: User texture binding.
//  [X] Renderer: Large meshes support (64k+ vertices) with 16-bit indices.

// You can use unmodified imgui_impl_* files in your project. See examples/ folder for examples of using this.
// Prefer including the entire imgui/ repository into your project (either as a copy or as a submodule), and only build the backends you need.
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)

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

    bool UseHardwareVB;

    // State cache to minimize redundant state changes
    struct
    {
        CKDWORD StateCache[VXRENDERSTATE_MAXSTATE];
        CKDWORD TextureStageStateCache[CKRST_TSS_MAXSTATE][CKRST_MAX_STAGES];
        bool StateCacheValid;

        // Previous states for restoration
        VxRect PrevViewport;
        CKDWORD PrevCullMode;
        CKDWORD PrevAlphaBlend;
        CKDWORD PrevSrcBlend;
        CKDWORD PrevDestBlend;
        CKDWORD PrevZEnable;
        CKDWORD PrevZWriteEnable;
        CKDWORD PrevAlphaTest;
        CKDWORD PrevFillMode;
        CKDWORD PrevShadeMode;
        CKDWORD PrevLighting;

        // Texture state tracking
        CKTexture *CurrentTexture;
    } Cache;

    ImGui_ImplCK2_Data()
    {
        memset(this, 0, sizeof(*this));
        UseHardwareVB = true;
    }
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

// Save current render state for later restoration
static void ImGui_ImplCK2_SaveRenderState()
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    if (!bd || !bd->RenderContext)
        return;

    CKRenderContext *dev = bd->RenderContext;

    // Save current viewport
    dev->GetViewRect(bd->Cache.PrevViewport);

    // Save key render states
    bd->Cache.PrevCullMode = dev->GetState(VXRENDERSTATE_CULLMODE);
    bd->Cache.PrevAlphaBlend = dev->GetState(VXRENDERSTATE_ALPHABLENDENABLE);
    bd->Cache.PrevSrcBlend = dev->GetState(VXRENDERSTATE_SRCBLEND);
    bd->Cache.PrevDestBlend = dev->GetState(VXRENDERSTATE_DESTBLEND);
    bd->Cache.PrevZEnable = dev->GetState(VXRENDERSTATE_ZENABLE);
    bd->Cache.PrevZWriteEnable = dev->GetState(VXRENDERSTATE_ZWRITEENABLE);
    bd->Cache.PrevAlphaTest = dev->GetState(VXRENDERSTATE_ALPHATESTENABLE);
    bd->Cache.PrevFillMode = dev->GetState(VXRENDERSTATE_FILLMODE);
    bd->Cache.PrevShadeMode = dev->GetState(VXRENDERSTATE_SHADEMODE);
    bd->Cache.PrevLighting = dev->GetState(VXRENDERSTATE_LIGHTING);
}

// Restore render state to what it was before ImGui rendering
static void ImGui_ImplCK2_RestoreRenderState()
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    if (!bd || !bd->RenderContext)
        return;

    CKRenderContext *dev = bd->RenderContext;

    // Restore viewport
    dev->SetViewRect(bd->Cache.PrevViewport);

    // Restore key render states
    dev->SetState(VXRENDERSTATE_CULLMODE, bd->Cache.PrevCullMode);
    dev->SetState(VXRENDERSTATE_ALPHABLENDENABLE, bd->Cache.PrevAlphaBlend);
    dev->SetState(VXRENDERSTATE_SRCBLEND, bd->Cache.PrevSrcBlend);
    dev->SetState(VXRENDERSTATE_DESTBLEND, bd->Cache.PrevDestBlend);
    dev->SetState(VXRENDERSTATE_ZENABLE, bd->Cache.PrevZEnable);
    dev->SetState(VXRENDERSTATE_ZWRITEENABLE, bd->Cache.PrevZWriteEnable);
    dev->SetState(VXRENDERSTATE_ALPHATESTENABLE, bd->Cache.PrevAlphaTest);
    dev->SetState(VXRENDERSTATE_FILLMODE, bd->Cache.PrevFillMode);
    dev->SetState(VXRENDERSTATE_SHADEMODE, bd->Cache.PrevShadeMode);
    dev->SetState(VXRENDERSTATE_LIGHTING, bd->Cache.PrevLighting);

    // Reset texture
    dev->SetTexture(NULL);
}

static void ImGui_ImplCK2_SetupRenderState(ImDrawData *draw_data, bool force_reset = false)
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    CKRenderContext *dev = bd->RenderContext;

    if (!bd->Cache.StateCacheValid || force_reset)
    {
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

        bd->Cache.CurrentTexture = NULL;
        bd->Cache.StateCacheValid = true;
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

    // Save current render state
    ImGui_ImplCK2_SaveRenderState();

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

            // Create vertex buffer with optimized flags
            data = dev->GetDrawPrimitiveStructure((CKRST_DPFLAGS)(CKRST_DP_CL_VCT | (bd->UseHardwareVB ? CKRST_DP_VBUFFER : 0)), vtx_count);
            if (!data)
            {
                // Fallback: Try without hardware VB
                bd->UseHardwareVB = false;
                data = dev->GetDrawPrimitiveStructure((CKRST_DPFLAGS)(CKRST_DP_CL_VCT), vtx_count);

                // If still failing, skip this command list
                if (!data)
                    continue;
            }

            // Copy and convert vertices with optimal stride access
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
                    ImGui_ImplCK2_SetupRenderState(draw_data, true);
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
                int vtx_count = pcmd->ElemCount; // Use a tighter count for optimization

                // Create optimized vertex buffer just for this command
                data = dev->GetDrawPrimitiveStructure((CKRST_DPFLAGS)(CKRST_DP_CL_VCT | (bd->UseHardwareVB ? CKRST_DP_VBUFFER : 0)), vtx_count);
                if (!data)
                {
                    // Fallback
                    data = dev->GetDrawPrimitiveStructure((CKRST_DPFLAGS)(CKRST_DP_CL_VCT), vtx_count);
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
                // Avoid redundant texture binding
                if (bd->Cache.CurrentTexture != texture)
                {
                    dev->SetTexture(texture);
                    bd->Cache.CurrentTexture = texture;
                }
                dev->DrawPrimitive(VX_TRIANGLELIST, (CKWORD *)(idx_buffer + pcmd->IdxOffset), pcmd->ElemCount, data);
            }
            else if (obj->GetClassID() == CKCID_MATERIAL)
            {
                // Custom material handling - preserve states that will be reset
                bd->Cache.StateCacheValid = false;
                ((CKMaterial *)obj)->SetAsCurrent(dev);
                dev->DrawPrimitive(VX_TRIANGLELIST, (CKWORD *)(idx_buffer + pcmd->IdxOffset), pcmd->ElemCount, data);
                // Restore ImGui render state after custom material
                ImGui_ImplCK2_SetupRenderState(draw_data, true);
            }
        }
    }

    // Restore render state
    ImGui_ImplCK2_RestoreRenderState();
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

    bd->Context = context;
    bd->RenderContext = render_context;

    // Detect capabilities and configure accordingly
    VxDriverDesc *driver = context->GetRenderManager()->GetRenderDriverDescription(render_context->GetDriverIndex());
    if (driver)
    {
        // Check for hardware vertex buffer support
        bd->UseHardwareVB = (driver->Caps3D.CKRasterizerSpecificCaps & CKRST_SPECIFICCAPS_CANDOVERTEXBUFFER) != 0;
    }

    return true;
}

void ImGui_ImplCK2_Shutdown()
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    IM_ASSERT(bd != NULL && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO &io = ImGui::GetIO();

    ImGui_ImplCK2_DestroyDeviceObjects();

    io.BackendRendererName = NULL;
    io.BackendRendererUserData = NULL;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    IM_DELETE(bd);
}

bool ImGui_ImplCK2_CreateFontsTexture()
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();

    if (!bd || !bd->Context)
        return false;

    CKContext *context = bd->Context;

    // Check if font texture already exists
    if (bd->FontTexture)
    {
        ImGui_ImplCK2_DestroyFontsTexture();
    }

    // Build texture atlas
    unsigned char *pixels;
    int width, height;
    // Load as RGBA 32-bit (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders.
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels)
        return false;

    // Upload texture to graphics system
    // (Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
    CKTexture *texture = (CKTexture *)context->CreateObject(CKCID_TEXTURE, (CKSTRING) "ImGuiFonts");
    if (!texture)
        return false;

    // Set texture to not be saved or deleted automatically
    texture->ModifyObjectFlags(CK_OBJECT_NOTTOBESAVED | CK_OBJECT_NOTTOBEDELETED, 0);

    // Create texture and check for success
    if (!texture->Create(width, height))
    {
        context->DestroyObject(texture);
        return false;
    }

    // Lock texture surface and copy pixel data
    CKBYTE *ptr = texture->LockSurfacePtr();
    if (!ptr)
    {
        context->DestroyObject(texture);
        return false;
    }

    // Copy pixel data
    memcpy(ptr, pixels, width * height * 4);
    texture->ReleaseSurfacePtr();

    // Set optimal format for UI texture
    texture->SetDesiredVideoFormat(_32_ARGB8888);

    // Force restore to video memory
    texture->SystemToVideoMemory(bd->RenderContext, TRUE);

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)texture);
    bd->FontTexture = texture;

    return true;
}

void ImGui_ImplCK2_DestroyFontsTexture()
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    if (bd && bd->FontTexture)
    {
        io.Fonts->SetTexID(NULL);
        if (bd->Context)
        {
            bd->Context->DestroyObject(bd->FontTexture);
        }
        bd->FontTexture = NULL;
    }
}

bool ImGui_ImplCK2_CreateDeviceObjects()
{
    return ImGui_ImplCK2_CreateFontsTexture();
}

void ImGui_ImplCK2_DestroyDeviceObjects()
{
    ImGui_ImplCK2_DestroyFontsTexture();
}

void ImGui_ImplCK2_NewFrame()
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplCK2_Init()?");

    if (!bd->FontTexture)
        ImGui_ImplCK2_CreateDeviceObjects();

    // Invalid cache at the start of each frame to ensure correct state setup
    bd->Cache.StateCacheValid = false;
}