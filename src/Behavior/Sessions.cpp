#include "Behavior/Sessions.h"

#include <limits>
#include <utility>

#include "Behavior/FrameStore.h"

namespace BML::Behavior {
namespace {

Status Fail(Error error, std::string message) {
    return {error, CK_OK, CKBR_OK, std::move(message)};
}

RunResult FailedRun(Error error, std::string message) {
    return {Fail(error, std::move(message)), RunState::Failed,
            CKBR_BEHAVIORERROR, {}};
}

RunState RunStateOf(ExecutionState state) noexcept {
    switch (state) {
    case ExecutionState::Idle:
        return RunState::Ready;
    case ExecutionState::Pending:
    case ExecutionState::Running:
        return RunState::Pending;
    case ExecutionState::Closing:
    case ExecutionState::Closed:
    case ExecutionState::Failed:
        return RunState::Failed;
    }
    return RunState::Failed;
}

} // namespace

Sessions::Sessions(Runtime &runtime, PrototypeCatalog *catalog,
                   std::unique_ptr<GraphSource> graph)
    : m_Runtime(runtime), m_Catalog(catalog), m_Graph(std::move(graph)),
      m_Thread(std::this_thread::get_id()) {}

Sessions::~Sessions() {
    ResetWorld();
    CollectWatches();
}

std::uint64_t Sessions::RegisterOwner(std::string ownerId) {
    if (ownerId.empty() || std::this_thread::get_id() != m_Thread)
        return 0;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    auto existing = m_Owners.find(ownerId);
    if (existing != m_Owners.end() &&
        existing->second.State != OwnerState::Released) {
        CloseOwner(ownerId, existing->second.Generation);
    }
    if (m_NextOwnerGeneration == 0)
        return 0;
    const std::uint64_t generation = m_NextOwnerGeneration++;
    m_Owners[ownerId] = {ownerId, generation, OwnerState::Active};
    return generation;
}

void Sessions::RetireOwner(const std::string &ownerId) {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    auto owner = m_Owners.find(ownerId);
    if (owner == m_Owners.end() || owner->second.State == OwnerState::Released)
        return;
    CloseOwner(ownerId, owner->second.Generation);
}

Status Sessions::OpenSession(const std::string &ownerId,
                              std::uintptr_t &sessionId) {
    sessionId = 0;
    Status ready = Ready();
    if (!ready)
        return ready;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto owner = m_Owners.find(ownerId);
    if (owner == m_Owners.end() || owner->second.State != OwnerState::Active)
        return Fail(Error::InvalidState, "The Mod is not an active Behavior owner.");
    const std::uintptr_t id = NextId();
    if (!id)
        return Fail(Error::InvalidState, "Behavior session ids are exhausted.");
    m_Sessions.emplace(id, Session{id, ownerId, owner->second.Generation});
    sessionId = id;
    return {};
}

void Sessions::CloseSession(std::uintptr_t sessionId) {
    if (!sessionId)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto session = m_Sessions.find(sessionId);
    if (session == m_Sessions.end())
        return;
    for (auto run = m_Runs.begin(); run != m_Runs.end();) {
        if (run->second->SessionId != sessionId) {
            ++run;
            continue;
        }
        QueueClose(std::move(run->second));
        run = m_Runs.erase(run);
    }
    for (auto watch = m_Watches.begin(); watch != m_Watches.end();) {
        if (watch->second.SessionId != sessionId) {
            ++watch;
            continue;
        }
        QueueWatch(std::move(watch->second.Value));
        watch = m_Watches.erase(watch);
    }
    m_Sessions.erase(session);
}

Status Sessions::ReadOwner(std::uintptr_t sessionId, SessionOwner &out) const {
    out = {};
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const Session *session = FindSession(sessionId);
    if (!session || !SessionIsActive(*session))
        return Fail(Error::InvalidState,
                    "The Behavior Session is stale or retiring.");
    out = {session->OwnerId, session->OwnerGeneration};
    return {};
}

OpenRun Sessions::Call(std::uintptr_t sessionId, CKBeObject *owner,
                        const Spec &block, const Slot &input) {
    Status ready = Ready();
    if (!ready)
        return {std::move(ready), 0, {}};
    Session session;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        Session *found = FindSession(sessionId);
        if (!found || !SessionIsActive(*found))
            return {Fail(Error::InvalidState, "The Behavior session is stale."), 0, {}};
        session = *found;
    }

