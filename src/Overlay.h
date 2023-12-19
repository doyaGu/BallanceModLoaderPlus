#ifndef BML_OVERLAY_H
#define BML_OVERLAY_H

#include "imgui.h"

#include "CKContext.h"

namespace Overlay {
    ImGuiContext *GetImGuiContext();

    bool ImGuiInit(CKContext *context, bool originalPlayer = false);
    void ImGuiShutdown(CKContext *context, bool originalPlayer = false);

    void ImGuiNewFrame();
    void ImGuiRender();
    void ImGuiOnRender();
};

#endif // BML_OVERLAY_H
