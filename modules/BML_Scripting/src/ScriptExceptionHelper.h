#ifndef BML_SCRIPTING_SCRIPT_EXCEPTION_HELPER_H
#define BML_SCRIPTING_SCRIPT_EXCEPTION_HELPER_H

#include <cstdio>

#include <angelscript.h>

#include "bml_logging.h"
#include "bml_topics.h"
#include "ModuleScope.h"
#include "ScriptInstance.h"

namespace BML::Scripting {

// Log an AngelScript exception with full context (function, file, line).
// Must be called before ReleaseContext because the context holds exception info.
inline void LogScriptException(asIScriptContext *ctx, const ScriptInstance &inst) {
    if (!ctx || !g_Builtins || !g_Builtins->Logging) return;

    const char *exception_str = ctx->GetExceptionString();
    if (!exception_str) exception_str = "(unknown exception)";

    int col = 0;
    const char *section = nullptr;
    int line = ctx->GetExceptionLineNumber(&col, &section);
    if (!section) section = "(unknown)";

    const char *func_decl = nullptr;
    asIScriptFunction *fn = ctx->GetExceptionFunction();
    if (fn) func_decl = fn->GetDeclaration();
    if (!func_decl) func_decl = "(unknown)";

    BML_Context bml_ctx = g_Builtins->Context
        ? g_Builtins->Context->GetGlobalContext() : nullptr;

    g_Builtins->Logging->Log(bml_ctx, BML_LOG_ERROR, "script",
                             "[%s] Exception in %s (%s:%d,%d): %s",
                             inst.mod_id.c_str(), func_decl, section, line, col, exception_str);

    // Also forward to console output
    if (g_Builtins->ImcBus) {
        char buf[512];
        int n = std::snprintf(buf, sizeof(buf),
                              "\x1b[31m[%s] %s (%s:%d): %s\x1b[0m",
                              inst.mod_id.c_str(), func_decl, section, line, exception_str);
        if (n > 0 && static_cast<size_t>(n) < sizeof(buf)) {
            BML_TopicId topic_id = BML_TOPIC_ID_INVALID;
            if (g_Builtins->ImcBus->GetTopicId(BML_TOPIC_CONSOLE_OUTPUT, &topic_id) == BML_RESULT_OK) {
                struct {
                    size_t struct_size;
                    const char *msg;
                    uint32_t flags;
                } event{sizeof(event), buf, 0};
                g_Builtins->ImcBus->Publish(topic_id, &event, sizeof(event));
            }
        }
    }
}

} // namespace BML::Scripting

#endif // BML_SCRIPTING_SCRIPT_EXCEPTION_HELPER_H
