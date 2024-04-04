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

    ImGuiContext *ImGuiCreateContext();
    void ImGuiDestroyContext();

    bool ImGuiInit(CKContext *context);
    void ImGuiShutdown(CKContext *context);

    void ImGuiNewFrame();
    void ImGuiRender();
    void ImGuiOnRender();
};

#endif // BML_OVERLAY_H
