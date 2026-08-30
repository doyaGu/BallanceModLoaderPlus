#ifndef BML_BEHAVIORRUNTIME_H
#define BML_BEHAVIORRUNTIME_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BML/Types.h"
#include "CKAll.h"

namespace BML {

class CKIdentityRegistry;

namespace Virtools {

enum class BehaviorSlotKind {
    Input,
    Output,
    InputParameter,
    OutputParameter,
    Setting,
    Local,
    Target,
};

struct SlotSelector {
    BehaviorSlotKind Kind = BehaviorSlotKind::InputParameter;
    int Index = -1;
    std::string Name;
    int Occurrence = 0;
    bool RequireUnique = false;
    CKGUID ExpectedType;

    static SlotSelector At(BehaviorSlotKind kind, int index, CKGUID expectedType = CKGUID());
    static SlotSelector Named(BehaviorSlotKind kind, std::string name,
                              CKGUID expectedType = CKGUID());
    static SlotSelector OccurrenceOf(BehaviorSlotKind kind, std::string name, int occurrence,
                                     CKGUID expectedType = CKGUID());
    [[nodiscard]] bool UsesName() const noexcept { return !Name.empty(); }
};

struct BehaviorSlot {
    BehaviorSlotKind Kind = BehaviorSlotKind::InputParameter;
    int Index = -1;
    int NativeIndex = -1;
    std::string Name;
    CKGUID Type;
    int DataSize = 0;
};

struct BehaviorLayout {
    CKGUID Prototype = CKGUID();
    CK_CLASSID CompatibleClass = CKCID_BEOBJECT;
    CKDWORD BehaviorFlags = 0;
    std::uint64_t Generation = 0;
    std::vector<BehaviorSlot> Slots;
};

// A resolved slot is only valid for one configured live instance layout.  It
// deliberately carries both the object identity and the layout generation so
// that a setting callback cannot make a cached ordinal silently name another
// parameter.
struct BehaviorSlotHandle {
    BML_ObjectRef Behavior;
    BML_ObjectRef Object;
    std::uint64_t LayoutGeneration = 0;
    BehaviorSlot Slot;
};

enum class ParameterValueKind {
    Raw,
    Text,
    Object,
    Snapshot,
    DirectSource,
    SharedSource,
};

class ParameterValue {
public:
    static ParameterValue Raw(CKGUID type, const void *data, std::size_t size);
    static ParameterValue UntypedRaw(const void *data, std::size_t size);
    static ParameterValue Text(CKGUID type, std::string text);
    static ParameterValue String(std::string text);
    static ParameterValue Object(CKGUID type, CKObject *object);
    static ParameterValue Snapshot(CKParameter *source);
    static ParameterValue DirectSource(CKParameter *source);
    static ParameterValue SharedSource(CKParameterIn *source);

    template <typename T>
    static ParameterValue Value(CKGUID type, const T &value) {
        return Raw(type, &value, sizeof(T));
    }

    [[nodiscard]] ParameterValueKind Kind() const noexcept { return m_Kind; }
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
    ParameterValueKind m_Kind = ParameterValueKind::Raw;
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

enum class TargetMode {
    Owner,
    Explicit,
    ExplicitNull,
};

class BehaviorSpec {
public:
    explicit BehaviorSpec(CKGUID prototype = CKGUID()) : m_Prototype(prototype) { m_SettingStages.emplace_back(); }

    BehaviorSpec &TargetOwner();
    BehaviorSpec &Target(CKGUID type, CKObject *object);
    BehaviorSpec &NullTarget(CKGUID type);
    BehaviorSpec &TargetSource(CKGUID type, CKParameter *source);
    BehaviorSpec &TargetShared(CKGUID type, CKParameterIn *source);
    BehaviorSpec &Setting(SlotSelector slot, ParameterValue value);
    BehaviorSpec &RefreshLayout();
    BehaviorSpec &Input(SlotSelector slot, ParameterValue value);
    BehaviorSpec &Local(SlotSelector slot, ParameterValue value);
    BehaviorSpec &AddInput(std::string name);
    BehaviorSpec &AddOutput(std::string name);

    [[nodiscard]] CKGUID Prototype() const noexcept { return m_Prototype; }

private:
    struct Binding {
        SlotSelector Slot;
        ParameterValue Value;
    };

    CKGUID m_Prototype = CKGUID();
    TargetMode m_TargetMode = TargetMode::Owner;
    CKGUID m_TargetType = CKGUID();
    ParameterValue m_TargetValue;
    std::vector<std::vector<Binding>> m_SettingStages;
    std::vector<Binding> m_Inputs;
    std::vector<Binding> m_Locals;
    std::vector<std::string> m_AddedInputs;
    std::vector<std::string> m_AddedOutputs;

    friend class BehaviorRuntime;
};

enum class BehaviorError {
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
};

struct BehaviorStatus {
    BehaviorError Error = BehaviorError::None;
    CKERROR CkError = CK_OK;
    int BehaviorResult = CKBR_OK;
    std::string Message;

