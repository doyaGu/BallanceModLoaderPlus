// Native Behavior authoring. This interface uses Virtools' own author-facing
// vocabulary: a Block is created from a Prototype, configured through Settings,
// Pins, Locals, and a Target, and driven through its Ins and Outs. Values passed
// in are borrowed for the duration of a call. Run frames returned by TakeFrames
// are copied into caller-owned buffers and contain no process pointers.
#ifndef BML_BEHAVIOR_H
#define BML_BEHAVIOR_H

#include "BML/Interface.h"
#include "BML/Types.h"

#if defined(_WIN64)
#error "bml.behavior is a Win32 Virtools 2.1 interface"
#endif

#if defined(_WIN32) && !defined(_WIN64)
#define BML_BEHAVIOR_CALL __cdecl
#else
#define BML_BEHAVIOR_CALL
#endif

#define BML_BEHAVIOR_INTERFACE_ID "bml.behavior"
#define BML_BEHAVIOR_INTERFACE_MAJOR 1u
#define BML_BEHAVIOR_INTERFACE_MINOR 0u
#define BML_BEHAVIOR_STATUS_MESSAGE_CAPACITY 256u
#define BML_BEHAVIOR_VALUE_ALIGNMENT 4u

BML_BEGIN_CDECLS

typedef struct BML_BehaviorSession__ *BML_BehaviorSession;
typedef struct BML_BehaviorRun__ *BML_BehaviorRun;
typedef struct BML_BehaviorWatch__ *BML_BehaviorWatch;
typedef struct BML_BehaviorPlan__ *BML_BehaviorPlan;
typedef struct BML_BehaviorPatch__ *BML_BehaviorPatch;
typedef struct BML_BehaviorScript__ *BML_BehaviorScript;

#pragma pack(push, 8)

typedef struct BML_BehaviorGuid {
    uint32_t Data1;
    uint32_t Data2;
} BML_BehaviorGuid;

typedef struct BML_BehaviorString {
    const char *Data;
    uint32_t Length;
} BML_BehaviorString;

// Text returned inside a caller-owned payload buffer. Offset is relative to
// the first payload byte and Length excludes any terminator.
typedef struct BML_BehaviorText {
    uint32_t Offset;
    uint32_t Length;
} BML_BehaviorText;

typedef enum BML_BehaviorSelectorKind {
    BML_BEHAVIOR_SELECTOR_INDEX = 1,
    BML_BEHAVIOR_SELECTOR_NAME = 2,
    BML_BEHAVIOR_SELECTOR_UNIQUE_NAME = 3,
    BML_BEHAVIOR_SELECTOR_ONLY = 4
} BML_BehaviorSelectorKind;

typedef struct BML_BehaviorSelector {
    uint32_t StructSize;
    uint32_t Kind;
    int32_t Index;
    int32_t Occurrence;
    BML_BehaviorString Name;
} BML_BehaviorSelector;

typedef enum BML_BehaviorValueKind {
    BML_BEHAVIOR_VALUE_BOOL = 1,
    BML_BEHAVIOR_VALUE_INT32 = 2,
    BML_BEHAVIOR_VALUE_FLOAT32 = 3,
    BML_BEHAVIOR_VALUE_UTF8 = 4,
    BML_BEHAVIOR_VALUE_VEC2 = 5,
    BML_BEHAVIOR_VALUE_VEC3 = 6,
    BML_BEHAVIOR_VALUE_QUATERNION = 7,
    BML_BEHAVIOR_VALUE_EULER = 8,
    BML_BEHAVIOR_VALUE_RECT = 9,
    BML_BEHAVIOR_VALUE_COLOR = 10,
    BML_BEHAVIOR_VALUE_BOX = 11,
    BML_BEHAVIOR_VALUE_MAT4 = 12,
    BML_BEHAVIOR_VALUE_OBJECT = 13
} BML_BehaviorValueKind;

typedef union BML_BehaviorValueData {
    uint32_t Bool;
    int32_t Int32;
    float Float32;
    BML_BehaviorString Utf8;
    BML_Vec2 Vec2;
    BML_Vec3 Vec3;
    BML_Quaternion Quaternion;
    BML_Euler Euler;
    BML_Rect Rect;
    BML_Color Color;
    BML_Box Box;
    BML_Mat4 Mat4;
    BML_ObjectRef Object;
} BML_BehaviorValueData;

typedef struct BML_BehaviorValue {
    uint32_t StructSize;
    uint32_t Kind;
    BML_BehaviorGuid Type;
    BML_BehaviorValueData Data;
} BML_BehaviorValue;

typedef struct BML_BehaviorBinding {
    uint32_t StructSize;
    BML_BehaviorSelector Slot;
    BML_BehaviorValue Value;
} BML_BehaviorBinding;

typedef struct BML_BehaviorSettingStage {
    uint32_t StructSize;
    const BML_BehaviorBinding *Settings;
    uint32_t SettingCount;
} BML_BehaviorSettingStage;

// A slot in one live Layout generation. Index selectors require a non-zero
// LayoutGeneration; named selectors may use zero to resolve the current live
// Layout at the time of the call.
typedef struct BML_BehaviorSlotRef {
    uint32_t StructSize;
    uint32_t Kind;
    uint64_t LayoutGeneration;
    BML_BehaviorGuid Type;
    BML_BehaviorSelector Slot;
} BML_BehaviorSlotRef;

// A parameter exposed by a live CKBehavior. Node is a Scene object reference,
// while Kind and Slot identify one of its Pin, Pout, Setting, Local, or Target
// parameters.
typedef struct BML_BehaviorValueRef {
    uint32_t StructSize;
    uint32_t Kind;
    uint64_t LayoutGeneration;
    BML_ObjectRef Node;
    BML_BehaviorSelector Slot;
} BML_BehaviorValueRef;

typedef enum BML_BehaviorTargetKind {
    BML_BEHAVIOR_TARGET_OWNER = 1,
    BML_BEHAVIOR_TARGET_OBJECT = 2,
    BML_BEHAVIOR_TARGET_NULL = 3
} BML_BehaviorTargetKind;

typedef struct BML_BehaviorTarget {
    uint32_t StructSize;
    uint32_t Kind;
    BML_BehaviorGuid Type;
    BML_ObjectRef Object;
} BML_BehaviorTarget;

typedef enum BML_BehaviorFramePolicyKind {
    // Keep the first Execute, every Execute that activates an Out, the final
    // non-continuing Execute, and every failed Execute, up to Limit. Reaching
    // Limit stops the Run instead of discarding an older Frame.
    BML_BEHAVIOR_FRAMES_SIGNALS = 1,
    // Keep every native Execute, up to Limit. A Run executes at most once in a
    // game frame. Reaching Limit stops the Run instead of overwriting a Frame.
    BML_BEHAVIOR_FRAMES_EACH_FRAME = 2,
    // Keep the latest continuing Execute plus the most recent failed and final
    // non-continuing Executes. Consequently TakeFrames may return up to three
    // Frames for this policy, ordered by Sequence.
    BML_BEHAVIOR_FRAMES_LATEST = 3,
    // Do not keep ordinary continuing Executes. Failed and final
    // non-continuing Executes remain observable through TakeFrames.
    BML_BEHAVIOR_FRAMES_NONE = 4
} BML_BehaviorFramePolicyKind;

typedef enum BML_BehaviorFramePolicyFlags {
    BML_BEHAVIOR_FRAME_POLICY_NONE = 0,
    // Copy every readable Pout into each retained Frame. Without this flag a
    // Frame reports execution and active Outs without reading Pouts.
    BML_BEHAVIOR_FRAME_POLICY_POUTS = 1u << 0
} BML_BehaviorFramePolicyFlags;

typedef struct BML_BehaviorFramePolicy {
    uint32_t StructSize;
    uint32_t Kind;
    uint32_t Limit;
    uint32_t Flags;
} BML_BehaviorFramePolicy;

typedef struct BML_BehaviorBlock {
    uint32_t StructSize;
    BML_BehaviorGuid Prototype;
    BML_BehaviorTarget Target;
    const BML_BehaviorSettingStage *SettingStages;
    uint32_t SettingStageCount;
    const BML_BehaviorBinding *Pins;
    uint32_t PinCount;
    const BML_BehaviorBinding *Locals;
    uint32_t LocalCount;
    // Zero selects the current provider. A nonzero generation pins the
    // Prototype provider selected by FindPrototypes.
    uint64_t PrototypeGeneration;
} BML_BehaviorBlock;

