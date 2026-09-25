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
#include "UI/imgui_impl_ck2.h"

#if !defined(BML_TEST_CK2_BACKEND_LOGIC)
// Virtools
#include "CKContext.h"
#include "CKRenderManager.h"
#include "CKRenderContext.h"
#include "CKTexture.h"
#include "CKMaterial.h"

#include "HookUtils.h"

enum ImGui_ImplCK2_TextureFailure
{
    ImGui_ImplCK2_TextureFailure_None,
    ImGui_ImplCK2_TextureFailure_InvalidRequest,
    ImGui_ImplCK2_TextureFailure_CreateObject,
    ImGui_ImplCK2_TextureFailure_CreateSurface,
    ImGui_ImplCK2_TextureFailure_UnsupportedSurface,
    ImGui_ImplCK2_TextureFailure_LockSurface,
    ImGui_ImplCK2_TextureFailure_ReleaseSurface,
    ImGui_ImplCK2_TextureFailure_Upload,
};

struct ImGui_ImplCK2_TextureState
{
    unsigned int FailureCount;
    unsigned int NextRetryFrame;
    ImGui_ImplCK2_TextureFailure Failure;
    ImTextureStatus RequestStatus;
    int Width;
    int Height;
    void *Pixels;
    ImTextureRect UpdateRect;
    int UpdateCount;
    ImU64 UpdateSignature;

    ImGui_ImplCK2_TextureState() { memset(this, 0, sizeof(*this)); }
};

// CK2 data
struct ImGui_ImplCK2_Data
{
    CKContext *Context;
    CKRenderContext *RenderContext;
    unsigned int FrameIndex;
    int TextureMaxWidth;
    int TextureMaxHeight;
    ImGui_ImplCK2_TextureFailure LastTextureFailure;
    unsigned int LastTextureFailureCount;

    ImGui_ImplCK2_Data() { memset(this, 0, sizeof(*this)); }
};
#endif

struct ImGui_ImplCK2_TextureLimits
{
    int Width;
    int Height;
    bool UsedFallback;
};

static ImGui_ImplCK2_TextureLimits ImGui_ImplCK2_SelectTextureLimits(unsigned int reported_width,
                                                                     unsigned int reported_height)
{
    const unsigned int project_maximum = 4096;
    const int conservative_fallback = 2048;
    if (reported_width == 0 || reported_height == 0)
        return {conservative_fallback, conservative_fallback, true};

    return {
        (int)(reported_width < project_maximum ? reported_width : project_maximum),
        (int)(reported_height < project_maximum ? reported_height : project_maximum),
        false,
    };
}

#if !defined(BML_TEST_CK2_BACKEND_LOGIC)
static const VXRENDERSTATETYPE ImGui_ImplCK2_RenderStates[] = {
    VXRENDERSTATE_FILLMODE,
    VXRENDERSTATE_SHADEMODE,
    VXRENDERSTATE_CULLMODE,
    VXRENDERSTATE_WRAP0,
    VXRENDERSTATE_SRCBLEND,
    VXRENDERSTATE_DESTBLEND,
    VXRENDERSTATE_ALPHATESTENABLE,
    VXRENDERSTATE_ZWRITEENABLE,
    VXRENDERSTATE_ZENABLE,
    VXRENDERSTATE_ALPHABLENDENABLE,
    VXRENDERSTATE_BLENDOP,
    VXRENDERSTATE_FOGENABLE,
    VXRENDERSTATE_SPECULARENABLE,
    VXRENDERSTATE_STENCILENABLE,
    VXRENDERSTATE_CLIPPING,
    VXRENDERSTATE_LIGHTING,
};

class ImGui_ImplCK2_RenderStateBackup
{
public:
    explicit ImGui_ImplCK2_RenderStateBackup(CKRenderContext *context) : Context(context)
    {
        Context->GetViewRect(ViewRect);
        for (int i = 0; i < IM_ARRAYSIZE(ImGui_ImplCK2_RenderStates); ++i)
            Values[i] = Context->GetState(ImGui_ImplCK2_RenderStates[i]);
    }

    ~ImGui_ImplCK2_RenderStateBackup()
    {
        Context->SetViewRect(ViewRect);
        for (int i = 0; i < IM_ARRAYSIZE(ImGui_ImplCK2_RenderStates); ++i)
            Context->SetState(ImGui_ImplCK2_RenderStates[i], Values[i]);
    }

