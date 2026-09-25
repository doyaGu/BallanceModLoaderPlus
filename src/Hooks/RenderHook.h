#ifndef BML_RENDERHOOK_H
#define BML_RENDERHOOK_H

#include <memory>

class CKRenderContext;

class RenderHook {
public:
    RenderHook();
    ~RenderHook();

    RenderHook(const RenderHook &) = delete;
    RenderHook &operator=(const RenderHook &) = delete;

    bool Attach(CKRenderContext *renderContext);
    bool Detach();
    bool IsAttached() const;

    static bool IsSkipRenderAvailable();
    static void SkipNextRender();

    static void EnableWidescreenFix(bool enable);
    static void ApplyWidescreenProjection(CKRenderContext *renderContext);
    static bool CalculateWidescreenFov(float cameraFov, float aspectRatio, float *correctedFov);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // BML_RENDERHOOK_H
