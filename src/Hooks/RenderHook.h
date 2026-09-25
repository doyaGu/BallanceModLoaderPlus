#ifndef BML_RENDERHOOK_H
#define BML_RENDERHOOK_H

class CKRenderContext;

namespace RenderHook {
    bool Attach(CKRenderContext *renderContext);
    bool Detach();

    bool IsSkipRenderAvailable();
    void SkipNextRender();

    void EnableWidescreenFix(bool enable);
    void ApplyWidescreenProjection(CKRenderContext *renderContext);
    bool CalculateWidescreenFov(float cameraFov, float aspectRatio, float *correctedFov);
}

#endif // BML_RENDERHOOK_H