typedef enum BML_BehaviorError {
    BML_BEHAVIOR_ERROR_NONE = 0,
    BML_BEHAVIOR_ERROR_OWNER_UNAVAILABLE = 1,
    BML_BEHAVIOR_ERROR_PROTOTYPE_NOT_FOUND = 2,
    BML_BEHAVIOR_ERROR_REQUIRED_MANAGER_MISSING = 3,
    BML_BEHAVIOR_ERROR_CREATION_FAILED = 4,
    BML_BEHAVIOR_ERROR_INITIALIZATION_FAILED = 5,
    BML_BEHAVIOR_ERROR_TARGET_INVALID = 6,
    BML_BEHAVIOR_ERROR_CALLBACK_FAILED = 7,
    BML_BEHAVIOR_ERROR_SLOT_NOT_FOUND = 8,
    BML_BEHAVIOR_ERROR_SLOT_AMBIGUOUS = 9,
    BML_BEHAVIOR_ERROR_LAYOUT_CHANGED = 10,
    BML_BEHAVIOR_ERROR_TYPE_MISMATCH = 11,
    BML_BEHAVIOR_ERROR_VALUE_INVALID = 12,
    BML_BEHAVIOR_ERROR_STATE_INVALID = 13,
    BML_BEHAVIOR_ERROR_NATIVE_ERROR = 14,
    BML_BEHAVIOR_ERROR_BREAK_UNSUPPORTED = 15,
    BML_BEHAVIOR_ERROR_POUT_UNSUPPORTED = 16,
    BML_BEHAVIOR_ERROR_POUT_UNAVAILABLE = 17,
    BML_BEHAVIOR_ERROR_FRAME_QUEUE_FULL = 18,
    BML_BEHAVIOR_ERROR_CANCELLED = 19,
    BML_BEHAVIOR_ERROR_PROTOTYPE_CHANGED = 20,
    BML_BEHAVIOR_ERROR_PROTOTYPE_LOAD_FAILED = 21,
    BML_BEHAVIOR_ERROR_LAYOUT_UNAVAILABLE = 22,
    BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNAVAILABLE = 23,
    BML_BEHAVIOR_ERROR_PARAMETER_TYPE_UNSUPPORTED = 24,
    BML_BEHAVIOR_ERROR_DETACHED_UNSUPPORTED = 25,
    BML_BEHAVIOR_ERROR_OBSERVER_UNAVAILABLE = 26,
    BML_BEHAVIOR_ERROR_GRAPH_CHANGED = 27,
    BML_BEHAVIOR_ERROR_GRAPH_LOCALITY_INVALID = 28,
    BML_BEHAVIOR_ERROR_DELAY_INVALID = 29,
    BML_BEHAVIOR_ERROR_SAME_FRAME_CYCLE = 30,
    BML_BEHAVIOR_ERROR_SHARED_SOURCE_CYCLE = 31,
    BML_BEHAVIOR_ERROR_PUSH_CYCLE = 32,
    BML_BEHAVIOR_ERROR_INTERFACE_UNSUPPORTED = 33,
    BML_BEHAVIOR_ERROR_SOURCE_CONFLICT = 34,
    BML_BEHAVIOR_ERROR_SOURCE_ORDER_CYCLE = 35,
    BML_BEHAVIOR_ERROR_ORDERING_TARGET_MISMATCH = 36,
    BML_BEHAVIOR_ERROR_OVERLAY_ORDER_CYCLE = 37,
    BML_BEHAVIOR_ERROR_LINK_NOT_FOUND = 38,
    BML_BEHAVIOR_ERROR_PATH_AMBIGUOUS = 39,
    BML_BEHAVIOR_ERROR_PATH_CYCLE = 40,
    BML_BEHAVIOR_ERROR_QUERY_NOT_FOUND = 41,
    BML_BEHAVIOR_ERROR_QUERY_AMBIGUOUS = 42,
    BML_BEHAVIOR_ERROR_WORLD_BOUND_VALUE = 43,
    BML_BEHAVIOR_ERROR_REVERT_CONFLICT = 44,
    BML_BEHAVIOR_ERROR_TARGET_CARDINALITY = 45,
    BML_BEHAVIOR_ERROR_SOURCE_INVALID = 46,
    BML_BEHAVIOR_ERROR_OPERATION_INVALID = 47,
    BML_BEHAVIOR_ERROR_BUSY = 48,
    BML_BEHAVIOR_ERROR_UNAVAILABLE = 49,
    BML_BEHAVIOR_ERROR_WRONG_THREAD = 50,
    BML_BEHAVIOR_ERROR_REDIRECT_CONFLICT = 51
} BML_BehaviorError;

typedef enum BML_BehaviorPhase {
    BML_BEHAVIOR_PHASE_NONE = 0,
    BML_BEHAVIOR_PHASE_PROTOTYPE = 1,
    BML_BEHAVIOR_PHASE_MANAGER = 2,
    BML_BEHAVIOR_PHASE_CREATION = 3,
    BML_BEHAVIOR_PHASE_INITIALIZATION = 4,
    BML_BEHAVIOR_PHASE_LAYOUT = 5,
    BML_BEHAVIOR_PHASE_OWNER = 6,
    BML_BEHAVIOR_PHASE_TARGET = 7,
    BML_BEHAVIOR_PHASE_SETTINGS = 8,
    BML_BEHAVIOR_PHASE_CALLBACK = 9,
    BML_BEHAVIOR_PHASE_BINDING = 10,
    BML_BEHAVIOR_PHASE_EXECUTION = 11,
    BML_BEHAVIOR_PHASE_TEARDOWN = 12,
    BML_BEHAVIOR_PHASE_EDIT = 13
} BML_BehaviorPhase;

typedef struct BML_BehaviorStatus {
    uint32_t StructSize;
    uint32_t Error;
    uint32_t Phase;
    int32_t CkError;
    int32_t NativeResult;
    BML_BehaviorGuid Prototype;
    BML_BehaviorGuid Type;
    uint32_t MessageLength;
    char Message[BML_BEHAVIOR_STATUS_MESSAGE_CAPACITY];
} BML_BehaviorStatus;

// A caller that supplies Status initializes StructSize to sizeof(Status).
// Whenever that size is valid, a Behavior function which accepts Status clears
// and writes it before returning. Error == BML_BEHAVIOR_ERROR_NONE means the
// outer BML_* result alone describes a generic argument, thread, availability,
// handle, or buffer condition. A nonzero Error is the stable Behavior-domain
// diagnostic; Message is explanatory text and must not be parsed for control
// flow. MessageLength follows the truncation rule in BML/Interface.h.

// Prototype discovery names one provider registration, not only a GUID.
// Generation is process-local and never denotes a CK pointer or DLL handle.
typedef struct BML_BehaviorPrototypeRef {
    uint32_t StructSize;
    BML_BehaviorGuid Prototype;
    uint64_t Generation;
} BML_BehaviorPrototypeRef;

typedef enum BML_BehaviorRunKind {
    BML_BEHAVIOR_RUN_CALL = 1,
    BML_BEHAVIOR_RUN_TASK = 2,
    BML_BEHAVIOR_RUN_INSTANCE = 3
} BML_BehaviorRunKind;

typedef enum BML_BehaviorRunState {
    // No native or queued continuation is waiting. The Run still owns its
    // native Behavior instance and any state established by that instance.
    BML_BEHAVIOR_RUN_READY = 1,
    BML_BEHAVIOR_RUN_PENDING = 2,
    // Mutation or execution cannot continue. Native execution failure, Frame
    // overflow, and a failed live Setting stage are terminal. This does not
    // release the native instance; CloseRun or the enclosing ownership
    // boundary performs teardown.
    BML_BEHAVIOR_RUN_FAILED = 3
} BML_BehaviorRunState;

typedef enum BML_BehaviorRunFlags {
    BML_BEHAVIOR_RUN_UNVERIFIED_DETACHED = 1u << 0
} BML_BehaviorRunFlags;

typedef struct BML_BehaviorRunInfo {
    uint32_t StructSize;
    uint32_t Kind;
    uint32_t State;
    uint32_t Flags;
    // The exact Prototype provider used to create this native Behavior.
    // Generation may be zero only when provider identity is unavailable.
    BML_BehaviorPrototypeRef Prototype;
    BML_BehaviorStatus Status;
} BML_BehaviorRunInfo;