    explicit operator bool() const noexcept { return Error == BehaviorError::None; }
};

enum class ExecutionState {
    Completed,
    Continuing,
    Suspended,
    Failed,
};

struct ExecutionResult {
    BehaviorStatus Status;
    ExecutionState State = ExecutionState::Completed;
    int ReturnCode = CKBR_OK;
    std::vector<int> ActiveOutputs;

    explicit operator bool() const noexcept { return static_cast<bool>(Status); }
};

class BehaviorRuntime;

class BehaviorInstance {
public:
    BehaviorInstance() = default;
    ~BehaviorInstance();
    BehaviorInstance(const BehaviorInstance &) = delete;
    BehaviorInstance &operator=(const BehaviorInstance &) = delete;
    BehaviorInstance(BehaviorInstance &&other) noexcept;
    BehaviorInstance &operator=(BehaviorInstance &&other) noexcept;

    [[nodiscard]] explicit operator bool() const noexcept { return m_Runtime && m_Token != 0; }
    [[nodiscard]] CKBehavior *Get() const;
    [[nodiscard]] std::uint64_t LayoutGeneration() const;
    void Reset();

private:
    BehaviorInstance(BehaviorRuntime *runtime, std::uint64_t token) : m_Runtime(runtime), m_Token(token) {}

    BehaviorRuntime *m_Runtime = nullptr;
    std::uint64_t m_Token = 0;

    friend class BehaviorRuntime;
};

struct InstanceResult {
    BehaviorStatus Status;
    BehaviorInstance Instance;
    BehaviorLayout Layout;

    explicit operator bool() const noexcept { return static_cast<bool>(Status); }
};

struct GraphBlockResult {
    BehaviorStatus Status;
    CKBehavior *Behavior = nullptr;
    BehaviorLayout Layout;

    explicit operator bool() const noexcept { return static_cast<bool>(Status); }
};

struct CallResult {
    BehaviorStatus Status;
    ExecutionResult Execution;
    BehaviorInstance Instance;
    BehaviorLayout Layout;

    explicit operator bool() const noexcept {
        return static_cast<bool>(Status) && static_cast<bool>(Execution);
    }
};

class BehaviorRuntime final {
public:
    BehaviorRuntime(CKContext *context, CKIdentityRegistry &identities);
    ~BehaviorRuntime();
    BehaviorRuntime(const BehaviorRuntime &) = delete;
    BehaviorRuntime &operator=(const BehaviorRuntime &) = delete;

    InstanceResult Instantiate(CKBeObject *owner, const BehaviorSpec &spec,
                               const CKBehaviorContext *frame = nullptr);
    CallResult Call(CKBeObject *owner, const BehaviorSpec &spec, const SlotSelector &input,
                    const CKBehaviorContext *frame = nullptr);
    GraphBlockResult AddToGraph(CKBehavior *parent, const BehaviorSpec &spec,
                                const CKBehaviorContext *frame = nullptr);

    [[nodiscard]] BehaviorLayout Describe(CKBehavior *behavior, std::uint64_t generation = 0) const;
    [[nodiscard]] BehaviorStatus Resolve(CKBehavior *behavior, const SlotSelector &selector,
                                         BehaviorSlot &slot) const;
    [[nodiscard]] BehaviorStatus Resolve(const BehaviorInstance &instance,
                                         const SlotSelector &selector,
                                         BehaviorSlotHandle &slot) const;
    [[nodiscard]] CKParameter *Parameter(const BehaviorInstance &instance,
                                         const SlotSelector &selector,
                                         BehaviorStatus *status = nullptr) const;
    [[nodiscard]] CKParameter *Parameter(const BehaviorInstance &instance,
                                         const BehaviorSlotHandle &slot,
                                         BehaviorStatus *status = nullptr) const;

    BehaviorStatus SetInput(BehaviorInstance &instance, const SlotSelector &selector,
                            const ParameterValue &value);
    BehaviorStatus SetInput(BehaviorInstance &instance, const BehaviorSlotHandle &slot,
                            const ParameterValue &value);
    BehaviorStatus SetLocal(BehaviorInstance &instance, const SlotSelector &selector,
                            const ParameterValue &value);
    BehaviorStatus SetLocal(BehaviorInstance &instance, const BehaviorSlotHandle &slot,
                            const ParameterValue &value);
    // Settings can rebuild arbitrary parts of the live layout.  Reconfigure
    // therefore takes the complete desired spec and reapplies target, locals,
    // and inputs after every settings stage; there is no misleading one-field
    // SetSetting operation.
    BehaviorStatus Reconfigure(BehaviorInstance &instance, const BehaviorSpec &spec,
                               const CKBehaviorContext *frame = nullptr);

