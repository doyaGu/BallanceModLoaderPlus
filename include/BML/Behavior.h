// Native Behavior authoring. This interface uses Virtools' own author-facing
// vocabulary: a Block is created from a Prototype, configured through Settings,
// Pins, Locals, and a Target, and driven through its Ins and Outs. Values passed
// in are borrowed for the duration of a call. Run frames returned by TakeFrames
// are copied into caller-owned buffers and contain no process pointers.
#ifndef BML_BEHAVIOR_H
#define BML_BEHAVIOR_H

#include "BML/Interface.h"
#include "BML/Types.h"

#if defined(_WIN32) && !defined(_WIN64)
#define BML_BEHAVIOR_CALL __cdecl
#else
#define BML_BEHAVIOR_CALL
#endif

#define BML_BEHAVIOR_INTERFACE_ID "bml.behavior"
#define BML_BEHAVIOR_INTERFACE_MAJOR 1u
#define BML_BEHAVIOR_INTERFACE_MINOR 3u
#define BML_BEHAVIOR_STATUS_MESSAGE_CAPACITY 256u

BML_BEGIN_CDECLS

typedef struct BML_BehaviorSession__ *BML_BehaviorSession;
typedef struct BML_BehaviorRun__ *BML_BehaviorRun;
typedef struct BML_BehaviorWatch__ *BML_BehaviorWatch;

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
    BML_BEHAVIOR_FRAMES_SIGNALS = 1,
    BML_BEHAVIOR_FRAMES_EACH_FRAME = 2,
    BML_BEHAVIOR_FRAMES_LATEST = 3,
    BML_BEHAVIOR_FRAMES_NONE = 4
} BML_BehaviorFramePolicyKind;

typedef struct BML_BehaviorFramePolicy {
    uint32_t StructSize;
    uint32_t Kind;
    uint32_t Limit;
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
    BML_BehaviorFramePolicy Frames;
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
    BML_BEHAVIOR_ERROR_OBSERVER_UNAVAILABLE = 26
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
    BML_BEHAVIOR_PHASE_TEARDOWN = 12
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

typedef enum BML_BehaviorRunKind {
    BML_BEHAVIOR_RUN_CALL = 1,
    BML_BEHAVIOR_RUN_TASK = 2,
    BML_BEHAVIOR_RUN_INSTANCE = 3
} BML_BehaviorRunKind;

typedef enum BML_BehaviorRunState {
    BML_BEHAVIOR_RUN_COMPLETED = 1,
    BML_BEHAVIOR_RUN_PENDING = 2,
    BML_BEHAVIOR_RUN_QUEUED = 3,
    BML_BEHAVIOR_RUN_FAILED = 4
} BML_BehaviorRunState;

typedef enum BML_BehaviorRunFlags {
    BML_BEHAVIOR_RUN_UNVERIFIED_DETACHED = 1u << 0
} BML_BehaviorRunFlags;

typedef struct BML_BehaviorRunInfo {
    uint32_t StructSize;
    uint32_t Kind;
    uint32_t State;
    uint32_t Flags;
    BML_BehaviorStatus Status;
} BML_BehaviorRunInfo;

typedef enum BML_BehaviorAdmission {
    BML_BEHAVIOR_ADMISSION_EXECUTED = 1,
    BML_BEHAVIOR_ADMISSION_QUEUED = 2
} BML_BehaviorAdmission;

typedef enum BML_BehaviorContinuation {
    BML_BEHAVIOR_CONTINUATION_NONE = 0,
    BML_BEHAVIOR_CONTINUATION_NATIVE = 1u << 0,
    BML_BEHAVIOR_CONTINUATION_QUEUED_INPUT = 1u << 1,
    BML_BEHAVIOR_CONTINUATION_GRAPH_ACTIVE = 1u << 2
} BML_BehaviorContinuation;

// Every offset below is relative to the first byte of the payload buffer passed
// to TakeFrames. Records and value bytes are naturally aligned within it.
typedef struct BML_BehaviorRunFrame {
    uint32_t StructSize;
    uint64_t Sequence;
    uint64_t Frame;
    int32_t NativeResult;
    uint32_t Continuation;
    uint32_t Terminal;
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

// Prototype discovery names one provider registration, not only a GUID.
// Generation is process-local and never denotes a CK pointer or DLL handle.
typedef struct BML_BehaviorPrototypeRef {
    uint32_t StructSize;
    BML_BehaviorGuid Prototype;
    uint64_t Generation;
} BML_BehaviorPrototypeRef;

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

typedef enum BML_BehaviorPrototypeKind {
    BML_BEHAVIOR_PROTOTYPE_FUNCTION = 1,
    BML_BEHAVIOR_PROTOTYPE_GRAPH = 2
} BML_BehaviorPrototypeKind;

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

typedef enum BML_BehaviorLayoutFlags {
    BML_BEHAVIOR_LAYOUT_MATERIALIZED_NOW = 1u << 0
} BML_BehaviorLayoutFlags;

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
    uint32_t Kind;
    int32_t Index;
    int32_t Occurrence;
    uint32_t Active;
    BML_BehaviorText Name;
} BML_BehaviorGraphPort;

typedef struct BML_BehaviorGraphNode {
    uint32_t StructSize;
    uint64_t Id;
    BML_ObjectRef Object;
    uint64_t Parent;
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

typedef enum BML_BehaviorWatchKind {
    BML_BEHAVIOR_WATCH_GRAPH = 1,
    BML_BEHAVIOR_WATCH_LAYOUT = 2,
    BML_BEHAVIOR_WATCH_SAMPLED_VALUE = 3,
    BML_BEHAVIOR_WATCH_EXACT_VALUE = 4
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

typedef void (BML_BEHAVIOR_CALL *BML_BehaviorWatchRetain)(void *state);
typedef void (BML_BEHAVIOR_CALL *BML_BehaviorWatchRelease)(void *state);
typedef void (BML_BEHAVIOR_CALL *BML_BehaviorWatchCallback)(
    void *state, const BML_BehaviorWatchEvent *event);

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
    BML_BehaviorSelector Slot;
    uint32_t Read;
} BML_BehaviorWatchSpec;

typedef struct BML_BehaviorInterface {
    BML_InterfaceHeader Header;

    int (BML_BEHAVIOR_CALL *OpenSession)(BML_BehaviorString ownerId,
                                          BML_BehaviorSession *outSession,
                                          BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *CloseSession)(BML_BehaviorSession session);
    int (BML_BEHAVIOR_CALL *Call)(BML_BehaviorSession session,
                                  BML_ObjectRef owner,
                                  const BML_BehaviorBlock *block,
                                  const BML_BehaviorSelector *input,
                                  BML_BehaviorRun *outRun,
                                  BML_BehaviorRunInfo *info,
                                  BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *Start)(BML_BehaviorSession session,
                                   BML_ObjectRef owner,
                                   const BML_BehaviorBlock *block,
                                   const BML_BehaviorSelector *input,
                                   BML_BehaviorRun *outRun,
                                   BML_BehaviorRunInfo *info,
                                   BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *Spawn)(BML_BehaviorSession session,
                                   BML_ObjectRef owner,
                                   const BML_BehaviorBlock *block,
                                   BML_BehaviorRun *outRun,
                                   BML_BehaviorRunInfo *info,
                                   BML_BehaviorStatus *status);
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
} BML_BehaviorInterface;

#pragma pack(pop)

BML_END_CDECLS

#endif // BML_BEHAVIOR_H