// A top-level Script is a graph-backed CKBehavior owned by one CKBeObject and
// participating in the Scene that was current when it was created. It starts
// inactive so its graph can be authored before Virtools schedules it.
typedef enum BML_BehaviorScriptState {
    BML_BEHAVIOR_SCRIPT_READY = 1,
    BML_BEHAVIOR_SCRIPT_CLOSING = 2,
    BML_BEHAVIOR_SCRIPT_FAILED = 3
} BML_BehaviorScriptState;

typedef struct BML_BehaviorScriptSpec {
    uint32_t StructSize;
    BML_ObjectRef Owner;
    BML_BehaviorString Name;
    int32_t Priority;
    // The complete initial graph. Handle BML_BEHAVIOR_EDIT_GRAPH denotes the
    // new Script root. The Loader validates and applies every step before it
    // publishes the Script or returns its handle.
    uint32_t StepCount;
    const struct BML_BehaviorEditStep *Steps;
} BML_BehaviorScriptSpec;

typedef struct BML_BehaviorScriptInfo {
    uint32_t StructSize;
    uint32_t State;
    // Active is the current Scene state. RequestedActive is the state that
    // will be applied at the next Behavior safe point.
    uint32_t Active;
    uint32_t RequestedActive;
    BML_ObjectRef Root;
    BML_ObjectRef Owner;
    BML_ObjectRef Scene;
    int32_t Priority;
    BML_BehaviorStatus Status;
} BML_BehaviorScriptInfo;

typedef enum BML_BehaviorAdmission {
    BML_BEHAVIOR_ADMISSION_EXECUTED = 1,
    BML_BEHAVIOR_ADMISSION_QUEUED = 2
} BML_BehaviorAdmission;

typedef enum BML_BehaviorContinuation {
    BML_BEHAVIOR_CONTINUATION_NONE = 0,
    // The native CKBehavior remains active after Execute.  CK2 computes this
    // for both function-backed and graph-backed Behaviors.
    BML_BEHAVIOR_CONTINUATION_NATIVE = 1u << 0,
    // BML accepted a Pulse which cannot execute before a later game frame.
    BML_BEHAVIOR_CONTINUATION_QUEUED_INPUT = 1u << 1
} BML_BehaviorContinuation;

// Every offset below is relative to the first byte of the payload buffer passed
// to TakeFrames. Record arrays are naturally aligned. Every Pout ValueOffset is
// a multiple of BML_BEHAVIOR_VALUE_ALIGNMENT, including UTF-8 values.
//
// PoutRecord.Kind is a BML_BehaviorValueKind. Values use the following owned
// wire representation; integer words and IEEE-754 binary32 words are little
// endian and strings have no trailing NUL:
//   BOOL, INT32, FLOAT32                       4 bytes
//   UTF8                                      ValueSize bytes
//   VEC2                                      x,y (8 bytes)
//   VEC3, EULER                               x,y,z (12 bytes)
//   QUATERNION                                x,y,z,w (16 bytes)
//   RECT                                      left,top,right,bottom (16 bytes)
//   COLOR                                     r,g,b,a (16 bytes)
//   BOX                                       Min.x,y,z, Max.x,y,z (24 bytes)
//   MAT4                                      row-major m00..m33 (64 bytes)
//   OBJECT                                    Domain,Slot,Generation (12 bytes)
typedef struct BML_BehaviorRunFrame {
    uint32_t StructSize;
    uint64_t Sequence;
    uint64_t Frame;
    int32_t NativeResult;
    uint32_t Continuation;
    uint32_t Error;
    uint32_t OutOffset;
    uint32_t OutCount;
    uint32_t PoutOffset;
    uint32_t PoutCount;
    uint32_t DiagnosticOffset;
    uint32_t DiagnosticCount;
} BML_BehaviorRunFrame;

typedef struct BML_BehaviorOutRecord {
    uint32_t StructSize;
    int32_t Index;
    int32_t Occurrence;
    uint32_t NameOffset;
    uint32_t NameLength;
} BML_BehaviorOutRecord;

typedef struct BML_BehaviorPoutRecord {
    uint32_t StructSize;
    int32_t Index;
    int32_t Occurrence;
    BML_BehaviorGuid Type;
    uint32_t Kind;
    uint32_t NameOffset;
    uint32_t NameLength;
    uint32_t ValueOffset;
    uint32_t ValueSize;
} BML_BehaviorPoutRecord;

typedef struct BML_BehaviorDiagnosticRecord {
    uint32_t StructSize;
    uint32_t Error;
    uint32_t Phase;
    int32_t CkError;
    int32_t NativeResult;
    BML_BehaviorGuid Prototype;
    BML_BehaviorGuid Type;
    uint32_t MessageOffset;
    uint32_t MessageLength;
} BML_BehaviorDiagnosticRecord;

typedef enum BML_BehaviorPrototypeMatch {
    BML_BEHAVIOR_MATCH_PROTOTYPE = 1u << 0,
    BML_BEHAVIOR_MATCH_NAME = 1u << 1,
    BML_BEHAVIOR_MATCH_CATEGORY = 1u << 2,
    BML_BEHAVIOR_MATCH_PROVIDER = 1u << 3,
    BML_BEHAVIOR_MATCH_PROVIDER_GUID = 1u << 4,
    BML_BEHAVIOR_MATCH_COMPATIBLE_CLASS = 1u << 5,
    BML_BEHAVIOR_MATCH_REQUIRED_MANAGERS = 1u << 6
} BML_BehaviorPrototypeMatch;

typedef struct BML_BehaviorPrototypeQuery {
    uint32_t StructSize;
    uint32_t Match;
    BML_BehaviorGuid Prototype;
    BML_BehaviorString Name;
    BML_BehaviorString Category;
    BML_BehaviorString Provider;
    BML_BehaviorGuid ProviderGuid;
    int32_t CompatibleClass;
    uint32_t Reserved;
    const BML_BehaviorGuid *RequiredManagers;
    uint32_t RequiredManagerCount;
} BML_BehaviorPrototypeQuery;

typedef struct BML_BehaviorManagerInfo {
    uint32_t StructSize;
    BML_BehaviorGuid Guid;
    uint32_t Available;
} BML_BehaviorManagerInfo;

typedef struct BML_BehaviorPrototypeInfo {
    uint32_t StructSize;
    BML_BehaviorPrototypeRef Ref;
    BML_BehaviorGuid Provider;
    uint32_t Version;
    int32_t CompatibleClass;
    BML_BehaviorText Name;
    BML_BehaviorText Category;
    BML_BehaviorText ProviderName;
    BML_BehaviorText Author;
    BML_BehaviorText Description;
    uint32_t ManagerOffset;
    uint32_t ManagerCount;
} BML_BehaviorPrototypeInfo;

typedef enum BML_BehaviorLayoutOrigin {
    BML_BEHAVIOR_LAYOUT_DECLARED = 1,
    BML_BEHAVIOR_LAYOUT_LIVE = 2
} BML_BehaviorLayoutOrigin;

typedef enum BML_BehaviorKind {
    BML_BEHAVIOR_KIND_FUNCTION = 1,
    // The Prototype contributes lifecycle callbacks but no Execute function.
    // A live instance may remain callback-only or become graph-backed in CREATE.
    BML_BEHAVIOR_KIND_CALLBACK = 2,
    BML_BEHAVIOR_KIND_GRAPH = 3
} BML_BehaviorKind;

typedef enum BML_BehaviorSlotKind {
    BML_BEHAVIOR_SLOT_IN = 1,
    BML_BEHAVIOR_SLOT_OUT = 2,
    BML_BEHAVIOR_SLOT_PIN = 3,
    BML_BEHAVIOR_SLOT_POUT = 4,
    BML_BEHAVIOR_SLOT_SETTING = 5,
    BML_BEHAVIOR_SLOT_LOCAL = 6,
    BML_BEHAVIOR_SLOT_TARGET = 7
} BML_BehaviorSlotKind;

typedef enum BML_BehaviorSlotFlags {
    BML_BEHAVIOR_SLOT_DYNAMIC = 1u << 0,
    BML_BEHAVIOR_SLOT_VALUE_SUPPORTED = 1u << 1
} BML_BehaviorSlotFlags;