    CallResult called = m_Runtime.Call(owner, block, input);
    if (!called.Handle)
        return {std::move(called.Detail), 0, {}};
    RunResult result = std::move(called.Run);
    if (result.Detail && !called.Detail)
        result.Detail = std::move(called.Detail);
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Session *current = FindSession(session.Id);
    if (!current || current->OwnerGeneration != session.OwnerGeneration) {
        called.Handle.Reset();
        m_Runtime.ClosePending();
        return {Fail(Error::InvalidState,
                     "The Behavior session closed during Call."), 0, {}};
    }
    return AddRun(session, RunKind::Call, std::move(called.Handle),
                  std::move(result), called.UnverifiedDetached);
}

OpenRun Sessions::Start(std::uintptr_t sessionId, CKBeObject *owner,
                         const Spec &block, const Slot &input) {
    Status ready = Ready();
    if (!ready)
        return {std::move(ready), 0, {}};
    Session session;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        Session *found = FindSession(sessionId);
        if (!found || !SessionIsActive(*found))
            return {Fail(Error::InvalidState, "The Behavior session is stale."), 0, {}};
        session = *found;
    }

    CreateResult created = m_Runtime.Instantiate(owner, block);
    if (!created)
        return {std::move(created.Detail), 0, {}};
    RunResult result = m_Runtime.StartTask(created.Handle, input);
    if (result.State == RunState::Pending) {
        Status continued = m_Runtime.Continue(created.Handle);
        if (!continued && result.Detail)
            result.Detail = std::move(continued);
    }
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Session *current = FindSession(session.Id);
    if (!current || current->OwnerGeneration != session.OwnerGeneration) {
        created.Handle.Reset();
        m_Runtime.ClosePending();
        return {Fail(Error::InvalidState,
                     "The Behavior session closed during Start."), 0, {}};
    }
    return AddRun(session, RunKind::Task, std::move(created.Handle),
                  std::move(result), created.UnverifiedDetached);
}

OpenRun Sessions::Spawn(std::uintptr_t sessionId, CKBeObject *owner,
                         const Spec &block) {
    Status ready = Ready();
    if (!ready)
        return {std::move(ready), 0, {}};
    Session session;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        Session *found = FindSession(sessionId);
        if (!found || !SessionIsActive(*found))
            return {Fail(Error::InvalidState, "The Behavior session is stale."), 0, {}};
        session = *found;
    }
    CreateResult created = m_Runtime.Instantiate(owner, block);
    if (!created)
        return {std::move(created.Detail), 0, {}};
    RunResult result;
    result.State = RunState::Ready;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Session *current = FindSession(session.Id);
    if (!current || current->OwnerGeneration != session.OwnerGeneration) {
        created.Handle.Reset();
        m_Runtime.ClosePending();
        return {Fail(Error::InvalidState,
                     "The Behavior session closed during Spawn."), 0, {}};
    }
    return AddRun(session, RunKind::Instance, std::move(created.Handle),
                  std::move(result), created.UnverifiedDetached);
}

RunResult Sessions::Continue(std::uintptr_t runId) {
    Status ready = Ready();
    if (!ready)
        return {std::move(ready), RunState::Failed, CKBR_BEHAVIORERROR, {}};
    std::shared_ptr<Run> run;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        run = FindRun(runId);
        if (!run || run->Info.Kind != RunKind::Call || !run->Block)
            return FailedRun(Error::InvalidState, "The Run is not a pending Call.");
    }
    Status status = m_Runtime.Continue(run->Block);
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        run->Info.LastStatus = status;
        run->Info.State = RunStateOf(m_Runtime.State(run->Block));
        if (status)
            run->Info.Kind = RunKind::Task;
    }
    return {std::move(status), run->Info.State, CKBR_OK, {}};
}

