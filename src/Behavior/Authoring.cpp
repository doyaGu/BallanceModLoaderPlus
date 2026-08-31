#include "Behavior/Authoring.h"

#include <limits>
#include <utility>

#include "Behavior/OutcomeStore.h"

namespace BML::Behavior {
namespace {

Status Fail(Error error, std::string message) {
    return {error, CK_OK, CKBR_OK, std::move(message)};
}

RunResult FailedRun(Error error, std::string message) {
    return {Fail(error, std::move(message)), RunState::Failed,
            CKBR_BEHAVIORERROR, {}};
}

} // namespace

Authoring::Authoring(Runtime &runtime)
    : m_Runtime(runtime), m_Thread(std::this_thread::get_id()) {}

std::uint64_t Authoring::RegisterOwner(std::string ownerId) {
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

void Authoring::RetireOwner(const std::string &ownerId) {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    auto owner = m_Owners.find(ownerId);
    if (owner == m_Owners.end() || owner->second.State == OwnerState::Released)
        return;
    CloseOwner(ownerId, owner->second.Generation);
}

Status Authoring::OpenSession(const std::string &ownerId,
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

void Authoring::CloseSession(std::uintptr_t sessionId) {
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
    m_Sessions.erase(session);
}

OpenRun Authoring::Call(std::uintptr_t sessionId, CKBeObject *owner,
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
        return {std::move(called.Outcome), 0, {}};
    RunResult result = std::move(called.Run);
    if (result.Outcome && !called.Outcome)
        result.Outcome = std::move(called.Outcome);
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Session *current = FindSession(session.Id);
    if (!current || current->OwnerGeneration != session.OwnerGeneration) {
        called.Handle.Reset();
        m_Runtime.ClosePending();
        return {Fail(Error::InvalidState,
                     "The Behavior session closed during Call."), 0, {}};
    }
    return AddRun(session, RunKind::Call, std::move(called.Handle), std::move(result));
}

OpenRun Authoring::Start(std::uintptr_t sessionId, CKBeObject *owner,
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
        return {std::move(created.Outcome), 0, {}};
    RunResult result = m_Runtime.StartTask(created.Handle, input);
    if (result.State == RunState::Pending || result.State == RunState::Queued) {
        Status continued = m_Runtime.Continue(created.Handle);
        if (!continued && result.Outcome)
            result.Outcome = std::move(continued);
    }
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Session *current = FindSession(session.Id);
    if (!current || current->OwnerGeneration != session.OwnerGeneration) {
        created.Handle.Reset();
        m_Runtime.ClosePending();
        return {Fail(Error::InvalidState,
                     "The Behavior session closed during Start."), 0, {}};
    }
    return AddRun(session, RunKind::Task, std::move(created.Handle), std::move(result));
}

OpenRun Authoring::Spawn(std::uintptr_t sessionId, CKBeObject *owner,
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
        return {std::move(created.Outcome), 0, {}};
    RunResult result;
    result.State = RunState::Completed;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Session *current = FindSession(session.Id);
    if (!current || current->OwnerGeneration != session.OwnerGeneration) {
        created.Handle.Reset();
        m_Runtime.ClosePending();
        return {Fail(Error::InvalidState,
                     "The Behavior session closed during Spawn."), 0, {}};
    }
    return AddRun(session, RunKind::Instance, std::move(created.Handle), std::move(result));
}

RunResult Authoring::Continue(std::uintptr_t runId) {
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
        run->Info.State = status ? RunState::Pending : RunState::Failed;
    }
    return {std::move(status), run->Info.State, CKBR_OK, {}};
}

RunResult Authoring::Pulse(std::uintptr_t runId, const Slot &input) {
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
    if (result.State == RunState::Pending || result.State == RunState::Queued) {
        Status continued = m_Runtime.Continue(run->Block);
        if (!continued && result.Outcome)
            result.Outcome = std::move(continued);
    }
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        run->Info.LastStatus = result.Outcome;
        run->Info.State = result.State;
    }
    return result;
}

Status Authoring::ReadRun(std::uintptr_t runId, RunInfo &info) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const std::shared_ptr<const Run> run = FindRun(runId);
    if (!run)
        return Fail(Error::InvalidState, "The Behavior Run is stale.");
    info = run->Info;
    if (run->Block) {
        const ExecutionState state = m_Runtime.State(run->Block);
        if (state == ExecutionState::Pending || state == ExecutionState::Running)
            info.State = RunState::Pending;
        else if (state == ExecutionState::Failed ||
                 state == ExecutionState::Closing ||
                 state == ExecutionState::Closed)
            info.State = RunState::Failed;
    }
    return {};
}

