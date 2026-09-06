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
#include "Behavior/Block.h"
#include "Behavior/Callback.h"
#include "Behavior/Execution.h"
#include "Behavior/Lifecycle.h"
#include "Behavior/Layout.h"
#include "Behavior/ObjectRef.h"
#include "Behavior/Parameter.h"
#include "Behavior/PrototypeCatalog.h"
#include "Behavior/Status.h"

namespace BML::Behavior::Internal {

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
    DetachedCompatibility Detached = DetachedCompatibility::Unverified;

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
    DetachedCompatibility Detached = DetachedCompatibility::Unverified;

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

    CreateResult Instantiate(CKBeObject *owner, const BlockSpec &spec,
                             const CKBehaviorContext *frame = nullptr,
                             FrameRetention retention = FrameRetention::Signals());
    CallResult Call(CKBeObject *owner, const BlockSpec &spec, const Slot &input,
                    const CKBehaviorContext *frame = nullptr,
                    FrameRetention retention = FrameRetention::Signals());
    AttachResult AddToGraph(CKBehavior *parent, const BlockSpec &spec,
                                const CKBehaviorContext *frame = nullptr);
    // Adds a Block to a live graph and keeps a managed handle for it, so the
    // caller reads and writes its Slots through this Runtime instead of poking
    // the native object. The handle also drives the Block: a parked Block that
    // its parent graph never activates is executed by whoever holds the
    // handle.
    CreateResult AttachToGraph(CKBehavior *parent, const BlockSpec &spec,
                               const CKBehaviorContext *frame = nullptr,
                               FrameRetention retention = FrameRetention::Signals());

