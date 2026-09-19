#include "AngelScriptImGuiBindings.h"

#if BML_ENABLE_ANGELSCRIPT

#include <cstdio>
#include <string>

#include "Loader/ModContext.h"
#include "UI/Overlay.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"

#include "imgui.h"

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

    if (BML::ScriptMod *scriptMod = BML::ScriptModRuntime::GetCurrentScriptMod())
        ScriptCall.Begin(scriptMod->GetScriptImGuiState(), ImGui::GetCurrentContext());
    Active = true;
    return true;
}

void BMLImGuiASCallScope::End() {
    if (!Active)
        return;

    ScriptCall.End();

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

    if (!BMLImGuiASActivateContext(Previous, Changed, false))
        return false;

    State = Overlay::CaptureImGuiState();
    Active = State.Active;
    return Active;
}

void BMLImGuiASCallbackRecoveryScope::End(const char *modId, const char *phase) {
    if (!Active)
        return;

    const bool needsRecovery = Overlay::RecoverImGuiState(State);

    if (Changed)
        ImGui::SetCurrentContext(Previous);

    Previous = nullptr;
    State = {};
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

std::string BMLImGuiASScopeWindowName(const std::string &name) {
    BML::ScriptMod *scriptMod = BML::ScriptModRuntime::GetCurrentScriptMod();
    return scriptMod
        ? Overlay::MakeScriptImGuiWindowName(name, scriptMod->GetID())
        : name;
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
