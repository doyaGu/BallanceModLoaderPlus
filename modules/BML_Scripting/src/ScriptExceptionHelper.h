#ifndef BML_SCRIPTING_SCRIPT_EXCEPTION_HELPER_H
#define BML_SCRIPTING_SCRIPT_EXCEPTION_HELPER_H

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string_view>

#include <angelscript.h>

#include "bml_console.h"
#include "bml_logging.h"
#include "bml_topics.h"
#include "ModuleScope.h"
#include "ScriptInstance.h"

namespace BML::Scripting {

inline void PublishConsoleOutputMessage(BML_Mod owner, std::string_view message, uint32_t flags) {
    if (!g_Services || !g_Services->ImcBus || !g_Services->ImcBus->PublishBuffer || !owner || message.empty()) {
        return;
    }

    BML_TopicId topicId = BML_TOPIC_ID_INVALID;
    if (g_Services->ImcBus->GetTopicId(
            g_Services->ImcBus->Context,
            BML_TOPIC_CONSOLE_OUTPUT,
            &topicId) != BML_RESULT_OK) {
        return;
    }

    const size_t textSize = message.size() + 1;
    const size_t totalSize = sizeof(BML_ConsoleOutputEvent) + textSize;
    auto *storage = static_cast<char *>(std::malloc(totalSize));
    if (!storage) {
        return;
    }

    auto *event = reinterpret_cast<BML_ConsoleOutputEvent *>(storage);
    *event = BML_CONSOLE_OUTPUT_EVENT_INIT;
    event->message_utf8 = storage + sizeof(BML_ConsoleOutputEvent);
    event->flags = flags;

    std::memcpy(const_cast<char *>(event->message_utf8), message.data(), message.size());
    const_cast<char *>(event->message_utf8)[message.size()] = '\0';

    BML_ImcBuffer buffer = BML_IMC_BUFFER_INIT;
    buffer.data = event;
    buffer.size = totalSize;
    buffer.cleanup = [](const void *, size_t, void *userData) {
        std::free(userData);
    };
    buffer.cleanup_user_data = storage;
    (void) g_Services->ImcBus->PublishBuffer(owner, topicId, &buffer);
}

// Log an AngelScript exception with full context (function, file, line).
// Must be called before ReleaseContext because the context holds exception info.
inline void LogScriptException(asIScriptContext *ctx, const ScriptInstance &inst) {
    if (!ctx || !g_Services || !g_Services->Logging || !g_Services->Logging->Log) return;

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

    g_Services->Logging->Log(inst.mod_handle, BML_LOG_ERROR, "script",
                             "[%s] Exception in %s (%s:%d,%d): %s",
                             inst.mod_id.c_str(), func_decl, section, line, col,
                             exception_str);

    // Also forward to console output
    if (g_Services->ImcBus && g_Services->ImcBus->PublishBuffer) {
        char buf[512];
        int n = std::snprintf(buf, sizeof(buf),
                              "\x1b[31m[%s] %s (%s:%d): %s\x1b[0m",
                              inst.mod_id.c_str(), func_decl, section, line, exception_str);
        if (n > 0 && static_cast<size_t>(n) < sizeof(buf)) {
            PublishConsoleOutputMessage(inst.mod_handle, std::string_view(buf, static_cast<size_t>(n)), 0);
        }
    }
}

} // namespace BML::Scripting

#endif // BML_SCRIPTING_SCRIPT_EXCEPTION_HELPER_H
