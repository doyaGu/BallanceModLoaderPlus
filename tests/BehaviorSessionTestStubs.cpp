#include "Behavior/Runtime.h"

#include <mutex>
#include <unordered_map>
#include <utility>

#include "Behavior/FrameStore.h"

namespace BML::Behavior {
namespace {

struct FakeInstance {
    ExecutionState State = ExecutionState::Idle;
    std::shared_ptr<FrameStore> Frames;
    Layout Descriptor;
    std::uint64_t NextSequence = 1;
};

std::mutex g_FakeMutex;
std::unordered_map<std::uint64_t, FakeInstance> g_FakeInstances;

RunFrame MakeFrame(FakeInstance &instance, bool terminal) {
    RunFrame frame;
    frame.Sequence = instance.NextSequence++;
    frame.Frame = frame.Sequence;
    frame.ReturnCode = terminal ? CKBR_OK : CKBR_ACTIVATENEXTFRAME;
    frame.NativeContinuation = !terminal;
    frame.Terminal = terminal;
    if (terminal)
        frame.ActiveOutputs.push_back({0, "Done", 0});
    return frame;
}

FakeInstance *FindFake(std::uint64_t id) {
    const auto found = g_FakeInstances.find(id);
    return found == g_FakeInstances.end() ? nullptr : &found->second;
}

} // namespace

void AdvanceBehaviorSessionRuntime() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    for (auto &[id, instance] : g_FakeInstances) {
        if (instance.State != ExecutionState::Pending)
            continue;
        (void) instance.Frames->Retain(MakeFrame(instance, true));
        instance.State = ExecutionState::Idle;
    }
}

std::size_t LiveBehaviorSessionInstances() {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    return g_FakeInstances.size();
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
                 PrototypeCatalog *catalog)
    : m_Context(context), m_IssueObjectRef(std::move(issueObjectRef)),
      m_Catalog(catalog), m_Thread(std::this_thread::get_id()),
      m_Access(std::make_shared<Instance::Access>()) {
    m_Access->Owner = this;
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    g_FakeInstances.clear();
}

Runtime::~Runtime() {
    if (m_Access) {
        std::lock_guard<std::mutex> accessLock(m_Access->Mutex);
        m_Access->Owner = nullptr;
    }
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    g_FakeInstances.clear();
}

CreateResult Runtime::Instantiate(CKBeObject *, const Spec &spec,
                                  const CKBehaviorContext *) {
    const std::uint64_t id = m_NextInstanceId++;
    FakeInstance instance;
    instance.Frames = std::make_shared<FrameStore>(spec.m_FrameRetention);
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

CallResult Runtime::Call(CKBeObject *owner, const Spec &spec,
                         const Slot &input, const CKBehaviorContext *frame) {
    CreateResult created = Instantiate(owner, spec, frame);
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
    if (!found || found->State != ExecutionState::Pending)
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

    const bool pending = input.Name == "Pending" &&
        found->NextSequence == 1;
    (void) found->Frames->Retain(MakeFrame(*found, !pending));
    found->State = pending ? ExecutionState::Pending : ExecutionState::Idle;
    RunResult result;
    result.State = pending ? RunState::Pending : RunState::Completed;
    result.ReturnCode = pending ? CKBR_ACTIVATENEXTFRAME : CKBR_OK;
    result.Admission = AdmissionState::Executed;
    if (!pending)
        result.ActiveOutputs.push_back(0);
    return result;
}

ExecutionState Runtime::State(const Instance &instance) const {
    std::lock_guard<std::mutex> lock(g_FakeMutex);
    const FakeInstance *found = FindFake(instance.m_Id);
    return found ? found->State : ExecutionState::Closed;
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

void Runtime::ClosePending() {}

} // namespace BML::Behavior
