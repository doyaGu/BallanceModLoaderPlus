#ifndef BML_ANGELSCRIPT_IMGUI_BINDINGS_H
#define BML_ANGELSCRIPT_IMGUI_BINDINGS_H

struct ImDrawList;
struct ImGuiContext;
class asIScriptEngine;

struct BMLImGuiASCallScope {
    ImGuiContext *Previous = nullptr;
    bool Active = false;
    bool Changed = false;
};

struct BMLImGuiASCallbackRecoveryScope {
    ImGuiContext *Previous = nullptr;
    alignas(8) unsigned char State[64] = {};
    bool Active = false;
    bool Changed = false;
    bool PreviousErrorRecoveryEnableAssert = true;
    bool PreviousErrorRecoveryEnableDebugLog = true;
    bool PreviousErrorRecoveryEnableTooltip = true;
};

bool BMLImGuiASBeginCall(BMLImGuiASCallScope *scope);
void BMLImGuiASEndCall(BMLImGuiASCallScope *scope);
bool BMLImGuiASBeginCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope);
void BMLImGuiASEndCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope,
                                   const char *modId,
                                   const char *phase);
void BMLImGuiASSetRegistrationError(const char **errorMessage, const char *expression, int code);
void BMLImGuiASReportRuntimeWarning(const char *message);
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
