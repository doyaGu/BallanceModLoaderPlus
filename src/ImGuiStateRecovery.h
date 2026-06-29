#ifndef BML_IMGUI_STATE_RECOVERY_H
#define BML_IMGUI_STATE_RECOVERY_H

struct ImGuiContext;

namespace BML {

struct ScriptImGuiCallState {
    const void *Owner = nullptr;
    ImGuiContext *Context = nullptr;
    unsigned int ActiveId = 0;
    void *ActiveIdWindow = nullptr;
    void *CurrentWindow = nullptr;
    void *MovingWindow = nullptr;
    int WantCaptureMouseNextFrame = -1;
    int WindowStackSize = 0;
    bool Active = false;
};

void BeginScriptImGuiCall(ScriptImGuiCallState &state, const void *owner, ImGuiContext *context = nullptr);
void EndScriptImGuiCall(ScriptImGuiCallState &state);
bool ReleaseStaleImGuiMouseCapture(const void *owner, ImGuiContext *context = nullptr);

} // namespace BML

#endif // BML_IMGUI_STATE_RECOVERY_H