    ExecutionResult Pulse(BehaviorInstance &instance, const SlotSelector &input,
                          const CKBehaviorContext *frame = nullptr);
    ExecutionResult Pulse(BehaviorInstance &instance, const BehaviorSlotHandle &input,
                          const CKBehaviorContext *frame = nullptr);
    ExecutionResult Step(BehaviorInstance &instance, const CKBehaviorContext *frame = nullptr);
    ExecutionResult StartTask(BehaviorInstance &instance, const SlotSelector &input,
                              const CKBehaviorContext *frame = nullptr);
    [[nodiscard]] bool IsTaskActive(const BehaviorInstance &instance) const;
    void ProcessTasks(const CKBehaviorContext *frame = nullptr);
    void ProcessFrame();

    void ObjectsToBeDeleted(const CK_ID *ids, int count);
    void ResetWorld();

private:
    struct Record {
        BML_ObjectRef Behavior;
        BML_ObjectRef Parent;
        std::uint64_t Token = 0;
        std::uint64_t LayoutGeneration = 1;
        bool GraphResident = false;
        bool Created = false;
        bool Attached = false;
        bool Running = false;
        bool Task = false;
        bool Expired = false;
        bool Poisoned = false;
        bool ReleaseRequested = false;
        bool ForceDestroy = false;
        std::vector<BML_ObjectRef> OwnedSources;
    };

    struct PendingDestroy {
        BML_ObjectRef Behavior;
        BML_ObjectRef Parent;
        std::vector<BML_ObjectRef> Sources;
        int Frames = 2;
        bool GraphResident = false;
        bool Created = false;
        bool Attached = false;
        bool Reset = false;
    };

    [[nodiscard]] BehaviorStatus ReadyStatus() const;
    [[nodiscard]] Record *FindRecord(std::uint64_t token);
    [[nodiscard]] const Record *FindRecord(std::uint64_t token) const;
    [[nodiscard]] Record *FindRecord(const BehaviorInstance &instance);
    [[nodiscard]] const Record *FindRecord(const BehaviorInstance &instance) const;
    [[nodiscard]] CKBehavior *ResolveBehavior(const Record &record) const;
    [[nodiscard]] CKParameter *ResolveParameter(CKBehavior *behavior, const BehaviorSlot &slot) const;
    [[nodiscard]] CKObject *ResolveSlotObject(CKBehavior *behavior, const BehaviorSlot &slot) const;
    [[nodiscard]] BehaviorStatus ValidateSlot(const Record &record,
                                              const BehaviorSlotHandle &slot) const;
    [[nodiscard]] BehaviorStatus ApplyValue(CKParameter *parameter, const ParameterValue &value) const;
    [[nodiscard]] BehaviorStatus BindInput(CKBehavior *behavior, Record &record,
                                           const BehaviorSlot &slot, const ParameterValue &value);
    [[nodiscard]] BehaviorStatus EnsurePrototypeDefaults(CKBehavior *behavior,
                                                         CKBehaviorPrototype *prototype,
                                                         Record &record);
    [[nodiscard]] BehaviorStatus ApplyBindings(CKBehavior *behavior, const BehaviorSpec &spec,
                                               Record &record);
    [[nodiscard]] BehaviorStatus BindTarget(CKBehavior *behavior, CKBeObject *owner,
                                            const BehaviorSpec &spec, Record &record);
    void PruneOwnedSources(CKBehavior *behavior, Record &record);
    [[nodiscard]] BehaviorStatus Configure(CKBehavior *behavior,
                                           CKBeObject *owner, CKBehavior *parent,
                                           const BehaviorSpec &spec,
                                           const CKBehaviorContext *frame,
                                           Record &record);
    [[nodiscard]] BehaviorStatus CallCallback(CKBehavior *behavior, CKDWORD message,
                                              const CKBehaviorContext *frame) const;
    [[nodiscard]] ExecutionResult Execute(std::uint64_t token, int input,
                                           bool activateInput,
                                           const CKBehaviorContext *frame);
    [[nodiscard]] int ExecuteNative(CKBehavior *behavior, const CKBehaviorContext *frame) const;
    void RequestRelease(std::uint64_t token);
    void Release(std::uint64_t token);
    void QueueDestroy(Record &record, bool reset = false);
    void QueueSourceDestroy(BML_ObjectRef source, int frames = 2);
    void DrainDeferredReleases();
    void DestroyReady(bool force);

    CKContext *m_Context = nullptr;
    CKIdentityRegistry *m_Identities = nullptr;
    std::thread::id m_Thread;
    std::uint64_t m_NextToken = 1;
    std::unordered_map<std::uint64_t, Record> m_Records;
    std::list<PendingDestroy> m_PendingDestroy;
    bool m_Destroying = false;
    bool m_ForceDestroyPending = false;
    std::mutex m_DeferredMutex;
    std::vector<std::uint64_t> m_DeferredReleases;

    friend class BehaviorInstance;
};

const char *DescribeBehaviorError(BehaviorError error);

} // namespace Virtools
} // namespace BML

#endif // BML_BEHAVIORRUNTIME_H
