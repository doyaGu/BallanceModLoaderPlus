#ifndef BML_SCRIPTAPICONTRACT_H
#define BML_SCRIPTAPICONTRACT_H

#include <cstddef>

namespace BML {

enum ScriptCallbackId {
    ScriptCallbackOnLoad,
    ScriptCallbackOnUnload,
    ScriptCallbackOnProcess,
    ScriptCallbackOnRender,
    ScriptCallbackOnGameEvent,
    ScriptCallbackOnCheatEnabled,
    ScriptCallbackOnLoadObject,
    ScriptCallbackOnLoadScript,
    ScriptCallbackOnCommandEvent,
    ScriptCallbackOnPhysicalize,
    ScriptCallbackOnUnphysicalize,
    ScriptCallbackCount
};

enum class ScriptCallbackPayloadKind {
    None,
    GameEventInt,
    BorrowedEventView
};

struct ScriptCallbackContract {
    ScriptCallbackId Id;
    const char *Name;
    const char *Declaration;
    const char *FailurePrefix;
    ScriptCallbackPayloadKind PayloadKind;
};

struct ScriptTypedefContract {
    const char *Name;
    const char *TargetType;
    const char *Declaration;
};

struct ScriptIntegerConstantContract {
    const char *Declaration;
    int Value;
};

struct ScriptEnumValueContract {
    const char *Name;
    int Value;
    const char *DiagnosticName;
};

struct ScriptEnumContract {
    const char *Name;
    const char *Declaration;
    const ScriptEnumValueContract *Values;
    size_t ValueCount;
};


struct ScriptEventMemberContract {
    const char *Declaration;
    const char *DiagnosticName;
};

struct ScriptEventTypeContract {
    const char *Name;
    const char *Declaration;
    const ScriptEventMemberContract *Members;
    size_t MemberCount;
};

template <typename T>
struct ScriptContractSpan {
    const T *Data;
    size_t Count;

    const T *begin() const { return Data; }
    const T *end() const { return Data + Count; }
};

namespace ScriptApiContract {

ScriptContractSpan<ScriptCallbackContract> Callbacks();
ScriptContractSpan<ScriptTypedefContract> Typedefs();
ScriptContractSpan<ScriptIntegerConstantContract> GameEventConstants();
ScriptContractSpan<ScriptIntegerConstantContract> ErrorConstants();
ScriptContractSpan<ScriptEnumContract> Enums();
ScriptContractSpan<ScriptEventTypeContract> EventTypes();

} // namespace ScriptApiContract

} // namespace BML

#endif
