#ifndef BML_BEHAVIOR_RUNTIME_H
#define BML_BEHAVIOR_RUNTIME_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "CKAll.h"
#include "Behavior/Callback.h"
#include "Behavior/Execution.h"
#include "Behavior/Lifecycle.h"

namespace BML::Behavior {

enum class SlotKind {
    Input,
    Output,
    InputParameter,
    OutputParameter,
    // Virtools stores settings in the same native local-parameter array as
    // ordinary locals. Setting exposes the filtered settings view, while
    // Local exposes only non-setting entries and retains their native index.
    Setting,
    Local,
    Target,
};

struct Slot {
    SlotKind Kind = SlotKind::InputParameter;
    int Index = -1;
    std::string Name;
    int Occurrence = 0;
    bool RequireUnique = false;
    CKGUID ExpectedType;

    static Slot At(SlotKind kind, int index, CKGUID expectedType = CKGUID());
    static Slot Named(SlotKind kind, std::string name,
                      CKGUID expectedType = CKGUID());
    static Slot OccurrenceOf(SlotKind kind, std::string name, int occurrence,
                             CKGUID expectedType = CKGUID());
    [[nodiscard]] bool UsesName() const noexcept { return !Name.empty(); }
};

struct SlotInfo {
    SlotKind Kind = SlotKind::InputParameter;
    int Index = -1;
    int NativeIndex = -1;
    std::string Name;
    CKGUID Type;
    int DataSize = 0;
};

struct Layout {
    CKGUID Prototype = CKGUID();
    std::string PrototypeName;
    std::string Category;
    CK_CLASSID CompatibleClass = CKCID_BEOBJECT;
    CKDWORD PrototypeFlags = 0;
    CKDWORD BehaviorFlags = 0;
    std::uint64_t Generation = 0;
    std::vector<CKGUID> RequiredManagers;
    std::vector<SlotInfo> Slots;
};

// A resolved slot is only valid for one configured live instance layout. The
// instance id and layout generation keep that lifetime local to Runtime;
// Object is compared with the live slot and is never dereferenced by itself.
struct SlotRef {
    std::uint64_t InstanceId = 0;
    std::uint64_t LayoutGeneration = 0;
    CKObject *Object = nullptr;
    SlotInfo Slot;
};

enum class ValueKind {
    Raw,
    Text,
    Object,
    Snapshot,
    DirectSource,
    SharedSource,
};

class Value {
public:
    static Value Raw(CKGUID type, const void *data, std::size_t size);
    static Value UntypedRaw(const void *data, std::size_t size);
    static Value Text(CKGUID type, std::string text);
    static Value String(std::string text);
    static Value Object(CKGUID type, CKObject *object);
    static Value Snapshot(CKParameter *source);
    static Value DirectSource(CKParameter *source);
    static Value SharedSource(CKParameterIn *source);

    template <typename T>
    static Value From(CKGUID type, const T &value) {
        return Raw(type, &value, sizeof(T));
    }

    [[nodiscard]] ValueKind Kind() const noexcept { return m_Kind; }
    [[nodiscard]] CKGUID Type() const noexcept { return m_Type; }
    [[nodiscard]] const std::vector<std::byte> &Bytes() const noexcept { return m_Bytes; }
    [[nodiscard]] const std::string &StringValue() const noexcept { return m_Text; }
    [[nodiscard]] CKObject *ObjectValue() const noexcept { return m_Object; }
    [[nodiscard]] CK_ID ObjectId() const noexcept { return m_ObjectId; }
    [[nodiscard]] CKParameter *ParameterSource() const noexcept { return m_Source; }
    [[nodiscard]] CK_ID ParameterSourceId() const noexcept { return m_SourceId; }
    [[nodiscard]] CKParameterIn *SharedParameterSource() const noexcept { return m_SharedSource; }
    [[nodiscard]] CK_ID SharedParameterSourceId() const noexcept { return m_SharedSourceId; }

private:
    ValueKind m_Kind = ValueKind::Raw;
    CKGUID m_Type = CKGUID();
    std::vector<std::byte> m_Bytes;
    std::string m_Text;
    CKObject *m_Object = nullptr;
    CK_ID m_ObjectId = 0;
    CKParameter *m_Source = nullptr;
    CK_ID m_SourceId = 0;
    CKParameterIn *m_SharedSource = nullptr;
    CK_ID m_SharedSourceId = 0;
};

class Operation {
public:
    explicit Operation(CKGUID operation = CKGUID()) : m_Operation(operation) {}

