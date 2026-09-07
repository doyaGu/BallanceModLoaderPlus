#include "Behavior/Runtime.h"

#include <mutex>
#include <functional>
#include <unordered_map>
#include <utility>

#include "Behavior/FrameStore.h"

namespace BML::Behavior::Internal {
namespace {

struct FakeInstance {
    ExecutionState State = ExecutionState::Idle;
    std::shared_ptr<FrameStore> Frames;
    Layout Descriptor;
    std::uint64_t NextSequence = 1;
    bool FailOnAdvance = false;
    Status Failure;
};

std::mutex g_FakeMutex;
std::unordered_map<std::uint64_t, FakeInstance> g_FakeInstances;
std::size_t g_StateReads = 0;
std::size_t g_WorldResets = 0;
std::size_t g_ClosePendingCalls = 0;
std::function<void()> g_ConfigureCallback;

RunFrame MakeFrame(FakeInstance &instance, bool endsActivation) {
    RunFrame frame;
    frame.Sequence = instance.NextSequence++;
    frame.Frame = frame.Sequence;
    frame.ReturnCode = endsActivation ? CKBR_OK : CKBR_ACTIVATENEXTFRAME;
    frame.NativeContinuation = !endsActivation;
    if (endsActivation)
        frame.ActiveOutputs.push_back({0, "Done", 0});
    return frame;
}

FakeInstance *FindFake(std::uint64_t id) {
    const auto found = g_FakeInstances.find(id);
    return found == g_FakeInstances.end() ? nullptr : &found->second;
}

} // namespace

void SetBehaviorSessionConfigureCallback(std::function<void()> callback) {
    g_ConfigureCallback = std::move(callback);
}

void AdvanceBehaviorSessionRuntime() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    for (auto &[id, instance] : g_FakeInstances) {
        if (instance.State != ExecutionState::Pending)
            continue;
        if (instance.FailOnAdvance) {
            instance.State = ExecutionState::Failed;
            instance.Failure = {Error::StaleLayout, CKERR_INVALIDOBJECT,
                                CKBR_PARAMETERERROR,
                                "The fake queued input layout changed."};
            instance.Failure.Details.Stage = Phase::Execution;
            continue;
        }
        (void) instance.Frames->Retain(MakeFrame(instance, true));
        instance.State = ExecutionState::Idle;
    }
}

std::size_t LiveBehaviorSessionInstances() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    return g_FakeInstances.size();
}

void ResetBehaviorSessionRuntimeStateReads() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    g_StateReads = 0;
}

std::size_t BehaviorSessionRuntimeStateReads() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    return g_StateReads;
}

std::size_t BehaviorSessionRuntimeWorldResets() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    return g_WorldResets;
}

void ResetBehaviorSessionRuntimeClosePendingCalls() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    g_ClosePendingCalls = 0;
}

std::size_t BehaviorSessionRuntimeClosePendingCalls() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    return g_ClosePendingCalls;
}

Instance::~Instance() {
    Reset();
}

Instance::Instance(Instance &&other) noexcept
    : m_Access(std::move(other.m_Access)),
      m_Id(std::exchange(other.m_Id, 0)) {}

Instance &Instance::operator=(Instance &&other) noexcept {
    if (this != &other) {
        Reset();
        m_Access = std::move(other.m_Access);
        m_Id = std::exchange(other.m_Id, 0);
    }
    return *this;
}

Instance::operator bool() const noexcept {
    return m_Id != 0;
}

CKBehavior *Instance::Get() const {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    return FindFake(m_Id)
        ? reinterpret_cast<CKBehavior *>(static_cast<std::uintptr_t>(m_Id))
        : nullptr;
}

std::uint64_t Instance::LayoutGeneration() const {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    const FakeInstance *found = FindFake(m_Id);
    return found ? found->Descriptor.Generation : 0;
}

void Instance::Reset() {
    if (m_Id) {
        std::lock_guard<std::mutex> lock(g_FakeMutex);
        g_FakeInstances.erase(m_Id);
    }
    m_Access.reset();
    m_Id = 0;
}

Runtime::Runtime(CKContext *context,
                 std::function<ObjectRef(const void *)> issueObjectRef,
                 PrototypeCatalog *catalog,
                 Runtime *)
    : m_Context(context), m_IssueObjectRef(std::move(issueObjectRef)),
      m_Catalog(catalog), m_Thread(std::this_thread::get_id()),
      m_Access(std::make_shared<Instance::Access>()) {
    m_Access->Owner = this;
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    g_FakeInstances.clear();
    g_WorldResets = 0;
}

Runtime::~Runtime() {
    if (m_Access) {
        std::lock_guard<std::mutex> accessLock(m_Access->Mutex);
        m_Access->Owner = nullptr;
    }
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    g_FakeInstances.clear();
}

