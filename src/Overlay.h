#ifndef BML_OVERLAY_H
#define BML_OVERLAY_H

#include "imgui.h"

#include "CKContext.h"

namespace Overlay {
    ImGuiContext *GetImGuiContext();
    bool IsImGuiReady();
    bool IsImGuiFrameActive();
    bool IsImGuiRenderReady();

    // Switches to the loader's ImGui context for as long as it lives, and switches
    // back on the way out. The loader has no context before ImGuiCreateContext and
    // none after ImGuiDestroyContext, so the scope does nothing at all in those
    // windows and IsActive reports false. Code inside a scope must not touch ImGui
    // unless the scope is active.
    class ImGuiContextScope {
    public:
        ImGuiContextScope()
            : m_PreviousContext(ImGui::GetCurrentContext()), m_Active(GetImGuiContext() != nullptr) {
            if (m_Active)
                ImGui::SetCurrentContext(GetImGuiContext());
        }

        ImGuiContextScope(const ImGuiContextScope &rhs) = delete;
        ImGuiContextScope(ImGuiContextScope &&rhs) noexcept = delete;

        ~ImGuiContextScope() {
            if (m_Active)
                ImGui::SetCurrentContext(m_PreviousContext);
        }

        ImGuiContextScope &operator=(const ImGuiContextScope &rhs) = delete;
        ImGuiContextScope &operator=(ImGuiContextScope &&rhs) noexcept = delete;

        bool IsActive() const { return m_Active; }

    private:
        ImGuiContext *m_PreviousContext;
        bool m_Active;
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
    void ImGuiEndFrame();
    void ImGuiRender();
    void ImGuiOnRender();
};

#endif // BML_OVERLAY_H
