#ifndef BML_OVERLAY_H
#define BML_OVERLAY_H

#include "imgui.h"

#include "CKContext.h"

namespace Overlay {
    ImGuiContext *GetImGuiContext();

    class ImGuiContextScope {
    public:
        explicit ImGuiContextScope(ImGuiContext *context = nullptr) {
            m_ImGuiContext = (context != nullptr) ? context : ImGui::GetCurrentContext();
            ImGui::SetCurrentContext(GetImGuiContext());
        }

        ~ImGuiContextScope() {
            ImGui::SetCurrentContext(m_ImGuiContext);
        }

    private:
        ImGuiContext *m_ImGuiContext;
    };

    bool ImGuiInstallWin32Hooks();
    bool ImGuiUninstallWin32Hooks();

    ImGuiContext *ImGuiCreateContext();
    void ImGuiDestroyContext();

    bool ImGuiInitPlatform(CKContext *context);
    bool ImGuiInitRenderer(CKContext *context);
    void ImGuiShutdownPlatform(CKContext *context);
    void ImGuiShutdownRenderer(CKContext *context);

    void ImGuiNewFrame();
    void ImGuiRender();
    void ImGuiOnRender();
};

#endif // BML_OVERLAY_H
