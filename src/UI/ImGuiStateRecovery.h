#ifndef BML_UI_IMGUI_STATE_RECOVERY_H
#define BML_UI_IMGUI_STATE_RECOVERY_H

struct ImGuiContext;

namespace Overlay {

struct ImGuiStateSnapshot {
    ImGuiContext *Context = nullptr;
    short WindowStackSize = 0;
    short IdStackSize = 0;
    short TreeStackSize = 0;
    short ColorStackSize = 0;
    short StyleVarStackSize = 0;
    short FontStackSize = 0;
    short FocusScopeStackSize = 0;
    short GroupStackSize = 0;
    short ItemFlagsStackSize = 0;
    short PopupStackSize = 0;
    short DisabledStackSize = 0;
    bool Active = false;
};

ImGuiStateSnapshot CaptureImGuiState();
bool RecoverImGuiState(const ImGuiStateSnapshot &snapshot);

} // namespace Overlay

#endif // BML_UI_IMGUI_STATE_RECOVERY_H
