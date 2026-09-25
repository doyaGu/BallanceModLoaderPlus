#include "Hooks/RenderHook.h"

#include "CKCamera.h"
#include "CKRenderContext.h"

#include <cmath>
#include <cstddef>

#include "Hooks/VTablePatch.h"
#include "Hooks/VTables.h"
#include "HookUtils.h"

namespace {
    using RenderContextVTable = CP_CLASS_VTABLE_NAME(CKRenderContext)<CKRenderContext>;

    VTablePatch g_RenderPatch;
    CKRenderContext *g_RenderContext = nullptr;
    void **g_RenderVTable = nullptr;
    RenderContextVTable::RenderFunc g_OriginalRender = nullptr;
    bool g_SkipNextRender = false;
    bool g_WidescreenFixEnabled = false;

    class RenderInterceptor {
    public:
        CP_DECLARE_METHOD_HOOK(CKERROR, Render, (CK_RENDER_FLAGS flags)) {
            auto *renderContext = reinterpret_cast<CKRenderContext *>(this);
            if (renderContext == g_RenderContext && g_SkipNextRender) {
                g_SkipNextRender = false;
                return CK_OK;
            }

            return g_OriginalRender ? CP_CALL_METHOD_PTR(renderContext, g_OriginalRender, flags) :
                                      CKERR_INVALIDRENDERCONTEXT;
        }
    };
}

bool RenderHook::Attach(CKRenderContext *renderContext) {
    if (!renderContext)
        return false;

    void **vtable = utils::GetVTable(renderContext);
    if (g_RenderPatch.IsInstalled()) {
        if (g_RenderVTable != vtable)
            return false;
        g_RenderContext = renderContext;
        g_SkipNextRender = false;
        return true;
    }

    const std::size_t renderSlot = offsetof(RenderContextVTable, Render) / sizeof(void *);
    const VTablePatch::Request request = {
        renderSlot,
        utils::TypeErase(&RenderInterceptor::CP_FUNC_HOOK_NAME(Render)),
    };
    const VTablePatchResult result = g_RenderPatch.Install(renderContext, &request, 1);
    if (!result) {
        utils::OutputDebugA("BML RenderHook install failed: %s (entry %zu)\n",
                            VTablePatch::GetErrorName(result.Code), result.EntryIndex);
        return false;
    }

    g_OriginalRender = utils::ForceReinterpretCast<RenderContextVTable::RenderFunc>(
        g_RenderPatch.GetOriginal(renderSlot));
    g_RenderContext = renderContext;
    g_RenderVTable = vtable;
    g_SkipNextRender = false;
    return true;
}

bool RenderHook::Detach() {
    const VTablePatchResult result = g_RenderPatch.Remove();
    if (!result) {
        utils::OutputDebugA("BML RenderHook removal warning: %s (entry %zu)\n",
                            VTablePatch::GetErrorName(result.Code), result.EntryIndex);
    }

    if (g_RenderPatch.IsInstalled())
        return false;

    g_RenderContext = nullptr;
    g_RenderVTable = nullptr;
    g_OriginalRender = nullptr;
    g_SkipNextRender = false;
    return true;
}

bool RenderHook::IsSkipRenderAvailable() {
    return g_RenderPatch.IsInstalled();
}

void RenderHook::SkipNextRender() {
    if (IsSkipRenderAvailable())
        g_SkipNextRender = true;
}

void RenderHook::EnableWidescreenFix(bool enable) {
    g_WidescreenFixEnabled = enable;
}

bool RenderHook::CalculateWidescreenFov(float cameraFov, float aspectRatio, float *correctedFov) {
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
    if (!g_WidescreenFixEnabled || !renderContext)
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
    if (!std::isfinite(frontPlane) || !std::isfinite(backPlane) || frontPlane <= 0.0f || backPlane <= frontPlane)
        return;

    float correctedFov = 0.0f;
    if (!CalculateWidescreenFov(camera->GetFov(), aspectRatio, &correctedFov))
        return;

    VxMatrix projection;
    projection.Perspective(correctedFov, aspectRatio, frontPlane, backPlane);
    renderContext->SetProjectionTransformationMatrix(projection);
}