RunResult Sessions::Pulse(std::uintptr_t runId, const Slot &input) {
    Status ready = Ready();
    if (!ready)
        return {std::move(ready), RunState::Failed, CKBR_BEHAVIORERROR, {}};
    std::shared_ptr<Run> run;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        run = FindRun(runId);
        if (!run || run->Info.Kind == RunKind::Call || !run->Block)
            return FailedRun(Error::InvalidState,
                             "Pulse requires a live Task or Instance Run.");
    }
    RunResult result = m_Runtime.Pulse(run->Block, input);
    if (result.State == RunState::Pending) {
        Status continued = m_Runtime.Continue(run->Block);
        if (!continued && result.Detail)
            result.Detail = std::move(continued);
    }
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        run->Info.LastStatus = result.Detail;
        run->Info.State = RunStateOf(m_Runtime.State(run->Block));
    }
    return result;
}

Status Sessions::ReadRun(std::uintptr_t runId, RunInfo &info) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const std::shared_ptr<const Run> run = FindRun(runId);
    if (!run)
        return Fail(Error::InvalidState, "The Behavior Run is stale.");
    info = run->Info;
    if (run->Block)
        info.State = RunStateOf(m_Runtime.State(run->Block));
    return {};
}

Status Sessions::FindPrototypes(std::uintptr_t sessionId,
                                const PrototypeQuery &query,
                                std::vector<PrototypeInfo> &out) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    Session *session = FindSession(sessionId);
    if (!session || !SessionIsActive(*session))
        return Fail(Error::InvalidState,
                    "Behavior Session is stale or retiring.");
    if (!m_Catalog || !m_Catalog->TracksRetirement())
        return Fail(Error::Unavailable,
                    "Behavior Prototype discovery is unavailable.");
    return m_Catalog->Find(query, out);
}

Status Sessions::ReadDeclaredLayout(std::uintptr_t sessionId,
                                    PrototypeRef prototype, Layout &out) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    Session *session = FindSession(sessionId);
    if (!session || !SessionIsActive(*session))
        return Fail(Error::InvalidState,
                    "Behavior Session is stale or retiring.");
    if (!m_Catalog || !m_Catalog->TracksRetirement())
        return Fail(Error::Unavailable,
                    "Behavior Prototype discovery is unavailable.");
    return m_Catalog->DeclaredLayout(prototype, out);
}

Status Sessions::ReadLiveLayout(std::uintptr_t runId, Layout &out) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    std::shared_ptr<const Run> run = FindRun(runId);
    if (!run)
        return Fail(Error::InvalidState,
                    "Behavior Run handle is stale.");
    return m_Runtime.Describe(run->Block, out);
}

Status Sessions::Set(std::uintptr_t runId,
                     std::uint64_t layoutGeneration, const Slot &slot,
                     const Parameter::Binding &value,
                     std::uint64_t &currentGeneration) {
    currentGeneration = 0;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    std::shared_ptr<Run> run = FindRun(runId);
    if (!run || !run->Block)
        return Fail(Error::InvalidState, "Behavior Run handle is stale.");
    if (layoutGeneration &&
        run->Block.LayoutGeneration() != layoutGeneration) {
        return Fail(Error::StaleLayout,
                    "The live Behavior Layout has changed.");
    }
    SlotRef resolved;
    Status status = m_Runtime.Resolve(run->Block, slot, resolved);
    if (!status)
        return status;
    if (slot.Kind == SlotKind::InputParameter)
        status = m_Runtime.SetInput(run->Block, resolved, value);
    else if (slot.Kind == SlotKind::Local)
        status = m_Runtime.SetLocal(run->Block, resolved, value);
    else
        return Fail(Error::InvalidState,
                    "Set accepts a live Pin or Local.");
    if (status)
        currentGeneration = run->Block.LayoutGeneration();
    return status;
}

Status Sessions::Bind(std::uintptr_t runId,
                      std::uint64_t layoutGeneration, const Slot &slot,
                      CKBehavior *source, const Slot &sourceSlot,
                      Parameter::BindingKind relation,
                      std::uint64_t &currentGeneration) {
    currentGeneration = 0;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    std::shared_ptr<Run> run = FindRun(runId);
    if (!run || !run->Block)
        return Fail(Error::InvalidState, "Behavior Run handle is stale.");
    if (layoutGeneration &&
        run->Block.LayoutGeneration() != layoutGeneration) {
        return Fail(Error::StaleLayout,
                    "The live Behavior Layout has changed.");
    }
    if (slot.Kind != SlotKind::InputParameter)
        return Fail(Error::InvalidState, "Bind accepts a live Pin.");
    SlotRef resolved;
    Status status = m_Runtime.Resolve(run->Block, slot, resolved);
    if (status)
        status = m_Runtime.Bind(run->Block, resolved, source,
                                sourceSlot, relation);
    if (status)
        currentGeneration = run->Block.LayoutGeneration();
    return status;
}

