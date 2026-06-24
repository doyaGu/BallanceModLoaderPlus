#include "AngelScriptImGuiBindings.h"

#if BML_ENABLE_ANGELSCRIPT

#include <cstdio>
#include <new>
#include <string>

#include "ModContext.h"
#include "Overlay.h"

#include "imgui.h"
#include "imgui_internal.h"

static std::string g_BMLImGuiASLastRegistrationError;

namespace {
CKBOOL BMLImGuiASActivateContext(ImGuiContext *&previous, CKBOOL &changed) {
    previous = nullptr;
    changed = FALSE;

    ModContext *context = BML_GetModContext();
    if (!context || !context->IsInited())
        return FALSE;
    if (!Overlay::IsImGuiReady() || !Overlay::IsImGuiFrameActive())
        return FALSE;

    ImGuiContext *imguiContext = Overlay::GetImGuiContext();
    if (!imguiContext)
        return FALSE;

    previous = ImGui::GetCurrentContext();
    if (previous != imguiContext) {
        ImGui::SetCurrentContext(imguiContext);
        changed = TRUE;
    }

    return TRUE;
}

bool BMLImGuiASNeedsRecovery(const ImGuiErrorRecoveryState &state) {
    ImGuiContext *context = ImGui::GetCurrentContext();
    if (!context)
        return false;

    ImGuiContext &g = *context;
    if (!g.CurrentWindow)
        return g.CurrentWindowStack.Size > state.SizeOfWindowStack;

    return g.CurrentWindowStack.Size > state.SizeOfWindowStack ||
           g.CurrentWindow->IDStack.Size > state.SizeOfIDStack ||
           g.CurrentWindow->DC.TreeDepth > state.SizeOfTreeStack ||
           g.ColorStack.Size > state.SizeOfColorStack ||
           g.StyleVarStack.Size > state.SizeOfStyleVarStack ||
           g.FontStack.Size > state.SizeOfFontStack ||
           g.FocusScopeStack.Size > state.SizeOfFocusScopeStack ||
           g.GroupStack.Size > state.SizeOfGroupStack ||
           g.ItemFlagsStack.Size > state.SizeOfItemFlagsStack ||
           g.BeginPopupStack.Size > state.SizeOfBeginPopupStack ||
           g.DisabledStackSize > state.SizeOfDisabledStack;
}
} // namespace

CKBOOL BMLImGuiASBeginCall(BMLImGuiASCallScope *scope) {
    if (!scope)
        return FALSE;

    scope->Previous = nullptr;
    scope->Active = FALSE;
    scope->Changed = FALSE;

    if (!BMLImGuiASActivateContext(scope->Previous, scope->Changed))
        return FALSE;

    scope->Active = TRUE;
    return TRUE;
}

void BMLImGuiASEndCall(BMLImGuiASCallScope *scope) {
    if (!scope || !scope->Active)
        return;

    if (scope->Changed)
        ImGui::SetCurrentContext(scope->Previous);

    scope->Previous = nullptr;
    scope->Active = FALSE;
    scope->Changed = FALSE;
}

CKBOOL BMLImGuiASBeginCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope) {
    if (!scope)
        return FALSE;

    scope->Previous = nullptr;
    scope->Active = FALSE;
    scope->Changed = FALSE;

    if (!BMLImGuiASActivateContext(scope->Previous, scope->Changed))
        return FALSE;

    static_assert(sizeof(ImGuiErrorRecoveryState) <= sizeof(scope->State),
                  "BMLImGuiASCallbackRecoveryScope::State is too small");
    ImGuiErrorRecoveryState *state = new (scope->State) ImGuiErrorRecoveryState();
    ImGui::ErrorRecoveryStoreState(state);
    scope->Active = TRUE;
    return TRUE;
}

void BMLImGuiASEndCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope,
                                   const char *modId,
                                   const char *phase) {
    if (!scope || !scope->Active)
        return;

    ImGuiErrorRecoveryState *state = reinterpret_cast<ImGuiErrorRecoveryState *>(scope->State);
    const bool recovered = BMLImGuiASNeedsRecovery(*state);
    if (recovered) {
        ImGui::ErrorRecoveryTryToRecoverState(state);
    }

    state->~ImGuiErrorRecoveryState();

    if (scope->Changed)
        ImGui::SetCurrentContext(scope->Previous);

    scope->Previous = nullptr;
    scope->Active = FALSE;
    scope->Changed = FALSE;

    if (recovered) {
        char buffer[512];
        std::snprintf(buffer,
                      sizeof(buffer),
                      "Recovered mismatched ImGui stack after script callback. mod=%s phase=%s",
                      modId && modId[0] ? modId : "<unknown>",
                      phase && phase[0] ? phase : "<unknown>");
        buffer[sizeof(buffer) - 1] = '\0';
        BMLImGuiASReportRuntimeWarning(buffer);
    }
}

void BMLImGuiASSetRegistrationError(const char **errorMessage, const char *expression, int code) {
    char buffer[512];

    std::snprintf(buffer,
                  sizeof(buffer),
                  "BML ImGui AngelScript registration failed: %s returned %d.",
                  expression ? expression : "<unknown>",
                  code);
    buffer[sizeof(buffer) - 1] = '\0';
    g_BMLImGuiASLastRegistrationError = buffer;

    if (errorMessage)
        *errorMessage = g_BMLImGuiASLastRegistrationError.c_str();
}

void BMLImGuiASReportRuntimeWarning(const char *message) {
    if (!message || message[0] == '\0')
        return;

    ModContext *context = BML_GetModContext();
    if (!context || !context->GetLogger())
        return;

    context->GetLogger()->Warn("BML ImGui AngelScript: %s", message);
}

ImDrawList *BMLImGuiASGetBackgroundDrawList() {
    BMLImGuiASCallScope scope;
    if (!BMLImGuiASBeginCall(&scope))
        return nullptr;

    ImDrawList *drawList = ImGui::GetBackgroundDrawList();
    BMLImGuiASEndCall(&scope);
    return drawList;
}

ImDrawList *BMLImGuiASGetForegroundDrawList() {
    BMLImGuiASCallScope scope;
    if (!BMLImGuiASBeginCall(&scope))
        return nullptr;

    ImDrawList *drawList = ImGui::GetForegroundDrawList();
    BMLImGuiASEndCall(&scope);
    return drawList;
}

#endif