    Operation &Result(CKGUID type);
    Operation &Input1(Value value);
    Operation &Input2(Value value);

    [[nodiscard]] CKGUID Guid() const noexcept { return m_Operation; }
    [[nodiscard]] CKGUID ResultType() const noexcept { return m_ResultType; }

private:
    CKGUID m_Operation = CKGUID();
    CKGUID m_ResultType = CKGUID();
    Value m_Input1;
    Value m_Input2;
    bool m_HasInput1 = false;
    bool m_HasInput2 = false;

    friend class Runtime;
};

enum class TargetMode {
    Owner,
    Explicit,
    ExplicitNull,
};

class Spec {
public:
    explicit Spec(CKGUID prototype = CKGUID()) : m_Prototype(prototype) { m_SettingStages.emplace_back(); }

    Spec &TargetOwner();
    Spec &Target(CKGUID type, CKObject *object);
    Spec &NullTarget(CKGUID type);
    Spec &TargetSource(CKGUID type, CKParameter *source);
    Spec &TargetShared(CKGUID type, CKParameterIn *source);
    Spec &Setting(Slot slot, Value value);
    Spec &RefreshLayout();
    Spec &Input(Slot slot, Value value);
    Spec &Input(Slot slot, Operation operation);
    Spec &Local(Slot slot, Value value);
    Spec &AddInput(std::string name);
    Spec &AddOutput(std::string name);
    Spec &Outcomes(OutcomeRetention retention);
    Spec &KeepAlive(std::shared_ptr<CallbackResource> resource);

    [[nodiscard]] CKGUID Prototype() const noexcept { return m_Prototype; }

private:
    struct Binding {
        Slot Target;
        Value Source;
    };

    struct OperationBinding {
        Slot Target;
        Operation Definition;
    };

    CKGUID m_Prototype = CKGUID();
    TargetMode m_TargetMode = TargetMode::Owner;
    CKGUID m_TargetType = CKGUID();
    Value m_TargetValue;
    std::vector<std::vector<Binding>> m_SettingStages;
    std::vector<Binding> m_Inputs;
    std::vector<OperationBinding> m_Operations;
    std::vector<Binding> m_Locals;
    std::vector<std::string> m_AddedInputs;
    std::vector<std::string> m_AddedOutputs;
    std::vector<std::shared_ptr<CallbackResource>> m_KeepAlive;
    OutcomeRetention m_OutcomeRetention = OutcomeRetention::Signals();

    friend class Runtime;
};

enum class Error {
    None,
    WrongThread,
    ContextExpired,
    PrototypeNotFound,
    RequiredManagerMissing,
    CreateFailed,
    InitFailed,
    OwnerInvalid,
    TargetInvalid,
    CallbackFailed,
    SlotNotFound,
    AmbiguousSlot,
    StaleLayout,
    TypeMismatch,
    ValueWriteFailed,
    SourceInvalid,
    InvalidState,
    ExecutionFailed,
    OperationInvalid,
    UnsupportedBreak,
    OutcomeQueueFull,
    ExecutionCancelled,
};

enum class Phase {
    None,
    PrototypeResolution,
    ManagerValidation,
    Creation,
    Initialization,
    StaticLayout,
    OwnerBinding,
    TargetBinding,
    Settings,
    LifecycleCallback,
    ParameterBinding,
    Execution,
    Teardown,
};

struct Diagnostic {
    Diagnostic()
        : Prototype(), RequiredManager(), Selector(), ActualType(), OperationGuid() {}