typedef struct BML_BehaviorSlotRecord {
    uint32_t StructSize;
    uint32_t Kind;
    uint32_t Flags;
    int32_t Index;
    int32_t Occurrence;
    BML_BehaviorGuid Type;
    uint32_t ValueKind;
    BML_BehaviorText Name;
    BML_BehaviorText TypeName;
} BML_BehaviorSlotRecord;

typedef struct BML_BehaviorLayout {
    uint32_t StructSize;
    uint32_t Origin;
    BML_BehaviorPrototypeRef Prototype;
    uint64_t LayoutGeneration;
    uint32_t Kind;
    // Reserved for future Layout facts; must be zero in v1.
    uint32_t Flags;
    int32_t CompatibleClass;
    uint32_t PrototypeFlags;
    uint32_t BehaviorFlags;
    BML_BehaviorGuid TargetType;
    BML_BehaviorText Name;
    BML_BehaviorText Category;
    BML_BehaviorText ProviderName;
    BML_BehaviorText Author;
    BML_BehaviorText Description;
    uint32_t ManagerOffset;
    uint32_t ManagerCount;
    uint32_t SlotOffset;
    uint32_t SlotCount;
} BML_BehaviorLayout;

typedef enum BML_BehaviorGraphView {
    BML_BEHAVIOR_GRAPH_LOGICAL = 1,
    BML_BEHAVIOR_GRAPH_LIVE = 2
} BML_BehaviorGraphView;

typedef enum BML_BehaviorTruth {
    BML_BEHAVIOR_FALSE = 0,
    BML_BEHAVIOR_TRUE = 1,
    BML_BEHAVIOR_UNKNOWN = 2
} BML_BehaviorTruth;

typedef struct BML_BehaviorGraphPort {
    uint32_t StructSize;
    uint64_t Node;
    uint64_t LayoutGeneration;
    uint32_t Kind;
    uint32_t Flags;
    int32_t Index;
    int32_t Occurrence;
    BML_BehaviorGuid Type;
    uint32_t Active;
    BML_BehaviorText Name;
} BML_BehaviorGraphPort;

typedef struct BML_BehaviorGraphNode {
    uint32_t StructSize;
    uint64_t Id;
    BML_ObjectRef Object;
    uint64_t Parent;
    // Zero-based position in the parent graph. The inspected root is -1.
    int32_t Index;
    // Zero-based occurrence among siblings with the same name.
    int32_t Occurrence;
    // A BML_BehaviorKind. Graph-backed Nodes are navigable with Inspect.
    uint32_t Kind;
    uint64_t LayoutGeneration;
    BML_BehaviorGuid Prototype;
    int32_t Priority;
    uint32_t Active;
    BML_BehaviorText Name;
    uint32_t PortOffset;
    uint32_t PortCount;
} BML_BehaviorGraphNode;

typedef struct BML_BehaviorGraphLink {
    uint32_t StructSize;
    uint64_t Id;
    BML_ObjectRef Object;
    uint64_t SourceNode;
    uint32_t SourceKind;
    int32_t SourceIndex;
    uint64_t TargetNode;
    uint32_t TargetKind;
    int32_t TargetIndex;
    int32_t InitialDelay;
    int32_t RemainingDelay;
    uint32_t Pending;
} BML_BehaviorGraphLink;

// One CKParameterOperation owned by the graph. Function and the parameter
// type tuple identify the exact registered Virtools operation overload.
typedef struct BML_BehaviorGraphOperation {
    uint32_t StructSize;
    uint64_t Id;
    BML_ObjectRef Object;
    uint64_t Owner;
    BML_BehaviorGuid Function;
    BML_BehaviorGuid Result;
    BML_BehaviorGuid Input1;
    BML_BehaviorGuid Input2;
    BML_BehaviorText Name;
} BML_BehaviorGraphOperation;

// All offsets are relative to the payload passed to Inspect.
typedef struct BML_BehaviorGraph {
    uint32_t StructSize;
    uint32_t View;
    BML_ObjectRef Root;
    uint64_t Generation;
    uint64_t Fingerprint;
    uint32_t NodeOffset;
    uint32_t NodeCount;
    uint32_t LinkOffset;
    uint32_t LinkCount;
    uint32_t OperationOffset;
    uint32_t OperationCount;
} BML_BehaviorGraph;

typedef enum BML_BehaviorReadMode {
    BML_BEHAVIOR_READ_NON_FORCING = 1
} BML_BehaviorReadMode;

typedef enum BML_BehaviorValueState {
    BML_BEHAVIOR_VALUE_AVAILABLE = 1,
    BML_BEHAVIOR_VALUE_INDETERMINATE = 2,
    BML_BEHAVIOR_VALUE_UNSUPPORTED = 3
} BML_BehaviorValueState;

typedef enum BML_BehaviorValueRelation {
    BML_BEHAVIOR_VALUE_STORED = 1,
    BML_BEHAVIOR_VALUE_DIRECT = 2,
    BML_BEHAVIOR_VALUE_SHARED = 3,
    BML_BEHAVIOR_VALUE_OPERATION = 4
} BML_BehaviorValueRelation;

typedef struct BML_BehaviorGraphValue {
    uint32_t StructSize;
    uint32_t State;
    uint32_t Relation;
    BML_BehaviorGuid Type;
    uint32_t Kind;
    uint32_t ValueOffset;
    uint32_t ValueSize;
} BML_BehaviorGraphValue;

// When State is AVAILABLE, Kind is a BML_BehaviorValueKind and ValueOffset /
// ValueSize use the same aligned wire representation documented for
// BML_BehaviorPoutRecord. Other states carry no value bytes.

typedef enum BML_BehaviorWatchKind {
    BML_BEHAVIOR_WATCH_GRAPH = 1,
    BML_BEHAVIOR_WATCH_LAYOUT = 2,
    BML_BEHAVIOR_WATCH_SAMPLED_VALUE = 3
} BML_BehaviorWatchKind;

typedef struct BML_BehaviorWatchValue {
    uint32_t StructSize;
    uint32_t State;
    uint32_t Relation;
    BML_BehaviorValue Value;
} BML_BehaviorWatchValue;

typedef struct BML_BehaviorWatchEvent {
    uint32_t StructSize;
    uint32_t Kind;
    uint64_t Sequence;
    uint64_t Frame;
    uint64_t Before;
    uint64_t After;
    BML_BehaviorWatchValue PreviousValue;
    BML_BehaviorWatchValue CurrentValue;
} BML_BehaviorWatchEvent;

typedef enum BML_BehaviorWatchState {
    BML_BEHAVIOR_WATCH_ACTIVE = 1,
    BML_BEHAVIOR_WATCH_FAILED = 2
} BML_BehaviorWatchState;

typedef struct BML_BehaviorWatchInfo {
    uint32_t StructSize;
    uint32_t State;
    // Set when State is FAILED. The first callback or observation failure is
    // retained until CloseWatch; Message follows the Status truncation rule.
    BML_BehaviorStatus Diagnostic;
} BML_BehaviorWatchInfo;

typedef void (BML_BEHAVIOR_CALL *BML_BehaviorWatchRetain)(void *state);
typedef void (BML_BEHAVIOR_CALL *BML_BehaviorWatchRelease)(void *state);

typedef enum BML_BehaviorWatchResult {
    BML_BEHAVIOR_WATCH_OK = 0,
    // Stop this Watch. The Loader never allows an exception to escape through
    // the callback seam; any other return value is treated as ERROR as well.
    BML_BEHAVIOR_WATCH_ERROR = 1
} BML_BehaviorWatchResult;

typedef int (BML_BEHAVIOR_CALL *BML_BehaviorWatchCallback)(
    void *state, const BML_BehaviorWatchEvent *event);

// One callback owned by a Watch. Retain and Release are either both null for
// static State, or both non-null. A successful Watch retains once before the
// caller may release its reference, and releases once after CloseWatch or an
// automatic failure has reached a game-thread safe point and no invocation is
// active. Invoke runs on the game thread. It may call CloseWatch; that closes
// admission immediately without waiting for the current invocation. None of
// Retain, Release, or Invoke may unwind an exception through this C interface.
typedef struct BML_BehaviorWatchFunction {
    uint32_t StructSize;
    void *State;
    BML_BehaviorWatchRetain Retain;
    BML_BehaviorWatchRelease Release;
    BML_BehaviorWatchCallback Invoke;
} BML_BehaviorWatchFunction;