    ImGui_ImplCK2_RenderStateBackup(const ImGui_ImplCK2_RenderStateBackup &) = delete;
    ImGui_ImplCK2_RenderStateBackup &operator=(const ImGui_ImplCK2_RenderStateBackup &) = delete;

private:
    CKRenderContext *Context;
    VxRect ViewRect;
    CKDWORD Values[IM_ARRAYSIZE(ImGui_ImplCK2_RenderStates)] = {};
};
#endif

struct ImGui_ImplCK2_DrawSlice
{
    unsigned int VertexOffset;
    unsigned int VertexCount;
    ImVector<ImDrawIdx> Indices;
};

static bool ImGui_ImplCK2_BuildDrawSlice(const ImDrawIdx *indices, unsigned int index_count,
                                         unsigned int vertex_offset, int vertex_count,
                                         ImGui_ImplCK2_DrawSlice *slice)
{
    slice->VertexOffset = 0;
    slice->VertexCount = 0;
    slice->Indices.resize(0);

    if (!indices || index_count == 0 || index_count > 0x7fffffffU || vertex_count <= 0 ||
        vertex_offset >= (unsigned int)vertex_count)
        return false;

    ImDrawIdx minimum = indices[0];
    ImDrawIdx maximum = indices[0];
    for (unsigned int i = 1; i < index_count; ++i)
    {
        if (indices[i] < minimum)
            minimum = indices[i];
        if (indices[i] > maximum)
            maximum = indices[i];
    }

    const ImU64 first_vertex = (ImU64)vertex_offset + minimum;
    const ImU64 last_vertex = (ImU64)vertex_offset + maximum;
    if (last_vertex >= (unsigned int)vertex_count)
        return false;

    const ImU64 slice_vertex_count = last_vertex - first_vertex + 1;
    if (slice_vertex_count > 0x10000U)
        return false;

    slice->Indices.resize((int)index_count);
    for (unsigned int i = 0; i < index_count; ++i)
        slice->Indices[(int)i] = (ImDrawIdx)(indices[i] - minimum);

    slice->VertexOffset = (unsigned int)first_vertex;
    slice->VertexCount = (unsigned int)slice_vertex_count;
    return true;
}

static unsigned int ImGui_ImplCK2_GetTextureRetryDelay(unsigned int failure_count)
{
    if (failure_count == 0)
        return 0;
    const unsigned int shift = failure_count > 7 ? 6 : failure_count - 1;
    return 1U << shift;
}

#ifdef IMGUI_USE_BGRA_PACKED_COLOR
#define IMGUI_COL_TO_ARGB(_COL) (_COL)
#else
#define IMGUI_COL_TO_ARGB(_COL) (((_COL) & 0xFF00FF00) | (((_COL) & 0xFF0000) >> 16) | (((_COL) & 0xFF) << 16))
#endif

// Copy texture region with optional format conversion
static void ImGui_ImplCK2_CopyTextureRegion(bool tex_use_colors, const ImU32 *src, int src_pitch,
                                             ImU32 *dst, int dst_pitch, int w, int h)
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
        else
#endif
        {
            memcpy(dst_p, src_p, w * 4); // Raw copy
        }
    }
}

#if defined(BML_TEST)
bool ImGui_ImplCK2_TestSelectTextureLimits(unsigned int reported_width,
                                           unsigned int reported_height,
                                           int *width, int *height, bool *used_fallback)
{
    if (!width || !height || !used_fallback)
        return false;
    const ImGui_ImplCK2_TextureLimits limits =
        ImGui_ImplCK2_SelectTextureLimits(reported_width, reported_height);
    *width = limits.Width;
    *height = limits.Height;
    *used_fallback = limits.UsedFallback;
    return true;
}

bool ImGui_ImplCK2_TestBuildDrawSlice(const ImDrawIdx *indices, unsigned int index_count,
                                      unsigned int vertex_offset, int vertex_count,
                                      unsigned int *slice_vertex_offset,
                                      unsigned int *slice_vertex_count,
                                      ImDrawIdx *rebased_indices,
                                      unsigned int rebased_capacity)
{
    if (!slice_vertex_offset || !slice_vertex_count ||
        (index_count != 0 && (!rebased_indices || rebased_capacity < index_count)))
        return false;

    ImGui_ImplCK2_DrawSlice slice;
    if (!ImGui_ImplCK2_BuildDrawSlice(indices, index_count, vertex_offset, vertex_count, &slice))
        return false;

    *slice_vertex_offset = slice.VertexOffset;
    *slice_vertex_count = slice.VertexCount;
    for (unsigned int i = 0; i < index_count; ++i)
        rebased_indices[i] = slice.Indices[(int)i];
    return true;
}