Status Sessions::Configure(std::uintptr_t runId, const Spec &settings,
                           std::uint64_t &layoutGeneration) {
    layoutGeneration = 0;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    std::shared_ptr<Run> run = FindRun(runId);
    if (!run || !run->Block)
        return Fail(Error::InvalidState, "Behavior Run handle is stale.");
    Status status = m_Runtime.Configure(run->Block, settings);
    if (status)
        layoutGeneration = run->Block.LayoutGeneration();
    return status;
}

Status Sessions::ReadGraph(std::uintptr_t runId, GraphView view,
                           GraphModel &out) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    std::shared_ptr<Run> run = FindRun(runId);
    if (!run)
        return Fail(Error::InvalidState,
                    "Behavior Run handle is stale.");
    CKBehavior *behavior = run->Block.Get();
    if (!behavior)
        return Fail(Error::InvalidState,
                    "The Behavior owned by this Run is stale.");
    if (!m_Graph)
        return Fail(Error::Unavailable,
                    "Behavior graph inspection is unavailable.");
    NativeRef reference;
    Status status = m_Graph->Refer(behavior, reference);
    return status ? m_Graph->Read(reference, view, out) : status;
}

Status Sessions::ReadGraph(std::uintptr_t sessionId, void *root,
                           GraphView view, GraphModel &out) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    Session *session = FindSession(sessionId);
    if (!session || !SessionIsActive(*session))
        return Fail(Error::InvalidState,
                    "Behavior Session is stale or retiring.");
    if (!m_Graph)
        return Fail(Error::Unavailable,
                    "Behavior graph inspection is unavailable.");
    NativeRef reference;
    Status status = m_Graph->Refer(root, reference);
    return status ? m_Graph->Read(reference, view, out) : status;
}

Status Sessions::ReadNodeLayout(std::uintptr_t sessionId, void *node,
                                Layout &out) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    Session *session = FindSession(sessionId);
    if (!session || !SessionIsActive(*session))
        return Fail(Error::InvalidState,
                    "Behavior Session is stale or retiring.");
    if (!m_Graph)
        return Fail(Error::Unavailable,
                    "Behavior graph inspection is unavailable.");
    NativeRef reference;
    Status status = m_Graph->Refer(node, reference);
    return status ? m_Graph->ReadLayout(reference, out) : status;
}

Status Sessions::ReadGraphValue(std::uintptr_t sessionId, void *node,
                                const Slot &slot, ReadMode mode,
                                GraphValue &out) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    Session *session = FindSession(sessionId);
    if (!session || !SessionIsActive(*session))
        return Fail(Error::InvalidState,
                    "Behavior Session is stale or retiring.");
    if (!m_Graph)
        return Fail(Error::Unavailable,
                    "Behavior graph inspection is unavailable.");
    NativeRef reference;
    Status status = m_Graph->Refer(node, reference);
    return status ? m_Graph->ReadValue(reference, slot, mode, out) : status;
}

