// Stable C interface for registering, inspecting, and executing console
// commands. Command.hpp is the optional C++ authoring layer.
//
// All text is UTF-8. Registration copies the definition strings, but retains
// UserData and the callback pointers until the registration is removed. The
// callbacks run synchronously on the game thread and must not unwind across the
// C boundary. Outstanding registrations are removed before their owner DLL is
// released.
#ifndef BML_COMMAND_H
#define BML_COMMAND_H

#include "BML/Interface.h"

BML_BEGIN_CDECLS

#pragma pack(push, 8)

#define BML_COMMAND_INTERFACE_ID "bml.command"
#define BML_COMMAND_INTERFACE_MAJOR 1
#define BML_COMMAND_INTERFACE_MINOR 0

#define BML_COMMAND_MAX_NAME_BYTES 256u
#define BML_COMMAND_MAX_LINE_BYTES 65535u
#define BML_COMMAND_MAX_COMPLETIONS 256u
#define BML_COMMAND_MAX_COMPLETION_BYTES 4096u

typedef uint64_t BML_CommandHandle;

#define BML_COMMAND_INVALID_HANDLE ((BML_CommandHandle) 0)

// Non-zero handles are unique for the lifetime of the loader process. Once a
// handle is removed it is never assigned to a later registration.

typedef enum BML_CommandFlags {
    BML_COMMAND_NONE = 0,
    BML_COMMAND_CHEAT = 1u << 0,
    BML_COMMAND_HIDDEN = 1u << 1,
    BML_COMMAND_DISABLED = 1u << 2,
    BML_COMMAND_FLAGS_FORCE_32BIT = 0x7fffffff
} BML_CommandFlags;

// Conventional shell statuses returned by Execute and written by ExecuteLine.
// A command may return another non-negative value when it needs a more specific
// status. Negative values are reserved for BML API errors.
typedef enum BML_CommandStatus {
    BML_COMMAND_STATUS_SUCCESS = 0,
    BML_COMMAND_STATUS_FAILURE = 1,
    BML_COMMAND_STATUS_SYNTAX = 2,
    BML_COMMAND_STATUS_DISABLED = 125,
    BML_COMMAND_STATUS_CHEAT_REFUSED = 126,
    BML_COMMAND_STATUS_UNKNOWN = 127,
    BML_COMMAND_STATUS_FORCE_32BIT = 0x7fffffff
} BML_CommandStatus;

typedef struct BML_CommandInvocation BML_CommandInvocation;
typedef struct BML_CommandCompletionRequest BML_CommandCompletionRequest;
typedef struct BML_CommandCompletion BML_CommandCompletion;

// Writes one message to the command's current output. During a pipeline this
// feeds the next stage; otherwise it goes to the message board. The context and
// function are valid only during Execute. Text need not be null-terminated and
// may contain embedded newlines, but must be valid UTF-8 without embedded null
// bytes.
typedef int (BML_CDECL *BML_CommandWrite)(
    void *context, const char *text, size_t length);

// Adds one non-empty, unfiltered UTF-8 completion candidate without embedded
// null bytes.
// The context and function are valid only during Complete. The command bar
// performs prefix filtering and duplicate removal after the callback returns.
typedef int (BML_CDECL *BML_CommandAddCompletion)(
    void *context, const char *text, size_t length);

struct BML_CommandInvocation {
    size_t StructSize;
    const char *Name;
    const char *InvokedAs;
    // Execute receives all arguments after the command word.
    size_t ArgumentCount;
    const char *const *Arguments;
    const char *Input;
    size_t InputLength;
    void *OutputContext;
    BML_CommandWrite Write;
};

#define BML_COMMAND_INVOCATION_1_0_SIZE                                      \
    (offsetof(BML_CommandInvocation, Write) +                                \
     sizeof(((BML_CommandInvocation *) 0)->Write))

struct BML_CommandCompletion {
    size_t StructSize;
    void *Context;
    BML_CommandAddCompletion Add;
};

#define BML_COMMAND_COMPLETION_1_0_SIZE                                      \
    (offsetof(BML_CommandCompletion, Add) +                                  \
     sizeof(((BML_CommandCompletion *) 0)->Add))

struct BML_CommandCompletionRequest {
    size_t StructSize;
    const char *Name;
    const char *InvokedAs;
    // Arguments exclude the command word. The active argument identifies the
    // entry being completed; Prefix is the same entry as a direct string. A
    // trailing space is represented by an empty active argument and prefix.
    size_t ArgumentCount;
    const char *const *Arguments;
    size_t ActiveArgument;
    const char *Prefix;
};

