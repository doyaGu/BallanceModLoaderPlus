#include "imgui.h"
#include "imgui_impl_ckrasterizer.h"

#include <cstdint>
#include <limits>
#include <cstring>

#include "CKRasterizer.h"

// CKRasterizer data
struct ImGui_ImplCKRasterizer_Data
{
    CKRasterizer *Rasterizer;
    CKRasterizerContext *Context;
    CKDWORD FontTextureIndex;

    // Vertex/index buffers and utilities
    CKDWORD VtxBufferIndex;
    CKDWORD IdxBufferIndex;
    int VtxBufferSize;
    int IdxBufferSize;

    // Cache for less state changes
    CKDWORD LastTextureId;
    CKRECT LastScissorRect;
    bool ScissorEnabled;

    ImGui_ImplCKRasterizer_Data()
    {
        memset(this, 0, sizeof(*this));
        VtxBufferSize = 5000; // Initial sizes
        IdxBufferSize = 10000;
        LastTextureId = 0;
        ScissorEnabled = false;
        memset(&LastScissorRect, 0, sizeof(LastScissorRect));
    }
};

#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IMGUI_COL_TO_ARGB(_COL) (_COL)
#else
#define IMGUI_COL_TO_ARGB(_COL) (((_COL) & 0xFF00FF00) | (((_COL) & 0xFF0000) >> 16) | (((_COL) & 0xFF) << 16))
#endif

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
static ImGui_ImplCKRasterizer_Data *ImGui_ImplCKRasterizer_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplCKRasterizer_Data *)ImGui::GetIO().BackendRendererUserData : nullptr;
}

static void ImGui_ImplCKRasterizer_SetupRenderState(ImDrawData *draw_data)
{
    ImGui_ImplCKRasterizer_Data *bd = ImGui_ImplCKRasterizer_GetBackendData();
    CKRasterizerContext *ctx = bd->Context;

    // Setup viewport
    CKViewportData viewport;
    viewport.ViewX = 0;
    viewport.ViewY = 0;
    viewport.ViewWidth = (int)draw_data->DisplaySize.x;
    viewport.ViewHeight = (int)draw_data->DisplaySize.y;
    viewport.ViewZMin = 0.0f;
    viewport.ViewZMax = 1.0f;
    ctx->SetViewport(&viewport);

    // Setup render state: alpha-blending, no face culling, no depth testing
    ctx->SetRenderState(VXRENDERSTATE_FILLMODE, VXFILL_SOLID);
    ctx->SetRenderState(VXRENDERSTATE_SHADEMODE, VXSHADE_GOURAUD);
    ctx->SetRenderState(VXRENDERSTATE_CULLMODE, VXCULL_NONE);

    // Alpha blending
    ctx->SetRenderState(VXRENDERSTATE_ALPHABLENDENABLE, TRUE);
    ctx->SetRenderState(VXRENDERSTATE_SRCBLEND, VXBLEND_SRCALPHA);
    ctx->SetRenderState(VXRENDERSTATE_DESTBLEND, VXBLEND_INVSRCALPHA);
    ctx->SetRenderState(VXRENDERSTATE_BLENDOP, VXBLENDOP_ADD);

    // Disable depth
    ctx->SetRenderState(VXRENDERSTATE_ZENABLE, FALSE);
    ctx->SetRenderState(VXRENDERSTATE_ZWRITEENABLE, FALSE);

    // Disable various features
    ctx->SetRenderState(VXRENDERSTATE_LIGHTING, FALSE);
    ctx->SetRenderState(VXRENDERSTATE_FOGENABLE, FALSE);
    ctx->SetRenderState(VXRENDERSTATE_SPECULARENABLE, FALSE);
    ctx->SetRenderState(VXRENDERSTATE_DITHERENABLE, FALSE);
    ctx->SetRenderState(VXRENDERSTATE_NORMALIZENORMALS, FALSE);
    ctx->SetRenderState(VXRENDERSTATE_COLORVERTEX, TRUE);

    // Alpha test - useful to have enabled for fonts
    ctx->SetRenderState(VXRENDERSTATE_ALPHATESTENABLE, TRUE);
    ctx->SetRenderState(VXRENDERSTATE_ALPHAREF, 0);
    ctx->SetRenderState(VXRENDERSTATE_ALPHAFUNC, VXCMP_NOTEQUAL);

    // Setup texture states for our font texture
    ctx->SetTextureStageState(0, CKRST_TSS_ADDRESS, VXTEXTURE_ADDRESSCLAMP);
    ctx->SetTextureStageState(0, CKRST_TSS_TEXTUREMAPBLEND, VXTEXTUREBLEND_MODULATEALPHA);
    ctx->SetTextureStageState(0, CKRST_TSS_MINFILTER, VXTEXTUREFILTER_NEAREST);
    ctx->SetTextureStageState(0, CKRST_TSS_MAGFILTER, VXTEXTUREFILTER_NEAREST);

    // Setup matrices - orthographic projection
    VxMatrix ortho;
    ortho.SetIdentity();

    // Orthographic projection matrix
    // Note: ImGui expects Y coordinates to be from top to bottom
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    // Create orthographic projection matrix
    ortho[0][0] = 2.0f / (R - L);
    ortho[1][1] = -2.0f / (B - T);
    ortho[3][0] = (L + R) / (L - R);
    ortho[3][1] = (T + B) / (B - T);

    ctx->SetTransformMatrix(VXMATRIX_PROJECTION, ortho);

    // Identity world and view matrices
    VxMatrix identity;
    identity.SetIdentity();
    ctx->SetTransformMatrix(VXMATRIX_WORLD, identity);
    ctx->SetTransformMatrix(VXMATRIX_VIEW, identity);

    // Reset texture binding
    bd->LastTextureId = 0;
    ctx->SetTexture(0);
}