unsigned int ImGui_ImplCK2_TestGetTextureRetryDelay(unsigned int failure_count)
{
    return ImGui_ImplCK2_GetTextureRetryDelay(failure_count);
}

void ImGui_ImplCK2_TestCopyTextureRegion(bool use_colors, const ImU32 *src, int src_pitch,
                                         ImU32 *dst, int dst_pitch, int width, int height)
{
    ImGui_ImplCK2_CopyTextureRegion(use_colors, src, src_pitch, dst, dst_pitch, width, height);
}
#endif

#if !defined(BML_TEST_CK2_BACKEND_LOGIC)
// Backend data stored in io.BackendPlatformUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplCK2_Data *ImGui_ImplCK2_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplCK2_Data *)ImGui::GetIO().BackendRendererUserData : NULL;
}

static const char *ImGui_ImplCK2_GetTextureFailureName(ImGui_ImplCK2_TextureFailure failure)
{
    switch (failure)
    {
    case ImGui_ImplCK2_TextureFailure_InvalidRequest:
        return "invalid request";
    case ImGui_ImplCK2_TextureFailure_CreateObject:
        return "object creation";
    case ImGui_ImplCK2_TextureFailure_CreateSurface:
        return "surface creation";
    case ImGui_ImplCK2_TextureFailure_UnsupportedSurface:
        return "unsupported system surface";
    case ImGui_ImplCK2_TextureFailure_LockSurface:
        return "surface lock";
    case ImGui_ImplCK2_TextureFailure_ReleaseSurface:
        return "surface release";
    case ImGui_ImplCK2_TextureFailure_Upload:
        return "video upload";
    default:
        return "none";
    }
}

static ImU64 ImGui_ImplCK2_GetUpdateSignature(const ImTextureData *tex)
{
    ImU64 signature = 14695981039346656037ULL;
    for (const ImTextureRect &rect : tex->Updates)
    {
        signature ^= rect.x;
        signature *= 1099511628211ULL;
        signature ^= rect.y;
        signature *= 1099511628211ULL;
        signature ^= rect.w;
        signature *= 1099511628211ULL;
        signature ^= rect.h;
        signature *= 1099511628211ULL;
    }
    return signature;
}

bool ImGui_ImplCK2_GetDiagnostics(ImGui_ImplCK2_Diagnostics *diagnostics)
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    if (!diagnostics || !bd)
        return false;

    diagnostics->TextureMaxWidth = bd->TextureMaxWidth;
    diagnostics->TextureMaxHeight = bd->TextureMaxHeight;
    diagnostics->LastTextureFailure = ImGui_ImplCK2_GetTextureFailureName(bd->LastTextureFailure);
    diagnostics->LastTextureFailureCount = bd->LastTextureFailureCount;
    return true;
}

static bool ImGui_ImplCK2_TextureRequestMatches(const ImGui_ImplCK2_TextureState *state,
                                                 const ImTextureData *tex)
{
    if (state->RequestStatus != tex->Status || state->Width != tex->Width || state->Height != tex->Height ||
        state->Pixels != tex->Pixels)
        return false;

    if (tex->Status != ImTextureStatus_WantUpdates)
        return true;

    return state->UpdateCount == tex->Updates.Size &&
           state->UpdateSignature == ImGui_ImplCK2_GetUpdateSignature(tex) &&
           state->UpdateRect.x == tex->UpdateRect.x && state->UpdateRect.y == tex->UpdateRect.y &&
           state->UpdateRect.w == tex->UpdateRect.w && state->UpdateRect.h == tex->UpdateRect.h;
}

static void ImGui_ImplCK2_SaveTextureRequest(ImGui_ImplCK2_TextureState *state, const ImTextureData *tex)
{
    state->RequestStatus = tex->Status;
    state->Width = tex->Width;
    state->Height = tex->Height;
    state->Pixels = tex->Pixels;
    state->UpdateRect = tex->UpdateRect;
    state->UpdateCount = tex->Updates.Size;
    state->UpdateSignature = ImGui_ImplCK2_GetUpdateSignature(tex);
}

static void ImGui_ImplCK2_ClearTextureFailure(ImTextureData *tex)
{
    if (!tex->BackendUserData)
        return;

    IM_DELETE((ImGui_ImplCK2_TextureState *)tex->BackendUserData);
    tex->BackendUserData = nullptr;
}