typedef struct BML_BehaviorWatchSpec {
    uint32_t StructSize;
    uint32_t Kind;
    uint32_t View;
    BML_ObjectRef Root;
    BML_ObjectRef Node;
    uint32_t SlotKind;
    uint64_t LayoutGeneration;
    BML_BehaviorSelector Slot;
    uint32_t Read;
} BML_BehaviorWatchSpec;

// A Plan owns a symbolic edit selected by an exact script name and applies it
// to every matching live installation. The
// Loader owns the installed edits and reconciles them as scripts load, reload,
// or are deleted, so a Plan outlives a level change while a Run does not.
typedef enum BML_BehaviorTargetSet {
    // Install into every live script that carries the name.
    BML_BEHAVIOR_TARGETS_EACH = 1,
    // Install into the single live script that carries the name. More than one
    // live match leaves the Plan Unsatisfied instead of choosing one.
    BML_BEHAVIOR_TARGETS_ONE = 2
} BML_BehaviorTargetSet;

typedef enum BML_BehaviorPlanState {
    // Accepted, and waiting for its first reconciliation pass. A Plan reads
    // this way between the call that submits it and the next frame.
    BML_BEHAVIOR_PLAN_RECONCILING = 1,
    // Installed into every current match.
    BML_BEHAVIOR_PLAN_ACTIVE = 2,
    // A pass ran and found no live match, or an ambiguous match under
    // BML_BEHAVIOR_TARGETS_ONE. The Plan is retained and reconsidered against
    // later worlds.
    BML_BEHAVIOR_PLAN_UNSATISFIED = 3,
    // At least one Script rule is installed while another rule is still
    // reconciling or unsatisfied.
    BML_BEHAVIOR_PLAN_PARTIAL = 4,
    BML_BEHAVIOR_PLAN_DISABLED = 5,
    // An installation could not be reverted. The Plan keeps its Hook state
    // alive because the game graph still refers to it.
    BML_BEHAVIOR_PLAN_CONFLICTED = 6,
    BML_BEHAVIOR_PLAN_RETIRING = 7
} BML_BehaviorPlanState;

typedef struct BML_BehaviorPlanInfo {
    uint32_t StructSize;
    uint32_t State;
    // The world epoch this Plan was last reconciled against.
    uint64_t World;
    uint32_t Matches;
    uint32_t Installations;
    // Why the Plan is Unsatisfied or Conflicted, not why the call failed.
    BML_BehaviorStatus Diagnostic;
} BML_BehaviorPlanInfo;

// State of one Patch applied to a specific live graph. Unlike a Plan, a Patch
// is not matched again after its graph is deleted or the world is reset.
typedef enum BML_BehaviorPatchState {
    BML_BEHAVIOR_PATCH_PENDING = 1,
    BML_BEHAVIOR_PATCH_ACTIVE = 2,
    BML_BEHAVIOR_PATCH_DISABLED = 3,
    BML_BEHAVIOR_PATCH_CLOSING = 4,
    BML_BEHAVIOR_PATCH_CONFLICTED = 5,
    BML_BEHAVIOR_PATCH_CLOSED = 6,
    BML_BEHAVIOR_PATCH_FAILED = 7
} BML_BehaviorPatchState;

typedef struct BML_BehaviorPatchInfo {
    uint32_t StructSize;
    uint32_t State;
    // Number of graph relations that could not be restored exactly. Details
    // remain in Diagnostic in v1; a later minor may add a caller-buffer view.
    uint32_t Conflicts;
    uint32_t Reserved;
    BML_BehaviorStatus Diagnostic;
} BML_BehaviorPatchInfo;

typedef struct BML_BehaviorHookContext {
    uint32_t StructSize;
    uint32_t Flags;
    float DeltaTime;
    uint32_t Reserved;
    // The Hook Block instance being executed.
    BML_ObjectRef Block;
    // The root script that owns the Block, when the Loader can name it.
    BML_ObjectRef Script;
    // The object the Block is attached to.
    BML_ObjectRef Owner;
} BML_BehaviorHookContext;

// Return codes an author callback may report. Any other value is an explicit
// error and stops the enclosing chain: the Hook Block leaves every Out inactive
// and reports a behavior error to Virtools. CK2 itself discards a
// sub-behavior's return code, so inactive Outs are the only way to stop a chain.
typedef enum BML_BehaviorHookResult {
    BML_BEHAVIOR_HOOK_OK = 0,
    // Keep the Hook Block active for one more frame. Its Outs still activate.
    BML_BEHAVIOR_HOOK_AGAIN_NEXT_FRAME = 1,
    // The callback did not complete (for example it threw). The Loader keeps
    // the first fault as the Hook diagnostic, stops invoking this callback
    // occurrence, and lets the Hook Block stay transparent: its Outs activate
    // as if no callback ran, so one Mod's bug does not stop the host script's
    // chain. An explicit error return keeps stopping the chain and keeps the
    // callback installed.
    BML_BEHAVIOR_HOOK_FAULT = 2
} BML_BehaviorHookResult;

typedef void (BML_BEHAVIOR_CALL *BML_BehaviorHookRetain)(void *state);
typedef void (BML_BEHAVIOR_CALL *BML_BehaviorHookRelease)(void *state);
typedef int (BML_BEHAVIOR_CALL *BML_BehaviorHookCallback)(
    void *state, const BML_BehaviorHookContext *context);

// One author callback occurrence in an edit. Retain runs once while a Patch or
// Plan accepts the Hook, so the caller may drop its own reference as soon as
// the call returns, and Release runs once the Loader has retired the occurrence
// and dropped every installation of it. A rejected edit retains nothing it has
// not already released. Invoke runs on the game thread inside the behavior
// execution the game itself drives. It may close the Patch, Plan, or Session
// that owns it: admission closes immediately, the request never waits for the
// current invocation, and native teardown plus Release finish later at a
// game-thread safe point.
typedef struct BML_BehaviorHookFunction {
    uint32_t StructSize;
    void *State;
    BML_BehaviorHookRetain Retain;
    BML_BehaviorHookRelease Release;
    BML_BehaviorHookCallback Invoke;
} BML_BehaviorHookFunction;

// Cross-Mod overlay ordering on one spliced link, by Patch identity. A Patch
// no one submitted does not constrain anything.
typedef enum BML_BehaviorOrderKind {
    BML_BEHAVIOR_ORDER_BEFORE = 1,
    BML_BEHAVIOR_ORDER_AFTER = 2
} BML_BehaviorOrderKind;

typedef struct BML_BehaviorEditOrder {
    uint32_t StructSize;
    uint32_t Kind;
    BML_BehaviorString Owner;
    BML_BehaviorString Name;
} BML_BehaviorEditOrder;

// One native CKParameterOperation overload. Operation identifies the
// registered Virtools operation type; the three parameter type GUIDs select
// its concrete function. A zero input type means that input is absent.
typedef struct BML_BehaviorOperationSpec {
    uint32_t StructSize;
    BML_BehaviorGuid Operation;
    BML_BehaviorGuid Result;
    BML_BehaviorGuid Input1;
    BML_BehaviorGuid Input2;
} BML_BehaviorOperationSpec;

// Steps name each other through caller-assigned handles in one namespace per
// edit. Handle 1 always denotes the target graph itself; every other handle
// must be defined by an earlier step before a later step reads it.
#define BML_BEHAVIOR_EDIT_GRAPH 1u

// A port of a node. Kind is a BML_BehaviorSlotKind naming which interface of
// Handle to address. A zero Kind means Handle is itself a port defined by an
// earlier BML_BEHAVIOR_EDIT_APPEND_SLOT, which is the only way to address an
// appended slot: an appended slot has no author-visible index until the edit
// is compiled against a live graph.
typedef struct BML_BehaviorPortRef {
    uint32_t StructSize;
    // Graph scope containing Handle.
    uint32_t Graph;
    uint32_t Handle;
    uint32_t Kind;
    // The Virtools parameter type expected of a Pin, Pout, or Local port. A
    // zero type accepts whatever the live slot declares.
    BML_BehaviorGuid Type;
    BML_BehaviorSelector Slot;
} BML_BehaviorPortRef;

