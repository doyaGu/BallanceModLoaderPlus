#ifndef BML_ANGELSCRIPT_IMGUI_BINDINGS_H
#define BML_ANGELSCRIPT_IMGUI_BINDINGS_H

#include <string>

#include "UI/ImGuiStateRecovery.h"
#include "UI/ScriptImGui.h"

struct ImDrawList;
struct ImGuiContext;
class asIScriptEngine;

struct BMLImGuiASCallScope {
    BMLImGuiASCallScope() = default;
    BMLImGuiASCallScope(const BMLImGuiASCallScope &) = delete;
    BMLImGuiASCallScope &operator=(const BMLImGuiASCallScope &) = delete;
    ~BMLImGuiASCallScope();

    bool Begin();
    void End();

private:
    ImGuiContext *Previous = nullptr;
    Overlay::ScriptImGuiCallScope ScriptCall;
    bool Active = false;
    bool Changed = false;
};

struct BMLImGuiASCallbackRecoveryScope {
    BMLImGuiASCallbackRecoveryScope() = default;
    BMLImGuiASCallbackRecoveryScope(const BMLImGuiASCallbackRecoveryScope &) = delete;
    BMLImGuiASCallbackRecoveryScope &operator=(const BMLImGuiASCallbackRecoveryScope &) = delete;
    ~BMLImGuiASCallbackRecoveryScope();

    bool Begin();
    void End(const char *modId, const char *phase);

private:
    ImGuiContext *Previous = nullptr;
    Overlay::ImGuiStateSnapshot State;
    bool Active = false;
    bool Changed = false;
};

bool BMLImGuiASBeginCall(BMLImGuiASCallScope *scope);
void BMLImGuiASEndCall(BMLImGuiASCallScope *scope);
bool BMLImGuiASBeginCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope);
void BMLImGuiASEndCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope,
                                   const char *modId,
                                   const char *phase);
void BMLImGuiASSetRegistrationError(const char **errorMessage, const char *expression, int code);
void BMLImGuiASReportRuntimeWarning(const char *message);
std::string BMLImGuiASScopeWindowName(const std::string &name);
ImDrawList *BMLImGuiASGetBackgroundDrawList();
ImDrawList *BMLImGuiASGetForegroundDrawList();

#define BML_IMGUI_AS_CHECK(result, expression)                         \
    do {                                                               \
        const int bmlImGuiAsResult = (result);                         \
        if (bmlImGuiAsResult < 0) {                                    \
            BMLImGuiASSetRegistrationError(errorMessage, expression,   \
                                           bmlImGuiAsResult);          \
            return bmlImGuiAsResult;                                   \
        }                                                              \
    } while (false)

#define BML_IMGUI_AS_CHECK_RESET_NAMESPACE(engine, result, expression) \
    do {                                                               \
        const int bmlImGuiAsResult = (result);                         \
        if (bmlImGuiAsResult < 0) {                                    \
            if (engine)                                                \
                (engine)->SetDefaultNamespace("");                     \
            BMLImGuiASSetRegistrationError(errorMessage, expression,   \
                                           bmlImGuiAsResult);          \
            return bmlImGuiAsResult;                                   \
        }                                                              \
    } while (false)

#endif // BML_ANGELSCRIPT_IMGUI_BINDINGS_H
