#ifndef BML_OVERLAY_H
#define BML_OVERLAY_H

#include "imgui.h"

#include "CKContext.h"

namespace Overlay {
    ImGuiContext *GetImGuiContext();

    void ImGuiSetCurrentContext();
    void ImGuiRestorePreviousContext();

    bool ImGuiCreateContext(CKContext *context, bool originalPlayer = false);
    void ImGuiDestroyContext(CKContext *context, bool originalPlayer = false);

    bool ImGuiInit(CKContext *context);
    void ImGuiShutdown();

    void ImGuiNewFrame();
    void ImGuiRender();
    void ImGuiOnRender();
};

#endif // BML_OVERLAY_H