typedef enum BML_BehaviorEditKind {
    // Result names the one existing node matching Name and Prototype.
    BML_BEHAVIOR_EDIT_REQUIRE_NODE = 1,
    // Result names the one existing link from Source to Sink.
    BML_BEHAVIOR_EDIT_REQUIRE_LINK = 2,
    // Result names the unique non-branching path leaving Source.
    BML_BEHAVIOR_EDIT_FOLLOW = 3,
    // Result names a new Block created from Prototype.
    BML_BEHAVIOR_EDIT_ADD_BLOCK = 4,
    // Result names a slot appended to Target, of SlotKind, called Name. A
    // Local may be appended to the graph root or to a Block created by this
    // same edit, never to a borrowed child node.
    BML_BEHAVIOR_EDIT_APPEND_SLOT = 5,
    // Adds a behavior link from Source to Sink.
    BML_BEHAVIOR_EDIT_FLOW = 6,
    // Writes Value into Sink.
    BML_BEHAVIOR_EDIT_BIND_VALUE = 7,
    // Makes Sink read Source directly.
    BML_BEHAVIOR_EDIT_BIND_PORT = 8,
    // Makes Sink share the parameter Source reads.
    BML_BEHAVIOR_EDIT_SHARE = 9,
    // Copies Source into Sink after each execution of the node owning Source.
    BML_BEHAVIOR_EDIT_PUSH = 10,
    // Inserts Hook on every link leaving Source.
    BML_BEHAVIOR_EDIT_TAP = 11,
    // Inserts Hook at the end of the path named by Target.
    BML_BEHAVIOR_EDIT_AFTER = 12,
    // Reroutes the link named by Target through Node, or through the Sink and
    // Source ports when Node is zero, keeping the delay of that link.
    BML_BEHAVIOR_EDIT_SPLICE = 13,
    // Result names the existing node Object refers to. The node must live
    // directly inside the target graph.
    BML_BEHAVIOR_EDIT_USE_NODE = 14,
    // Result names the existing behavior link Object refers to.
    BML_BEHAVIOR_EDIT_USE_LINK = 15,
    // Inserts Hook inside the link named by Target, so the callback runs
    // before the node that link feeds. The Hook Block is infrastructure and
    // does not appear in the Logical view of the graph.
    BML_BEHAVIOR_EDIT_BEFORE = 16,
    // Sends the link named by Target to Sink instead of its own destination.
    // Unlike a splice the original destination is dropped while the patch is
    // open, so the Logical view of the graph reports the new one. Only one
    // patch at a time may redirect one link.
    BML_BEHAVIOR_EDIT_REDIRECT = 17,
    // Result names a CKParameterOperation owned by the target graph. Its
    // inputs and result are addressed as Pin 0, Pin 1, and Pout 0 on Result.
    BML_BEHAVIOR_EDIT_ADD_OPERATION = 18,
    // Replaces the existing child Node named by Target with the configured
    // Block. Result names the replacement. The two Nodes must expose the same
    // public control and parameter interface; private Settings and Locals are
    // owned by their respective Blocks and are not copied.
    BML_BEHAVIOR_EDIT_REPLACE_BLOCK = 19,
    // Removes the existing child Node named by Target, together with every
    // behavior link entering or leaving it. The Patch keeps the exact native
    // objects, disconnects the Links while they are parked, and restores their
    // original endpoints when it closes; it does not destroy the Node.
    // The Node, its control ports, and every incident Link source must be idle.
    // A Link with an in-flight activation delay cannot be removed.
    BML_BEHAVIOR_EDIT_REMOVE_NODE = 20,
    // Result names a graph scope rooted at the graph-backed Node in Target.
    BML_BEHAVIOR_EDIT_ENTER_GRAPH = 21,
    // Result names a new graph-backed child Node in the current graph scope.
    BML_BEHAVIOR_EDIT_ADD_GRAPH = 22,
    // Adds an exact port-count condition to the Node Pattern named by Target.
    // SlotKind selects the port family and Delay carries the non-negative
    // count. The step defines no handle.
    BML_BEHAVIOR_EDIT_PATTERN_PORT_COUNT = 23,
    // Adds an observed-value condition to the Node Pattern owning Sink.
    // Value is compared through the port's registered Virtools value form.
    // The step defines no handle.
    BML_BEHAVIOR_EDIT_PATTERN_PORT_VALUE = 24,
    // Result names the Node reached by the unique Link leaving Source. When
    // this step carries a Node Pattern, only Links whose destination Node
    // matches that Pattern participate in the uniqueness check.
    BML_BEHAVIOR_EDIT_NEXT_NODE = 25,
    // Result names the Node feeding the unique Link entering Sink. When this
    // step carries a Node Pattern, only Links whose source Node matches that
    // Pattern participate in the uniqueness check.
    BML_BEHAVIOR_EDIT_PREVIOUS_NODE = 26,
    // Result names the unique Link leaving Source.
    BML_BEHAVIOR_EDIT_LEAVING_LINK = 27,
    // Result names the unique Link entering Sink.
    BML_BEHAVIOR_EDIT_ENTERING_LINK = 28,
    // Result names the unique Link leaving Source and entering any In of the
    // Node named by Target.
    BML_BEHAVIOR_EDIT_LINK_TO_NODE = 29,
    // Sends the Link named by Target to the same destination as the Link
    // named by Node. The destination Link itself is not changed.
    BML_BEHAVIOR_EDIT_REDIRECT_TO_LINK = 30,
    // Result names every child Node matching the Pattern, in native child
    // index order. At least one Node must match. Actions using one of its
    // ports are repeated for every matched Node.
    BML_BEHAVIOR_EDIT_EACH_NODE = 31
} BML_BehaviorEditKind;

typedef enum BML_BehaviorEditFlags {
    // BML_BEHAVIOR_EDIT_REQUIRE_LINK matches Delay as well as its endpoints.
    BML_BEHAVIOR_EDIT_HAS_DELAY = 1u << 0,
    // BML_BEHAVIOR_EDIT_FLOW may close a same-frame cycle. Without this the
    // edit is rejected instead.
    BML_BEHAVIOR_EDIT_CONFIRM_CYCLE = 1u << 1,
} BML_BehaviorEditFlags;

// One step of an edit program. Only the fields its Kind documents are read,
// and the whole program is validated before any of it reaches a live graph.
typedef struct BML_BehaviorEditStep {
    uint32_t StructSize;
    uint32_t Kind;
    // Graph scope containing this step. BML_BEHAVIOR_EDIT_GRAPH is the root.
    uint32_t Graph;
    // The handle this step defines, or zero for a step that defines none.
    uint32_t Result;
    uint32_t Flags;
    // The node, link, or path this step reads.
    uint32_t Target;
    // The Block a splice routes through, or the destination Link used by
    // REDIRECT_TO_LINK.
    uint32_t Node;
    // BML_BehaviorSlotKind of an appended slot.
    uint32_t SlotKind;
    int32_t Delay;
    // Priority of a graph-backed Node created by ADD_GRAPH.
    int32_t Priority;
    // Node name to require, or the name of an appended slot.
    BML_BehaviorString Name;
    // Node selector used by REQUIRE_NODE. Name above remains the interface or
    // graph name used by other step kinds.
    BML_BehaviorSelector Selector;
    // Prototype matched by REQUIRE_NODE.
    BML_BehaviorPrototypeRef Prototype;
    // Optional structural facts copied by Require(snapshotNode).
    uint32_t ExpectedKind;
    uint32_t ReservedShape;
    uint64_t PortShape;
    // Complete configured Block created by ADD_BLOCK. Its Prototype provider
    // generation must already be resolved. The Loader deep-copies the Block
    // with the edit program.
    const BML_BehaviorBlock *Block;
    // Virtools parameter type of an appended Pin, Pout, or Local.
    BML_BehaviorGuid Type;
    BML_BehaviorPortRef Source;
    BML_BehaviorPortRef Sink;
    BML_BehaviorValue Value;
    const BML_BehaviorHookFunction *Hook;
    const BML_BehaviorEditOrder *Ordering;
    uint32_t OrderCount;
    uint32_t Reserved;
    // The node or link a USE step names. Identity is resolved once, against
    // the graph this program is applied to, so a Plan cannot carry it across worlds.
    BML_ObjectRef Object;
    // Concrete native operation overload created by ADD_OPERATION.
    BML_BehaviorOperationSpec Operation;
} BML_BehaviorEditStep;