std::shared_ptr<OutcomeStore> Authoring::Outcomes(std::uintptr_t runId) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const std::shared_ptr<const Run> run = FindRun(runId);
    return run ? run->Outcome : nullptr;
}

void Authoring::CloseRun(std::uintptr_t runId) {
    if (!runId)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    auto run = m_Runs.find(runId);
    if (run == m_Runs.end())
        return;
    QueueClose(std::move(run->second));
    m_Runs.erase(run);
}

void Authoring::ProcessFrame() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    CloseQueuedRuns();
    for (auto &[id, entry] : m_Runs) {
        Run &run = *entry;
        if (!run.Block)
            continue;
        const ExecutionState state = m_Runtime.State(run.Block);
        if (state == ExecutionState::Pending || state == ExecutionState::Running)
            run.Info.State = RunState::Pending;
        else {
            run.Info.State = state == ExecutionState::Failed
                ? RunState::Failed : RunState::Completed;
            if (run.Info.Kind != RunKind::Instance ||
                state == ExecutionState::Failed ||
                state == ExecutionState::Closing ||
                state == ExecutionState::Closed)
                CloseNative(run);
        }
    }
    m_Runtime.ClosePending();
}

void Authoring::ResetWorld() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (auto &[id, run] : m_Runs)
        QueueClose(std::move(run));
    m_Runs.clear();
    CloseQueuedRuns();
    m_Runtime.ClosePending();
}

Status Authoring::Ready() const {
    return std::this_thread::get_id() == m_Thread
        ? Status{}
        : Fail(Error::WrongThread,
               "Behavior authoring is only available on the game thread.");
}

std::uintptr_t Authoring::NextId() {
    if (m_NextId == 0 || m_NextId ==
            (std::numeric_limits<std::uintptr_t>::max)()) {
        return 0;
    }
    return m_NextId++;
}

Authoring::Session *Authoring::FindSession(std::uintptr_t sessionId) {
    const auto session = m_Sessions.find(sessionId);
    return session == m_Sessions.end() ? nullptr : &session->second;
}

const Authoring::Session *Authoring::FindSession(std::uintptr_t sessionId) const {
    const auto session = m_Sessions.find(sessionId);
    return session == m_Sessions.end() ? nullptr : &session->second;
}

std::shared_ptr<Authoring::Run> Authoring::FindRun(std::uintptr_t runId) {
    const auto run = m_Runs.find(runId);
    return run == m_Runs.end() ? nullptr : run->second;
}

std::shared_ptr<const Authoring::Run> Authoring::FindRun(
    std::uintptr_t runId) const {
    const auto run = m_Runs.find(runId);
    return run == m_Runs.end() ? nullptr : run->second;
}

bool Authoring::SessionIsActive(const Session &session) const {
    const auto owner = m_Owners.find(session.OwnerId);
    return owner != m_Owners.end() && owner->second.State == OwnerState::Active &&
           owner->second.Generation == session.OwnerGeneration;
}

OpenRun Authoring::AddRun(const Session &session, RunKind kind,
                          Instance block, RunResult result) {
    std::shared_ptr<OutcomeStore> outcomes = m_Runtime.Outcomes(block);
    const bool nativeExecuted = outcomes && !outcomes->Read().empty();
    if (!result && !nativeExecuted) {
        block.Reset();
        m_Runtime.ClosePending();
        return {std::move(result.Outcome), 0, {}};
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
    run->Info.LastStatus = result.Outcome;
    run->Block = std::move(block);
    run->Outcome = std::move(outcomes);
    auto [stored, inserted] = m_Runs.emplace(id, std::move(run));
    if (!inserted)
        return {Fail(Error::InvalidState, "Behavior Run id collision."), 0, {}};

    if (kind == RunKind::Call && result.State != RunState::Pending)
        CloseNative(*stored->second);
    else if (kind == RunKind::Task && result.State != RunState::Pending &&
             result.State != RunState::Queued)
        CloseNative(*stored->second);

    // A native error is an Outcome, not an admission failure.
    Status admitted;
    return {std::move(admitted), id, stored->second->Info};
}

void Authoring::CloseNative(Run &run) {
    run.Block.Reset();
}

void Authoring::QueueClose(std::shared_ptr<Run> run) {
    if (run)
        m_CloseQueue.push_back(std::move(run));
}

void Authoring::CloseQueuedRuns() {
    for (const std::shared_ptr<Run> &run : m_CloseQueue) {
        if (run)
            run->Block.Reset();
    }
    m_CloseQueue.clear();
}

void Authoring::CloseOwner(const std::string &ownerId,
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
    owner->second.State = OwnerState::Draining;
    CloseQueuedRuns();
    m_Runtime.ClosePending();
    owner->second.State = OwnerState::Released;
}

} // namespace BML::Behavior
