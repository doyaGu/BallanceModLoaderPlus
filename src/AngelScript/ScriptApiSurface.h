#ifndef BML_SCRIPT_API_SURFACE_H
#define BML_SCRIPT_API_SURFACE_H

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
    ScriptCallbackOnModifyConfig,
    ScriptCallbackOnPhysicalize,
    ScriptCallbackOnUnphysicalize,
    ScriptCallbackCount
};

enum class ScriptCallbackPayloadKind {
    None,
    GameEventInt,
    EventObject
};

struct ScriptCallbackDescriptor {
    ScriptCallbackId Id;
    const char *Name;
    const char *Declaration;
    const char *FailurePrefix;
    ScriptCallbackPayloadKind PayloadKind;
};

struct ScriptTypedefDescriptor {
    const char *Name;
    const char *TargetType;
    const char *Declaration;
};

struct ScriptIntegerConstantDescriptor {
    const char *Declaration;
    int Value;
};

struct ScriptEnumValueDescriptor {
    const char *Name;
    int Value;
    const char *DiagnosticName;
};

struct ScriptEnumDescriptor {
    const char *Name;
    const char *Declaration;
    const ScriptEnumValueDescriptor *Values;
    size_t ValueCount;
};


struct ScriptEventMemberDescriptor {
    const char *Declaration;
    const char *DiagnosticName;
};

struct ScriptEventTypeDescriptor {
    const char *Name;
    const char *Declaration;
    const ScriptEventMemberDescriptor *Members;
    size_t MemberCount;
};

template <typename T>
struct ScriptDescriptorSpan {
    const T *Data;
    size_t Count;

    const T *begin() const { return Data; }
    const T *end() const { return Data ? Data + Count : Data; }
};

namespace ScriptApiSurface {

ScriptDescriptorSpan<ScriptCallbackDescriptor> Callbacks();
ScriptDescriptorSpan<ScriptTypedefDescriptor> Typedefs();
ScriptDescriptorSpan<ScriptIntegerConstantDescriptor> GameEventConstants();
ScriptDescriptorSpan<ScriptIntegerConstantDescriptor> ErrorConstants();
ScriptDescriptorSpan<ScriptEnumDescriptor> Enums();
ScriptDescriptorSpan<ScriptEventTypeDescriptor> EventTypes();

} // namespace ScriptApiSurface

} // namespace BML

#endif // BML_SCRIPT_API_SURFACE_H