    Phase Stage = Phase::None;
    CKGUID Prototype = CKGUID();
    CKGUID RequiredManager = CKGUID();
    Slot Selector;
    CKGUID ActualType = CKGUID();
    CKGUID OperationGuid = CKGUID();
    CKDWORD CallbackMessage = 0;
};

struct Status {
    Status() = default;
    Status(Error error, CKERROR ckError, int behaviorResult,
           std::string message)
        : Code(error), CkError(ckError), BehaviorResult(behaviorResult),
          Message(std::move(message)) {}

    Error Code = Error::None;
    CKERROR CkError = CK_OK;
    int BehaviorResult = CKBR_OK;
    std::string Message;
    Diagnostic Details;

    explicit operator bool() const noexcept { return Code == Error::None; }
};

enum class RunState {
    Completed,
    Pending,
    Queued,
    Failed,
};

struct RunResult {
    Status Outcome;
    RunState State = RunState::Completed;
    int ReturnCode = CKBR_OK;
    std::vector<int> ActiveOutputs;

    explicit operator bool() const noexcept { return static_cast<bool>(Outcome); }
};

class Runtime;

class Instance {
public:
    Instance() = default;
    ~Instance();
    Instance(const Instance &) = delete;
    Instance &operator=(const Instance &) = delete;
    Instance(Instance &&other) noexcept;
    Instance &operator=(Instance &&other) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] CKBehavior *Get() const;
    [[nodiscard]] std::uint64_t LayoutGeneration() const;
    void Reset();

private:
    struct Access {
        std::mutex Mutex;
        Runtime *Owner = nullptr;
    };

    Instance(std::shared_ptr<Access> access, std::uint64_t id)
        : m_Access(std::move(access)), m_Id(id) {}
    std::weak_ptr<Access> m_Access;
    std::uint64_t m_Id = 0;

    friend class Runtime;
};

struct CreateResult {
    Status Outcome;
    Instance Handle;
    Layout Descriptor;

    explicit operator bool() const noexcept { return static_cast<bool>(Outcome); }
};

struct AttachResult {
    Status Outcome;
    CKBehavior *Block = nullptr;
    Layout Descriptor;

    explicit operator bool() const noexcept { return static_cast<bool>(Outcome); }
};

struct CallResult {
    Status Outcome;
    RunResult Run;
    Instance Handle;
    Layout Descriptor;

    explicit operator bool() const noexcept {
        return static_cast<bool>(Outcome) && static_cast<bool>(Run);
    }
};

class Runtime final {
public:
    explicit Runtime(CKContext *context);
    ~Runtime();
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;

    CreateResult Instantiate(CKBeObject *owner, const Spec &spec,
                               const CKBehaviorContext *frame = nullptr);
    CallResult Call(CKBeObject *owner, const Spec &spec, const Slot &input,
                    const CKBehaviorContext *frame = nullptr);
    AttachResult AddToGraph(CKBehavior *parent, const Spec &spec,
                                const CKBehaviorContext *frame = nullptr);

    [[nodiscard]] Layout Describe(CKBehavior *behavior, std::uint64_t generation = 0) const;
    [[nodiscard]] Status Resolve(CKBehavior *behavior, const Slot &selector,
                                         SlotInfo &slot) const;
    [[nodiscard]] Status Resolve(const Instance &instance,
                                         const Slot &selector,
                                         SlotRef &slot) const;
    [[nodiscard]] CKParameter *Parameter(const Instance &instance,
                                         const Slot &selector,
                                         Status *status = nullptr) const;
    [[nodiscard]] CKParameter *Parameter(const Instance &instance,
                                         const SlotRef &slot,
                                         Status *status = nullptr) const;

    Status SetInput(Instance &instance, const Slot &selector,
                            const Value &value);
    Status SetInput(Instance &instance, const SlotRef &slot,
                            const Value &value);
    Status SetLocal(Instance &instance, const Slot &selector,
                            const Value &value);
    Status SetLocal(Instance &instance, const SlotRef &slot,
                            const Value &value);
    // Settings can rebuild arbitrary parts of the live layout.  Reconfigure
    // therefore takes the complete desired spec and reapplies target, locals,
    // and inputs after every settings stage; there is no misleading one-field
    // SetSetting operation.
    Status Reconfigure(Instance &instance, const Spec &spec,
                               const CKBehaviorContext *frame = nullptr);

