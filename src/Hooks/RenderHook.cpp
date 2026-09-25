#include "Hooks/RenderHook.h"

#include "CKCamera.h"
#include "CKRenderContext.h"

#include <cmath>
#include <cstddef>
#include <exception>

#include "Hooks/VTablePatch.h"
#include "Hooks/VTables.h"
#include "HookUtils.h"

using RenderContextVTable = CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>;

struct RenderHook::Impl {
    VTablePatch Patch;
    CKRenderContext *Context = nullptr;
    void **VTable = nullptr;
    RenderContextVTable::RenderFunc OriginalRender = nullptr;
    bool SkipNextRender = false;

    static Impl *Active;
    static bool WidescreenFixEnabled;

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
        m_Impl->Context = renderContext;
        m_Impl->SkipNextRender = false;
        Impl::Active = m_Impl.get();
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
    return true;
}

bool RenderHook::Detach() {
    if (!m_Impl)
        return true;
    if (Impl::Active && Impl::Active != m_Impl.get())
        return !m_Impl->Patch.IsInstalled();

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
    Impl::WidescreenFixEnabled = enable;
}

bool RenderHook::CalculateWidescreenFov(float cameraFov, float aspectRatio,
                                        float *correctedFov) {
    if (!correctedFov || !std::isfinite(cameraFov) || !std::isfinite(aspectRatio) ||
        cameraFov <= 0.0f || cameraFov >= 3.14159265358979323846f || aspectRatio <= 0.0f) {
        return false;
    }

    constexpr float referenceAspect = 4.0f / 3.0f;
    if (aspectRatio <= referenceAspect) {
        *correctedFov = cameraFov;
        return true;
    }

    const float tangent = std::tan(cameraFov * 0.5f);
    const float result = 2.0f * std::atan(tangent * aspectRatio / referenceAspect);
    if (!std::isfinite(result) || result <= 0.0f || result >= 3.14159265358979323846f)
        return false;

    *correctedFov = result;
    return true;
}

void RenderHook::ApplyWidescreenProjection(CKRenderContext *renderContext) {
    if (!Impl::WidescreenFixEnabled || !renderContext)
        return;

    CKCamera *camera = renderContext->GetAttachedCamera();
    if (!camera || camera->GetProjectionType() != CK_PERSPECTIVEPROJECTION)
        return;

    VxRect viewRect;
    renderContext->GetViewRect(viewRect);
    const float width = viewRect.GetWidth();
    const float height = viewRect.GetHeight();
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0f || height <= 0.0f)
        return;

    const float aspectRatio = width / height;
    constexpr float referenceAspect = 4.0f / 3.0f;
    if (!std::isfinite(aspectRatio) || aspectRatio <= referenceAspect)
        return;

    const float frontPlane = camera->GetFrontPlane();
    const float backPlane = camera->GetBackPlane();
    if (!std::isfinite(frontPlane) || !std::isfinite(backPlane) || frontPlane <= 0.0f ||
        backPlane <= frontPlane)
        return;

    float correctedFov = 0.0f;
    if (!CalculateWidescreenFov(camera->GetFov(), aspectRatio, &correctedFov))
        return;

    VxMatrix projection;
    projection.Perspective(correctedFov, aspectRatio, frontPlane, backPlane);
    renderContext->SetProjectionTransformationMatrix(projection);
}
