#include "Behavior/Patches.h"

#include <limits>
#include <set>
#include <utility>

namespace BML::Behavior {
namespace {

Status Failure(Error error, std::string message,
               Phase phase = Phase::Edit) {
    Status status{error, CKERR_INVALIDPARAMETER, CKBR_PARAMETERERROR,
                  std::move(message)};
    status.Details.Stage = phase;
    return status;
}

} // namespace

Patches::Patches(CKContext *context, Runtime &runtime,
                 PrototypeCatalog *catalog, GraphSource &graph)
    : m_Edit(context, runtime, catalog, graph),
      m_Thread(std::this_thread::get_id()) {}

Patches::~Patches() {
    ResetWorld();
}

Status Patches::Ready() const {
    return std::this_thread::get_id() == m_Thread
        ? Status{}
        : Failure(Error::WrongThread,
                  "Behavior Patches require the game thread.");
}

PatchId Patches::NextId() {
    if (m_NextId == 0 ||
        m_NextId == (std::numeric_limits<PatchId>::max)())
        return 0;
    return m_NextId++;
}

Status Patches::Begin(const SessionOwner &owner, CKBehavior *graph,
                      std::string name, Edit &out) {
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || name.empty())
        return Failure(Error::InvalidState,
                       "A Behavior Patch requires a live owner and name.");
    return m_Edit.Begin(graph, {owner.Id, std::move(name)}, out);
}

Status Patches::Use(Edit &edit, CKBehavior *behavior, Node &out) {
    return m_Edit.Use(edit, behavior, out);
}

Status Patches::Use(Edit &edit, CKBehaviorLink *link, Link &out) {
    return m_Edit.Use(edit, link, out);
}

Status Patches::Add(Edit &edit, Spec block, Node &out) {
    return m_Edit.Add(edit, std::move(block), out);
}

Status Patches::Apply(const SessionOwner &owner, const Edit &edit,
                      PatchId &out) {
    out = 0;
    Status status = Ready();
    if (!status)
        return status;
    if (!owner || edit.Key().Owner != owner.Id || edit.Key().Name.empty())
        return Failure(Error::OwnerInvalid,
                       "The Behavior Edit does not belong to this Mod generation.");

    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const PatchId id = NextId();
    if (!id)
        return Failure(Error::InvalidState,
                       "Behavior Patch ids are exhausted.");

    const NativeRef graph = edit.GraphRef();
    if (!graph || graph.Id >
            static_cast<std::uint64_t>((std::numeric_limits<CK_ID>::max)())) {
        return Failure(Error::InvalidGraphLocality,
                       "The Behavior Edit graph has no live CK identity.");
    }

    Patch patch;
    status = m_Edit.Apply(edit, patch);
    if (!patch)
        return status;

    try {
        auto [stored, inserted] = m_Patches.try_emplace(id);
        if (!inserted) {
            (void) m_Edit.Close(patch);
            return Failure(Error::InvalidState,
                           "Behavior Patch id collision.");
        }
        try {
            stored->second.Id = id;
            stored->second.Owner = owner;
            stored->second.Graph = static_cast<CK_ID>(graph.Id);
            stored->second.Value = std::move(patch);
        } catch (...) {
            m_Patches.erase(stored);
            throw;
        }
    } catch (...) {
        (void) m_Edit.Close(patch);
        return Failure(Error::CreateFailed,
                       "The Loader could not retain the Behavior Patch.");
    }
    // A failed Apply normally rolls back completely and has no Patch value.
    // RevertConflict is different: retain its repair journal under the owner,
    // but do not hand a successful installation id to the caller.
    if (status)
        out = id;
    return status;
}

Status Patches::Read(const SessionOwner &owner, PatchId patch,
                     PatchInfo &out) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end() ||
        found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::InvalidState,
                       "The Behavior Patch handle is stale.");
    out.State = found->second.Value.State();
    out.Diagnostic = found->second.Value.Diagnostic();
    return {};
}

Status Patches::Close(OwnedPatch &patch) {
    return m_Edit.Close(patch.Value);
}

Status Patches::Close(const SessionOwner &owner, PatchId patch) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const auto found = m_Patches.find(patch);
    if (found == m_Patches.end())
        return {};
    if (found->second.Owner.Id != owner.Id ||
        found->second.Owner.Generation != owner.Generation)
        return Failure(Error::OwnerInvalid,
                       "The Behavior Patch belongs to another Mod generation.");
    Status status = Close(found->second);
    const PatchState state = found->second.Value.State();
    if (state == PatchState::Closed || state == PatchState::Failed)
        m_Patches.erase(found);
    return status;
}

Status Patches::RetireOwner(const std::string &ownerId) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    Status first;
    for (auto &[id, patch] : m_Patches) {
        if (patch.Owner.Id != ownerId)
            continue;
        Status status = Close(patch);
        if (!status && first)
            first = std::move(status);
    }
    m_Edit.ProcessFrame();
    Collect();
    return first;
}

void Patches::ObjectsToBeDeleted(const CK_ID *ids, int count) {
    if (!ids || count <= 0 || std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    const std::set<CK_ID> deleting(ids, ids + count);
    for (auto &[id, patch] : m_Patches) {
        if (deleting.contains(patch.Graph))
            (void) Close(patch);
    }
    m_Edit.ProcessFrame();
    Collect();
}

void Patches::ResetWorld() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (auto &[id, patch] : m_Patches)
        (void) Close(patch);
    m_Edit.ProcessFrame();
    Collect();

    // A conflicting external edit may prevent exact restoration. Admission is
    // already closed, and the CK world is about to disappear; do not retain a
    // journal whose native identities belong to the old world.
    m_Patches.clear();
}

void Patches::ProcessFrame() {
    if (std::this_thread::get_id() != m_Thread)
        return;
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_Edit.ProcessFrame();
    Collect();
}

void Patches::Collect() {
    for (auto patch = m_Patches.begin(); patch != m_Patches.end();) {
        const PatchState state = patch->second.Value.State();
        if (state == PatchState::Closed || state == PatchState::Failed)
            patch = m_Patches.erase(patch);
        else
            ++patch;
    }
}

} // namespace BML::Behavior
