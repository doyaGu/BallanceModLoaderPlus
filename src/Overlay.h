#ifndef BML_OVERLAY_H
#define BML_OVERLAY_H

#include <vector>

#include "imgui.h"

class CKContext;

namespace Overlay {
    ImGuiContext *GetImGuiContext();
    bool IsImGuiReady();
    bool IsImGuiFrameActive();

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

    class ScriptImGuiCallScope;

    // Tracks the ImGui mouse state created by one script Mod. Ownership lives on
    // the ScriptMod itself, so unloading it can release only its state without a
    // process-wide owner registry or opaque owner keys.
    class ScriptImGuiState {
    public:
        ScriptImGuiState() = default;
        ScriptImGuiState(const ScriptImGuiState &) = delete;
        ScriptImGuiState &operator=(const ScriptImGuiState &) = delete;

        bool Release(ImGuiContext *context = nullptr);

    private:
        friend class ScriptImGuiCallScope;

        struct CallSnapshot {
            ImGuiContext *Context = nullptr;
            unsigned int ActiveId = 0;
            void *ActiveIdWindow = nullptr;
            void *CurrentWindow = nullptr;
            void *MovingWindow = nullptr;
            int WantCaptureMouseNextFrame = -1;
            int WindowStackSize = 0;
            bool Active = false;
        };

        void BeginCall(CallSnapshot &snapshot, ImGuiContext *context);
        void EndCall(CallSnapshot &snapshot);
        bool HasWindow(const void *window) const;
        void AddWindow(const void *window);
        void Clear();

        ImGuiContext *m_Context = nullptr;
        std::vector<void *> m_Windows;
        unsigned int m_ActiveId = 0;
        void *m_ActiveIdWindow = nullptr;
        void *m_MovingWindow = nullptr;
        bool m_OwnsNextFrameMouseCapture = false;
    };

    class ScriptImGuiCallScope {
    public:
        ScriptImGuiCallScope() = default;
        explicit ScriptImGuiCallScope(ScriptImGuiState &state, ImGuiContext *context = nullptr) {
            Begin(state, context);
        }
        ScriptImGuiCallScope(const ScriptImGuiCallScope &) = delete;
        ScriptImGuiCallScope &operator=(const ScriptImGuiCallScope &) = delete;
        ~ScriptImGuiCallScope() { End(); }

        bool Begin(ScriptImGuiState &state, ImGuiContext *context = nullptr);
        void End();

    private:
        ScriptImGuiState *m_State = nullptr;
        ScriptImGuiState::CallSnapshot m_Snapshot;
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