    [[nodiscard]] Layout Describe(CKBehavior *behavior, std::uint64_t generation = 0) const;
    [[nodiscard]] std::uint64_t LayoutGeneration(
        CKBehavior *behavior) const noexcept;
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
    Status Bind(Instance &instance, const SlotRef &slot,
                CKBehavior *source, const Slot &sourceSlot,
                Parameter::BindingKind relation);
    Status Configure(Instance &instance, const BlockSpec &settings,
                     const CKBehaviorContext *frame = nullptr);
    // Settings can rebuild arbitrary parts of the live layout.  Reconfigure
    // therefore takes the complete desired spec and reapplies target, locals,
    // and inputs after every settings stage; there is no misleading one-field
    // SetSetting operation.
    Status Reconfigure(Instance &instance, const BlockSpec &spec,
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
    bool ProcessTasks(const CKBehaviorContext *frame = nullptr);
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

    struct Record {
        ObjectStamp Behavior;
        ObjectStamp Parent;
        CKGUID PrototypeGuid;
        CKBehaviorPrototype *Prototype = nullptr;
        std::uint64_t ProviderGeneration = 0;
        std::uint64_t Id = 0;
        std::uint64_t LayoutGeneration = 1;
        bool GraphResident = false;
        // A graph-resident Block whose owner holds a Run handle. Its parent
        // graph does not activate it, so Pulse and Step may execute it.  Each
        // driven execution clears the native active flag: Ballanced schedules
        // every ACTIVE sub-behavior, linked or not, so a continuation left
        // flagged would also be executed by the parent graph.  The Run's
        // ProcessTasks is the only continuation driver.
        bool OwnerDriven = false;
        bool Expired = false;
        bool Poisoned = false;
        bool QueuedForFrame = false;
        Lifecycle NativeLifecycle;
        Execution Protocol;
        // The Target, Pins, Locals, and operations that define the current
        // live instance. Settings are deliberately not retained: each stage
        // is an event and must not be replayed by a later Configure call.
        BlockSpec Desired;
        std::vector<ObjectStamp> OwnedSources;
        std::vector<std::shared_ptr<CallbackResource>> KeepAlive;
    };

    struct PendingDestroy {
        ObjectStamp Behavior;
        ObjectStamp Parent;
        std::vector<ObjectStamp> Sources;
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
    [[nodiscard]] Status ResolvePrototype(PrototypeRef requested,
                                          PrototypeRef &selected) const;
    [[nodiscard]] Status CheckDetached(
        const BlockSpec &spec, DetachedCompatibility &compatibility,
        bool graphResident = false) const;
    [[nodiscard]] Status ValidateTarget(CKBeObject *owner,
                                        const BlockSpec &spec) const;
    [[nodiscard]] Status CreateBehavior(const BlockSpec &spec,
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
    [[nodiscard]] Status EnsurePrototypeLayout(CKBehavior *behavior,
                                                       CKBehaviorPrototype *prototype,
                                                       bool afterSettings);
    [[nodiscard]] Status EnsurePrototypeDefaults(CKBehavior *behavior,
                                                         CKBehaviorPrototype *prototype,
                                                         Record &record);
    [[nodiscard]] Status ApplyBindings(CKBehavior *behavior, const BlockSpec &spec,
                                               Record &record);
    [[nodiscard]] Status BindTarget(CKBehavior *behavior, CKBeObject *owner,
                                            const BlockSpec &spec, Record &record);
    // The shared body of AddToGraph and AttachToGraph. It hands back a
    // managed handle only when the caller asked for one, and reports what
    // the catalog knows about detached support without refusing a GraphOnly
    // Block: a parent graph is exactly where such a Block belongs.
    AttachResult Attach(CKBehavior *parent, const BlockSpec &spec,
                        const CKBehaviorContext *frame, Instance *handle,
                        DetachedCompatibility *detached = nullptr,
                        FrameRetention retention = FrameRetention::Ignore());
    void PruneOwnedSources(Record &record);
    void SweepRecords();
    class NativeLifecycleAdapter;
    [[nodiscard]] Status Configure(CKBehavior *behavior,
                                           CKBeObject *owner, CKBehavior *parent,
                                           const BlockSpec &spec,
                                           const CKBehaviorContext *frame,
                                           Record &record);
    [[nodiscard]] Status CreateBlock(CKBehavior *behavior,
                                     CKBeObject *owner, CKBehavior *parent,
                                     const BlockSpec &spec,
                                     const CKBehaviorContext *frame,
                                     Record &record);
    [[nodiscard]] Status EditBlock(CKBehavior *behavior,
                                   CKBeObject *owner, CKBehavior *parent,
                                   const BlockSpec &spec,
                                   const CKBehaviorContext *frame,
                                   Record &record);
    [[nodiscard]] Status LifecycleStatus(
        const NativeLifecycleAdapter &adapter, const Record &record) const;
    [[nodiscard]] AttachResult CreateInGraph(
        CKBehavior *parent, const BlockSpec &spec,
        const CKBehaviorContext *frame = nullptr);
    [[nodiscard]] Status EditInGraph(
        CKBehavior *behavior, const BlockSpec &spec,
        const CKBehaviorContext *frame = nullptr);
    [[nodiscard]] Status ApplySettings(
        Instance &instance,
        const std::vector<std::vector<BlockSpec::Binding>> &settings,
        const BlockSpec &desired, const CKBehaviorContext *frame);
    [[nodiscard]] Status CallCallback(Record &record, CKDWORD message,
                                      const CKBehaviorContext *frame) const;
    class NativeAdapter;
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
    void DestroyConnectedLinks(CKBehavior *parent, CKBehavior *behavior);
    void DrainDeferredReleases();
    void QueueFrame(Record &record);
    bool DrainCloseQueue(bool force = false);
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
    std::vector<std::uint64_t> m_FrameQueue;
    std::vector<std::uint64_t> m_FrameRecords;
    std::shared_ptr<Instance::Access> m_Access;
    std::shared_ptr<SharedBindings> m_SharedBindings;
    bool m_ProcessingFrame = false;
    bool m_ProcessingTasks = false;
    std::vector<Record *> m_ConfiguringRecords;

    friend class Instance;
    friend class CKEdit;
};

const char *DescribeError(Error error);
const char *DescribePhase(Phase phase);

} // namespace BML::Behavior::Internal

#endif // BML_BEHAVIOR_RUNTIME_H