CreateResult Runtime::Instantiate(CKBeObject *, const BlockSpec &spec,
                                  const CKBehaviorContext *,
                                  FrameRetention retention) {
    const std::uint64_t id = m_NextInstanceId++;
    FakeInstance instance;
    instance.Frames = std::make_shared<FrameStore>(retention);
    instance.Descriptor.Prototype = spec.m_Prototype;
    instance.Descriptor.ProviderGeneration = spec.m_PrototypeGeneration;
    instance.Descriptor.Generation = 1;
    instance.Descriptor.Origin = LayoutOrigin::Live;
    const Layout descriptor = instance.Descriptor;
    {
        std::lock_guard<std::mutex> lock(g_FakeMutex);
        g_FakeInstances.emplace(id, std::move(instance));
    }
    return {{}, Instance(m_Access, id), descriptor};
}

CreateResult Runtime::AttachToGraph(CKBehavior *parent, const BlockSpec &spec,
                                    const CKBehaviorContext *frame,
                                    FrameRetention retention) {
    if (!parent)
        return {{Error::OwnerInvalid, CKERR_INVALIDOBJECT, CKBR_BEHAVIORERROR,
                 "The fake graph is not live."}, {}, {}};
    return Instantiate(nullptr, spec, frame, retention);
}

CallResult Runtime::Call(CKBeObject *owner, const BlockSpec &spec,
                         const Slot &input, const CKBehaviorContext *frame,
                         FrameRetention retention) {
    CreateResult created = Instantiate(owner, spec, frame, retention);
    if (!created)
        return {created.Detail, {}, {}, created.Descriptor};
    RunResult run = Pulse(created.Handle, input, frame);
    return {run.Detail, std::move(run), std::move(created.Handle),
            std::move(created.Descriptor)};
}

RunResult Runtime::StartTask(Instance &instance, const Slot &input,
                             const CKBehaviorContext *frame) {
    return Pulse(instance, input, frame);
}

Status Runtime::Continue(Instance &instance) {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    FakeInstance *found = FindFake(instance.m_Id);
    if (!found)
        return {Error::InvalidState, CKERR_INVALIDOBJECT,
                CKBR_BEHAVIORERROR, "The fake Behavior is not pending."};
    if (found->Failure.Code != Error::None)
        return found->Failure;
    if (found->State != ExecutionState::Pending)
        return {Error::InvalidState, CKERR_INVALIDOBJECT,
                CKBR_BEHAVIORERROR, "The fake Behavior is not pending."};
    return {};
}

RunResult Runtime::Pulse(Instance &instance, const Slot &input,
                         const CKBehaviorContext *) {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    FakeInstance *found = FindFake(instance.m_Id);
    if (!found)
        return {{Error::InvalidState, CKERR_INVALIDOBJECT,
                 CKBR_BEHAVIORERROR, "The fake Behavior is stale."},
                RunState::Failed, CKBR_BEHAVIORERROR, {},
                AdmissionState::Failed};
    if (found->Failure.Code != Error::None) {
        return {found->Failure, RunState::Failed, CKBR_BEHAVIORERROR, {},
                AdmissionState::Failed};
    }
    if (input.Name == "Missing") {
        return {{Error::SlotNotFound, CK_OK, CKBR_PARAMETERERROR,
                 "The fake Behavior input does not exist."},
                RunState::Failed, CKBR_PARAMETERERROR, {},
                AdmissionState::Failed};
    }

    const bool pending = (input.Name == "Pending" ||
                          input.Name == "PendingFail") &&
        found->NextSequence == 1;
    const bool failed = input.Name == "Fail";
    RunFrame captured = MakeFrame(*found, !pending);
    if (failed) {
        captured.ReturnCode = CKBR_BEHAVIORERROR;
        captured.Fault = {ExecutionError::NativeFailed,
                          CKBR_BEHAVIORERROR,
                          "The fake Behavior execution failed."};
    }
    (void) found->Frames->Retain(std::move(captured));
    found->FailOnAdvance = input.Name == "PendingFail";
    found->State = failed ? ExecutionState::Failed
                         : pending ? ExecutionState::Pending
                                   : ExecutionState::Idle;
    RunResult result;
    result.State = failed ? RunState::Failed
                          : pending ? RunState::Pending : RunState::Ready;
    result.ReturnCode = failed ? CKBR_BEHAVIORERROR
                               : pending ? CKBR_ACTIVATENEXTFRAME : CKBR_OK;
    result.Admission = AdmissionState::Executed;
    if (failed) {
        result.Detail = {Error::ExecutionFailed, CK_OK,
                         CKBR_BEHAVIORERROR,
                         "The fake Behavior execution failed."};
        found->Failure = result.Detail;
        found->Failure.Details.Stage = Phase::Execution;
    } else if (!pending) {
        result.ActiveOutputs.push_back(0);
    }
    return result;
}