// Create/resize vertex and index buffers
static bool ImGui_ImplCKRasterizer_CreateBuffers(int vtx_count, int idx_count)
{
    ImGui_ImplCKRasterizer_Data *bd = ImGui_ImplCKRasterizer_GetBackendData();
    CKRasterizerContext *ctx = bd->Context;

    // Create/resize vertex buffer if needed
    if (bd->VtxBufferIndex == 0 || vtx_count > bd->VtxBufferSize)
    {
        // Clean up old buffer if it exists
        if (bd->VtxBufferIndex)
        {
            ctx->DeleteObject(bd->VtxBufferIndex, CKRST_OBJ_VERTEXBUFFER);
            bd->Rasterizer->ReleaseObjectIndex(bd->VtxBufferIndex, CKRST_OBJ_VERTEXBUFFER);
            bd->VtxBufferIndex = 0;
        }

        // Allocate a new buffer with some extra space
        bd->VtxBufferSize = vtx_count + 5000;
        bd->VtxBufferIndex = bd->Rasterizer->CreateObjectIndex(CKRST_OBJ_VERTEXBUFFER);
        if (!bd->VtxBufferIndex)
            return false;

        // Create vertex buffer object
        CKVertexBufferDesc vbDesc;
        vbDesc.m_VertexFormat = CKRST_VF_POSITION | CKRST_VF_DIFFUSE | CKRST_VF_TEX1;
        vbDesc.m_VertexSize = sizeof(float) * 3 + sizeof(CKDWORD) + sizeof(float) * 2; // pos(xyz) + color + uv
        vbDesc.m_MaxVertexCount = bd->VtxBufferSize;
        vbDesc.m_Flags = CKRST_VB_DYNAMIC | CKRST_VB_WRITEONLY | CKRST_VB_VALID;

        if (!ctx->CreateObject(bd->VtxBufferIndex, CKRST_OBJ_VERTEXBUFFER, &vbDesc))
        {
            bd->Rasterizer->ReleaseObjectIndex(bd->VtxBufferIndex, CKRST_OBJ_VERTEXBUFFER);
            bd->VtxBufferIndex = 0;
            return false;
        }
    }

    // Create/resize index buffer if needed
    if (bd->IdxBufferIndex == 0 || idx_count > bd->IdxBufferSize)
    {
        // Clean up old buffer if it exists
        if (bd->IdxBufferIndex)
        {
            ctx->DeleteObject(bd->IdxBufferIndex, CKRST_OBJ_INDEXBUFFER);
            bd->Rasterizer->ReleaseObjectIndex(bd->IdxBufferIndex, CKRST_OBJ_INDEXBUFFER);
            bd->IdxBufferIndex = 0;
        }

        // Allocate a new buffer with some extra space
        bd->IdxBufferSize = idx_count + 10000;
        bd->IdxBufferIndex = bd->Rasterizer->CreateObjectIndex(CKRST_OBJ_INDEXBUFFER);
        if (!bd->IdxBufferIndex)
            return false;

        // Create index buffer object
        CKIndexBufferDesc ibDesc;
        ibDesc.m_Flags = CKRST_VB_DYNAMIC | CKRST_VB_WRITEONLY | CKRST_VB_VALID;
        ibDesc.m_MaxIndexCount = bd->IdxBufferSize;

        if (!ctx->CreateObject(bd->IdxBufferIndex, CKRST_OBJ_INDEXBUFFER, &ibDesc))
        {
            bd->Rasterizer->ReleaseObjectIndex(bd->IdxBufferIndex, CKRST_OBJ_INDEXBUFFER);
            bd->IdxBufferIndex = 0;
            return false;
        }
    }

    return true;
}