Status Sessions::OpenWatch(std::uintptr_t sessionId, void *root, void *node,
                           WatchSpec spec, PlanCallbackState state,
                           WatchBinding::Function callback,
                           std::uintptr_t &watchId) {
    watchId = 0;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status ready = Ready();
    if (!ready)
        return ready;
    Session *session = FindSession(sessionId);
    if (!session || !SessionIsActive(*session))
        return Fail(Error::InvalidState,
                    "Behavior Session is stale or retiring.");
    if (!m_Graph)
        return Fail(Error::Unavailable,
                    "Behavior graph observation is unavailable.");
    if (spec.Kind == WatchKind::ExactValueChanged)
        return Fail(
            Error::ObserverUnavailable,
            "CK2.1 does not provide a portable exact parameter-change observer.");

    Status status;
    if (root) {
        status = m_Graph->Refer(root, spec.Root);
        if (!status)
            return status;
    }
    if (node) {
        status = m_Graph->Refer(node, spec.Node);
        if (!status)
            return status;
    }
    std::shared_ptr<Watch> watch;
    status = Watch::Open(*m_Graph, std::move(spec), std::move(state),
                         std::move(callback), watch);
    if (!status)
        return status;
    const std::uintptr_t id = NextId();
    if (!id) {
        watch->Close();
        QueueWatch(std::move(watch));
        return Fail(Error::InvalidState, "Behavior Watch ids are exhausted.");
    }
    m_Watches.emplace(
        id, OwnedWatch{id, session->Id, session->OwnerId,
                       session->OwnerGeneration, std::move(watch)});
    watchId = id;
    return {};
}

void Sessions::CloseWatch(std::uintptr_t watchId) {
    if (!watchId)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    auto watch = m_Watches.find(watchId);
    if (watch == m_Watches.end())
        return;
    QueueWatch(std::move(watch->second.Value));
    m_Watches.erase(watch);
}

std::shared_ptr<FrameStore> Sessions::Frames(std::uintptr_t runId) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const std::shared_ptr<const Run> run = FindRun(runId);
    return run ? run->Frames : nullptr;
}

void Sessions::CloseRun(std::uintptr_t runId) {
    if (!runId)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    auto run = m_Runs.find(runId);
    if (run == m_Runs.end())
        return;
    QueueClose(std::move(run->second));
    m_Runs.erase(run);
}

void Sessions::ProcessFrame() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::uint64_t frame = 0;
    std::vector<std::pair<std::uintptr_t, std::shared_ptr<Watch>>> watches;
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        frame = ++m_Frame;
        CloseQueuedRuns();
        for (auto &[id, entry] : m_Runs) {
            Run &run = *entry;
            if (!run.Block)
                continue;
            run.Info.State = RunStateOf(m_Runtime.State(run.Block));
        }
        watches.reserve(m_Watches.size());
        for (const auto &[id, watch] : m_Watches)
            watches.emplace_back(id, watch.Value);
    }

    for (const auto &[id, watch] : watches) {
        {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            const auto current = m_Watches.find(id);
            if (current == m_Watches.end() || current->second.Value != watch)
                continue;
        }
        Status status = watch->Poll(frame);
        if (!status || !watch->IsOpen()) {
            std::lock_guard<std::recursive_mutex> lock(m_Mutex);
            const auto current = m_Watches.find(id);
            if (current != m_Watches.end() &&
                current->second.Value == watch) {
                QueueWatch(std::move(current->second.Value));
                m_Watches.erase(current);
            }
        }
    }
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        CollectWatches();
        m_Runtime.ClosePending();
    }
}

void Sessions::ResetWorld() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (auto &[id, run] : m_Runs)
        QueueClose(std::move(run));
    m_Runs.clear();
    for (auto &[id, watch] : m_Watches)
        QueueWatch(std::move(watch.Value));
    m_Watches.clear();
    CloseQueuedRuns();
    CollectWatches();
    m_Runtime.ClosePending();
}

Status Sessions::Ready() const {
    return std::this_thread::get_id() == m_Thread
        ? Status{}
        : Fail(Error::WrongThread,
               "Behavior authoring is only available on the game thread.");
}

std::uintptr_t Sessions::NextId() {
    if (m_NextId == 0 || m_NextId ==
            (std::numeric_limits<std::uintptr_t>::max)()) {
        return 0;
    }
    return m_NextId++;
}

Sessions::Session *Sessions::FindSession(std::uintptr_t sessionId) {
    const auto session = m_Sessions.find(sessionId);
    return session == m_Sessions.end() ? nullptr : &session->second;
}

const Sessions::Session *Sessions::FindSession(std::uintptr_t sessionId) const {
    const auto session = m_Sessions.find(sessionId);
    return session == m_Sessions.end() ? nullptr : &session->second;
}

std::shared_ptr<Sessions::Run> Sessions::FindRun(std::uintptr_t runId) {
    const auto run = m_Runs.find(runId);
    return run == m_Runs.end() ? nullptr : run->second;
}

