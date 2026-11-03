#ifndef BML_RENDERHOOK_H
#define BML_RENDERHOOK_H

#include <memory>

class CKContext;
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
#if BML_RUNTIME_SDL
    // Cancel a request when this CK world is reset, without detaching its context.
    static void ResetPendingSkip(CKContext *context);
#endif

    static void EnableWidescreenFix(bool enable);
    static bool IsWidescreenFixAvailable();

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

#endif // BML_RENDERHOOK_H
