// Native Behavior authoring. This interface uses Virtools' own author-facing
// vocabulary: a Block is created from a Prototype, configured through Settings,
// Pins, Locals, and a Target, and driven through its Ins and Outs. Values passed
// in are borrowed for the duration of a call. Outcomes returned by DrainOutcomes
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
#define BML_BEHAVIOR_INTERFACE_MINOR 0u
#define BML_BEHAVIOR_STATUS_MESSAGE_CAPACITY 256u

BML_BEGIN_CDECLS

typedef struct BML_BehaviorSession__ *BML_BehaviorSession;
typedef struct BML_BehaviorRun__ *BML_BehaviorRun;

#pragma pack(push, 8)

typedef struct BML_BehaviorGuid {
    uint32_t Data1;
    uint32_t Data2;
} BML_BehaviorGuid;

typedef struct BML_BehaviorString {
    const char *Data;
    uint32_t Length;
} BML_BehaviorString;

typedef enum BML_BehaviorSelectorKind {
    BML_BEHAVIOR_SELECTOR_INDEX = 1,
    BML_BEHAVIOR_SELECTOR_NAME = 2,
    BML_BEHAVIOR_SELECTOR_UNIQUE_NAME = 3
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

typedef enum BML_BehaviorRetentionKind {
    BML_BEHAVIOR_RETENTION_SIGNALS = 1,
    BML_BEHAVIOR_RETENTION_EACH_FRAME = 2,
    BML_BEHAVIOR_RETENTION_LATEST = 3,
    BML_BEHAVIOR_RETENTION_NONE = 4
} BML_BehaviorRetentionKind;

typedef struct BML_BehaviorRetention {
    uint32_t StructSize;
    uint32_t Kind;
    uint32_t Limit;
} BML_BehaviorRetention;

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
    BML_BehaviorRetention Outcomes;
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
    BML_BEHAVIOR_ERROR_OUTCOME_LIMIT_REACHED = 18,
    BML_BEHAVIOR_ERROR_CANCELLED = 19
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

typedef struct BML_BehaviorRunInfo {
    uint32_t StructSize;
    uint32_t Kind;
    uint32_t State;
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
// to DrainOutcomes. Records and value bytes are naturally aligned within it.
typedef struct BML_BehaviorOutcomeHeader {
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
} BML_BehaviorOutcomeHeader;

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
    int (BML_BEHAVIOR_CALL *DrainOutcomes)(BML_BehaviorRun run,
                                           BML_BehaviorOutcomeHeader *headers,
                                           uint32_t headerCapacity,
                                           uint32_t headerStride,
                                           void *payload,
                                           uint32_t payloadCapacity,
                                           uint32_t *outHeaderCount,
                                           uint32_t *outPayloadSize,
                                           BML_BehaviorStatus *status);
    int (BML_BEHAVIOR_CALL *CloseRun)(BML_BehaviorRun run);
} BML_BehaviorInterface;

#pragma pack(pop)

BML_END_CDECLS

#endif // BML_BEHAVIOR_H
