#ifndef BML_BEHAVIOR_RUNTIME_H
#define BML_BEHAVIOR_RUNTIME_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
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
#include "Behavior/Layout.h"
#include "Behavior/ObjectRef.h"
#include "Behavior/Parameter.h"
#include "Behavior/PrototypeCatalog.h"
#include "Behavior/Status.h"

namespace BML::Behavior {

class Operation {
public:
    explicit Operation(CKGUID operation = CKGUID()) : m_Operation(operation) {}

    Operation &Result(CKGUID type);
    Operation &Input1(Parameter::Binding value);
    Operation &Input2(Parameter::Binding value);

    [[nodiscard]] CKGUID Guid() const noexcept { return m_Operation; }
    [[nodiscard]] CKGUID ResultType() const noexcept { return m_ResultType; }

private:
    CKGUID m_Operation = CKGUID();
    CKGUID m_ResultType = CKGUID();
    Parameter::Binding m_Input1;
    Parameter::Binding m_Input2;
    bool m_HasInput1 = false;
    bool m_HasInput2 = false;

    friend class Runtime;
    friend class Edit;
    friend class CKEdit;
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
    Spec &Setting(Slot slot, Parameter::Binding value);
    Spec &RefreshLayout();
    Spec &Input(Slot slot, Parameter::Binding value);
    Spec &Input(Slot slot, Operation operation);
    Spec &Local(Slot slot, Parameter::Binding value);
    Spec &AddInput(std::string name);
    Spec &AddOutput(std::string name);
    Spec &Frames(FrameRetention retention);
    Spec &KeepAlive(std::shared_ptr<CallbackResource> resource);
    Spec &PrototypeGeneration(std::uint64_t generation) noexcept {
        m_PrototypeGeneration = generation;
        return *this;
    }

    [[nodiscard]] CKGUID Prototype() const noexcept { return m_Prototype; }
    [[nodiscard]] std::uint64_t PrototypeGeneration() const noexcept {
        return m_PrototypeGeneration;
    }

private:
    struct Binding {
        Slot Target;
        Parameter::Binding Source;
    };

    struct OperationBinding {
        Slot Target;
        Operation Definition;
    };

    CKGUID m_Prototype = CKGUID();
    std::uint64_t m_PrototypeGeneration = 0;
    TargetMode m_TargetMode = TargetMode::Owner;
    CKGUID m_TargetType = CKGUID();
    Parameter::Binding m_TargetValue;
    std::vector<std::vector<Binding>> m_SettingStages;
    std::vector<Binding> m_Inputs;
    std::vector<OperationBinding> m_Operations;
    std::vector<Binding> m_Locals;
    std::vector<std::string> m_AddedInputs;
    std::vector<std::string> m_AddedOutputs;
    std::vector<std::shared_ptr<CallbackResource>> m_KeepAlive;
    FrameRetention m_FrameRetention = FrameRetention::Signals();

    friend class Runtime;
    friend class Edit;
    friend class CKEdit;
};

enum class RunState {
    Ready,
    Pending,
    Failed,
};

struct RunResult {
    Status Detail;
    RunState State = RunState::Ready;
    int ReturnCode = CKBR_OK;
    std::vector<int> ActiveOutputs;
    AdmissionState Admission = AdmissionState::Failed;

    explicit operator bool() const noexcept { return static_cast<bool>(Detail); }
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
    Status Detail;
    Instance Handle;
    Layout Descriptor;
    bool UnverifiedDetached = false;

    explicit operator bool() const noexcept { return static_cast<bool>(Detail); }
};

struct AttachResult {
    Status Detail;
    CKBehavior *Block = nullptr;
    Layout Descriptor;

    explicit operator bool() const noexcept { return static_cast<bool>(Detail); }
};

struct CallResult {
    Status Detail;
    RunResult Run;
    Instance Handle;
    Layout Descriptor;
    bool UnverifiedDetached = false;