#define BML_COMMAND_COMPLETION_REQUEST_1_0_SIZE                              \
    (offsetof(BML_CommandCompletionRequest, Prefix) +                        \
     sizeof(((BML_CommandCompletionRequest *) 0)->Prefix))

// The invocation and all referenced memory are valid only for this call.
// Execute returns the command's non-negative shell status: zero succeeds and a
// non-zero value fails. A negative return is invalid and is normalized to 1.
typedef int (BML_CDECL *BML_CommandExecute)(
    void *userData, const BML_CommandInvocation *invocation);

// The request, completion sink, and all referenced memory are valid only for
// this call. Complete returns BML_OK or a negative BML error code. On failure
// all candidates produced by that call are discarded.
typedef int (BML_CDECL *BML_CommandComplete)(
    void *userData, const BML_CommandCompletionRequest *request,
    const BML_CommandCompletion *completion);

// Releases UserData after the command has been removed and no invocation or
// completion can still reference it. When this callback is provided, ownership
// transfers to the loader only after Register succeeds.
typedef void (BML_CDECL *BML_CommandRelease)(void *userData);

typedef struct BML_CommandDefinition {
    size_t StructSize;
    // Name and Alias are bounded by BML_COMMAND_MAX_NAME_BYTES, excluding the
    // terminating null byte.
    const char *Name;
    // One optional alternate spelling. Pass NULL or an empty string for none.
    const char *Alias;
    const char *Description;
    const char *Usage;
    const char *Category;
    BML_CommandFlags Flags;
    void *UserData;
    BML_CommandExecute Execute;
    BML_CommandComplete Complete;
    BML_CommandRelease Release;
} BML_CommandDefinition;

#define BML_COMMAND_DEFINITION_1_0_SIZE                                      \
    (offsetof(BML_CommandDefinition, Release) +                              \
     sizeof(((BML_CommandDefinition *) 0)->Release))

// Info is valid only for the duration of a visitor call. Handle is zero for a
// legacy or script command that was not registered through this interface.
typedef struct BML_CommandInfo {
    size_t StructSize;
    BML_CommandHandle Handle;
    const char *Name;
    const char *Alias;
    const char *Description;
    const char *Usage;
    const char *Category;
    BML_CommandFlags Flags;
} BML_CommandInfo;

#define BML_COMMAND_INFO_1_0_SIZE                                            \
    (offsetof(BML_CommandInfo, Flags) +                                      \
     sizeof(((BML_CommandInfo *) 0)->Flags))

// Return BML_OK to continue. Any other value stops traversal and is returned
// by Visit or Find.
typedef int (BML_CDECL *BML_CommandVisitor)(
    void *userData, const BML_CommandInfo *info);

typedef struct BML_CommandInterface {
    BML_InterfaceHeader Header;

    // All functions must be called on the game thread. Callbacks run
    // synchronously there.

    // ownerId may be null when the calling DLL owns exactly one Mod. A
    // successful registration writes a non-zero handle. The loader copies all
    // metadata, rejects callbacks outside the owner DLL, and assumes ownership
    // of UserData when Release is non-null.
    int (BML_CDECL *Register)(const char *ownerId,
                              const BML_CommandDefinition *definition,
                              BML_CommandHandle *outHandle);
    // Unregister removes the command from lookup immediately. Managed UserData
    // is released after every dispatch that already acquired the command ends.
    // An unmanaged command may return BML_ERROR_BUSY when removed from another
    // command's dispatch because its caller still owns UserData.
    int (BML_CDECL *Unregister)(const char *ownerId,
                                BML_CommandHandle handle);
    int (BML_CDECL *SetEnabled)(const char *ownerId,
                                BML_CommandHandle handle, int enabled);

    // Visit traverses a stable snapshot sorted by command name. Find calls the
    // visitor exactly once for the matching name or alias. Visitor callbacks
    // may register, disable, or remove other commands; those changes do not
    // alter the snapshot already being traversed.
    int (BML_CDECL *Visit)(BML_CommandVisitor visitor, void *userData);
    int (BML_CDECL *Find)(const char *name, BML_CommandVisitor visitor,
                          void *userData);

    // Runs a complete shell line of at most BML_COMMAND_MAX_LINE_BYTES,
    // excluding the terminating null byte. This includes quoting, expansion,
    // pipelines, and conditional operators. API success is returned separately
    // from the command's shell status.
    int (BML_CDECL *ExecuteLine)(const char *line, int *outStatus);
} BML_CommandInterface;

#pragma pack(pop)

BML_END_CDECLS

#endif // BML_COMMAND_H