    RunResult Pulse(Instance &instance, const Slot &input,
                          const CKBehaviorContext *frame = nullptr);
    RunResult Pulse(Instance &instance, const SlotRef &input,
                          const CKBehaviorContext *frame = nullptr);
    RunResult Step(Instance &instance, const CKBehaviorContext *frame = nullptr);
    RunResult StartTask(Instance &instance, const Slot &input,
                              const CKBehaviorContext *frame = nullptr);
    Status Continue(Instance &instance);
    [[nodiscard]] bool IsTaskActive(const Instance &instance) const;
    [[nodiscard]] ExecutionState State(const Instance &instance) const;
    [[nodiscard]] std::vector<ExecutionOutcome> Drain(Instance &instance);
    [[nodiscard]] Status TerminalError(const Instance &instance) const;
    void ProcessTasks(const CKBehaviorContext *frame = nullptr);
    void ProcessFrame();

    void ObjectsToBeDeleted(const CK_ID *ids, int count);
    void ResetWorld();
    Status Close(CKBehavior *behavior);

private:
    struct ObjectStamp {
        CK_ID Id = 0;
        CKObject *Address = nullptr;

        [[nodiscard]] bool operator==(const ObjectStamp &other) const noexcept {
            return Id == other.Id && Address == other.Address;
        }
    };

    struct OwnedOperation {
        ObjectStamp Owner;
        ObjectStamp Operation;
        std::vector<ObjectStamp> Sources;
        bool AddedToOwner = false;
    };

    struct Record {
        ObjectStamp Behavior;
        ObjectStamp Parent;
        std::uint64_t Id = 0;
        std::uint64_t LayoutGeneration = 1;
        bool GraphResident = false;
        bool Expired = false;
        bool Poisoned = false;
        Lifecycle NativeLifecycle;
        Execution Protocol;
        std::vector<ObjectStamp> OwnedSources;
        std::vector<OwnedOperation> OwnedOperations;
        std::vector<std::shared_ptr<CallbackResource>> KeepAlive;
    };

    struct PendingDestroy {
        ObjectStamp Behavior;
        ObjectStamp Parent;
        std::vector<ObjectStamp> Sources;
        std::vector<OwnedOperation> Operations;
        std::vector<ObjectStamp> IgnoredInputs;
        std::vector<std::shared_ptr<CallbackResource>> KeepAlive;
        int Frames = 2;
        bool DestroyBehavior = false;
        bool GraphResident = false;
    };

    struct SharedBindings {
        // Sources and operations may outlive the Runtime that created them
        // when another input in the same CKContext still consumes them.
        std::list<PendingDestroy> Pending;
    };

    enum class DestroyMode {
        Ready,
        Close,
        Reset,
    };