static bool ImGui_ImplCK2_CanAttemptTextureUpdate(ImGui_ImplCK2_Data *bd, ImTextureData *tex)
{
    if (tex->Status == ImTextureStatus_WantDestroy)
        return true;

    ImGui_ImplCK2_TextureState *state = (ImGui_ImplCK2_TextureState *)tex->BackendUserData;
    if (!state)
        return true;

    if (!ImGui_ImplCK2_TextureRequestMatches(state, tex))
    {
        ImGui_ImplCK2_ClearTextureFailure(tex);
        return true;
    }

    return (int)(bd->FrameIndex - state->NextRetryFrame) >= 0;
}

static void ImGui_ImplCK2_RecordTextureFailure(ImGui_ImplCK2_Data *bd, ImTextureData *tex,
                                                ImGui_ImplCK2_TextureFailure failure)
{
    ImGui_ImplCK2_TextureState *state = (ImGui_ImplCK2_TextureState *)tex->BackendUserData;
    if (!state)
    {
        state = IM_NEW(ImGui_ImplCK2_TextureState)();
        tex->BackendUserData = state;
    }
    else if (!ImGui_ImplCK2_TextureRequestMatches(state, tex))
    {
        state->FailureCount = 0;
        state->Failure = ImGui_ImplCK2_TextureFailure_None;
    }

    const bool failure_changed = state->Failure != failure;
    state->Failure = failure;
    ++state->FailureCount;
    ImGui_ImplCK2_SaveTextureRequest(state, tex);

    state->NextRetryFrame = bd->FrameIndex + ImGui_ImplCK2_GetTextureRetryDelay(state->FailureCount);
    bd->LastTextureFailure = failure;
    bd->LastTextureFailureCount = state->FailureCount;

    if (failure_changed || (state->FailureCount & (state->FailureCount - 1)) == 0)
        utils::OutputDebugA("BML CK2 renderer texture %d failed during %s (attempt %u)\n",
                            tex->UniqueID, ImGui_ImplCK2_GetTextureFailureName(failure), state->FailureCount);
}

static bool ImGui_ImplCK2_ValidateTextureRequest(const ImGui_ImplCK2_Data *bd, const ImTextureData *tex)
{
    return tex->Format == ImTextureFormat_RGBA32 && tex->BytesPerPixel == 4 && tex->Pixels &&
           tex->Width > 0 && tex->Height > 0 && tex->Width <= bd->TextureMaxWidth &&
           tex->Height <= bd->TextureMaxHeight;
}

static bool ImGui_ImplCK2_ValidateSystemSurface(CKTexture *texture, int width, int height,
                                                VxImageDescEx *description)
{
    if (!texture->GetSystemTextureDesc(*description) || description->Width != width ||
        description->Height != height || description->BitsPerPixel != 32 ||
        description->BytesPerLine < width * 4)
        return false;

    VxImageDescEx expected;
    VxPixelFormat2ImageDesc(_32_ARGB8888, expected);
    return description->RedMask == expected.RedMask && description->GreenMask == expected.GreenMask &&
           description->BlueMask == expected.BlueMask && description->AlphaMask == expected.AlphaMask;
}

