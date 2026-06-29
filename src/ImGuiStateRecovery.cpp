#include "ImGuiStateRecovery.h"

#include <algorithm>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

namespace BML {
namespace {

struct ScriptImGuiOwnerState {
    ImGuiContext *Context = nullptr;
    const void *Owner = nullptr;
    std::vector<void *> Windows;
    unsigned int ActiveId = 0;
    void *ActiveIdWindow = nullptr;
    void *MovingWindow = nullptr;
    bool OwnsNextFrameMouseCapture = false;
};

std::vector<ScriptImGuiOwnerState> g_ScriptImGuiOwners;

ScriptImGuiOwnerState *FindOwnerState(ImGuiContext *context, const void *owner) {
    if (!context || !owner)
        return nullptr;

    const auto it = std::find_if(g_ScriptImGuiOwners.begin(),
                                 g_ScriptImGuiOwners.end(),
                                 [&](const ScriptImGuiOwnerState &state) {
                                     return state.Context == context && state.Owner == owner;
                                 });
    return it == g_ScriptImGuiOwners.end() ? nullptr : &*it;
}

ScriptImGuiOwnerState &GetOwnerState(ImGuiContext *context, const void *owner) {
    if (ScriptImGuiOwnerState *state = FindOwnerState(context, owner))
        return *state;

    ScriptImGuiOwnerState state;
    state.Context = context;
    state.Owner = owner;
    g_ScriptImGuiOwners.push_back(std::move(state));
    return g_ScriptImGuiOwners.back();
}

void RemoveOwnerState(ImGuiContext *context, const void *owner) {
    g_ScriptImGuiOwners.erase(std::remove_if(g_ScriptImGuiOwners.begin(),
                                             g_ScriptImGuiOwners.end(),
                                             [&](const ScriptImGuiOwnerState &state) {
                                                 return state.Context == context && state.Owner == owner;
                                             }),
                              g_ScriptImGuiOwners.end());
}

bool HasWindow(const ScriptImGuiOwnerState &state, const void *window) {
    return window && std::find(state.Windows.begin(), state.Windows.end(), window) != state.Windows.end();
}

void AddWindow(ScriptImGuiOwnerState &state, const void *window) {
    if (!window || HasWindow(state, window))
        return;
    state.Windows.push_back(const_cast<void *>(window));
}

bool IsMouseActiveId(const ImGuiContext &g) {
    return g.ActiveId != 0 &&
           (g.ActiveIdSource == ImGuiInputSource_Mouse || g.ActiveIdMouseButton >= 0);
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

void BeginScriptImGuiCall(ScriptImGuiCallState &state, const void *owner, ImGuiContext *context) {
    state = ScriptImGuiCallState();
    if (!owner)
        return;
    if (!context)
        context = ImGui::GetCurrentContext();
    if (!context)
        return;

    ImGuiContext *previous = ImGui::GetCurrentContext();
    const bool contextChanged = previous != context;
    if (contextChanged)
        ImGui::SetCurrentContext(context);

    ImGuiContext &g = *context;
    state.Owner = owner;
    state.Context = context;
    state.ActiveId = g.ActiveId;
    state.ActiveIdWindow = g.ActiveIdWindow;
    state.CurrentWindow = g.CurrentWindow;
    state.MovingWindow = g.MovingWindow;
    state.WantCaptureMouseNextFrame = g.WantCaptureMouseNextFrame;
    state.WindowStackSize = g.CurrentWindowStack.Size;
    state.Active = true;

    if (contextChanged)
        ImGui::SetCurrentContext(previous);
}

void EndScriptImGuiCall(ScriptImGuiCallState &state) {
    if (!state.Active || !state.Owner || !state.Context)
        return;

    ImGuiContext *previous = ImGui::GetCurrentContext();
    const bool contextChanged = previous != state.Context;
    if (contextChanged)
        ImGui::SetCurrentContext(state.Context);

    ImGuiContext &g = *state.Context;
    ScriptImGuiOwnerState &owner = GetOwnerState(state.Context, state.Owner);
    const int windowStackSize = g.CurrentWindowStack.Size;

    if (windowStackSize > state.WindowStackSize) {
        AddWindow(owner, g.CurrentWindow);
    } else if (windowStackSize < state.WindowStackSize) {
        AddWindow(owner, state.CurrentWindow);
    } else if (HasWindow(owner, state.CurrentWindow)) {
        AddWindow(owner, state.CurrentWindow);
    } else if (HasWindow(owner, g.CurrentWindow)) {
        AddWindow(owner, g.CurrentWindow);
    }

    if (IsMouseActiveId(g) && (g.ActiveId != state.ActiveId || g.ActiveIdWindow != state.ActiveIdWindow)) {
        owner.ActiveId = g.ActiveId;
        owner.ActiveIdWindow = g.ActiveIdWindow;
        AddWindow(owner, g.ActiveIdWindow);
    }
    if (g.MovingWindow && g.MovingWindow != state.MovingWindow) {
        owner.MovingWindow = g.MovingWindow;
        AddWindow(owner, g.MovingWindow);
    }
    if (g.WantCaptureMouseNextFrame != state.WantCaptureMouseNextFrame)
        owner.OwnsNextFrameMouseCapture = true;

    if (contextChanged)
        ImGui::SetCurrentContext(previous);

    state = ScriptImGuiCallState();
}

bool ReleaseStaleImGuiMouseCapture(const void *owner, ImGuiContext *context) {
    if (!owner)
        return false;
    if (!context)
        context = ImGui::GetCurrentContext();
    if (!context)
        return false;

    ScriptImGuiOwnerState *ownerState = FindOwnerState(context, owner);
    if (!ownerState)
        return false;

    ImGuiContext *previous = ImGui::GetCurrentContext();
    const bool contextChanged = previous != context;
    if (contextChanged)
        ImGui::SetCurrentContext(context);

    ImGuiContext &g = *context;
    ImGuiIO &io = g.IO;

    const bool ownsMovingWindow = g.MovingWindow &&
                                  (g.MovingWindow == ownerState->MovingWindow ||
                                   HasWindow(*ownerState, g.MovingWindow));
    const bool ownsActiveId = g.ActiveId != 0 &&
                              (g.ActiveId == ownerState->ActiveId ||
                               HasWindow(*ownerState, g.ActiveIdWindow));
    const bool ownsHoveredWindow = HasWindow(*ownerState, g.HoveredWindow);
    const bool ownsHoveredWindowUnderMovingWindow = HasWindow(*ownerState, g.HoveredWindowUnderMovingWindow);
    const bool ownsHoveredWindowBeforeClear = HasWindow(*ownerState, g.HoveredWindowBeforeClear);
    const bool ownsHoveredState = ownsHoveredWindow ||
                                  ownsHoveredWindowUnderMovingWindow ||
                                  ownsHoveredWindowBeforeClear;
    const bool hasForeignHoveredWindow = (g.HoveredWindow && !HasWindow(*ownerState, g.HoveredWindow)) ||
                                         (g.HoveredWindowUnderMovingWindow &&
                                          !HasWindow(*ownerState, g.HoveredWindowUnderMovingWindow));
    const bool ownsNextFrameMouseCapture =
        ownerState->OwnsNextFrameMouseCapture && g.WantCaptureMouseNextFrame != -1;
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
        g.WantCaptureMouseNextFrame = -1;
        released = true;
    }
    if (ownsHoveredWindow) {
        g.HoveredWindow = nullptr;
        released = true;
    }
    if (ownsHoveredWindowUnderMovingWindow) {
        g.HoveredWindowUnderMovingWindow = nullptr;
        released = true;
    }
    if (ownsHoveredWindowBeforeClear) {
        g.HoveredWindowBeforeClear = nullptr;
        released = true;
    }
    if (ownsHoveredState && !hasForeignHoveredWindow) {
        g.HoveredId = 0;
        g.HoveredIdPreviousFrame = 0;
        g.HoveredIdPreviousFrameItemCount = 0;
        g.HoveredIdTimer = 0.0f;
        g.HoveredIdNotActiveTimer = 0.0f;
        g.HoveredIdAllowOverlap = false;
        g.HoveredIdIsDisabled = false;
    }
    if (ownsMouseDownCapture && !hasForeignHoveredWindow) {
        ClearMouseDownOwnership(io);
        released = true;
    }
    if (released && !hasForeignHoveredWindow && g.OpenPopupStack.Size == 0) {
        io.WantCaptureMouse = false;
        io.WantCaptureMouseUnlessPopupClose = false;
    }

    if (contextChanged)
        ImGui::SetCurrentContext(previous);

    RemoveOwnerState(context, owner);
    return released;
}

} // namespace BML