// Helper function to set scissor rect
static void ImGui_ImplCKRasterizer_SetScissorRect(CKRasterizerContext *ctx, const CKRECT *rect, bool enable)
{
    ImGui_ImplCKRasterizer_Data *bd = ImGui_ImplCKRasterizer_GetBackendData();
    if (!bd || !ctx)
        return;

    // Check if there's any change needed
    if (enable == bd->ScissorEnabled &&
        enable && rect &&
        memcmp(rect, &bd->LastScissorRect, sizeof(CKRECT)) == 0)
        return; // No change needed

    // Update scissor state
    if (enable && rect)
    {
        // Use viewport clipping as our scissor mechanism
        CKViewportData viewport;
        viewport.ViewX = rect->left;
        viewport.ViewY = rect->top;
        viewport.ViewWidth = rect->right - rect->left;
        viewport.ViewHeight = rect->bottom - rect->top;
        viewport.ViewZMin = 0.0f;
        viewport.ViewZMax = 1.0f;
        ctx->SetViewport(&viewport);

        // Store current scissor rect for state tracking
        bd->LastScissorRect = *rect;
    }
    else
    {
        // Restore full viewport when disabling scissor
        CKViewportData viewport;
        viewport.ViewX = 0;
        viewport.ViewY = 0;
        viewport.ViewWidth = (int)ImGui::GetIO().DisplaySize.x;
        viewport.ViewHeight = (int)ImGui::GetIO().DisplaySize.y;
        viewport.ViewZMin = 0.0f;
        viewport.ViewZMax = 1.0f;
        ctx->SetViewport(&viewport);
    }

    bd->ScissorEnabled = enable;
}