// One exact live Graph and its owned Edit program. Fingerprint is the logical
// snapshot fingerprint observed by the author; Apply rejects a changed Graph
// instead of compiling the Edit against a different topology.
typedef struct BML_BehaviorGraphEdit {
    uint32_t StructSize;
    uint32_t Reserved;
    BML_ObjectRef Graph;
    uint64_t Fingerprint;
    const BML_BehaviorEditStep *Steps;
    uint32_t StepCount;
    // Added to Result handles when ResolvePatchNode addresses this Edit.
    // Handle ranges must not overlap within one Patch.
    uint32_t HandleBase;
} BML_BehaviorGraphEdit;

// One Script rule and its owned Edit program.
typedef struct BML_BehaviorScriptEdit {
    uint32_t StructSize;
    uint32_t Targets;
    BML_BehaviorString Script;
    const BML_BehaviorEditStep *Steps;
    uint32_t StepCount;
    uint32_t Reserved;
} BML_BehaviorScriptEdit;

typedef struct BML_BehaviorPlanSpec {
    uint32_t StructSize;
    uint32_t Reserved;
    // Names this Plan within the owner of the Session. Submitting a name that
    // is already live replaces the Plan carrying it.
    BML_BehaviorString Name;
    const BML_BehaviorScriptEdit *Edits;
    uint32_t EditCount;
    uint32_t Reserved2;
} BML_BehaviorPlanSpec;

// Applies one edit program to this exact live graph. The graph is resolved and
// the entire program is compiled before native mutation begins. Name identifies
// the Patch within the Session owner for ordering and conflict detection.
typedef struct BML_BehaviorPatchSpec {
    uint32_t StructSize;
    uint32_t Reserved;
    BML_BehaviorString Name;
    const BML_BehaviorGraphEdit *Edits;
    uint32_t EditCount;
    uint32_t Reserved2;
} BML_BehaviorPatchSpec;

