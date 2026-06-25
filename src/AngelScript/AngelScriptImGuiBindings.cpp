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
bool BMLImGuiASActivateContext(ImGuiContext *&previous, bool &changed, bool reportOutsideFrame) {
    previous = nullptr;
    changed = false;

    ModContext *context = BML_GetModContext();
    if (!context || !context->IsInited())
        return false;
    if (!Overlay::IsImGuiReady() || !Overlay::IsImGuiFrameActive()) {
        static bool loggedOutsideFrame = false;
        if (reportOutsideFrame && !loggedOutsideFrame) {
            loggedOutsideFrame = true;
            BMLImGuiASReportRuntimeWarning("ImGui calls require an active BML ImGui frame.");
        }
        return false;
    }

    ImGuiContext *imguiContext = Overlay::GetImGuiContext();
    if (!imguiContext)
        return false;

    previous = ImGui::GetCurrentContext();
    if (previous != imguiContext) {
        ImGui::SetCurrentContext(imguiContext);
        changed = true;
    }

    return true;
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

BMLImGuiASCallScope::~BMLImGuiASCallScope() {
    End();
}

bool BMLImGuiASCallScope::Begin() {
    Previous = nullptr;
    Active = false;
    Changed = false;

    if (!BMLImGuiASActivateContext(Previous, Changed, true))
        return false;

    Active = true;
    return true;
}

void BMLImGuiASCallScope::End() {
    if (!Active)
        return;

    if (Changed)
        ImGui::SetCurrentContext(Previous);

    Previous = nullptr;
    Active = false;
    Changed = false;
}

bool BMLImGuiASBeginCall(BMLImGuiASCallScope *scope) {
    return scope && scope->Begin();
}

void BMLImGuiASEndCall(BMLImGuiASCallScope *scope) {
    if (scope)
        scope->End();
}

BMLImGuiASCallbackRecoveryScope::~BMLImGuiASCallbackRecoveryScope() {
    End(nullptr, nullptr);
}

bool BMLImGuiASCallbackRecoveryScope::Begin() {
    Previous = nullptr;
    Active = false;
    Changed = false;
    PreviousErrorRecoveryEnableAssert = true;
    PreviousErrorRecoveryEnableDebugLog = true;
    PreviousErrorRecoveryEnableTooltip = true;

    if (!BMLImGuiASActivateContext(Previous, Changed, false))
        return false;

    static_assert(sizeof(ImGuiErrorRecoveryState) <= sizeof(State),
                  "BMLImGuiASCallbackRecoveryScope::State is too small");
    ImGuiErrorRecoveryState *state = new (State) ImGuiErrorRecoveryState();
    ImGui::ErrorRecoveryStoreState(state);
    if (ImGuiContext *context = ImGui::GetCurrentContext()) {
        PreviousErrorRecoveryEnableAssert = context->IO.ConfigErrorRecoveryEnableAssert;
        PreviousErrorRecoveryEnableDebugLog = context->IO.ConfigErrorRecoveryEnableDebugLog;
        PreviousErrorRecoveryEnableTooltip = context->IO.ConfigErrorRecoveryEnableTooltip;
        context->IO.ConfigErrorRecoveryEnableAssert = false;
        context->IO.ConfigErrorRecoveryEnableDebugLog = false;
        context->IO.ConfigErrorRecoveryEnableTooltip = false;
    }
    Active = true;
    return true;
}

void BMLImGuiASCallbackRecoveryScope::End(const char *modId, const char *phase) {
    if (!Active)
        return;

    ImGuiErrorRecoveryState *state = reinterpret_cast<ImGuiErrorRecoveryState *>(State);
    const bool needsRecovery = BMLImGuiASNeedsRecovery(*state);
    if (needsRecovery) {
        ImGui::ErrorRecoveryTryToRecoverState(state);
    }

    if (ImGuiContext *context = ImGui::GetCurrentContext()) {
        context->IO.ConfigErrorRecoveryEnableAssert = PreviousErrorRecoveryEnableAssert;
        context->IO.ConfigErrorRecoveryEnableDebugLog = PreviousErrorRecoveryEnableDebugLog;
        context->IO.ConfigErrorRecoveryEnableTooltip = PreviousErrorRecoveryEnableTooltip;
    }

    state->~ImGuiErrorRecoveryState();

    if (Changed)
        ImGui::SetCurrentContext(Previous);

    Previous = nullptr;
    Active = false;
    Changed = false;

    if (needsRecovery) {
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

bool BMLImGuiASBeginCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope) {
    return scope && scope->Begin();
}

void BMLImGuiASEndCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope,
                                   const char *modId,
                                   const char *phase) {
    if (scope)
        scope->End(modId, phase);
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
    if (!scope.Begin())
        return nullptr;

    return ImGui::GetBackgroundDrawList();
}

ImDrawList *BMLImGuiASGetForegroundDrawList() {
    BMLImGuiASCallScope scope;
    if (!scope.Begin())
        return nullptr;

    return ImGui::GetForegroundDrawList();
}

#endif