// Render function
void ImGui_ImplCKRasterizer_RenderDrawData(ImDrawData *draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;

    // Get backend and context
    ImGui_ImplCKRasterizer_Data *bd = ImGui_ImplCKRasterizer_GetBackendData();
    if (!bd || !bd->Context)
        return;

    CKRasterizerContext *ctx = bd->Context;

    // Get total number of vertices and indices
    int total_vtx_count = draw_data->TotalVtxCount;
    int total_idx_count = draw_data->TotalIdxCount;
    if (total_vtx_count == 0 || total_idx_count == 0)
        return;

    // Create or resize vertex/index buffers if needed
    if (!ImGui_ImplCKRasterizer_CreateBuffers(total_vtx_count, total_idx_count))
    {
        return;
    }

    // Upload vertex/index data
    {
        // Lock vertex buffer for writing
        void *vtx_dst = ctx->LockVertexBuffer(bd->VtxBufferIndex, 0, total_vtx_count, CKRST_LOCK_DISCARD);
        if (!vtx_dst)
        {
            return;
        }

        // Lock index buffer for writing
        void *idx_dst = ctx->LockIndexBuffer(bd->IdxBufferIndex, 0, total_idx_count, CKRST_LOCK_DISCARD);
        if (!idx_dst)
        {
            ctx->UnlockVertexBuffer(bd->VtxBufferIndex);
            return;
        }

        // Copy and convert all vertices data
        ImDrawVert *vtx_src;
        ImDrawIdx *idx_src;
        int vtx_offset = 0;
        int idx_offset = 0;

        for (int n = 0; n < draw_data->CmdListsCount; n++)
        {
            const ImDrawList *cmd_list = draw_data->CmdLists[n];
            vtx_src = cmd_list->VtxBuffer.Data;
            idx_src = cmd_list->IdxBuffer.Data;

            // Copy vertices
            unsigned char *vtx_ptr = (unsigned char *)vtx_dst + (vtx_offset * (sizeof(float) * 3 + sizeof(CKDWORD) + sizeof(float) * 2));
            for (int v = 0; v < cmd_list->VtxBuffer.Size; v++)
            {
                // Position (x, y, z)
                float *pos = (float *)vtx_ptr;
                pos[0] = vtx_src[v].pos.x;
                pos[1] = vtx_src[v].pos.y;
                pos[2] = 0.0f;
                vtx_ptr += sizeof(float) * 3;

                // Color (ARGB)
                *(CKDWORD *)vtx_ptr = IMGUI_COL_TO_ARGB(vtx_src[v].col);
                vtx_ptr += sizeof(CKDWORD);

                // UV
                float *uv = (float *)vtx_ptr;
                uv[0] = vtx_src[v].uv.x;
                uv[1] = vtx_src[v].uv.y;
                vtx_ptr += sizeof(float) * 2;
            }

            // Copy indices with offset
            unsigned short *idx_ptr = (unsigned short *)idx_dst + idx_offset;
            if (sizeof(ImDrawIdx) == 2)
            {
                // Direct copy if indices are already 16-bit
                memcpy(idx_ptr, idx_src, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            }
            else
            {
                // Convert from 32-bit to 16-bit
                for (int i = 0; i < cmd_list->IdxBuffer.Size; i++)
                    idx_ptr[i] = (unsigned short)(idx_src[i] + vtx_offset);
            }

            vtx_offset += cmd_list->VtxBuffer.Size;
            idx_offset += cmd_list->IdxBuffer.Size;
        }

        // Unlock buffers
        ctx->UnlockVertexBuffer(bd->VtxBufferIndex);
        ctx->UnlockIndexBuffer(bd->IdxBufferIndex);
    }

    // Setup desired CKRasterizer state
    ImGui_ImplCKRasterizer_SetupRenderState(draw_data);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display

    // Render command lists
    int global_vtx_offset = 0;
    int global_idx_offset = 0;

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList *cmd_list = draw_data->CmdLists[n];

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];

            if (pcmd->UserCallback)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplCKRasterizer_SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                CKRECT clip_rect;
                clip_rect.left = (long)(pcmd->ClipRect.x - clip_off.x);
                clip_rect.top = (long)(pcmd->ClipRect.y - clip_off.y);
                clip_rect.right = (long)(pcmd->ClipRect.z - clip_off.x);
                clip_rect.bottom = (long)(pcmd->ClipRect.w - clip_off.y);

                // Apply scissor/clipping rectangle, Draw
                if (clip_rect.right > clip_rect.left && clip_rect.bottom > clip_rect.top)
                {
                    // Apply scissor/clipping rectangle
                    ImGui_ImplCKRasterizer_SetScissorRect(ctx, &clip_rect, true);

                    // Change texture if needed
                    CKDWORD tex_id = (CKDWORD)(intptr_t)pcmd->GetTexID();
                    if (tex_id != bd->LastTextureId)
                    {
                        ctx->SetTexture(tex_id);
                        bd->LastTextureId = tex_id;
                    }

                    // Draw indexed triangles
                    ctx->DrawPrimitiveVBIB(
                        VX_TRIANGLELIST,
                        bd->VtxBufferIndex,
                        bd->IdxBufferIndex,
                        global_vtx_offset + pcmd->VtxOffset,
                        cmd_list->VtxBuffer.Size,
                        global_idx_offset + pcmd->IdxOffset,
                        pcmd->ElemCount);
                }
            }
        }

        global_vtx_offset += cmd_list->VtxBuffer.Size;
        global_idx_offset += cmd_list->IdxBuffer.Size;
    }

    // Disable scissor test
    ImGui_ImplCKRasterizer_SetScissorRect(ctx, nullptr, false);
}