    [[nodiscard]] Status ReadyStatus() const;
    [[nodiscard]] Record *FindRecord(std::uint64_t instanceId);
    [[nodiscard]] const Record *FindRecord(std::uint64_t instanceId) const;
    [[nodiscard]] Record *FindRecord(const Instance &instance);
    [[nodiscard]] const Record *FindRecord(const Instance &instance) const;
    [[nodiscard]] ObjectStamp CaptureObject(CKObject *object) const;
    [[nodiscard]] CKObject *ResolveObject(ObjectStamp object) const;
    [[nodiscard]] CKBehavior *ResolveBehavior(const Record &record) const;
    [[nodiscard]] Status ResolvePrototype(CKGUID guid) const;
    [[nodiscard]] Status CreateBehavior(const Spec &spec,
                                                CKBehavior *&behavior) const;
    [[nodiscard]] CKParameter *ResolveParameter(CKBehavior *behavior, const SlotInfo &slot) const;
    [[nodiscard]] CKObject *ResolveSlotObject(CKBehavior *behavior, const SlotInfo &slot) const;
    [[nodiscard]] Status ValidateSlot(const Record &record,
                                              const SlotRef &slot) const;
    [[nodiscard]] Status ApplyValue(CKParameter *parameter, const Value &value) const;
    [[nodiscard]] Status BindInput(CKBehavior *behavior, Record &record,
                                           const SlotInfo &slot, const Value &value);
    [[nodiscard]] Status BindOperation(CKBehavior *behavior, Record &record,
                                               const SlotInfo &slot,
                                               const Operation &operation);
    [[nodiscard]] Status EnsurePrototypeLayout(CKBehavior *behavior,
                                                       CKBehaviorPrototype *prototype,
                                                       bool afterSettings);
    [[nodiscard]] Status EnsurePrototypeDefaults(CKBehavior *behavior,
                                                         CKBehaviorPrototype *prototype,
                                                         Record &record);
    [[nodiscard]] Status ApplyBindings(CKBehavior *behavior, const Spec &spec,
                                               Record &record);
    [[nodiscard]] Status BindTarget(CKBehavior *behavior, CKBeObject *owner,
                                            const Spec &spec, Record &record);
    [[nodiscard]] int SourceReferenceCount(
        CKParameter *source, CKBehavior *ignoredBehavior = nullptr,
        const std::vector<ObjectStamp> *ignoredInputs = nullptr) const;
    [[nodiscard]] bool IsSourceReferenced(CKParameter *source) const;
    void PruneOwnedSources(CKBehavior *behavior, Record &record);
    void PruneOwnedOperations(CKBehavior *behavior, Record &record);
    void SweepRecords();
    void DetachOperation(OwnedOperation &operation);
    [[nodiscard]] Status Configure(CKBehavior *behavior,
                                           CKBeObject *owner, CKBehavior *parent,
                                           const Spec &spec,
                                           const CKBehaviorContext *frame,
                                           Record &record);
    [[nodiscard]] Status CallCallback(CKBehavior *behavior, CKDWORD message,
                                              const CKBehaviorContext *frame) const;
    class NativeAdapter;
    class NativeLifecycleAdapter;
    [[nodiscard]] RunResult Execute(std::uint64_t instanceId,
                                    const ExecutionInput *input, bool once,
                                    const CKBehaviorContext *frame);
    [[nodiscard]] int ExecuteNative(CKBehavior *behavior, const CKBehaviorContext *frame) const;
    [[nodiscard]] Status Reacquire(std::uint64_t instanceId, CKBehavior *behavior,
                                           Record *&record);
    void RequestRelease(std::uint64_t instanceId);
    void Release(std::uint64_t instanceId);
    void QueueDestroy(Record &record);
    void QueueSourceDestroy(ObjectStamp source, int frames = 2);
    void QueueOperationDestroy(OwnedOperation operation, int frames = 2);
    void DestroyConnectedLinks(CKBehavior *parent, CKBehavior *behavior);
    void DrainDeferredReleases();
    void DrainCloseQueue(bool force = false);
    static void CloseCallbacks(Record &record) noexcept;
    void AdoptSharedBindings();
    void Close();
    void DestroyReady(DestroyMode mode);
    [[nodiscard]] static std::shared_ptr<SharedBindings>
    AcquireSharedBindings(CKContext *context);

    CKContext *m_Context = nullptr;
    std::thread::id m_Thread;
    std::uint64_t m_NextInstanceId = 1;
    std::uint64_t m_Frame = 0;
    std::unordered_map<std::uint64_t, Record> m_Records;
    std::list<PendingDestroy> m_PendingDestroy;
    bool m_Destroying = false;
    bool m_ForceDestroyPending = false;
    std::mutex m_DeferredMutex;
    std::vector<std::uint64_t> m_DeferredReleases;
    std::shared_ptr<Instance::Access> m_Access;
    std::shared_ptr<SharedBindings> m_SharedBindings;
    bool m_ProcessingFrame = false;
    bool m_ProcessingTasks = false;
    std::vector<Record *> m_ConfiguringRecords;

    friend class Instance;
};

const char *DescribeError(Error error);
const char *DescribePhase(Phase phase);

} // namespace BML::Behavior

#endif // BML_BEHAVIOR_RUNTIME_H
