#include "Hooks/RenderHook.h"

#include "CKRenderContext.h"
#include "CK2dEntity.h"
#include "Hooks/CKRasterizer.h"

#include <Windows.h>
#include <MinHook.h>

#include <cmath>
#include <cstddef>
#include <exception>

#include "Hooks/VTablePatch.h"
#include "Hooks/VTables.h"
#include "HookUtils.h"

using RenderContextVTable = CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>;

class CKRenderedScene;

namespace {

struct VxCallBack {
    void *callback;
    void *argument;
    CKBOOL temp;
};

class CKCallbacksContainer {
public:
    XClassArray<VxCallBack> m_PreCallBacks;
    VxCallBack *m_OnCallBack;
    XClassArray<VxCallBack> m_PostCallBacks;
};

struct CKRenderContextSettings {
    CKRECT m_Rect;
    int m_Bpp;
    int m_Zbpp;
    int m_StencilBpp;
};

// Original CK2_3D layout, through the fields used by UpdateProjection.
class CKRenderContextHook : public CKRenderContext {
public:
    CKBOOL UpdateProjection(CKBOOL force, bool widescreen) {
        if (!force && m_ProjectionUpdated)
            return TRUE;
        if (!m_RasterizerContext)
            return FALSE;

        const float aspect = (float) ((double) m_ViewportData.ViewWidth / (double) m_ViewportData.ViewHeight);
        if (m_Perspective) {
            const float fov = widescreen ? atan2f(tanf(m_Fov * 0.5f) * 0.75f * aspect, 1.0f) * 2.0f : m_Fov;
            m_ProjectionMatrix.Perspective(fov, aspect, m_NearPlane, m_FarPlane);
        } else {
            m_ProjectionMatrix.Orthographic(m_Zoom, aspect, m_NearPlane, m_FarPlane);
        }

        m_RasterizerContext->SetTransformMatrix(VXMATRIX_PROJECTION, m_ProjectionMatrix);
        m_RasterizerContext->SetViewport(&m_ViewportData);
        m_ProjectionUpdated = TRUE;

        VxRect rect(0.0f, 0.0f, (float) m_Settings.m_Rect.right, (float) m_Settings.m_Rect.bottom);
        Get2dRoot(TRUE)->SetRect(rect);
        Get2dRoot(FALSE)->SetRect(rect);
        return TRUE;
    }

    WIN_HANDLE m_WinHandle;
    WIN_HANDLE m_AppHandle;
    CKRECT m_WinRect;
    CK_RENDER_FLAGS m_RenderFlags;
    CKRenderedScene *m_RenderedScene;
    CKBOOL m_Fullscreen;
    CKBOOL m_Active;
    CKBOOL m_Perspective;
    CKBOOL m_ProjectionUpdated;
    CKBOOL m_Start;
    CKBOOL m_TransparentMode;
    CKBOOL m_DeviceValid;
    CKCallbacksContainer m_PreRenderCallBacks;
    CKCallbacksContainer m_PreRenderTempCallBacks;
    CKCallbacksContainer m_PostRenderCallBacks;
    CKRenderManager *m_RenderManager;
    CKRasterizerContext *m_RasterizerContext;
    CKRasterizerDriver *m_RasterizerDriver;
    int m_DriverIndex;
    CKDWORD m_Shading;
    CKDWORD m_TextureEnabled;
    CKDWORD m_DisplayWireframe;
    VxFrustum m_Frustum;
    float m_Fov;
    float m_Zoom;
    float m_NearPlane;
    float m_FarPlane;
    VxMatrix m_ProjectionMatrix;
    CKViewportData m_ViewportData;
    CKRenderContextSettings m_Settings;
};

static_assert(offsetof(CKRenderContextHook, m_ProjectionUpdated) == 0x40);
static_assert(offsetof(CKRenderContextHook, m_RasterizerContext) == 0xa8);
static_assert(offsetof(CKRenderContextHook, m_ProjectionMatrix) == 0x17c);
static_assert(offsetof(CKRenderContextHook, m_Settings) == 0x1d4);

void InvalidateProjection(CKRenderContext *context) {
    reinterpret_cast<CKRenderContextHook *>(context)->m_ProjectionUpdated = FALSE;
}

void *FindProjectionUpdate(void **vtable) {
    HMODULE module = ::GetModuleHandleW(L"CK2_3D.dll");
    MEMORY_BASIC_INFORMATION memory{};
    if (!module || !::VirtualQuery(vtable, &memory, sizeof(memory)) || memory.AllocationBase != module)
        return nullptr;
    return reinterpret_cast<unsigned char *>(module) + 0x6c68d;
}

} // namespace

struct RenderHook::Impl {
    VTablePatch Patch;
    CKRenderContext *Context = nullptr;
    void **VTable = nullptr;
    RenderContextVTable::RenderFunc OriginalRender = nullptr;
    bool SkipNextRender = false;
    void *ProjectionTarget = nullptr;
    using ProjectionFunction = CKBOOL (__thiscall *)(CKRenderContext *, CKBOOL);
    ProjectionFunction OriginalProjection = nullptr;
    bool ProjectionInstalled = false;

    static Impl *Active;
    static bool WidescreenFixEnabled;

    static CKBOOL __fastcall UpdateProjection(CKRenderContext *context, void *, CKBOOL force) {
        Impl *active = Active;
        if (!active || !active->OriginalProjection)
            return FALSE;
        if (context != active->Context)
            return active->OriginalProjection(context, force);
        return reinterpret_cast<CKRenderContextHook *>(context)->UpdateProjection(force, WidescreenFixEnabled);
    }