bool ImGui_ImplCKRasterizer_Init(CKRasterizer *rasterizer, CKRasterizerContext *context)
{
    ImGuiIO &io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == NULL && "Already initialized a renderer backend!");

    // Create backend data
    ImGui_ImplCKRasterizer_Data *bd = IM_NEW(ImGui_ImplCKRasterizer_Data)();
    io.BackendRendererUserData = (void *)bd;
    io.BackendRendererName = "imgui_impl_ckrasterizer";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field

    // Store rasterizer and context
    bd->Rasterizer = rasterizer;
    bd->Context = context;

    return true;
}

void ImGui_ImplCKRasterizer_Shutdown()
{
    ImGui_ImplCKRasterizer_Data *bd = ImGui_ImplCKRasterizer_GetBackendData();
    IM_ASSERT(bd != NULL && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO &io = ImGui::GetIO();

    // Clean up
    ImGui_ImplCKRasterizer_InvalidateDeviceObjects();

    // Nullify references
    io.BackendRendererName = NULL;
    io.BackendRendererUserData = NULL;

    // Delete backend data
    IM_DELETE(bd);
}

void ImGui_ImplCKRasterizer_InvalidateDeviceObjects()
{
    ImGui_ImplCKRasterizer_Data *bd = ImGui_ImplCKRasterizer_GetBackendData();
    if (!bd || !bd->Rasterizer || !bd->Context)
        return;

    // Release the font texture
    if (bd->FontTextureIndex)
    {
        bd->Context->DeleteObject(bd->FontTextureIndex, CKRST_OBJ_TEXTURE);
        bd->Rasterizer->ReleaseObjectIndex(bd->FontTextureIndex, CKRST_OBJ_TEXTURE);
        bd->FontTextureIndex = 0;
        ImGui::GetIO().Fonts->SetTexID(0);
    }

    // Release vertex buffer
    if (bd->VtxBufferIndex)
    {
        bd->Context->DeleteObject(bd->VtxBufferIndex, CKRST_OBJ_VERTEXBUFFER);
        bd->Rasterizer->ReleaseObjectIndex(bd->VtxBufferIndex, CKRST_OBJ_VERTEXBUFFER);
        bd->VtxBufferIndex = 0;
    }

    // Release index buffer
    if (bd->IdxBufferIndex)
    {
        bd->Context->DeleteObject(bd->IdxBufferIndex, CKRST_OBJ_INDEXBUFFER);
        bd->Rasterizer->ReleaseObjectIndex(bd->IdxBufferIndex, CKRST_OBJ_INDEXBUFFER);
        bd->IdxBufferIndex = 0;
    }
}

bool ImGui_ImplCKRasterizer_CreateDeviceObjects()
{
    ImGui_ImplCKRasterizer_Data *bd = ImGui_ImplCKRasterizer_GetBackendData();
    if (!bd || !bd->Rasterizer || !bd->Context)
        return false;

    // Make sure device objects are cleaned up
    ImGui_ImplCKRasterizer_InvalidateDeviceObjects();

    // Build texture atlas
    ImGuiIO &io = ImGui::GetIO();
    unsigned char *pixels;
    int width, height;

    // Load as Alpha8 (more efficient for font textures)
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    // The font texture needs to be 32-bit (we'll convert Alpha8 to ARGB)
    unsigned char *argb_pixels = new unsigned char[width * height * sizeof(CKDWORD)];
    if (!argb_pixels)
    {
        return false;
    }

    // Convert Alpha8 to ARGB32 (white color with alpha from font data)
    for (int i = 0; i < width * height; i++)
    {
        argb_pixels[i * 4 + 0] = 255;       // B
        argb_pixels[i * 4 + 1] = 255;       // G
        argb_pixels[i * 4 + 2] = 255;       // R
        argb_pixels[i * 4 + 3] = pixels[i]; // A
    }

    // Set up texture description
    CKTextureDesc texDesc;
    texDesc.Flags = CKRST_TEXTURE_VALID | CKRST_TEXTURE_MANAGED | CKRST_TEXTURE_RGB | CKRST_TEXTURE_ALPHA;
    texDesc.Format.Width = width;
    texDesc.Format.Height = height;
    VxPixelFormat2ImageDesc(_32_ARGB8888, texDesc.Format);

    // Create a texture
    bd->FontTextureIndex = bd->Rasterizer->CreateObjectIndex(CKRST_OBJ_TEXTURE);
    if (!bd->FontTextureIndex)
    {
        delete[] argb_pixels;
        return false;
    }

    // Create the texture object
    if (!bd->Context->CreateObject(bd->FontTextureIndex, CKRST_OBJ_TEXTURE, &texDesc))
    {
        delete[] argb_pixels;
        bd->Rasterizer->ReleaseObjectIndex(bd->FontTextureIndex, CKRST_OBJ_TEXTURE);
        bd->FontTextureIndex = 0;
        return false;
    }

    CKTextureDesc *texData = bd->Context->GetTextureData(bd->FontTextureIndex);
    if (!texData)
    {
        delete[] argb_pixels;
        bd->Context->DeleteObject(bd->FontTextureIndex, CKRST_OBJ_TEXTURE);
        bd->Rasterizer->ReleaseObjectIndex(bd->FontTextureIndex, CKRST_OBJ_TEXTURE);
        bd->FontTextureIndex = 0;
        return false;
    }

    texData->Format.Image = argb_pixels;

    // Upload texture data
    if (!bd->Context->LoadTexture(bd->FontTextureIndex, texData->Format))
    {
        delete[] argb_pixels;
        bd->Context->DeleteObject(bd->FontTextureIndex, CKRST_OBJ_TEXTURE);
        bd->Rasterizer->ReleaseObjectIndex(bd->FontTextureIndex, CKRST_OBJ_TEXTURE);
        bd->FontTextureIndex = 0;
        return false;
    }

    delete[] argb_pixels;

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)bd->FontTextureIndex);

    return true;
}

void ImGui_ImplCKRasterizer_NewFrame()
{
    ImGui_ImplCKRasterizer_Data *bd = ImGui_ImplCKRasterizer_GetBackendData();
    IM_ASSERT(bd != NULL && "Did you call ImGui_ImplCKRasterizer_Init()?");

    // Create font texture if not already done
    if (!bd->FontTextureIndex)
        ImGui_ImplCKRasterizer_CreateDeviceObjects();
}

void ImGui_ImplCKRasterizer_SetCurrentContext(CKRasterizerContext *context)
{
    ImGui_ImplCKRasterizer_Data *bd = ImGui_ImplCKRasterizer_GetBackendData();
    if (bd)
        bd->Context = context;
}