static bool ImGui_ImplCK2_ValidateUpdateRects(const ImTextureData *tex)
{
    for (const ImTextureRect &rect : tex->Updates)
    {
        if (rect.w == 0 || rect.h == 0 || rect.x >= tex->Width || rect.y >= tex->Height ||
            rect.w > tex->Width - rect.x || rect.h > tex->Height - rect.y)
            return false;
    }
    return true;
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

void ImGui_ImplCK2_UpdateTexture(ImTextureData *tex)
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    if (!bd || !bd->Context || !bd->RenderContext || !tex ||
        !ImGui_ImplCK2_CanAttemptTextureUpdate(bd, tex))
        return;

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        if (tex->TexID != ImTextureID_Invalid || !ImGui_ImplCK2_ValidateTextureRequest(bd, tex))
        {
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_InvalidRequest);
            return;
        }

        CKTexture *ck_tex = (CKTexture *)bd->Context->CreateObject(CKCID_TEXTURE, (CKSTRING) "ImGuiDynamicTexture");
        if (!ck_tex)
        {
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_CreateObject);
            return;
        }

        ck_tex->ModifyObjectFlags(CK_OBJECT_NOTTOBESAVED | CK_OBJECT_NOTTOBEDELETED, 0);
        ck_tex->SetDynamicHint(TRUE);
        ck_tex->SetDesiredVideoFormat(_32_ARGB8888);

        if (!ck_tex->Create(tex->Width, tex->Height))
        {
            bd->Context->DestroyObject(ck_tex);
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_CreateSurface);
            return;
        }

        VxImageDescEx description;
        if (!ImGui_ImplCK2_ValidateSystemSurface(ck_tex, tex->Width, tex->Height, &description))
        {
            bd->Context->DestroyObject(ck_tex);
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_UnsupportedSurface);
            return;
        }

        CKBYTE *ptr = ck_tex->LockSurfacePtr();
        if (!ptr)
        {
            bd->Context->DestroyObject(ck_tex);
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_LockSurface);
            return;
        }

        ImGui_ImplCK2_CopyTextureRegion(tex->UseColors, (const ImU32 *)tex->GetPixels(), tex->GetPitch(),
                                        (ImU32 *)ptr, description.BytesPerLine, tex->Width, tex->Height);
        if (!ck_tex->ReleaseSurfacePtr())
        {
            bd->Context->DestroyObject(ck_tex);
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_ReleaseSurface);
            return;
        }

        if (!ck_tex->SystemToVideoMemory(bd->RenderContext, TRUE))
        {
            bd->Context->DestroyObject(ck_tex);
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_Upload);
            return;
        }

        tex->SetTexID((ImTextureID)ck_tex);
        ImGui_ImplCK2_ClearTextureFailure(tex);
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantUpdates)
    {
        if (tex->TexID == ImTextureID_Invalid || !ImGui_ImplCK2_ValidateTextureRequest(bd, tex) ||
            !ImGui_ImplCK2_ValidateUpdateRects(tex))
        {
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_InvalidRequest);
            return;
        }

        CKTexture *ck_tex = (CKTexture *)tex->TexID;
        VxImageDescEx description;
        if (!ImGui_ImplCK2_ValidateSystemSurface(ck_tex, tex->Width, tex->Height, &description))
        {
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_UnsupportedSurface);
            return;
        }

        if (tex->Updates.Size == 0)
        {
            ImGui_ImplCK2_ClearTextureFailure(tex);
            tex->SetStatus(ImTextureStatus_OK);
            return;
        }

        CKBYTE *ptr = ck_tex->LockSurfacePtr();
        if (!ptr)
        {
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_LockSurface);
            return;
        }

        for (const ImTextureRect &rect : tex->Updates)
        {
            const ImU32 *src_data = (const ImU32 *)tex->GetPixelsAt(rect.x, rect.y);
            ImU32 *dst_data = (ImU32 *)(ptr + rect.y * description.BytesPerLine + rect.x * 4);
            ImGui_ImplCK2_CopyTextureRegion(tex->UseColors, src_data, tex->GetPitch(), dst_data,
                                            description.BytesPerLine, rect.w, rect.h);
        }

        if (!ck_tex->ReleaseSurfacePtr())
        {
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_ReleaseSurface);
            return;
        }

        const CKBOOL uploaded = ck_tex->IsInVideoMemory() ? ck_tex->Restore(TRUE) :
                                                            ck_tex->SystemToVideoMemory(bd->RenderContext, TRUE);
        if (!uploaded)
        {
            ImGui_ImplCK2_RecordTextureFailure(bd, tex, ImGui_ImplCK2_TextureFailure_Upload);
            return;
        }

        ImGui_ImplCK2_ClearTextureFailure(tex);
        tex->SetStatus(ImTextureStatus_OK);
    }
    else if (tex->Status == ImTextureStatus_WantDestroy)
    {
        CKTexture *ck_tex = (CKTexture *)tex->TexID;
        if (ck_tex)
        {
            IM_ASSERT(tex->TexID == (ImTextureID)ck_tex);
            bd->Context->DestroyObject(ck_tex);
        }

        tex->SetTexID(ImTextureID_Invalid);
        ImGui_ImplCK2_ClearTextureFailure(tex);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

// Render function.
void ImGui_ImplCK2_RenderDrawData(ImDrawData *draw_data)
{
    static_assert(sizeof(ImDrawIdx) == sizeof(CKWORD),
                  "The CK2 backend requires 16-bit ImGui indices");

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

    ImGui_ImplCK2_RenderStateBackup state_backup(dev);

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
        bool use_command_slices = cmd_list->VtxBuffer.Size >= 0xFFFF;
        for (int cmd_i = 0; !use_command_slices && cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i)
            use_command_slices = cmd_list->CmdBuffer[cmd_i].VtxOffset != 0;
        ImGui_ImplCK2_DrawSlice draw_slice;

        // For normal sized meshes, prepare all vertices at once
        if (!use_command_slices)
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

            if (pcmd->ElemCount == 0 || pcmd->IdxOffset > static_cast<unsigned int>(cmd_list->IdxBuffer.Size) ||
                pcmd->ElemCount > static_cast<unsigned int>(cmd_list->IdxBuffer.Size) - pcmd->IdxOffset)
                continue;

            const ImDrawIdx *command_indices = idx_buffer + pcmd->IdxOffset;
            const CKWORD *draw_indices = reinterpret_cast<const CKWORD *>(command_indices);

            // CK2 has no base-vertex draw call. For commands using VtxOffset, upload
            // only the referenced vertex range and rebase this command's indices.
            if (use_command_slices || pcmd->VtxOffset != 0)
            {
                if (!ImGui_ImplCK2_BuildDrawSlice(command_indices, pcmd->ElemCount,
                                                  pcmd->VtxOffset, cmd_list->VtxBuffer.Size,
                                                  &draw_slice))
                    continue;

                const ImDrawVert *vtx_src = vtx_buffer + draw_slice.VertexOffset;
                const int vtx_count = static_cast<int>(draw_slice.VertexCount);

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

                draw_indices = draw_slice.Indices.Data;
            }

            // Set texture or material
            CKObject *obj = (CKObject *)pcmd->GetTexID();
            if (!obj)
                continue; // Skip if no texture/material

            if (obj->GetClassID() == CKCID_TEXTURE)
            {
                CKTexture *texture = (CKTexture *)obj;
                dev->SetTexture(texture);
                dev->DrawPrimitive(VX_TRIANGLELIST, (CKWORD *)draw_indices, pcmd->ElemCount, data);
            }
            else if (obj->GetClassID() == CKCID_MATERIAL)
            {
                ((CKMaterial *)obj)->SetAsCurrent(dev);
                dev->DrawPrimitive(VX_TRIANGLELIST, (CKWORD *)draw_indices, pcmd->ElemCount, data);
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

    unsigned int reported_width = 0;
    unsigned int reported_height = 0;
    CKRenderManager *render_manager = context->GetRenderManager();
    const int driver_index = render_context->GetDriverIndex();
    if (render_manager && driver_index >= 0 && driver_index < render_manager->GetRenderDriverCount())
    {
        const VxDriverDesc *driver = render_manager->GetRenderDriverDescription(driver_index);
        if (driver)
        {
            reported_width = (unsigned int)driver->Caps3D.MaxTextureWidth;
            reported_height = (unsigned int)driver->Caps3D.MaxTextureHeight;
        }
    }

    const ImGui_ImplCK2_TextureLimits texture_limits =
        ImGui_ImplCK2_SelectTextureLimits(reported_width, reported_height);

    // Setup backend capabilities flags
    ImGui_ImplCK2_Data *bd = IM_NEW(ImGui_ImplCK2_Data)();
    io.BackendRendererUserData = (void *)bd;
    io.BackendRendererName = "imgui_impl_ck2";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;  // We can honor ImGuiPlatformIO::Textures[] requests during render.

    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_TextureMaxWidth = texture_limits.Width;
    platform_io.Renderer_TextureMaxHeight = texture_limits.Height;

    bd->Context = context;
    bd->RenderContext = render_context;
    bd->TextureMaxWidth = texture_limits.Width;
    bd->TextureMaxHeight = texture_limits.Height;

    if (texture_limits.UsedFallback)
        utils::OutputDebugA("BML CK2 renderer received invalid texture limits; using %dx%d\n",
                            texture_limits.Width, texture_limits.Height);

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
    ImGui::GetPlatformIO().ClearRendererHandlers();
    IM_DELETE(bd);
}

bool ImGui_ImplCK2_CreateDeviceObjects()
{
    ImGui_ImplCK2_Data *bd = ImGui_ImplCK2_GetBackendData();
    if (!bd || !bd->Context)
        return false;

    for (ImTextureData *tex : ImGui::GetPlatformIO().Textures)
        if (tex->Status != ImTextureStatus_OK)
            ImGui_ImplCK2_ClearTextureFailure(tex);
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
    ++bd->FrameIndex;
}
#endif