std::shared_ptr<const Sessions::Run> Sessions::FindRun(
    std::uintptr_t runId) const {
    const auto run = m_Runs.find(runId);
    return run == m_Runs.end() ? nullptr : run->second;
}

bool Sessions::SessionIsActive(const Session &session) const {
    const auto owner = m_Owners.find(session.OwnerId);
    return owner != m_Owners.end() && owner->second.State == OwnerState::Active &&
           owner->second.Generation == session.OwnerGeneration;
}

OpenRun Sessions::AddRun(const Session &session, RunKind kind,
                          Instance block, RunResult result,
                          bool unverifiedDetached) {
    std::shared_ptr<FrameStore> frames = m_Runtime.Frames(block);
    const bool nativeExecuted = frames && !frames->Read().empty();
    if (!result && !nativeExecuted) {
        block.Reset();
        m_Runtime.ClosePending();
        return {std::move(result.Detail), 0, {}};
    }
    const std::uintptr_t id = NextId();
    if (!id) {
        block.Reset();
        m_Runtime.ClosePending();
        return {Fail(Error::InvalidState, "Behavior Run ids are exhausted."), 0, {}};
    }

    auto run = std::make_shared<Run>();
    run->Id = id;
    run->SessionId = session.Id;
    run->OwnerId = session.OwnerId;
    run->OwnerGeneration = session.OwnerGeneration;
    run->Info.Kind = kind;
    run->Info.State = result.State;
    run->Info.LastStatus = result.Detail;
    run->Info.UnverifiedDetached = unverifiedDetached;
    run->Block = std::move(block);
    run->Frames = std::move(frames);
    auto [stored, inserted] = m_Runs.emplace(id, std::move(run));
    if (!inserted)
        return {Fail(Error::InvalidState, "Behavior Run id collision."), 0, {}};

    // A native error is a Frame, not an admission failure.
    Status admitted;
    return {std::move(admitted), id, stored->second->Info};
}

void Sessions::QueueClose(std::shared_ptr<Run> run) {
    if (run)
        m_CloseQueue.push_back(std::move(run));
}

void Sessions::CloseQueuedRuns() {
    for (const std::shared_ptr<Run> &run : m_CloseQueue) {
        if (run)
            run->Block.Reset();
    }
    m_CloseQueue.clear();
}

void Sessions::CloseOwner(const std::string &ownerId,
                           std::uint64_t generation) {
    auto owner = m_Owners.find(ownerId);
    if (owner == m_Owners.end() || owner->second.Generation != generation)
        return;
    owner->second.State = OwnerState::Retiring;
    for (auto run = m_Runs.begin(); run != m_Runs.end();) {
        if (run->second->OwnerId != ownerId ||
            run->second->OwnerGeneration != generation) {
            ++run;
            continue;
        }
        QueueClose(std::move(run->second));
        run = m_Runs.erase(run);
    }
    for (auto session = m_Sessions.begin(); session != m_Sessions.end();) {
        if (session->second.OwnerId == ownerId &&
            session->second.OwnerGeneration == generation)
            session = m_Sessions.erase(session);
        else
            ++session;
    }
    for (auto watch = m_Watches.begin(); watch != m_Watches.end();) {
        if (watch->second.OwnerId != ownerId ||
            watch->second.OwnerGeneration != generation) {
            ++watch;
            continue;
        }
        QueueWatch(std::move(watch->second.Value));
        watch = m_Watches.erase(watch);
    }
    owner->second.State = OwnerState::Draining;
    CloseQueuedRuns();
    CollectWatches();
    m_Runtime.ClosePending();
    owner->second.State = OwnerState::Released;
}

void Sessions::QueueWatch(std::shared_ptr<Watch> watch) {
    if (!watch)
        return;
    watch->Close();
    m_ClosingWatches.push_back(std::move(watch));
}

void Sessions::CollectWatches() {
    for (auto watch = m_ClosingWatches.begin();
         watch != m_ClosingWatches.end();) {
        if ((*watch)->RetireAtSafePoint())
            watch = m_ClosingWatches.erase(watch);
        else
            ++watch;
    }
}

} // namespace BML::Behavior
