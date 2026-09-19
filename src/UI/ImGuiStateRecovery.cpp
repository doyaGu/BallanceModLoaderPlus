#include "UI/ImGuiStateRecovery.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace {

ImGuiErrorRecoveryState MakeRecoveryState(const Overlay::ImGuiStateSnapshot &snapshot) {
    ImGuiErrorRecoveryState state;
    state.SizeOfWindowStack = snapshot.WindowStackSize;
    state.SizeOfIDStack = snapshot.IdStackSize;
    state.SizeOfTreeStack = snapshot.TreeStackSize;
    state.SizeOfColorStack = snapshot.ColorStackSize;
    state.SizeOfStyleVarStack = snapshot.StyleVarStackSize;
    state.SizeOfFontStack = snapshot.FontStackSize;
    state.SizeOfFocusScopeStack = snapshot.FocusScopeStackSize;
    state.SizeOfGroupStack = snapshot.GroupStackSize;
    state.SizeOfItemFlagsStack = snapshot.ItemFlagsStackSize;
    state.SizeOfBeginPopupStack = snapshot.PopupStackSize;
    state.SizeOfDisabledStack = snapshot.DisabledStackSize;
    return state;
}

bool NeedsRecovery(const ImGuiErrorRecoveryState &state) {
    ImGuiContext *context = ImGui::GetCurrentContext();
    if (!context)
        return false;

    ImGuiContext &imgui = *context;
    if (!imgui.CurrentWindow)
        return imgui.CurrentWindowStack.Size > state.SizeOfWindowStack;

    return imgui.CurrentWindowStack.Size > state.SizeOfWindowStack ||
           imgui.CurrentWindow->IDStack.Size > state.SizeOfIDStack ||
           imgui.CurrentWindow->DC.TreeDepth > state.SizeOfTreeStack ||
           imgui.ColorStack.Size > state.SizeOfColorStack ||
           imgui.StyleVarStack.Size > state.SizeOfStyleVarStack ||
           imgui.FontStack.Size > state.SizeOfFontStack ||
           imgui.FocusScopeStack.Size > state.SizeOfFocusScopeStack ||
           imgui.GroupStack.Size > state.SizeOfGroupStack ||
           imgui.ItemFlagsStack.Size > state.SizeOfItemFlagsStack ||
           imgui.BeginPopupStack.Size > state.SizeOfBeginPopupStack ||
           imgui.DisabledStackSize > state.SizeOfDisabledStack;
}

} // namespace

Overlay::ImGuiStateSnapshot Overlay::CaptureImGuiState() {
    ImGuiStateSnapshot snapshot;
    ImGuiContext *context = ImGui::GetCurrentContext();
    if (!context || !context->WithinFrameScope)
        return snapshot;

    ImGuiErrorRecoveryState state;
    ImGui::ErrorRecoveryStoreState(&state);
    snapshot.Context = context;
    snapshot.WindowStackSize = state.SizeOfWindowStack;
    snapshot.IdStackSize = state.SizeOfIDStack;
    snapshot.TreeStackSize = state.SizeOfTreeStack;
    snapshot.ColorStackSize = state.SizeOfColorStack;
    snapshot.StyleVarStackSize = state.SizeOfStyleVarStack;
    snapshot.FontStackSize = state.SizeOfFontStack;
    snapshot.FocusScopeStackSize = state.SizeOfFocusScopeStack;
    snapshot.GroupStackSize = state.SizeOfGroupStack;
    snapshot.ItemFlagsStackSize = state.SizeOfItemFlagsStack;
    snapshot.PopupStackSize = state.SizeOfBeginPopupStack;
    snapshot.DisabledStackSize = state.SizeOfDisabledStack;
    snapshot.Active = true;
    return snapshot;
}

bool Overlay::RecoverImGuiState(const ImGuiStateSnapshot &snapshot) {
    if (!snapshot.Active || !snapshot.Context)
        return false;

    if (ImGui::GetCurrentContext() != snapshot.Context)
        ImGui::SetCurrentContext(snapshot.Context);

    const ImGuiErrorRecoveryState state = MakeRecoveryState(snapshot);
    const bool needsRecovery = NeedsRecovery(state);
    if (needsRecovery) {
        ImGuiIO &io = ImGui::GetIO();
        const bool enableAssert = io.ConfigErrorRecoveryEnableAssert;
        const bool enableDebugLog = io.ConfigErrorRecoveryEnableDebugLog;
        const bool enableTooltip = io.ConfigErrorRecoveryEnableTooltip;
        io.ConfigErrorRecoveryEnableAssert = false;
        io.ConfigErrorRecoveryEnableDebugLog = false;
        io.ConfigErrorRecoveryEnableTooltip = false;
        ImGui::ErrorRecoveryTryToRecoverState(&state);
        io.ConfigErrorRecoveryEnableAssert = enableAssert;
        io.ConfigErrorRecoveryEnableDebugLog = enableDebugLog;
        io.ConfigErrorRecoveryEnableTooltip = enableTooltip;
    }

    return needsRecovery;
}
