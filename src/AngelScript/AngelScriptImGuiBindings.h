#ifndef BML_ANGELSCRIPT_IMGUI_BINDINGS_H
#define BML_ANGELSCRIPT_IMGUI_BINDINGS_H

#include "CKTypes.h"

struct ImDrawList;
struct ImGuiContext;
class asIScriptEngine;

struct BMLImGuiASCallScope {
    ImGuiContext *Previous = nullptr;
    CKBOOL Active = FALSE;
    CKBOOL Changed = FALSE;
};

struct BMLImGuiASCallbackRecoveryScope {
    ImGuiContext *Previous = nullptr;
    CKBOOL Active = FALSE;
    CKBOOL Changed = FALSE;
    CKBOOL PreviousErrorRecoveryEnableAssert = TRUE;
    CKBOOL PreviousErrorRecoveryEnableDebugLog = TRUE;
    CKBOOL PreviousErrorRecoveryEnableTooltip = TRUE;
    alignas(8) unsigned char State[64] = {};
};

CKBOOL BMLImGuiASBeginCall(BMLImGuiASCallScope *scope);
void BMLImGuiASEndCall(BMLImGuiASCallScope *scope);
CKBOOL BMLImGuiASBeginCallbackRecovery(BMLImGuiASCallbackRecoveryScope *scope);
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
