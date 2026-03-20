#ifndef BML_CONSOLE_H
#define BML_CONSOLE_H

#include "bml_errors.h"
#include "bml_interface.h"
#include "bml_types.h"

BML_BEGIN_CDECLS

#define BML_CONSOLE_COMMAND_REGISTRY_INTERFACE_ID "bml.console.command_registry"

typedef struct BML_ConsoleCommandEvent {
    size_t struct_size;
    const char *command_utf8;
} BML_ConsoleCommandEvent;

typedef struct BML_ConsoleOutputEvent {
    size_t struct_size;
    const char *message_utf8;
    uint32_t flags;
} BML_ConsoleOutputEvent;

typedef struct BML_ConsoleCommandInvocation {
    size_t struct_size;
    const char *command_line_utf8;
    const char *args_utf8;
    const char *name_utf8;
    const char *const *argv_utf8;
    uint32_t argc;
} BML_ConsoleCommandInvocation;

typedef struct BML_ConsoleCompletionContext {
    size_t struct_size;
    const char *input_utf8;
    const char *name_utf8;
    const char *current_token_utf8;
    const char *const *argv_utf8;
    uint32_t argc;
    uint32_t token_index;
    uint32_t flags;
} BML_ConsoleCompletionContext;

typedef void (*PFN_BML_ConsoleCompletionSink)(const char *candidate_utf8, void *user_data);
typedef BML_Result (*PFN_BML_ConsoleCommandExecute)(const BML_ConsoleCommandInvocation *invocation, void *user_data);
typedef void (*PFN_BML_ConsoleCommandComplete)(const BML_ConsoleCompletionContext *context,
    PFN_BML_ConsoleCompletionSink sink,
    void *sink_user_data,
    void *command_user_data);

typedef struct BML_ConsoleCommandDesc {
    size_t struct_size;
    const char *name_utf8;
    const char *alias_utf8;
    const char *description_utf8;
    uint32_t flags;
    PFN_BML_ConsoleCommandExecute execute;
    PFN_BML_ConsoleCommandComplete complete;
    void *user_data;
} BML_ConsoleCommandDesc;

typedef struct BML_ConsoleCommandRegistry {
    BML_InterfaceHeader header;
    BML_Result (*RegisterCommand)(const BML_ConsoleCommandDesc *desc);
    BML_Result (*UnregisterCommand)(const char *name_utf8);
    BML_Result (*ExecuteCommand)(const char *command_utf8);
    void (*Print)(const char *message_utf8, uint32_t flags);
} BML_ConsoleCommandRegistry;

#define BML_CONSOLE_COMMAND_EVENT_INIT { sizeof(BML_ConsoleCommandEvent), NULL }
#define BML_CONSOLE_OUTPUT_EVENT_INIT { sizeof(BML_ConsoleOutputEvent), NULL, 0u }
#define BML_CONSOLE_COMMAND_INVOCATION_INIT { sizeof(BML_ConsoleCommandInvocation), NULL, NULL, NULL, NULL, 0u }
#define BML_CONSOLE_COMPLETION_CONTEXT_INIT { sizeof(BML_ConsoleCompletionContext), NULL, NULL, NULL, NULL, 0u, 0u, 0u }
#define BML_CONSOLE_COMMAND_DESC_INIT { sizeof(BML_ConsoleCommandDesc), NULL, NULL, NULL, 0u, NULL, NULL, NULL }

#define BML_CONSOLE_OUTPUT_FLAG_NONE 0u
#define BML_CONSOLE_OUTPUT_FLAG_ERROR (1u << 0)
#define BML_CONSOLE_OUTPUT_FLAG_SYSTEM (1u << 1)

#define BML_CONSOLE_COMMAND_FLAG_NONE 0u
#define BML_CONSOLE_COMMAND_FLAG_HIDDEN (1u << 0)

#define BML_CONSOLE_COMPLETION_FLAG_NONE 0u
#define BML_CONSOLE_COMPLETION_FLAG_TRAILING_SPACE (1u << 0)

BML_END_CDECLS

#endif // BML_CONSOLE_H
