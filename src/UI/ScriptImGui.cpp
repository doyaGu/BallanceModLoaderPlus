#include "UI/ScriptImGui.h"

#include <algorithm>

#include "imgui_internal.h"

namespace Overlay {
namespace {

bool IsMouseActiveId(const ImGuiContext &context) {
    return context.ActiveId != 0 &&
           (context.ActiveIdSource == ImGuiInputSource_Mouse || context.ActiveIdMouseButton >= 0);
}

bool HasMouseDownOwnership(const ImGuiIO &io) {
    for (int i = 0; i < IM_COUNTOF(io.MouseDownOwned); ++i) {
        if (io.MouseDownOwned[i] || io.MouseDownOwnedUnlessPopupClose[i])
            return true;
    }
    return false;
}

void ClearMouseDownOwnership(ImGuiIO &io) {
    for (int i = 0; i < IM_COUNTOF(io.MouseDownOwned); ++i) {
        io.MouseDownOwned[i] = false;
        io.MouseDownOwnedUnlessPopupClose[i] = false;
    }
}

} // namespace

bool ScriptImGuiState::HasWindow(const void *window) const {
    return window && std::find(m_Windows.begin(), m_Windows.end(), window) != m_Windows.end();
}

void ScriptImGuiState::AddWindow(const void *window) {
    if (!window || HasWindow(window))
        return;
    m_Windows.push_back(const_cast<void *>(window));
}

void ScriptImGuiState::Clear() {
    m_Context = nullptr;
    m_Windows.clear();
    m_ActiveId = 0;
    m_ActiveIdWindow = nullptr;
    m_MovingWindow = nullptr;
    m_OwnsNextFrameMouseCapture = false;
}

void ScriptImGuiState::BeginCall(CallSnapshot &snapshot, ImGuiContext *context) {
    snapshot = CallSnapshot();
    if (!context)
        context = ImGui::GetCurrentContext();
    if (!context)
        return;

    if (m_Context && m_Context != context)
        Clear();

    ImGuiContext *previous = ImGui::GetCurrentContext();
    const bool contextChanged = previous != context;
    if (contextChanged)
        ImGui::SetCurrentContext(context);

    ImGuiContext &imgui = *context;
    snapshot.context = context;
    snapshot.activeId = imgui.ActiveId;
    snapshot.activeIdWindow = imgui.ActiveIdWindow;
    snapshot.currentWindow = imgui.CurrentWindow;
    snapshot.movingWindow = imgui.MovingWindow;
    snapshot.wantCaptureMouseNextFrame = imgui.WantCaptureMouseNextFrame;
    snapshot.windowStackSize = imgui.CurrentWindowStack.Size;
    snapshot.active = true;

    if (contextChanged)
        ImGui::SetCurrentContext(previous);
}

void ScriptImGuiState::EndCall(CallSnapshot &snapshot) {
    if (!snapshot.active || !snapshot.context)
        return;

    ImGuiContext *previous = ImGui::GetCurrentContext();
    const bool contextChanged = previous != snapshot.context;
    if (contextChanged)
        ImGui::SetCurrentContext(snapshot.context);

    m_Context = snapshot.context;
    ImGuiContext &imgui = *snapshot.context;
    const int windowStackSize = imgui.CurrentWindowStack.Size;

    if (windowStackSize > snapshot.windowStackSize) {
        AddWindow(imgui.CurrentWindow);
    } else if (windowStackSize < snapshot.windowStackSize) {
        AddWindow(snapshot.currentWindow);
    } else if (HasWindow(snapshot.currentWindow)) {
        AddWindow(snapshot.currentWindow);
    } else if (HasWindow(imgui.CurrentWindow)) {
        AddWindow(imgui.CurrentWindow);
    }

    if (IsMouseActiveId(imgui) &&
        (imgui.ActiveId != snapshot.activeId || imgui.ActiveIdWindow != snapshot.activeIdWindow)) {
        m_ActiveId = imgui.ActiveId;
        m_ActiveIdWindow = imgui.ActiveIdWindow;
        AddWindow(imgui.ActiveIdWindow);
    }
    if (imgui.MovingWindow && imgui.MovingWindow != snapshot.movingWindow) {
        m_MovingWindow = imgui.MovingWindow;
        AddWindow(imgui.MovingWindow);
    }
    if (imgui.WantCaptureMouseNextFrame != snapshot.wantCaptureMouseNextFrame)
        m_OwnsNextFrameMouseCapture = true;

    if (contextChanged)
        ImGui::SetCurrentContext(previous);

    snapshot = CallSnapshot();
}

bool ScriptImGuiState::Release(ImGuiContext *context) {
    if (!context)
        context = m_Context ? m_Context : ImGui::GetCurrentContext();
    if (!context || context != m_Context) {
        Clear();
        return false;
    }

    ImGuiContext *previous = ImGui::GetCurrentContext();
    const bool contextChanged = previous != context;
    if (contextChanged)
        ImGui::SetCurrentContext(context);

    ImGuiContext &imgui = *context;
    ImGuiIO &io = imgui.IO;
    const bool ownsMovingWindow = imgui.MovingWindow &&
                                  (imgui.MovingWindow == m_MovingWindow || HasWindow(imgui.MovingWindow));
    const bool ownsActiveId = imgui.ActiveId != 0 &&
                              (imgui.ActiveId == m_ActiveId || HasWindow(imgui.ActiveIdWindow));
    const bool ownsHoveredWindow = HasWindow(imgui.HoveredWindow);
    const bool ownsHoveredWindowUnderMovingWindow = HasWindow(imgui.HoveredWindowUnderMovingWindow);
    const bool ownsHoveredWindowBeforeClear = HasWindow(imgui.HoveredWindowBeforeClear);
    const bool ownsHoveredState = ownsHoveredWindow ||
                                  ownsHoveredWindowUnderMovingWindow ||
                                  ownsHoveredWindowBeforeClear;
    const bool hasForeignHoveredWindow =
        (imgui.HoveredWindow && !HasWindow(imgui.HoveredWindow)) ||
        (imgui.HoveredWindowUnderMovingWindow && !HasWindow(imgui.HoveredWindowUnderMovingWindow));
    const bool ownsNextFrameMouseCapture =
        m_OwnsNextFrameMouseCapture && imgui.WantCaptureMouseNextFrame != -1;
    const bool ownsMouseDownCapture = HasMouseDownOwnership(io) &&
                                      (ownsActiveId || ownsMovingWindow || ownsHoveredWindow ||
                                       ownsNextFrameMouseCapture);

    bool released = false;
    if (ownsMovingWindow) {
        ImGui::StopMouseMovingWindow();
        released = true;
    }
    if (ownsActiveId) {
        ImGui::ClearActiveID();
        released = true;
    }
    if (ownsNextFrameMouseCapture) {
        imgui.WantCaptureMouseNextFrame = -1;
        released = true;
    }
    if (ownsHoveredWindow) {
        imgui.HoveredWindow = nullptr;
        released = true;
    }
    if (ownsHoveredWindowUnderMovingWindow) {
        imgui.HoveredWindowUnderMovingWindow = nullptr;
        released = true;
    }
    if (ownsHoveredWindowBeforeClear) {
        imgui.HoveredWindowBeforeClear = nullptr;
        released = true;
    }
    if (ownsHoveredState && !hasForeignHoveredWindow) {
        imgui.HoveredId = 0;
        imgui.HoveredIdPreviousFrame = 0;
        imgui.HoveredIdPreviousFrameItemCount = 0;
        imgui.HoveredIdTimer = 0.0f;
        imgui.HoveredIdNotActiveTimer = 0.0f;
        imgui.HoveredIdAllowOverlap = false;
        imgui.HoveredIdIsDisabled = false;
    }
    if (ownsMouseDownCapture && !hasForeignHoveredWindow) {
        ClearMouseDownOwnership(io);
        released = true;
    }
    if (released && !hasForeignHoveredWindow && imgui.OpenPopupStack.Size == 0) {
        io.WantCaptureMouse = false;
        io.WantCaptureMouseUnlessPopupClose = false;
    }

    if (contextChanged)
        ImGui::SetCurrentContext(previous);

    Clear();
    return released;
}

bool ScriptImGuiCallScope::Begin(ScriptImGuiState &state, ImGuiContext *context) {
    End();
    m_State = &state;
    m_State->BeginCall(m_Snapshot, context);
    if (!m_Snapshot.active) {
        m_State = nullptr;
        return false;
    }
    return true;
}

void ScriptImGuiCallScope::End() {
    if (!m_State)
        return;
    m_State->EndCall(m_Snapshot);
    m_State = nullptr;
}

} // namespace Overlay
