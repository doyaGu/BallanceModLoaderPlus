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
#define BML_BEHAVIOR_INTERFACE_MINOR 5u
#define BML_BEHAVIOR_STATUS_MESSAGE_CAPACITY 256u

BML_BEGIN_CDECLS

typedef struct BML_BehaviorSession__ *BML_BehaviorSession;
typedef struct BML_BehaviorRun__ *BML_BehaviorRun;
typedef struct BML_BehaviorWatch__ *BML_BehaviorWatch;
typedef struct BML_BehaviorPlan__ *BML_BehaviorPlan;

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
    // No native or queued continuation is waiting. The Run still owns its
    // native Behavior instance and any state established by that instance.
    BML_BEHAVIOR_RUN_READY = 1,
    BML_BEHAVIOR_RUN_PENDING = 2,
    // Execution cannot continue. This does not release the native instance;
    // CloseRun or the enclosing ownership boundary performs teardown.
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
    BML_BehaviorStatus Status;
} BML_BehaviorRunInfo;

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
// to TakeFrames. Records and value bytes are naturally aligned within it.
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

// A Plan is durable authoring intent. It names one script by its exact name
// and a symbolic edit to apply to every live installation of that script. The
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
    // An installation could not be reverted. The Plan keeps its Hook state
    // alive because the game graph still refers to it.
    BML_BEHAVIOR_PLAN_CONFLICTED = 4,
    BML_BEHAVIOR_PLAN_RETIRING = 5
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

// Return codes an author callback may report. Any other value is reported to
// Virtools as a behavior error, which stops the enclosing chain.
typedef enum BML_BehaviorHookResult {
    BML_BEHAVIOR_HOOK_OK = 0,
    // Keep the Hook Block active for one more frame. Its Outs still activate.
    BML_BEHAVIOR_HOOK_AGAIN_NEXT_FRAME = 1
} BML_BehaviorHookResult;

typedef void (BML_BEHAVIOR_CALL *BML_BehaviorHookRetain)(void *state);
typedef void (BML_BEHAVIOR_CALL *BML_BehaviorHookRelease)(void *state);
typedef int (BML_BEHAVIOR_CALL *BML_BehaviorHookCallback)(
    void *state, const BML_BehaviorHookContext *context);

// One author callback occurrence in a Plan. Retain runs once while SubmitPlan
// accepts the Hook, so the caller may drop its own reference as soon as the
// call returns, and Release runs once the Loader has retired the occurrence and
// dropped every installation of it, which keeps State alive for a Conflicted
// Plan. A rejected Plan retains nothing it has not already released. Invoke
// runs on the game thread inside the behavior execution the game itself drives,
// and must not close the Plan or its Session.
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

// Steps name each other through caller-assigned handles in one namespace per
// Plan. Handle 1 always denotes the matched script itself; every other handle
// must be defined by an earlier step before a later step reads it.
#define BML_BEHAVIOR_EDIT_SCRIPT 1u

// A port of a node. Kind is a BML_BehaviorSlotKind naming which interface of
// Handle to address. A zero Kind means Handle is itself a port defined by an
// earlier BML_BEHAVIOR_EDIT_APPEND_SLOT, which is the only way to address an
// appended slot: an appended slot has no author-visible index until the edit
// is compiled against a live graph.
typedef struct BML_BehaviorPortRef {
    uint32_t StructSize;
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
    // Result names a slot appended to Target, of SlotKind, called Name.
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
    BML_BEHAVIOR_EDIT_SPLICE = 13
} BML_BehaviorEditKind;

typedef enum BML_BehaviorEditFlags {
    // BML_BEHAVIOR_EDIT_REQUIRE_LINK matches Delay as well as its endpoints.
    BML_BEHAVIOR_EDIT_HAS_DELAY = 1u << 0,
    // BML_BEHAVIOR_EDIT_FLOW may close a same-frame cycle. Without this the
    // edit is rejected instead.
    BML_BEHAVIOR_EDIT_CONFIRM_CYCLE = 1u << 1
} BML_BehaviorEditFlags;

// One step of an edit program. Only the fields its Kind documents are read,
// and the whole program is validated before any of it reaches a live graph.
typedef struct BML_BehaviorEditStep {
    uint32_t StructSize;
    uint32_t Kind;
    // The handle this step defines, or zero for a step that defines none.
    uint32_t Result;
    uint32_t Flags;
    // The node, link, or path this step reads.
    uint32_t Target;
    // The Block a splice routes through.
    uint32_t Node;
    // BML_BehaviorSlotKind of an appended slot.
    uint32_t SlotKind;
    int32_t Delay;
    // Node name to require, or the name of an appended slot.
    BML_BehaviorString Name;
    // Prototype to require or to create.
    BML_BehaviorGuid Prototype;
    // Virtools parameter type of an appended Pin, Pout, or Local.
    BML_BehaviorGuid Type;
    BML_BehaviorPortRef Source;
    BML_BehaviorPortRef Sink;
    BML_BehaviorValue Value;
    const BML_BehaviorHookFunction *Hook;
    const BML_BehaviorEditOrder *Ordering;
    uint32_t OrderCount;
    uint32_t Reserved;
} BML_BehaviorEditStep;

typedef struct BML_BehaviorPlanSpec {
    uint32_t StructSize;
    uint32_t Targets;
    // Names this Plan within the owner of the Session. Submitting a name that
    // is already live replaces the Plan carrying it.
    BML_BehaviorString Name;
    // The exact script name to match.
    BML_BehaviorString Script;
    const BML_BehaviorEditStep *Steps;
    uint32_t StepCount;
    uint32_t Reserved;
} BML_BehaviorPlanSpec;

typedef struct BML_BehaviorInterface {
    BML_InterfaceHeader Header;

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
    // Reverts every installation the Plan still owns. A revert the game graph
    // no longer permits leaves the Plan Conflicted rather than failing here.
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
} BML_BehaviorInterface;

#pragma pack(pop)

BML_END_CDECLS

#endif // BML_BEHAVIOR_H
