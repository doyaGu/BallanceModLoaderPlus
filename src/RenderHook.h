#ifndef BML_RENDERHOOK_H
#define BML_RENDERHOOK_H

#include "CKRenderManager.h"
#include "CKRenderContext.h"

namespace RenderHook {
    void HookRenderManager(CKRenderManager *man);
    void UnhookRenderManager(CKRenderManager *man);
    void HookRenderContext(CKRenderContext *rc);
    void UnhookRenderContext(CKRenderContext *rc);

    void DisableRender(bool disable);
    void EnableWidescreenFix(bool enable);
}

#endif // BML_RENDERHOOK_H