    explicit operator bool() const noexcept {
        return static_cast<bool>(Detail) && static_cast<bool>(Run);
    }
};

class Runtime final {
public:
    explicit Runtime(
        CKContext *context,
        std::function<ObjectRef(const void *)> issueObjectRef = {},
        PrototypeCatalog *catalog = nullptr,
        Runtime *sourceRuntime = nullptr);
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
    [[nodiscard]] Status Describe(const Instance &instance,
                                  Layout &layout) const;
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
                            const Parameter::Binding &value);
    Status SetInput(Instance &instance, const SlotRef &slot,
                            const Parameter::Binding &value);
    Status SetLocal(Instance &instance, const Slot &selector,
                            const Parameter::Binding &value);
    Status SetLocal(Instance &instance, const SlotRef &slot,
                            const Parameter::Binding &value);
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
    [[nodiscard]] std::vector<RunFrame> Take(Instance &instance);
    [[nodiscard]] std::shared_ptr<FrameStore> Frames(
        const Instance &instance) const;
    [[nodiscard]] Status InstanceFailure(const Instance &instance) const;
    void ProcessTasks(const CKBehaviorContext *frame = nullptr);
    void ProcessFrame();
    void ClosePending();

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
        CKGUID PrototypeGuid;
        CKBehaviorPrototype *Prototype = nullptr;
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
        std::vector<std::shared_ptr<CallbackResource>> KeepAlive;
        int Frames = 2;
        bool DestroyBehavior = false;
        bool GraphResident = false;
    };

    struct SharedBindings {
        explicit SharedBindings(CKContext *context) : Sources(context) {}

        Parameter::Sources Sources;
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
    [[nodiscard]] Record *FindRecord(CKBehavior *behavior);
    [[nodiscard]] const Record *FindRecord(CKBehavior *behavior) const;
    [[nodiscard]] Record *FindRecord(const Instance &instance);
    [[nodiscard]] const Record *FindRecord(const Instance &instance) const;
    [[nodiscard]] ObjectStamp CaptureObject(CKObject *object) const;
    [[nodiscard]] CKObject *ResolveObject(ObjectStamp object) const;
    [[nodiscard]] CKBehavior *ResolveBehavior(const Record &record) const;
    [[nodiscard]] Status ResolvePrototype(CKGUID guid,
                                          std::uint64_t generation = 0) const;
    [[nodiscard]] Status CheckDetached(const Spec &spec,
                                       bool &unverified) const;
    [[nodiscard]] Status ValidateTarget(CKBeObject *owner,
                                        const Spec &spec) const;
    [[nodiscard]] Status CreateBehavior(const Spec &spec,
                                        CKBehavior *&behavior,
                                        Record &record) const;
    [[nodiscard]] CKGUID PrototypeGuid(CKBehavior *behavior) const;
    [[nodiscard]] CKBehaviorPrototype *PrototypeOf(CKBehavior *behavior) const;
    [[nodiscard]] CKParameter *ResolveParameter(CKBehavior *behavior, const SlotInfo &slot) const;
    [[nodiscard]] CKObject *ResolveSlotObject(CKBehavior *behavior, const SlotInfo &slot) const;
    [[nodiscard]] Status ValidateSlot(const Record &record,
                                              const SlotRef &slot) const;
    [[nodiscard]] Status BindInput(CKBehavior *behavior, Record &record,
                                           const SlotInfo &slot,
                                           const Parameter::Binding &value);
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
    [[nodiscard]] bool IsSourceReferenced(CKParameter *source);
    void PruneOwnedSources(CKBehavior *behavior, Record &record);
    void PruneOwnedOperations(CKBehavior *behavior, Record &record);
    void SweepRecords();
    void DetachOperation(OwnedOperation &operation);
    [[nodiscard]] Status Configure(CKBehavior *behavior,
                                           CKBeObject *owner, CKBehavior *parent,
                                           const Spec &spec,
                                           const CKBehaviorContext *frame,
                                           Record &record);
    [[nodiscard]] Status CallCallback(Record &record, CKDWORD message,
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
    CKContext *m_Context = nullptr;
    std::function<ObjectRef(const void *)> m_IssueObjectRef;
    PrototypeCatalog *m_Catalog = nullptr;
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
