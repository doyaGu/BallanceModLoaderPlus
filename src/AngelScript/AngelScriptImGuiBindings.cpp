#include "AngelScriptImGuiBindings.h"

#if BML_ENABLE_ANGELSCRIPT

#include <cstdio>
#include <string>

#include "ModContext.h"
#include "Overlay.h"

#include "imgui.h"

static std::string g_BMLImGuiASLastRegistrationError;

CKBOOL BMLImGuiASBeginCall(BMLImGuiASCallScope *scope) {
    if (!scope)
        return FALSE;

    scope->Previous = nullptr;
    scope->Active = FALSE;
    scope->Changed = FALSE;

    ModContext *context = BML_GetModContext();
    if (!context || !context->IsInited())
        return FALSE;
    if (!Overlay::IsImGuiReady() || !Overlay::IsImGuiFrameActive())
        return FALSE;

    ImGuiContext *imguiContext = Overlay::GetImGuiContext();
    if (!imguiContext)
        return FALSE;

    scope->Previous = ImGui::GetCurrentContext();
    if (scope->Previous != imguiContext) {
        ImGui::SetCurrentContext(imguiContext);
        scope->Changed = TRUE;
    }

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
