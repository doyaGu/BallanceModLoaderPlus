#ifndef BML_UI_SCRIPT_IMGUI_H
#define BML_UI_SCRIPT_IMGUI_H

#include <string>
#include <string_view>
#include <vector>

#include "imgui.h"

namespace Overlay {
    std::string MakeScriptImGuiWindowName(std::string_view name, std::string_view ownerId);

    class ScriptImGuiCallScope;

    // Tracks ImGui mouse state created by one Script Mod. Ownership stays on
    // that Mod so unload can release only its windows and active interaction.
    class ScriptImGuiState {
    public:
        ScriptImGuiState() = default;
        ScriptImGuiState(const ScriptImGuiState &) = delete;
        ScriptImGuiState &operator=(const ScriptImGuiState &) = delete;

        bool Release(ImGuiContext *context = nullptr);

    private:
        friend class ScriptImGuiCallScope;

        struct CallSnapshot {
            ImGuiContext *context = nullptr;
            ImGuiID activeId = 0;
            void *activeIdWindow = nullptr;
            void *currentWindow = nullptr;
            void *movingWindow = nullptr;
            int wantCaptureMouseNextFrame = -1;
            int windowStackSize = 0;
            bool active = false;
        };

        void BeginCall(CallSnapshot &snapshot, ImGuiContext *context);
        void EndCall(CallSnapshot &snapshot);
        bool HasWindow(const void *window) const;
        void AddWindow(const void *window);
        void Clear();

        ImGuiContext *m_Context = nullptr;
        std::vector<void *> m_Windows;
        ImGuiID m_ActiveId = 0;
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
}

#endif // BML_UI_SCRIPT_IMGUI_H
