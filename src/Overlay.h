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

        ImGuiContextScope(const ImGuiContextScope &rhs) = delete;
        ImGuiContextScope(ImGuiContextScope &&rhs) noexcept = delete;

        ~ImGuiContextScope() {
            ImGui::SetCurrentContext(m_ImGuiContext);
        }

        ImGuiContextScope &operator=(const ImGuiContextScope &rhs) = delete;
        ImGuiContextScope &operator=(ImGuiContextScope &&rhs) noexcept = delete;

    private:
        ImGuiContext *m_ImGuiContext;
    };

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