ExecutionState Runtime::State(const Instance &instance) const {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    ++g_StateReads;
    const FakeInstance *found = FindFake(instance.m_Id);
    return found ? found->State : ExecutionState::Closed;
}

Status Runtime::InstanceFailure(const Instance &instance) const {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    const FakeInstance *found = FindFake(instance.m_Id);
    if (!found)
        return {Error::InvalidState, CKERR_INVALIDOBJECT,
                CKBR_BEHAVIORERROR, "The fake Behavior is stale."};
    return found->Failure;
}

std::shared_ptr<FrameStore> Runtime::Frames(const Instance &instance) const {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    const FakeInstance *found = FindFake(instance.m_Id);
    return found ? found->Frames : nullptr;
}

Status Runtime::Describe(const Instance &instance, Layout &layout) const {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    const FakeInstance *found = FindFake(instance.m_Id);
    if (!found)
        return {Error::LayoutUnavailable, CKERR_INVALIDOBJECT,
                CKBR_PARAMETERERROR, "The fake Behavior is stale."};
    layout = found->Descriptor;
    return {};
}

Status Runtime::Resolve(const Instance &instance, const Slot &selector,
                        SlotRef &slot) const {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    const FakeInstance *found = FindFake(instance.m_Id);
    if (!found)
        return {Error::InvalidState, CKERR_INVALIDOBJECT,
                CKBR_PARAMETERERROR, "The fake Behavior is stale."};
    slot = SlotRef();
    slot.InstanceId = instance.m_Id;
    slot.LayoutGeneration = found->Descriptor.Generation;
    slot.Object = reinterpret_cast<CKObject *>(
        static_cast<std::uintptr_t>(instance.m_Id));
    slot.Slot.Kind = selector.Kind;
    slot.Slot.Index = selector.Index >= 0 ? selector.Index : 0;
    slot.Slot.NativeIndex = slot.Slot.Index;
    slot.Slot.Name = selector.Name;
    slot.Slot.Occurrence = selector.Occurrence;
    slot.Slot.Type = selector.ExpectedType;
    return {};
}

Status Runtime::SetInput(Instance &instance, const SlotRef &slot,
                         const Parameter::Binding &) {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    FakeInstance *found = FindFake(instance.m_Id);
    if (!found)
        return {Error::InvalidState, CKERR_INVALIDOBJECT,
                CKBR_PARAMETERERROR, "The fake Behavior is stale."};
    if (slot.LayoutGeneration != found->Descriptor.Generation)
        return {Error::StaleLayout, CKERR_INVALIDOBJECT,
                CKBR_PARAMETERERROR, "The fake Layout changed."};
    return {};
}

Status Runtime::SetLocal(Instance &instance, const SlotRef &slot,
                         const Parameter::Binding &value) {
    return SetInput(instance, slot, value);
}

Status Runtime::Bind(Instance &instance, const SlotRef &slot,
                     CKBehavior *, const Slot &,
                     Parameter::BindingKind) {
    return SetInput(instance, slot, Parameter::Binding{});
}

Status Runtime::Configure(Instance &instance, const BlockSpec &settings,
                          const CKBehaviorContext *) {
    if (g_ConfigureCallback)
        g_ConfigureCallback();
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    FakeInstance *found = FindFake(instance.m_Id);
    if (!found)
        return {Error::InvalidState, CKERR_INVALIDOBJECT,
                CKBR_PARAMETERERROR, "The fake Behavior is stale."};
    if (found->Failure.Code != Error::None)
        return found->Failure;
    if (found->State != ExecutionState::Idle)
        return {Error::InvalidState, CKERR_INVALIDOBJECT,
                CKBR_PARAMETERERROR, "Configuration requires an idle behavior instance."};
    if (settings.Prototype() == CKGUID(91, 92)) {
        found->Failure = {Error::CallbackFailed, CKERR_INVALIDPARAMETER,
                          CKBR_BEHAVIORERROR,
                          "The fake Setting callback failed."};
        found->Failure.Details.Stage = Phase::LifecycleCallback;
        found->State = ExecutionState::Failed;
        return found->Failure;
    }
    ++found->Descriptor.Generation;
    return {};
}

bool PrototypeCatalog::TracksRetirement() const noexcept {
    return false;
}

Status PrototypeCatalog::Find(const PrototypeQuery &,
                              std::vector<PrototypeInfo> &) {
    return {Error::InvalidState, CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
            "No Prototype Catalog is present in the Session golden test."};
}

Status PrototypeCatalog::DeclaredLayout(PrototypeRef, Layout &) {
    return {Error::InvalidState, CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
            "No Prototype Catalog is present in the Session golden test."};
}

void Runtime::ClosePending() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    ++g_ClosePendingCalls;
}

void Runtime::ResetWorld() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    ++g_WorldResets;
    g_FakeInstances.clear();
}
} // namespace BML::Behavior::Internal