    void AttachProjection() {
        if (ProjectionTarget)
            return;
        void *target = FindProjectionUpdate(VTable);
        if (!target)
            return;
        const MH_STATUS created = MH_CreateHook(target, reinterpret_cast<void *>(&UpdateProjection),
                                               reinterpret_cast<void **>(&OriginalProjection));
        if (created != MH_OK)
            return;
        ProjectionTarget = target;
        if (MH_EnableHook(target) != MH_OK) {
            DetachProjection();
            return;
        }
        ProjectionInstalled = true;
        if (WidescreenFixEnabled)
            InvalidateProjection(Context);
    }

    bool DetachProjection() {
        if (!ProjectionTarget)
            return true;
        const MH_STATUS disabled = MH_DisableHook(ProjectionTarget);
        if (disabled != MH_OK && disabled != MH_ERROR_DISABLED && disabled != MH_ERROR_NOT_CREATED)
            return false;
        ProjectionInstalled = false;
        const MH_STATUS removed = MH_RemoveHook(ProjectionTarget);
        if (removed != MH_OK && removed != MH_ERROR_NOT_CREATED)
            return false;
        InvalidateProjection(Context);
        ProjectionTarget = nullptr;
        OriginalProjection = nullptr;
        return true;
    }

    CP_DECLARE_METHOD_HOOK(CKERROR, Render, (CK_RENDER_FLAGS flags)) {
        auto *renderContext = reinterpret_cast<CKRenderContext *>(this);
        Impl *active = Active;
        if (!active || !active->OriginalRender)
            return CKERR_INVALIDRENDERCONTEXT;
        if (renderContext == active->Context && active->SkipNextRender) {
            active->SkipNextRender = false;
            return CK_OK;
        }
        return CP_CALL_METHOD_PTR(renderContext, active->OriginalRender, flags);
    }
};

RenderHook::Impl *RenderHook::Impl::Active = nullptr;
bool RenderHook::Impl::WidescreenFixEnabled = false;

RenderHook::RenderHook() : m_Impl(std::make_unique<Impl>()) {}

RenderHook::~RenderHook() {
    if (!Detach())
        std::terminate();
}

bool RenderHook::Attach(CKRenderContext *renderContext) {
    if (!renderContext || !m_Impl)
        return false;
    if (Impl::Active && Impl::Active != m_Impl.get())
        return false;

    void **vtable = utils::GetVTable(renderContext);
    if (m_Impl->Patch.IsInstalled()) {
        if (m_Impl->VTable != vtable)
            return false;
        if (m_Impl->ProjectionTarget && m_Impl->Context != renderContext) {
            InvalidateProjection(m_Impl->Context);
            InvalidateProjection(renderContext);
        }
        m_Impl->Context = renderContext;
        m_Impl->SkipNextRender = false;
        Impl::Active = m_Impl.get();
        m_Impl->AttachProjection();
        return true;
    }

    const std::size_t renderSlot = offsetof(RenderContextVTable, Render) / sizeof(void *);
    const VTablePatch::Request request = {
        renderSlot,
        utils::TypeErase(&Impl::CP_FUNC_HOOK_NAME(Render)),
    };
    const VTablePatchResult result = m_Impl->Patch.Install(renderContext, &request, 1);
    if (!result) {
        utils::OutputDebugA("BML RenderHook install failed: %s (entry %zu)\n",
                            VTablePatch::GetErrorName(result.Code), result.EntryIndex);
        return false;
    }

    m_Impl->OriginalRender = utils::ForceReinterpretCast<RenderContextVTable::RenderFunc>(
        m_Impl->Patch.GetOriginal(renderSlot));
    m_Impl->Context = renderContext;
    m_Impl->VTable = vtable;
    m_Impl->SkipNextRender = false;
    Impl::Active = m_Impl.get();
    m_Impl->AttachProjection();
    return true;
}

bool RenderHook::Detach() {
    if (!m_Impl)
        return true;
    if (Impl::Active && Impl::Active != m_Impl.get())
        return !m_Impl->Patch.IsInstalled();

    if (!m_Impl->DetachProjection()) {
        utils::OutputDebugA("BML projection hook could not be removed\n");
        return false;
    }

    const VTablePatchResult result = m_Impl->Patch.Remove();
    if (!result) {
        utils::OutputDebugA("BML RenderHook removal warning: %s (entry %zu)\n",
                            VTablePatch::GetErrorName(result.Code), result.EntryIndex);
    }
    if (m_Impl->Patch.IsInstalled())
        return false;

    if (Impl::Active == m_Impl.get())
        Impl::Active = nullptr;
    m_Impl->Context = nullptr;
    m_Impl->VTable = nullptr;
    m_Impl->OriginalRender = nullptr;
    m_Impl->SkipNextRender = false;
    return true;
}

bool RenderHook::IsAttached() const {
    return m_Impl && Impl::Active == m_Impl.get() && m_Impl->Patch.IsInstalled();
}

bool RenderHook::IsSkipRenderAvailable() {
    return Impl::Active && Impl::Active->Patch.IsInstalled();
}

void RenderHook::SkipNextRender() {
    if (IsSkipRenderAvailable())
        Impl::Active->SkipNextRender = true;
}

void RenderHook::EnableWidescreenFix(bool enable) {
    if (Impl::WidescreenFixEnabled == enable)
        return;
    Impl::WidescreenFixEnabled = enable;
    if (IsWidescreenFixAvailable())
        InvalidateProjection(Impl::Active->Context);
}

bool RenderHook::IsWidescreenFixAvailable() {
    return Impl::Active && Impl::Active->ProjectionInstalled;
}