typedef struct BML_BehaviorInterface {
    BML_InterfaceHeader Header;

    // Except for the six Close functions, every function in this Interface
    // must be called on the game thread. CloseSession, CloseRun, CloseWatch,
    // ClosePlan, ClosePatch, and CloseScript may be called from any thread.
    // They close new
    // admission immediately and never wait for a running callback or Execute;
    // native teardown and callback Release finish at a later game-thread safe
    // point. Repeating CloseSession, CloseRun, or CloseWatch with the same
    // non-null stale handle is harmless and returns BML_OK.
    //
    // OpenSession authenticates the calling Native Mod from the DLL containing
    // the call site. An empty ownerId selects that Mod. A non-empty ownerId must
    // exactly match it and cannot be used to act for another Mod generation.
    // Every successful Call, Start, or Spawn returns a Run that owns one
    // native Behavior instance until CloseRun, session/owner retirement, or
    // world reset. Run state and TakeFrames never imply native teardown.
    int (BML_BEHAVIOR_CALL *OpenSession)(BML_BehaviorString ownerId,
                                          BML_BehaviorSession *outSession,
                                          BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *CloseSession)(BML_BehaviorSession session);
    int (BML_BEHAVIOR_CALL *Call)(BML_BehaviorSession session,
                                  BML_ObjectRef owner,
                                  const BML_BehaviorBlock *block,
                                  const BML_BehaviorFramePolicy *frames,
                                  const BML_BehaviorSelector *input,
                                  BML_BehaviorRun *outRun,
                                  BML_BehaviorRunInfo *info,
                                  BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *Start)(BML_BehaviorSession session,
                                   BML_ObjectRef owner,
                                   const BML_BehaviorBlock *block,
                                   const BML_BehaviorFramePolicy *frames,
                                   const BML_BehaviorSelector *input,
                                   BML_BehaviorRun *outRun,
                                   BML_BehaviorRunInfo *info,
                                   BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *Spawn)(BML_BehaviorSession session,
                                   BML_ObjectRef owner,
                                   const BML_BehaviorBlock *block,
                                   const BML_BehaviorFramePolicy *frames,
                                   BML_BehaviorRun *outRun,
                                   BML_BehaviorRunInfo *info,
                                   BML_BehaviorStatus *status);
    // Continue accepts only a pending Call. Once accepted, the same Run becomes
    // a managed Task; ReadRun subsequently reports BML_BEHAVIOR_RUN_TASK.
    int (BML_BEHAVIOR_CALL *Continue)(BML_BehaviorRun run,
                                      BML_BehaviorRunInfo *info,
                                      BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *Pulse)(BML_BehaviorRun run,
                                   const BML_BehaviorSelector *input,
                                   uint32_t *admission,
                                   BML_BehaviorRunInfo *info,
                                   BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReadRun)(BML_BehaviorRun run,
                                     BML_BehaviorRunInfo *info,
                                     BML_BehaviorStatus *status);
    // TakeFrames is all-or-nothing and consuming only on success. The first call
    // may pass zero capacities to obtain the complete required counts and size;
    // BML_ERROR_BUFFER_TOO_SMALL writes neither records nor payload and consumes
    // nothing. A successful call writes complete Frames in Sequence order and
    // atomically consumes exactly that batch.
    int (BML_BEHAVIOR_CALL *TakeFrames)(BML_BehaviorRun run,
                                        BML_BehaviorRunFrame *headers,
                                        uint32_t headerCapacity,
                                        uint32_t headerStride,
                                        void *payload,
                                        uint32_t payloadCapacity,
                                        uint32_t *outHeaderCount,
                                        uint32_t *outPayloadSize,
                                        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *CloseRun)(BML_BehaviorRun run);
    // Discovery and Layout reads use an all-or-nothing caller-buffer protocol.
    // On BML_ERROR_BUFFER_TOO_SMALL they report the complete required counts
    // and payload size without writing a partial record or payload.
    int (BML_BEHAVIOR_CALL *FindPrototypes)(
        BML_BehaviorSession session,
        const BML_BehaviorPrototypeQuery *query,
        BML_BehaviorPrototypeInfo *prototypes,
        uint32_t prototypeCapacity,
        uint32_t prototypeStride,
        void *payload,
        uint32_t payloadCapacity,
        uint32_t *outPrototypeCount,
        uint32_t *outPayloadSize,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReadDeclaredLayout)(
        BML_BehaviorSession session,
        const BML_BehaviorPrototypeRef *prototype,
        BML_BehaviorLayout *layout,
        void *payload,
        uint32_t payloadCapacity,
        uint32_t *outPayloadSize,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReadLiveLayout)(
        BML_BehaviorRun run,
        BML_BehaviorLayout *layout,
        void *payload,
        uint32_t payloadCapacity,
        uint32_t *outPayloadSize,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *Inspect)(
        BML_BehaviorSession session,
        BML_ObjectRef root,
        uint32_t view,
        BML_BehaviorGraph *graph,
        void *payload,
        uint32_t payloadCapacity,
        uint32_t *outPayloadSize,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReadNodeLayout)(
        BML_BehaviorSession session,
        BML_ObjectRef node,
        BML_BehaviorLayout *layout,
        void *payload,
        uint32_t payloadCapacity,
        uint32_t *outPayloadSize,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReadValue)(
        BML_BehaviorSession session,
        BML_ObjectRef node,
        uint32_t slotKind,
        uint64_t layoutGeneration,
        const BML_BehaviorSelector *slot,
        uint32_t read,
        BML_BehaviorGraphValue *value,
        void *payload,
        uint32_t payloadCapacity,
        uint32_t *outPayloadSize,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *Watch)(
        BML_BehaviorSession session,
        const BML_BehaviorWatchSpec *spec,
        const BML_BehaviorWatchFunction *callback,
        BML_BehaviorWatch *outWatch,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *CloseWatch)(BML_BehaviorWatch watch);
    // A Plan is submitted once and reconciled by the Loader from then on.
    // SubmitPlan validates the whole edit program before accepting it, so a
    // Plan that was accepted is well formed even while it is Unsatisfied.
    int (BML_BEHAVIOR_CALL *SubmitPlan)(BML_BehaviorSession session,
                                        const BML_BehaviorPlanSpec *spec,
                                        BML_BehaviorPlan *outPlan,
                                        BML_BehaviorPlanInfo *info,
                                        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReadPlan)(BML_BehaviorSession session,
                                      BML_BehaviorPlan plan,
                                      BML_BehaviorPlanInfo *info,
                                      BML_BehaviorStatus *status);
    // May be called from any thread. Retires every installation the Plan still
    // owns. BML_ERROR_BUSY means an inverse is waiting for the next safe point.
    // A graph conflict returns an error and leaves the Plan readable. Once
    // ClosePlan has been accepted, the Loader retries unfinished inverses at
    // safe points.
    int (BML_BEHAVIOR_CALL *ClosePlan)(BML_BehaviorSession session,
                                       BML_BehaviorPlan plan);
    // Reads the native Behavior owned by a Run as a graph. This avoids a
    // separate Scene lookup and names the same instance used by ReadLiveLayout,
    // Pulse, and TakeFrames.
    int (BML_BEHAVIOR_CALL *InspectRun)(
        BML_BehaviorRun run,
        uint32_t view,
        BML_BehaviorGraph *graph,
        void *payload,
        uint32_t payloadCapacity,
        uint32_t *outPayloadSize,
        BML_BehaviorStatus *status);
    // Set writes a Pin or Local. Bind connects a Pin to a live Virtools
    // parameter using direct-source or shared-source semantics.
    int (BML_BEHAVIOR_CALL *Set)(
        BML_BehaviorRun run,
        const BML_BehaviorSlotRef *slot,
        const BML_BehaviorValue *value,
        uint64_t *outLayoutGeneration,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *Bind)(
        BML_BehaviorRun run,
        const BML_BehaviorSlotRef *slot,
        const BML_BehaviorValueRef *source,
        uint32_t relation,
        uint64_t *outLayoutGeneration,
        BML_BehaviorStatus *status);
    // Each non-empty stage is written against the current Layout and followed
    // by one CKM_BEHAVIORSETTINGSEDITED. Existing Target, Pin, and Local
    // bindings are then restored against the resulting Layout. Once native
    // mutation begins, any failure makes the Run FAILED and closes further
    // mutation and execution admission while preserving the first Status.
    int (BML_BEHAVIOR_CALL *Configure)(
        BML_BehaviorRun run,
        const BML_BehaviorSettingStage *stages,
        uint32_t stageCount,
        uint64_t *outLayoutGeneration,
        BML_BehaviorStatus *status);
    // A Patch targets one live graph and is never reconciled against later
    // worlds. It uses the same edit program as a Plan.
    int (BML_BEHAVIOR_CALL *ApplyPatch)(
        BML_BehaviorSession session,
        const BML_BehaviorPatchSpec *spec,
        BML_BehaviorPatch *outPatch,
        BML_BehaviorPatchInfo *info,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReadPatch)(
        BML_BehaviorSession session,
        BML_BehaviorPatch patch,
        BML_BehaviorPatchInfo *info,
        BML_BehaviorStatus *status);
    // May be called from any thread. Stops callback admission immediately.
    // BML_ERROR_BUSY means restoration will continue at the next game-thread
    // safe point. A revert conflict returns an error and keeps the Patch
    // readable; the Loader also retries the requested retirement at later safe
    // points.
    int (BML_BEHAVIOR_CALL *ClosePatch)(BML_BehaviorSession session,
                                        BML_BehaviorPatch patch);
    // Reads the retained state of a live Watch. A callback ERROR or exception
    // makes the Watch FAILED, closes further callback admission, and remains
    // observable here until CloseWatch retires the handle.
    int (BML_BEHAVIOR_CALL *ReadWatch)(BML_BehaviorWatch watch,
                                       BML_BehaviorWatchInfo *info,
                                       BML_BehaviorStatus *status);
    // Issues a reference for a CK object the caller already holds, named by
    // its CK_ID. This is how a Mod that received a CKBehavior from the game
    // enters the Behavior interface without searching for it by name. An
    // unknown or already destroyed ID is BML_ERROR_NOT_FOUND.
    int (BML_BEHAVIOR_CALL *Reference)(BML_BehaviorSession session,
                                       uint32_t object,
                                       BML_ObjectRef *outReference,
                                       BML_BehaviorStatus *status);
    // Reads back the live node an applied edit program named, by the handle
    // the program gave it. BML_ERROR_BUSY means the Patch has not reached its
    // safe point yet, so no live node exists to name.
    int (BML_BEHAVIOR_CALL *ResolvePatchNode)(BML_BehaviorSession session,
                                              BML_BehaviorPatch patch,
                                              uint32_t handle,
                                              BML_ObjectRef *outNode,
                                              BML_BehaviorStatus *status);
    // Parks a Block inside the live graph Graph names and returns a Run for
    // it. The graph does not activate a parked Block, so this Run is what
    // drives it: Set writes its Pins, Pulse runs it once, and CloseRun removes
    // it from the graph again. Graph must name a CKBehavior that owns
    // sub-behaviors.
    int (BML_BEHAVIOR_CALL *AttachBlock)(BML_BehaviorSession session,
                                         BML_ObjectRef graph,
                                         const BML_BehaviorBlock *block,
                                         const BML_BehaviorFramePolicy *frames,
                                         BML_BehaviorRun *outRun,
                                         BML_BehaviorRunInfo *info,
                                         BML_BehaviorStatus *status);
    // Creates an inactive top-level Virtools Script and applies its initial
    // graph as one admission. Owner must be a live CKBeObject in the current
    // Scene. AddScript establishes both owner and scene membership. Failure
    // returns no Script handle and retires the unpublished root; success
    // leaves the initial graph owned by the Script until CloseScript,
    // Session/Mod retirement, external deletion, or world reset.
    int (BML_BEHAVIOR_CALL *CreateScript)(
        BML_BehaviorSession session,
        const BML_BehaviorScriptSpec *spec,
        BML_BehaviorScript *outScript,
        BML_BehaviorScriptInfo *info,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReadScript)(
        BML_BehaviorSession session,
        BML_BehaviorScript script,
        BML_BehaviorScriptInfo *info,
        BML_BehaviorStatus *status);
    // Activity changes are admitted immediately and applied at the next
    // game-thread Behavior safe point. The last request before that point wins.
    int (BML_BEHAVIOR_CALL *SetScriptActive)(
        BML_BehaviorSession session,
        BML_BehaviorScript script,
        uint32_t active,
        uint32_t reset,
        BML_BehaviorScriptInfo *info,
        BML_BehaviorStatus *status);
    // BML_ERROR_BUSY means teardown was admitted and will finish at a later
    // safe point. Repeating the call after retirement returns BML_OK.
    int (BML_BEHAVIOR_CALL *CloseScript)(BML_BehaviorSession session,
                                         BML_BehaviorScript script);
    // Patch and Plan definitions retain their handle while disabled. The last
    // active-state request made before a Behavior safe point wins.
    int (BML_BEHAVIOR_CALL *SetPatchActive)(
        BML_BehaviorSession session,
        BML_BehaviorPatch patch,
        uint32_t active,
        BML_BehaviorPatchInfo *info,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReplacePatch)(
        BML_BehaviorSession session,
        BML_BehaviorPatch patch,
        const BML_BehaviorGraphEdit *edits,
        uint32_t editCount,
        BML_BehaviorPatchInfo *info,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *SetPlanActive)(
        BML_BehaviorSession session,
        BML_BehaviorPlan plan,
        uint32_t active,
        BML_BehaviorPlanInfo *info,
        BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *ReplacePlan)(
        BML_BehaviorSession session,
        BML_BehaviorPlan plan,
        const BML_BehaviorScriptEdit *edits,
        uint32_t editCount,
        BML_BehaviorPlanInfo *info,
        BML_BehaviorStatus *status);
} BML_BehaviorInterface;

// The complete pre-release function table for bml.behavior 1.0. Use
// BML_IFACE_HAS on a function a later minor appends.
#define BML_BEHAVIOR_INTERFACE_1_0_SIZE                                      \
    (offsetof(BML_BehaviorInterface, ReplacePlan) +                           \
     sizeof(((BML_BehaviorInterface *) 0)->ReplacePlan))

// The single capability checkpoint for the complete 1.0 surface. A Mod may
// accept a later minor when this is true, then probe later additions with
// BML_IFACE_HAS before calling them.
#define BML_BEHAVIOR_HAS_1_0(iface)                                          \
    BML_IFACE_HAS((iface), BML_BehaviorInterface, ReplacePlan)

#pragma pack(pop)

BML_END_CDECLS

#endif // BML_BEHAVIOR_H
