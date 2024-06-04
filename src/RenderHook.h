#ifndef BML_RENDERHOOK_H
#define BML_RENDERHOOK_H

#include "CKRenderContext.h"

namespace RenderHook {
    void HookRenderContext(CKRenderContext *rc);
    void UnhookRenderContext(CKRenderContext *rc);

    void DisableRender(bool disable);
}

#endif // BML_RENDERHOOK_H
